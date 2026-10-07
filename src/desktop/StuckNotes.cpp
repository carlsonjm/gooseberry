// SPDX-License-Identifier: GPL-2.0-or-later
#include "StuckNotes.h"

#include "Log.h"
#include "NoteStore.h"
#include "WindowContext.h"

#include <LayerShellQt/Window>

#include <QDBusConnection>
#include <QDBusError>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>

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
{
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, &StuckNotes::refresh);
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
    for (const Note &note : StuckWindows::notesOn(*window, m_store->notes())) {
        list.append(QVariantMap{
            {QStringLiteral("id"), note.id},
            {QStringLiteral("title"), note.title()},
            {QStringLiteral("text"), note.text},
            {QStringLiteral("colourHex"), colourHex(note.colour)},
            {QStringLiteral("x"), note.place.x()},
            {QStringLiteral("y"), note.place.y()},
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
    // The notes are drawn over the window in front when it has them up; a
    // desktop that says no window is in front leaves them where they were.
    if (!active.isEmpty() || !m_shownKeys.contains(m_drawnKey)) {
        m_drawnKey = m_shownKeys.contains(active) ? active : QString();
    }
    const bool drawn = !m_paused && !m_cardOpen && !m_drawnKey.isEmpty();
    if (drawn) {
        if (!m_surface) {
            m_surface = m_makeSurface ? m_makeSurface() : nullptr;
            if (m_surface) {
                prepareSurface();
            }
        }
        follow();
    }
    const QVariantList shown = currentShownNotes();
    if (shown != m_shownNotes) {
        m_shownNotes = shown;
        Q_EMIT shownChanged();
    }
    if (m_surface && m_surface->isVisible() != drawn) {
        qCDebug(DESKTOP) << (drawn ? "stuck notes drawn over" : "stuck notes step aside from") << m_drawnKey;
        m_surface->setVisible(drawn);
    }
    const QVariantList entries = StuckWindows::entries(m_windows, m_store->notes(), m_shownKeys);
    if (entries != m_entries) {
        m_entries = entries;
        Q_EMIT WindowsChanged(m_entries);
    }
}

void StuckNotes::prepareSurface()
{
    if (onWayland()) {
        // A surface of the desktop's own above the windows, laid exactly
        // over the one whose notes it shows. It takes no keys: a note is
        // written on the quick-note card. It takes presses only on the notes
        // and their button (setPressable); the rest reaches the window.
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
    if (shown) {
        m_shownKeys.insert(window.key());
        // Brought up from a title bar or Spread: drawn over that window now,
        // even before the desktop says it is in front.
        m_drawnKey = window.key();
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
    if (m_surface->screen() != screen) {
        // A layer surface stays on the display it was first shown on; on
        // another, it is shown again there.
        const bool visible = m_surface->isVisible();
        m_surface->hide();
        m_surface->setScreen(screen);
        if (visible) {
            m_surface->show();
        }
    }
    const QRect area = screen->geometry();
    LayerShellQt::Window::get(m_surface)->setMargins(
        QMargins(qMax(0, geometry.x() - area.x()), qMax(0, geometry.y() - area.y()), 0, 0));
    m_surface->resize(geometry.size().boundedTo(area.size()));
}

void StuckNotes::Hide()
{
    if (m_shownKeys.isEmpty()) {
        return;
    }
    m_shownKeys.clear();
    refresh();
}

void StuckNotes::Pause(bool paused)
{
    if (paused != m_paused) {
        m_paused = paused;
        refresh();
    }
}

void StuckNotes::setCardOpen(bool open)
{
    if (open != m_cardOpen) {
        m_cardOpen = open;
        refresh();
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

} // namespace Gooseberry
