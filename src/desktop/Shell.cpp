// SPDX-License-Identifier: GPL-2.0-or-later
#include "Shell.h"

#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "WindowContext.h"
#include "Log.h"

#include <KWindowSystem>
#include <LayerShellQt/Window>

#include <QCommandLineParser>
#include <QCursor>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QScreen>
#include <QTimer>

Q_LOGGING_CATEGORY(DESKTOP, "gooseberry.desktop", QtWarningMsg)

namespace Gooseberry {

namespace {
const QString Module = QStringLiteral("io.github.carlsonjm.gooseberry");
}

Shell::Shell(NoteStore *store, QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_engine(engine)
    , m_capture(new Capture(store, this))
    , m_places(new Places(store, this))
    , m_notes(new PlaceNotes(store, this))
    , m_context(new WindowContext(QGuiApplication::desktopFileName(), this))
{
    // On the first tap, before the desktop has said what is in front, a card
    // still empty takes up the window once it does.
    connect(m_context, &WindowContext::firstReported, this, [this] {
        if (m_captureWindow && m_captureWindow->isVisible() && !m_capture->kept() && !m_capture->editing()
            && m_capture->text().isEmpty() && m_capture->window().isEmpty()) {
            m_capture->startNew(m_context->current());
            qCDebug(DESKTOP) << "the empty card takes up the window in front:" << m_capture->window();
        }
    });
    connect(m_context, &WindowContext::workChanged, this, [this] {
        if (m_captureWindow && m_captureWindow->isVisible()) {
            qCDebug(DESKTOP) << "another window came to the front: the card goes away";
            m_capture->finish();
        }
    });
    // A session ending asks for unsaved work first.
    connect(qApp, &QGuiApplication::commitDataRequest, m_capture, &Capture::flush);
}

Shell::~Shell()
{
    delete m_captureWindow;
    delete m_boardWindow;
}

QObject *Shell::captureObject() const
{
    return m_capture;
}

QObject *Shell::placesObject() const
{
    return m_places;
}

QObject *Shell::notesObject() const
{
    return m_notes;
}

QObject *Shell::storeObject() const
{
    return m_store;
}

void Shell::handle(const QStringList &arguments)
{
    QCommandLineParser parser;
    const QCommandLineOption capture(QStringLiteral("capture"));
    const QCommandLineOption board(QStringLiteral("board"));
    const QCommandLineOption background(QStringLiteral("background"));
    parser.addOptions({capture, board, background});
    parser.parse(arguments);

    if (parser.isSet(background)) {
        // Started with the session: everything is made ready now, so the first
        // tap brings the card up without waiting.
        captureWindow();
        return;
    }
    if (parser.isSet(capture)) {
        showCapture();
        return;
    }
    // Opening Gooseberry, from a launcher, a search or a pinned button, opens
    // the board as an ordinary window; --board says the same.
    showBoard();
}

QQuickWindow *Shell::create(const QString &name)
{
    QQmlComponent component(m_engine, Module, name);
    QObject *object = component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(this)}});
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        qWarning().noquote() << "Gooseberry could not draw" << name << component.errorString();
        delete object;
    }
    return window;
}

QQuickWindow *Shell::captureWindow()
{
    if (m_captureWindow) {
        return m_captureWindow;
    }
    m_captureWindow = create(QStringLiteral("CaptureWindow"));
    if (!m_captureWindow) {
        return nullptr;
    }
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        // A surface of the desktop's own, across the room the panels leave,
        // with the note's card centred on it; a tap on the work around the
        // card puts it away, and the windows behind are not moved. It is a
        // top-layer surface: KWin keeps the on-screen keys in the overlay
        // layer, and an overlay surface mapped after them would stack above
        // the keys and take every touch meant for them.
        auto *layer = LayerShellQt::Window::get(m_captureWindow);
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorBottom,
                           LayerShellQt::Window::AnchorLeft, LayerShellQt::Window::AnchorRight});
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromCompositor);
        layer->setScope(QStringLiteral("gooseberry-capture"));
        qCDebug(DESKTOP) << "card on the desktop's own surface";
    } else {
        m_captureWindow->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    }
    return m_captureWindow;
}

QQuickWindow *Shell::boardWindow()
{
    if (!m_boardWindow) {
        m_boardWindow = create(QStringLiteral("BoardWindow"));
    }
    return m_boardWindow;
}

void Shell::raiseCard()
{
    QQuickWindow *window = captureWindow();
    if (!window) {
        return;
    }
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen) {
            window->setGeometry(screen->availableGeometry());
        }
    }
    QMetaObject::invokeMethod(window, "open");
    if (DESKTOP().isDebugEnabled()) {
        QTimer::singleShot(800, window, [window] {
            qCDebug(DESKTOP) << "card shown:" << window->isVisible() << "has the keyboard:" << window->isActive()
                             << "focus on:" << (window->activeFocusItem() ? window->activeFocusItem()->metaObject()->className() : "nothing")
                             << "size:" << window->size();
        });
    }
}

void Shell::showCapture()
{
    const CaptureContext context = m_context->current();
    qCDebug(DESKTOP) << "capture: window" << context.window << "app" << context.app << "workspace" << context.workspace;
    m_capture->startNew(context);
    raiseCard();
}

void Shell::openNote(const QString &id)
{
    if (m_capture->open(id)) {
        raiseCard();
    }
}

void Shell::newNoteIn(const QString &placeKey)
{
    m_capture->startNewIn(placeKey);
    raiseCard();
}

void Shell::showBoard()
{
    QQuickWindow *window = boardWindow();
    if (!window) {
        return;
    }
    QMetaObject::invokeMethod(window, "present");
    // The launcher's activation token, handed over with this start, lets the
    // board come to the front rather than wait behind the window in use.
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
}

} // namespace Gooseberry
