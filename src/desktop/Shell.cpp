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
    connect(m_context, &WindowContext::workChanged, this, [this] {
        if (m_captureWindow && m_captureWindow->isVisible()) {
            qCDebug(DESKTOP) << "another window came to the front: the sheet goes down";
            m_capture->finish();
        }
    });
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
    const QCommandLineOption board(QStringLiteral("board"));
    const QCommandLineOption background(QStringLiteral("background"));
    parser.addOptions({board, background});
    parser.parse(arguments);

    if (parser.isSet(background)) {
        // Started with the session: everything is made ready now, so the first
        // tap brings the sheet up without waiting.
        captureWindow();
        return;
    }
    if (parser.isSet(board)) {
        showBoard();
        return;
    }
    showCapture();
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
        // A surface of the desktop's own, across the room the panels leave:
        // the sheet rises from its bottom edge, and a tap on the dimmed work
        // around it puts the sheet away. The windows behind are not moved.
        auto *layer = LayerShellQt::Window::get(m_captureWindow);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorBottom,
                           LayerShellQt::Window::AnchorLeft, LayerShellQt::Window::AnchorRight});
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromCompositor);
        layer->setScope(QStringLiteral("gooseberry-capture"));
        qCDebug(DESKTOP) << "sheet on the desktop's own surface";
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

void Shell::raiseSheet()
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
            qCDebug(DESKTOP) << "sheet shown:" << window->isVisible() << "has the keyboard:" << window->isActive()
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
    raiseSheet();
}

void Shell::openNote(const QString &id)
{
    if (m_capture->open(id)) {
        raiseSheet();
    }
}

void Shell::newNoteIn(const QString &placeKey)
{
    m_capture->startNewIn(placeKey);
    raiseSheet();
}

void Shell::showBoard()
{
    QQuickWindow *window = boardWindow();
    if (!window) {
        return;
    }
    QMetaObject::invokeMethod(window, "present");
    KWindowSystem::activateWindow(window);
}

} // namespace Gooseberry
