// SPDX-License-Identifier: GPL-2.0-or-later
#include "Shell.h"

#include "Board.h"
#include "Capture.h"
#include "ContextBar.h"
#include "NoteStore.h"
#include "Notifier.h"
#include "Planner.h"
#include "ReminderWords.h"
#include "Reminders.h"
#include "SpreadGuest.h"
#include "StuckNotes.h"
#include "WindowContext.h"
#include "Log.h"

#include <KService>
#include <KWaylandExtras>
#include <KWindowSystem>
#include <LayerShellQt/Window>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QCursor>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QScreen>
#include <QTimer>

#include <memory>

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
    , m_guest(new SpreadGuest(this))
    , m_planner(new Planner(store, this))
    , m_reminders(new Reminders(store, this))
    , m_notifier(new Notifier(store, m_reminders, this))
    , m_stuck(new StuckNotes(store, m_context, [this] {
        return create(QStringLiteral("StuckWindow"));
    }, this))
    , m_contextBar(new ContextBar(this))
{
    // The title bar's + starts a note stuck to its window, on the card.
    connect(m_stuck, &StuckNotes::newRequested, this, [this](const QString &window, const QString &app) {
        m_capture->startNew({window, app, m_context->current().workspace});
        setBack({});
        m_stuck->setCardOpen(true);
        raiseCard();
    });
    // A reminder tapped opens its note on the card.
    connect(m_notifier, &Notifier::openRequested, this, &Shell::openNote);
    connect(m_context, &WindowContext::documentOpened, m_reminders, &Reminders::documentOpened);
    // A timer stands still while the computer sleeps; on waking, every
    // reminder that came due meanwhile is shown at once.
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                                         QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PrepareForSleep"),
                                         this, SLOT(sleeping(bool)));
    // Another guest took Spread's centre: the card closes, the note kept.
    connect(m_guest, &SpreadGuest::dismissed, m_capture, &Capture::finish);
    // A card that closes gives Spread's centre back, and the way back to
    // whoever opened it goes with it.
    connect(m_capture, &Capture::finished, this, [this] {
        m_guest->end();
        setBack({});
    });
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

QObject *Shell::guestObject() const
{
    return m_guest;
}

QObject *Shell::plannerObject() const
{
    return m_planner;
}

QObject *Shell::stuckObject() const
{
    return m_stuck;
}

QObject *Shell::contextBarObject() const
{
    return m_contextBar;
}

void Shell::sleeping(bool goingToSleep)
{
    if (!goingToSleep) {
        m_reminders->check();
    }
}

QString Shell::reminderLabel(const QDateTime &remind, bool onOpen) const
{
    return ReminderWords::label(remind, onOpen);
}

QVariantList Shell::reminderChoices() const
{
    return ReminderWords::choices(!m_capture->window().isEmpty());
}

void Shell::boardFromCard()
{
    const QString note = m_capture->noteId();
    QQuickWindow *card = m_captureWindow;
    // The board opens as Gooseberry's own window, as its entry opens it, and
    // the card goes. On Wayland the card, which has the keyboard, asks for
    // the board's turn in front first.
    if (card && card->isVisible() && KWindowSystem::isPlatformWayland()) {
        const quint32 serial = KWaylandExtras::lastInputSerial(card);
        // The board opens once, with the token or, without one in time,
        // without it; the waiter goes with the first.
        auto *waiter = new QObject(this);
        auto opened = std::make_shared<bool>(false);
        auto open = [this, note, opened, waiter](const QString &token) {
            if (*opened) {
                return;
            }
            *opened = true;
            waiter->deleteLater();
            if (!token.isEmpty()) {
                KWindowSystem::setCurrentXdgActivationToken(token);
            }
            showBoardOn(note);
        };
        connect(KWaylandExtras::self(), &KWaylandExtras::xdgActivationTokenArrived, waiter,
                [serial, open](int answered, const QString &token) {
                    if (quint32(answered) == serial) {
                        open(token);
                    }
                });
        QTimer::singleShot(500, waiter, [open] {
            open({});
        });
        KWaylandExtras::requestXdgActivationToken(card, serial, QGuiApplication::desktopFileName());
    } else {
        showBoardOn(note);
    }
    m_capture->finish();
}

void Shell::setBack(const QString &label)
{
    if (label != m_backLabel) {
        m_backLabel = label;
        Q_EMIT backLabelChanged();
    }
}

void Shell::goBack()
{
    if (m_backLabel.isEmpty()) {
        return;
    }
    Q_EMIT backRequested();
    m_capture->finish();
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
    // Notes shown over a window come back once the card is gone, however it
    // went.
    connect(m_captureWindow, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            m_stuck->setCardOpen(false);
        } else if (m_captureWindow->screen()) {
            m_contextBar->read(m_captureWindow->screen()->name());
        }
    });
    // The compositor can place the card on another display once it is shown.
    connect(m_captureWindow, &QWindow::screenChanged, this, [this](QScreen *screen) {
        if (screen && m_captureWindow->isVisible()) {
            m_contextBar->read(screen->name());
        }
    });
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        // A surface of the desktop's own, across the whole display, with the
        // note's card centred on it; a tap on the work around the
        // card puts it away, and the windows behind are not moved. It is a
        // top-layer surface: KWin keeps the on-screen keys in the overlay
        // layer, and an overlay surface mapped after them would stack above
        // the keys and take every touch meant for them.
        auto *layer = LayerShellQt::Window::get(m_captureWindow);
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorBottom,
                           LayerShellQt::Window::AnchorLeft, LayerShellQt::Window::AnchorRight});
        // The whole display, as the desktop's search covers it, so Kadunce's
        // places for the card are taken as they are given.
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromCompositor);
        // KWin makes a layer surface of a scope it does not know a normal
        // window, which its open and close animation zooms from four fifths
        // with a fade and a blur. The card stands at its size from its first
        // frame, so the surface asks as a utility one, which no such
        // animation takes.
        layer->setScope(QStringLiteral("utility"));
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
    if (!window->isVisible()) {
        // In Kadunce's Spread the card holds its centre, on Kadunce's display.
        const bool guest = m_guest->begin(window);
        if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
            LayerShellQt::Window::get(window)->setScreenConfiguration(
                guest ? LayerShellQt::Window::ScreenFromQWindow : LayerShellQt::Window::ScreenFromCompositor);
        }
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
    showCaptureWith(QString());
}

void Shell::showCaptureWith(const QString &text, const QString &back)
{
    const CaptureContext context = m_context->current();
    qCDebug(DESKTOP) << "capture: window" << context.window << "app" << context.app << "workspace" << context.workspace;
    m_capture->startNew(context);
    // After the note before has been finished, which takes its way back.
    setBack(back);
    if (!text.isEmpty()) {
        m_capture->setText(text);
    }
    raiseCard();
}

void Shell::openNote(const QString &id)
{
    openNoteWith(id, {});
}

bool Shell::openNoteWith(const QString &id, const QString &back)
{
    if (!m_capture->open(id)) {
        return false;
    }
    setBack(back);
    raiseCard();
    return true;
}

void Shell::newNoteIn(const QString &placeKey)
{
    setBack({});
    m_capture->startNewIn(placeKey);
    raiseCard();
}

CaptureContext Shell::currentContext() const
{
    return m_context->current();
}

QVariantList Shell::openWindows() const
{
    return m_context->openWindows();
}

QString Shell::currentWorkspace() const
{
    return m_context->current().workspace;
}

QString Shell::applicationName() const
{
    if (const auto service = KService::serviceByDesktopName(QGuiApplication::desktopFileName())) {
        if (!service->name().isEmpty()) {
            return service->name();
        }
    }
    return QGuiApplication::applicationDisplayName();
}

void Shell::showBoard()
{
    showBoardOn({});
}

void Shell::showBoardOn(const QString &noteId)
{
    QQuickWindow *window = boardWindow();
    if (!window) {
        return;
    }
    QString place;
    if (const auto note = m_store->note(noteId)) {
        place = note->tucked ? QStringLiteral("tucked") : note->placeKey();
    }
    // The first frame drawn after this request is the board having arrived;
    // a frame is asked for, so one comes even when nothing on it changes.
    connect(window, &QQuickWindow::frameSwapped, this, &Shell::boardShown, Qt::SingleShotConnection);
    QMetaObject::invokeMethod(window, "present", Q_ARG(QVariant, place));
    window->update();
    // The launcher's activation token, handed over with this start, lets the
    // board come to the front rather than wait behind the window in use.
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
}

} // namespace Gooseberry
