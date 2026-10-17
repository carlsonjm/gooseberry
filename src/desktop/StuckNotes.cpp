// SPDX-License-Identifier: GPL-2.0-or-later
#include "StuckNotes.h"

#include "Capture.h"
#include "Log.h"
#include "NoteStore.h"
#include "WindowContext.h"

#include <LayerShellQt/Window>

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>

#include <algorithm>

namespace Gooseberry {

namespace {

bool onWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

} // namespace

StuckNotes::StuckNotes(NoteStore *store, WindowContext *context, std::function<QQuickWindow *()> makeSurface,
                       QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_context(context)
    , m_makeSurface(std::move(makeSurface))
    , m_writer(new Capture(store, this))
{
    // Words changed elsewhere while a note is typed on reach the surface.
    connect(m_writer, &Capture::textChanged, this, &StuckNotes::writingTextChanged);
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, &StuckNotes::refresh);
    m_foldLater.setSingleShot(true);
    m_foldLater.setInterval(150);
    connect(&m_foldLater, &QTimer::timeout, this, &StuckNotes::foldIfLeft);
    const auto later = [this] {
        m_refresh.start();
    };
    connect(m_store, &NoteStore::noteAdded, this, later);
    connect(m_store, &NoteStore::noteChanged, this, later);
    connect(m_store, &NoteStore::noteRemoved, this, later);
    connect(m_store, &NoteStore::foldersChanged, this, later);
    connect(m_context, &WindowContext::windowsChanged, this, later);
    refresh();
}

StuckNotes::~StuckNotes()
{
    // Typing still waiting for a pause is kept before anything goes.
    m_writer->flush();
    delete m_surface;
}

bool StuckNotes::publish()
{
    return QDBusConnection::sessionBus().registerObject(QString::fromLatin1(Path), this,
                                                        QDBusConnection::ExportScriptableContents);
}

uint StuckNotes::ProtocolVersion() const
{
    return Version;
}

QVariantList StuckNotes::Windows() const
{
    return m_entries;
}

const OpenWindow *StuckNotes::drawnWindow() const
{
    if (m_drawnKey.isEmpty()) {
        return nullptr;
    }
    for (const OpenWindow &window : m_windows) {
        if (window.key() == m_drawnKey) {
            return &window;
        }
    }
    return nullptr;
}

QVariantList StuckNotes::shownNotes() const
{
    return m_shownNotes;
}

QVariantList StuckNotes::currentShownNotes() const
{
    QVariantList list;
    const OpenWindow *window = drawnWindow();
    if (!window) {
        return list;
    }
    QList<Note> notes = StuckWindows::notesOn(*window, m_store->notes());
    if (!m_writingId.isEmpty()) {
        // The note typed on becomes the one changed last; the notes keep the
        // places they had when typing began, so none moves under the words.
        QStringList order;
        for (const QVariant &shown : std::as_const(m_shownNotes)) {
            order.append(shown.toMap().value(QStringLiteral("id")).toString());
        }
        notes = StuckWindows::keptInOrder(notes, order);
    }
    for (const Note &note : std::as_const(notes)) {
        const bool writing = note.id == m_writingId && !m_writingShown.isEmpty();
        list.append(QVariantMap{
            {QStringLiteral("id"), note.id},
            // Its words as typing began: the surface holds the words typed
            // since, and a change here would make the note anew under them.
            {QStringLiteral("title"), writing ? m_writingShown.value(QStringLiteral("title")).toString() : note.title()},
            {QStringLiteral("text"), writing ? m_writingShown.value(QStringLiteral("text")).toString() : note.text},
            {QStringLiteral("colourHex"), colourHex(note.colour)},
            {QStringLiteral("x"), note.place.x()},
            {QStringLiteral("y"), note.place.y()},
            {QStringLiteral("width"), note.hasSize() ? note.size.width() : -1.0},
            {QStringLiteral("height"), note.hasSize() ? note.size.height() : -1.0},
        });
    }
    return list;
}

void StuckNotes::refresh()
{
    m_windows = m_context->windows();
    // A window that closed, or whose last note came off, has nothing up.
    QSet<QString> open;
    QString active;
    for (const OpenWindow &window : std::as_const(m_windows)) {
        if (!StuckWindows::notesOn(window, m_store->notes()).isEmpty()) {
            open.insert(window.key());
        }
        if (window.active) {
            active = window.key();
        }
    }
    m_shownKeys.intersect(open);
    m_foldedKeys.intersect(m_shownKeys);
    // The notes are drawn over the window in front when it has them up; a
    // desktop that says no window is in front leaves them where they were,
    // and so does one that has not yet said the window they were brought up
    // on came to the front.
    if (!active.isEmpty() && active != m_activeWhenShown) {
        m_activeWhenShown.clear();
    }
    const bool behind = !active.isEmpty() && active == m_activeWhenShown;
    if ((!active.isEmpty() && !behind) || !m_shownKeys.contains(m_drawnKey)) {
        QString over = m_shownKeys.contains(active) ? active : QString();
        // A window brought to the front on another display does not cover
        // these notes: they stay over their own window, folded once the keys
        // have gone to it, as for a tap on any other window.
        if (over.isEmpty() && !active.isEmpty() && m_shownKeys.contains(m_drawnKey)
            && onOtherDisplays(active, m_drawnKey)) {
            over = m_drawnKey;
        }
        drawOver(over);
    }
    const bool drawn = !m_paused && !m_cardOpen && !m_drawnKey.isEmpty();
    const bool folded = !m_drawnKey.isEmpty() && m_foldedKeys.contains(m_drawnKey);
    // Typing on a note ends when it is no longer up over its window, or when
    // another window's notes are drawn. While the notes step aside for
    // Spread it waits, so a note asked for from Spread is ready on the window.
    bool writingEnded = false;
    if (!m_writingId.isEmpty()
        && (!m_shownKeys.contains(m_writingKey) || !noteOnWindow(m_writingId, m_writingKey)
            || (drawn && m_drawnKey != m_writingKey))) {
        writingEnded = endWriting();
    }
    if (drawn) {
        if (!m_surface) {
            m_surface = m_makeSurface ? m_makeSurface() : nullptr;
            if (m_surface) {
                prepareSurface();
                // Shown, the surface asks for the keys only once it has
                // drawn: the desktop hands them to a surface the moment it
                // asks, and to one not yet drawn it may not.
                connect(m_surface, &QQuickWindow::frameSwapped, this, [this] {
                    if (m_waitingToDraw) {
                        m_waitingToDraw = false;
                        applyKeys();
                    }
                });
            }
        }
        follow();
    }
    const QVariantList shown = currentShownNotes();
    if (shown != m_shownNotes) {
        m_shownNotes = shown;
        Q_EMIT shownChanged();
    }
    const bool foldedChange = folded != m_folded;
    m_folded = folded;
    if (foldedChange) {
        Q_EMIT foldedChanged();
    }
    // Shown only once it has a size: a surface the desktop has never been
    // given one for waits, rather than being shown at none.
    const bool up = drawn && m_surface && m_surface->width() > 0 && m_surface->height() > 0;
    if (m_surface && m_surface->isVisible() != up) {
        qCDebug(DESKTOP) << (up ? "stuck notes drawn over" : "stuck notes step aside from") << m_drawnKey;
        // Each time it is shown the desktop makes the surface anew, taking
        // the keys as last asked; it is shown without them.
        m_waitingToDraw = up;
        applyKeys();
        m_surface->setVisible(up);
    }
    applyKeys();
    const QVariantList entries = StuckWindows::entries(m_windows, m_store->notes(), m_shownKeys, m_foldedKeys);
    if (entries != m_entries) {
        m_entries = entries;
        Q_EMIT WindowsChanged(m_entries);
    }
    if (writingEnded) {
        Q_EMIT writingChanged();
    }
}

bool StuckNotes::noteOnWindow(const QString &noteId, const QString &key) const
{
    for (const OpenWindow &window : m_windows) {
        if (window.key() != key) {
            continue;
        }
        const QList<Note> notes = StuckWindows::notesOn(window, m_store->notes());
        return std::any_of(notes.cbegin(), notes.cend(), [&noteId](const Note &note) {
            return note.id == noteId;
        });
    }
    return false;
}

QString StuckNotes::writingText() const
{
    return m_writingId.isEmpty() ? QString() : m_writer->text();
}

bool StuckNotes::beginWriting(const QString &noteId, const QString &key)
{
    if (noteId == m_writingId && key == m_writingKey) {
        return true;
    }
    const bool ended = endWriting();
    const auto note = m_store->note(noteId);
    if (!note || m_store->readOnly() || note->newerFormat() || !m_writer->open(noteId)) {
        if (ended) {
            refresh();
            Q_EMIT writingChanged();
        }
        return false;
    }
    m_writingId = noteId;
    m_writingKey = key;
    m_writingShown = {{QStringLiteral("title"), note->title()}, {QStringLiteral("text"), note->text}};
    qCDebug(DESKTOP) << "typing on note" << noteId << "where it stands";
    refresh();
    Q_EMIT writingChanged();
    Q_EMIT writingTextChanged();
    return true;
}

bool StuckNotes::endWriting()
{
    if (m_writingId.isEmpty()) {
        return false;
    }
    qCDebug(DESKTOP) << "typing on note" << m_writingId << "ends";
    m_writingId.clear();
    m_writingKey.clear();
    m_writingShown.clear();
    // Written now, as the quick note is when its card goes; a note left with
    // no words goes to the trash, as there.
    m_writer->finish();
    return true;
}

void StuckNotes::write(const QString &noteId)
{
    const OpenWindow *window = drawnWindow();
    if (!window || !noteOnWindow(noteId, window->key())) {
        return;
    }
    beginWriting(noteId, window->key());
}

void StuckNotes::setWritingText(const QString &text)
{
    if (!m_writingId.isEmpty()) {
        m_writer->setText(text);
    }
}

void StuckNotes::stopWriting()
{
    if (endWriting()) {
        refresh();
        Q_EMIT writingChanged();
    }
}

void StuckNotes::keysLeft()
{
    // Shown again on another display with its window, the surface lets go
    // of the keys and takes them back; the notes stay as they were.
    if (m_movedDisplay.isValid() && m_movedDisplay.elapsed() < 1000) {
        return;
    }
    stopWriting();
    if (!m_folded) {
        m_foldLater.start();
    }
}

void StuckNotes::foldIfLeft()
{
    // Stepping aside for Spread or the quick-note card, or the keys back on
    // the surface, is not Robin going back to the work.
    if (!m_surface || !m_surface->isVisible() || m_surface->isActive() || m_paused || m_cardOpen
        || (m_movedDisplay.isValid() && m_movedDisplay.elapsed() < 1000)
        || m_drawnKey.isEmpty() || !m_shownKeys.contains(m_drawnKey) || m_foldedKeys.contains(m_drawnKey)) {
        return;
    }
    qCDebug(DESKTOP) << "stuck notes fold into a dot on" << m_drawnKey;
    m_foldedAt.start();
    m_foldedOn = m_drawnKey;
    m_foldedKeys.insert(m_drawnKey);
    refresh();
}

void StuckNotes::unfold()
{
    const OpenWindow *window = drawnWindow();
    if (!window || !m_foldedKeys.contains(window->key())) {
        return;
    }
    m_foldLater.stop();
    const QString key = window->key();
    const QList<Note> notes = StuckWindows::notesOn(*window, m_store->notes());
    qCDebug(DESKTOP) << "stuck notes open again on" << window->caption;
    m_foldedKeys.remove(key);
    refresh();
    // The top note, the one changed last, is ready to type.
    if (!notes.isEmpty()) {
        beginWriting(notes.constFirst().id, key);
    }
}

void StuckNotes::drawOver(const QString &key)
{
    // Open notes hold the keys, and only the notes drawn hold them: notes left
    // open over a window that goes behind another fold, as they would for a
    // tap on that other window.
    if (!m_drawnKey.isEmpty() && m_drawnKey != key && m_shownKeys.contains(m_drawnKey)) {
        m_foldedKeys.insert(m_drawnKey);
    }
    m_drawnKey = key;
}

bool StuckNotes::Write(const QString &noteId, const QString &windowId, const QString &caption, const QString &app)
{
    m_windows = m_context->windows();
    const qsizetype at = StuckWindows::find(m_windows, windowId, caption, app);
    if (at < 0) {
        qCDebug(DESKTOP) << "no window with note" << noteId << "for" << windowId << caption << app;
        return false;
    }
    const OpenWindow window = m_windows.at(at);
    if (!noteOnWindow(noteId, window.key())) {
        qCDebug(DESKTOP) << "note" << noteId << "is not stuck to" << window.caption;
        return false;
    }
    setShown(window, true);
    return beginWriting(noteId, window.key());
}

void StuckNotes::prepareSurface()
{
    if (onWayland()) {
        // A surface of the desktop's own above the windows, laid exactly
        // over the one whose notes it shows. It takes the keys only while a
        // note on it is typed on or words on one are selected
        // (setTakesKeys): the desktop brings a top-layer surface to the
        // front as soon as it will take the keys, so it never asks for them
        // otherwise. It takes presses only on the notes and their button
        // (setPressable); the rest reaches the window.
        auto *layer = LayerShellQt::Window::get(m_surface);
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorLeft});
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
        layer->setScope(QStringLiteral("gooseberry-stuck"));
    } else {
        m_surface->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                            | Qt::WindowDoesNotAcceptFocus);
    }
}

void StuckNotes::setShown(const OpenWindow &window, bool shown)
{
    // Brought up or put away, they are open the next time they are up.
    m_foldedKeys.remove(window.key());
    if (shown) {
        m_shownKeys.insert(window.key());
        // Brought up from a title bar or Spread: drawn over that window now,
        // even before the desktop says it is in front.
        drawOver(window.key());
        m_activeWhenShown.clear();
        for (const OpenWindow &open : std::as_const(m_windows)) {
            if (open.active && open.key() != window.key()) {
                m_activeWhenShown = open.key();
            }
        }
    } else {
        m_shownKeys.remove(window.key());
    }
    qCDebug(DESKTOP) << "stuck notes" << (shown ? "up on" : "put away from") << window.caption;
    refresh();
}

bool StuckNotes::Toggle(const QString &windowId, const QString &caption, const QString &app)
{
    m_windows = m_context->windows();
    const qsizetype at = StuckWindows::find(m_windows, windowId, caption, app);
    if (at < 0 || StuckWindows::notesOn(m_windows.at(at), m_store->notes()).isEmpty()) {
        qCDebug(DESKTOP) << "no window with notes for" << windowId << caption << app;
        return false;
    }
    const OpenWindow window = m_windows.at(at);
    const bool up = !m_shownKeys.contains(window.key());
    setShown(window, up);
    return up;
}

bool StuckNotes::Show(const QString &windowId, const QString &caption, const QString &app)
{
    m_windows = m_context->windows();
    const qsizetype at = StuckWindows::find(m_windows, windowId, caption, app);
    if (at < 0 || StuckWindows::notesOn(m_windows.at(at), m_store->notes()).isEmpty()) {
        return false;
    }
    setShown(OpenWindow(m_windows.at(at)), true);
    return true;
}

void StuckNotes::follow()
{
    const OpenWindow *window = drawnWindow();
    if (!window || !m_surface || !window->geometry.isValid()) {
        return;
    }
    const QRect geometry = window->geometry;
    if (!onWayland()) {
        m_surface->setGeometry(geometry);
        return;
    }
    QScreen *screen = QGuiApplication::screenAt(geometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }
    // A surface of no width or height is one the desktop refuses, and it
    // closes Gooseberry's connection for asking: such a size is not laid.
    const QRect area = screen->geometry();
    const QSize size = geometry.size().boundedTo(area.size());
    if (size.isEmpty()) {
        return;
    }
    // A layer surface stays on the display it was first shown on; on
    // another, it is shown again there, laid before it is shown.
    const bool moving = m_surface->screen() != screen;
    const bool visible = m_surface->isVisible();
    if (moving) {
        m_surface->hide();
        m_surface->setScreen(screen);
    }
    LayerShellQt::Window::get(m_surface)->setMargins(
        QMargins(qMax(0, geometry.x() - area.x()), qMax(0, geometry.y() - area.y()), 0, 0));
    m_surface->resize(size);
    if (moving && visible) {
        m_movedDisplay.start();
        // Shown anew, it asks for the keys again once it has drawn.
        m_waitingToDraw = true;
        applyKeys();
        m_surface->show();
    }
}

bool StuckNotes::onOtherDisplays(const QString &key, const QString &other) const
{
    QScreen *first = nullptr;
    QScreen *second = nullptr;
    for (const OpenWindow &window : m_windows) {
        if (window.key() == key) {
            first = QGuiApplication::screenAt(window.geometry.center());
        } else if (window.key() == other) {
            second = QGuiApplication::screenAt(window.geometry.center());
        }
    }
    return first && second && first != second;
}

void StuckNotes::Hide()
{
    if (m_shownKeys.isEmpty()) {
        return;
    }
    m_shownKeys.clear();
    m_foldedKeys.clear();
    refresh();
}

void StuckNotes::Pause(bool paused)
{
    if (paused != m_paused) {
        // Spread opening ends any typing, and keeps it; a note asked for
        // while it is open waits for it to close.
        const bool ended = paused && endWriting();
        // A pause that follows a fold at once is their window being picked
        // up to be carried, by the press that folded them: they come back
        // open where it lands.
        if (paused && m_foldedAt.isValid() && m_foldedAt.elapsed() < 1500) {
            m_foldedKeys.remove(m_foldedOn);
            m_foldedAt.invalidate();
        }
        m_paused = paused;
        refresh();
        if (ended) {
            Q_EMIT writingChanged();
        }
    }
}

void StuckNotes::DrawFolded(bool drawn)
{
    if (!calledFromDBus()) {
        return;
    }
    const QString caller = message().service();
    if (!m_holderWatch) {
        m_holderWatch = new QDBusServiceWatcher(this);
        m_holderWatch->setConnection(connection());
        m_holderWatch->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(m_holderWatch, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
            m_holderWatch->removeWatchedService(name);
            if (m_dotHolders.remove(name) && m_dotHolders.isEmpty()) {
                Q_EMIT dotElsewhereChanged();
            }
        });
    }
    const bool before = dotElsewhere();
    if (drawn && !m_dotHolders.contains(caller)) {
        m_dotHolders.insert(caller);
        m_holderWatch->addWatchedService(caller);
    } else if (!drawn && m_dotHolders.remove(caller)) {
        m_holderWatch->removeWatchedService(caller);
    }
    if (dotElsewhere() != before) {
        Q_EMIT dotElsewhereChanged();
    }
}

void StuckNotes::setCardOpen(bool open)
{
    if (open != m_cardOpen) {
        // The note goes to the card whole, so typing on the window ends.
        const bool ended = open && endWriting();
        m_cardOpen = open;
        refresh();
        if (ended) {
            Q_EMIT writingChanged();
        }
    }
}

void StuckNotes::setPressable(const QVariantList &rects)
{
    if (!m_surface) {
        return;
    }
    QRegion region;
    for (const QVariant &rect : rects) {
        region += rect.toRectF().toAlignedRect();
    }
    // An empty mask would take every press; one pixel in the corner takes
    // none that matter.
    m_surface->setMask(region.isEmpty() ? QRegion(0, 0, 1, 1) : region);
}

void StuckNotes::setTakesKeys(bool takes)
{
    m_notesWantKeys = takes;
    applyKeys();
}

void StuckNotes::applyKeys()
{
    const bool wanted = m_surface && !m_paused && !m_cardOpen && !m_drawnKey.isEmpty() && !m_waitingToDraw
        && (m_notesWantKeys || !m_folded);
    if (!m_surface || wanted == m_keysTaken) {
        return;
    }
    m_keysTaken = wanted;
    if (!onWayland()) {
        return;
    }
    // The desktop hands the keys to a surface over the windows as soon as it
    // asks for them, and back to the window once it stops.
    qCDebug(DESKTOP) << "stuck notes" << (wanted ? "take" : "give back") << "the keys";
    LayerShellQt::Window::get(m_surface)->setKeyboardInteractivity(wanted ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                                                          : LayerShellQt::Window::KeyboardInteractivityNone);
    m_surface->requestUpdate();
}

bool StuckNotes::NewOn(const QString &windowId, const QString &caption, const QString &app)
{
    m_windows = m_context->windows();
    const qsizetype at = StuckWindows::find(m_windows, windowId, caption, app);
    if (at < 0 || m_windows.at(at).window.isEmpty() || m_store->readOnly()) {
        qCDebug(DESKTOP) << "no window to start a note on for" << windowId << caption << app;
        return false;
    }
    const OpenWindow &window = m_windows.at(at);
    Q_EMIT newRequested(window.window, window.app);
    return true;
}

bool StuckNotes::StickTo(const QString &noteId, const QString &windowId, const QString &caption, const QString &app)
{
    m_windows = m_context->windows();
    const qsizetype at = StuckWindows::find(m_windows, windowId, caption, app);
    auto note = m_store->note(noteId);
    if (at < 0 || !note || note->newerFormat() || m_store->readOnly() || m_windows.at(at).window.isEmpty()) {
        return false;
    }
    const OpenWindow &window = m_windows.at(at);
    if (note->isStuck() && note->window == window.window && note->app == window.app) {
        return true;
    }
    note->stuck = true;
    note->window = window.window;
    note->app = window.app;
    // On another window the note starts where an unplaced note does.
    note->place = QPointF(-1, -1);
    if (!m_store->save(*note)) {
        return false;
    }
    qCDebug(DESKTOP) << "note" << noteId << "stuck to" << window.window << "in" << window.app;
    m_refresh.start();
    return true;
}

void StuckNotes::place(const QString &noteId, qreal x, qreal y)
{
    auto note = m_store->note(noteId);
    if (!note || note->newerFormat() || m_store->readOnly()) {
        return;
    }
    note->place = QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
    // Where a note sits on its window is not a change to the note.
    m_store->save(*note, NoteStore::Touch::Kept);
}

void StuckNotes::resize(const QString &noteId, qreal width, qreal height)
{
    auto note = m_store->note(noteId);
    if (!note || note->newerFormat() || m_store->readOnly()) {
        return;
    }
    note->size = QSizeF(qBound<qreal>(Note::SmallestSide, qRound(width), Note::LargestSide),
                        qBound<qreal>(Note::SmallestSide, qRound(height), Note::LargestSide));
    // Nor is how big it is drawn there.
    m_store->save(*note, NoteStore::Touch::Kept);
}

} // namespace Gooseberry
