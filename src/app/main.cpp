// SPDX-License-Identifier: GPL-2.0-or-later
#include "NoteStore.h"
#include "Product.h"
#include "QuickNoteService.h"
#include "Shell.h"
#include "StuckNotes.h"
#include "SpreadGuest.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KSignalHandler>

#include <QApplication>
#include <QIcon>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QSessionManager>
#include <QtQml/qqmlextensionplugin.h>

#include <csignal>

Q_IMPORT_QML_PLUGIN(io_github_carlsonjm_gooseberryPlugin)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("gooseberry");

    // Named once, before anything shows: Notes where Shuffle is installed,
    // Gooseberry elsewhere. Every title and sentence that names the program
    // reads it from here.
    Gooseberry::setInsideShuffle(Gooseberry::shuffleInstalled());
    KAboutData about(QStringLiteral("gooseberry"), Gooseberry::productName(), QStringLiteral(GOOSEBERRY_VERSION),
                     i18n("Notes, stickies and a planner"), KAboutLicense::GPL_V2);
    about.setOrganizationDomain("carlsonjm.github.io");
    about.setDesktopFileName(QStringLiteral(GOOSEBERRY_APP_ID));
    KAboutData::setApplicationData(about);
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(GOOSEBERRY_APP_ID)));

    // Gooseberry stays running between notes, so the next tap is answered at
    // once; closing the board or the card only puts it away.
    app.setQuitOnLastWindowClosed(false);
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    // A second start hands its arguments to the running Gooseberry and ends.
    KDBusService service(KDBusService::Unique | KDBusService::NoExitOnFailure);

    // The session starts Gooseberry again from its autostart entry. Typing
    // still waiting for a pause is written when the session asks (Shell).
    QObject::connect(&app, &QGuiApplication::commitDataRequest, [](QSessionManager &manager) {
        manager.setRestartHint(QSessionManager::RestartNever);
    });

    // A logout or shutdown ends Gooseberry with a signal. It quits the usual
    // way instead, so typing waiting for a pause is written first.
    for (int signal : {SIGTERM, SIGINT, SIGHUP}) {
        KSignalHandler::self()->watchSignal(signal);
    }
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, &app, &QCoreApplication::quit);

    Gooseberry::NoteStore store(Gooseberry::NoteStore::defaultFolder());
    if (!store.open()) {
        qWarning().noquote() << store.lastError();
    }

    QQmlEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    Gooseberry::Shell shell(&store, &engine);
    // The quick note for a desktop search that draws it in its own window.
    Gooseberry::QuickNoteService quickNote(&shell, &store);
    if (!quickNote.publish()) {
        qWarning() << "Gooseberry's quick note is not on the session bus";
    }
    // Which windows have notes stuck to them, for a title bar's dot and
    // Spread's stacks.
    if (!shell.stuck()->publish()) {
        qWarning() << "Gooseberry's stuck notes are not on the session bus";
    }
    // Where Kadunce gives the card Spread's centre, it answers here.
    shell.guest()->publish();
    QObject::connect(&service, &KDBusService::activateRequested, &shell,
                     [&shell](const QStringList &arguments, const QString &) {
                         shell.handle(arguments);
                     });

    shell.handle(app.isSessionRestored() ? QStringList{app.arguments().value(0), QStringLiteral("--background")}
                                         : app.arguments());
    return app.exec();
}
