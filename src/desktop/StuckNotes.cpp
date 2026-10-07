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

const OpenWindow *StuckNotes::shownWindow() const
{
    if (m_shownKey.isEmpty()) {
        return nullptr;
    }
    for (const OpenWindow &window : m_windows) {
        if (window.key() == m_shownKey) {
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
    const OpenWindow *window = shownWindow();
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

QString StuckNotes::activeKey() const
{
    for (const OpenWindow &window : m_windows) {
        if (window.active) {
            return window.key();
        }
    }
    return {};
}

void StuckNotes::refresh()
{
    m_windows = m_context->windows();
    if (!m_shownKey.isEmpty()) {
        const OpenWindow *window = shownWindow();
        // Another window came to the front after the notes were shown. The
        // window in front when the dot was tapped is not news: the tap may
        // be told before the window it was on is.
        const QString active = activeKey();
        const bool elsewhere = !active.isEmpty() && active != m_activeAtShow && active != m_shownKey;
        if (active == m_shownKey) {
            m_activeAtShow = active;
        }
        // The window closed, its last note came off, or Robin went to other
        // work: its notes are put away.
        if (!window || StuckWindows::notesOn(*window, m_store->notes()).isEmpty() || elsewhere) {
            qCDebug(DESKTOP) << "stuck notes put away:" << (window ? "work elsewhere or no notes" : "window closed");
            m_shownKey.clear();
            if (m_surface) {
                m_surface->hide();
            }
        } else {
            follow();
        }
    }
    const QVariantList shown = currentShownNotes();
    if (shown != m_shownNotes) {
        m_shownNotes = shown;
        Q_EMIT shownChanged();
    }
    const QVariantList entries = StuckWindows::entries(m_windows, m_store->notes(), m_shownKey);
    if (entries != m_entries) {
        m_entries = entries;
        Q_EMIT WindowsChanged(m_entries);
    }
}

bool StuckNotes::Toggle(const QString &windowId, const QString &caption, const QString &app)
{
    m_windows = m_context->windows();
    const qsizetype at = StuckWindows::find(m_windows, windowId, caption, app);
    if (at < 0 || StuckWindows::notesOn(m_windows.at(at), m_store->notes()).isEmpty()) {
        qCDebug(DESKTOP) << "no window with notes for" << windowId << caption << app;
        return false;
    }
    if (m_windows.at(at).key() == m_shownKey) {
        Hide();
        return false;
    }
    show(m_windows.at(at));
    return true;
}

void StuckNotes::show(const OpenWindow &window)
{
    if (!m_surface) {
        m_surface = m_makeSurface ? m_makeSurface() : nullptr;
        if (!m_surface) {
            return;
        }
        if (onWayland()) {
            // A surface of the desktop's own above the windows, laid exactly
            // over the one whose notes it shows. It takes no keys: a note is
            // written on the quick-note card.
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
    m_shownKey = window.key();
    m_activeAtShow = activeKey();
    qCDebug(DESKTOP) << "stuck notes shown on" << window.caption << window.geometry;
    follow();
    m_shownNotes = currentShownNotes();
    Q_EMIT shownChanged();
    m_surface->show();
    m_refresh.start();
}

void StuckNotes::follow()
{
    const OpenWindow *window = shownWindow();
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
    if (m_shownKey.isEmpty()) {
        return;
    }
    m_shownKey.clear();
    if (m_surface) {
        m_surface->hide();
    }
    m_refresh.start();
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
