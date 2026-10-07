// SPDX-License-Identifier: GPL-2.0-or-later
#include "Shell.h"

#include "Board.h"
#include "Capture.h"
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
#include <QUuid>

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
{
    // A note tapped over its window opens on the card; the notes step aside
    // while it is open and come back when it closes.
    connect(m_stuck, &StuckNotes::openRequested, this, [this](const QString &id) {
        if (m_capture->open(id)) {
            m_stuck->setCardOpen(true);
            raiseCard();
        }
    });
    // The title bar's + starts a note stuck to its window, on the card.
    connect(m_stuck, &StuckNotes::newRequested, this, [this](const QString &window, const QString &app) {
        m_capture->startNew({window, app, m_context->current().workspace});
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
    // A card that closes on its own gives Spread's centre back. One that
    // handed its place to the board, or was dismissed, holds it no longer.
    connect(m_capture, &Capture::finished, this, [this] {
        stopHandOff();
        m_guest->end();
    });
    connect(m_guest, &SpreadGuest::launchCompleted, this, [this](const QString &token) {
        if (m_handingOff && token == m_handOffToken) {
            finishHandOff();
        }
    });
    connect(m_guest, &SpreadGuest::boardSelected, this, [this] {
        if (m_handingOff) {
            finishHandOff();
        }
    });
    // Not seen in the card's place in time: the grown card stays, showing
    // the board itself.
    connect(m_guest, &SpreadGuest::watchGaveUp, this, &Shell::stopHandOff);
    m_handOffTimeout.setSingleShot(true);
    connect(&m_handOffTimeout, &QTimer::timeout, this, [this] {
        qCDebug(DESKTOP) << "the board did not take the card's place in time";
        stopHandOff();
        m_guest->end();
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

QString Shell::boardId() const
{
    const QString id = QGuiApplication::desktopFileName();
    return (id.isEmpty() ? QStringLiteral("io.github.carlsonjm.Gooseberry") : id) + QStringLiteral(".desktop");
}

void Shell::setHandingOff(bool handingOff)
{
    if (handingOff != m_handingOff) {
        m_handingOff = handingOff;
        Q_EMIT handingOffChanged();
    }
}

void Shell::stopHandOff()
{
    m_handOffTimeout.stop();
    m_guest->stopWatching();
    m_guest->cancelLaunch();
    m_handOffToken.clear();
    setHandingOff(false);
}

void Shell::finishHandOff()
{
    m_handOffTimeout.stop();
    m_guest->stopWatching();
    m_handOffToken.clear();
    Q_EMIT handOffDone();
}

void Shell::boardFromCard()
{
    const QString note = m_capture->noteId();
    if (m_guest->active()) {
        // In Spread: the neighbours fade and the card grows to the Active
        // card's room; then the board opens, and Kadunce says when its window
        // has taken the card's place.
        setHandingOff(true);
        connect(m_guest, &SpreadGuest::expandAnswered, this, [this, note](bool expanded) {
            if (!m_handingOff) {
                return;
            }
            if (!expanded) {
                // Refused: the card shows the board itself, as without Kadunce.
                setHandingOff(false);
                return;
            }
            // The card grows over 220 ms before the board is asked for.
            QTimer::singleShot(220, this, [this, note] {
                if (!m_handingOff || !m_guest->active()) {
                    return;
                }
                m_handOffToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
                if (m_guest->prepareLaunch({boardId()}, m_handOffToken)) {
                    m_handOffTimeout.start(m_handOffWait);
                } else {
                    // Kadunce will not wait for it: the card gives Spread's
                    // centre back and fades once the board has drawn.
                    m_handOffToken.clear();
                    m_guest->end(true);
                    connect(this, &Shell::boardShown, this, [this] {
                        if (m_handingOff) {
                            finishHandOff();
                        }
                    }, Qt::SingleShotConnection);
                }
                showBoardOn(note);
            });
        }, Qt::SingleShotConnection);
        m_guest->expand();
    } else if (m_guest->overActiveCard()) {
        // Over an Active card: the board opens and Kadunce puts it in the
        // Active card's place; the grown card stays up until it has.
        setHandingOff(true);
        m_guest->watchForSelected(boardId(), m_handOffWait);
        showBoardOn(note);
    }
}

void Shell::collapseCard()
{
    stopHandOff();
    m_guest->collapse();
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
    if (window->isVisible()) {
        // A note opened from the board in the card: the card comes back to
        // its size.
        collapseCard();
    } else {
        stopHandOff();
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
