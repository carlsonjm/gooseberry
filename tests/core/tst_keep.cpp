// SPDX-License-Identifier: GPL-2.0-or-later
// A note survives a crash, a logout and a restart. A second copy of this test
// types a note letter by letter into its own folder, faster than the pause that
// writes typing, and says each letter once the call that typed it has returned,
// and "rested" once the writing has paused and been written. The first copy
// then ends it the way a crash or a logout would, and reads the folder as
// Gooseberry would on the next start.
#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "TestHome.h"

#include <KSignalHandler>

#include <QCoreApplication>
#include <QProcess>
#include <QTest>
#include <QTimer>

#include <csignal>
#include <cstdio>

using namespace Gooseberry;

namespace {

const QString Words = QStringLiteral("Flick threshold feels short. Measure it.");

// Quick typing: well inside the pause, so nothing is written between letters.
constexpr int TypingMs = 10;

int typeAndWait(const QString &folder, int letters)
{
    NoteStore store(folder);
    if (!store.open()) {
        return 2;
    }
    Capture capture(&store);
    capture.startNew({QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")});
    capture.setColour(QStringLiteral("rhyolite"));
    capture.makeFolder(QStringLiteral("Shuffle"));

    // As Gooseberry does: a logout's signal quits the usual way.
    for (int signal : {SIGTERM, SIGINT, SIGHUP}) {
        KSignalHandler::self()->watchSignal(signal);
    }
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, qApp, &QCoreApplication::quit);

    int typed = 0;
    QTimer keys;
    keys.setInterval(TypingMs);
    QTimer rest;
    rest.setInterval(10);
    QObject::connect(&keys, &QTimer::timeout, [&] {
        ++typed;
        capture.setText(Words.left(typed));
        std::printf("%d\n", typed);
        std::fflush(stdout);
        if (typed == letters) {
            keys.stop();
            rest.start();
        }
    });
    QObject::connect(&rest, &QTimer::timeout, [&] {
        if (!capture.waiting()) {
            rest.stop();
            std::printf("rested\n");
            std::fflush(stdout);
        }
    });
    keys.start();
    // Still running, the sheet still up, when the end comes.
    return QCoreApplication::exec();
}

} // namespace

class KeepTest : public QObject
{
    Q_OBJECT

public:
    explicit KeepTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;

    enum class End {
        CrashAtOnce, // Right after the last letter, with typing still waiting.
        CrashAfterAPause, // Once the writing has paused and been written.
        Logout, // Right after the last letter, as a logout asks a program to end.
    };

    QString typeInChild(const QString &name, int letters, End end)
    {
        const QString folder = m_home->path() + QLatin1Char('/') + name;
        QProcess child;
        child.setProgram(QCoreApplication::applicationFilePath());
        child.setArguments({QStringLiteral("--type"), folder, QString::number(letters)});
        child.start();
        if (!child.waitForStarted()) {
            return {};
        }
        QByteArray said;
        const QByteArray last = end == End::CrashAfterAPause ? QByteArray("rested\n")
                                                             : QByteArray::number(letters).append('\n');
        while (!said.contains(last)) {
            if (!child.waitForReadyRead(10000)) {
                return {};
            }
            said += child.readAllStandardOutput();
        }
        if (end == End::Logout) {
            child.terminate();
        } else {
            child.kill(); // As a crash: no chance to finish anything.
        }
        child.waitForFinished();
        return folder;
    }

    // Nothing half-written is left behind beside the note.
    static void onlyTheNote(const QString &folder, const Note &note)
    {
        // Nothing half written beside the note, in its folder or the notes
        // folder.
        const QStringList left = QDir(folder).entryList(QDir::Files | QDir::Hidden);
        QCOMPARE(left, QStringList{QStringLiteral(".gooseberry")});
        const QStringList inFolder = QDir(folder + QLatin1Char('/') + note.folder).entryList(QDir::Files | QDir::Hidden);
        QCOMPARE(inFolder, QStringList{note.id + QStringLiteral(".md")});
    }

private Q_SLOTS:
    void survivesACrashFromTheFirstLetter()
    {
        const QString folder = typeInChild(QStringLiteral("crash-1"), 1, End::CrashAtOnce);
        QVERIFY(m_home->holds(folder));
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        QCOMPARE(next.notes().first().text, QStringLiteral("F"));
    }

    void aCrashMidSentenceLosesOnlyTheTypingSinceThePause()
    {
        const int letters = 23;
        const QString folder = typeInChild(QStringLiteral("crash-23"), letters, End::CrashAtOnce);
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        const Note note = next.notes().first();
        QVERIFY2(!note.text.isEmpty() && Words.left(letters).startsWith(note.text), qPrintable(note.text));
        QCOMPARE(note.colour, QStringLiteral("rhyolite"));
        QCOMPARE(note.folder, QStringLiteral("Shuffle"));
        onlyTheNote(folder, note);
    }

    void survivesACrashAfterAPause()
    {
        const int letters = 23;
        const QString folder = typeInChild(QStringLiteral("crash-rested"), letters, End::CrashAfterAPause);
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        const Note note = next.notes().first();
        QCOMPARE(note.text, Words.left(letters));
        QCOMPARE(note.colour, QStringLiteral("rhyolite"));
        QCOMPARE(note.folder, QStringLiteral("Shuffle"));
        QVERIFY(note.isStuck());
        QCOMPARE(note.window, QStringLiteral("SpreadGesture.qml"));
        onlyTheNote(folder, note);
    }

    void survivesALogout()
    {
        // Ended mid-run, with typing still waiting: the logout writes it.
        const QString folder = typeInChild(QStringLiteral("logout"), int(Words.size()), End::Logout);
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        QCOMPARE(next.notes().first().text, Words);
    }

    void comesBackOnTheBoardAfterARestart()
    {
        const QString folder = typeInChild(QStringLiteral("restart"), int(Words.size()), End::CrashAfterAPause);
        NoteStore next(folder);
        QVERIFY(next.open());
        Places places(&next);
        QCOMPARE(places.countFor(QStringLiteral("folder:Shuffle")), 1);
        QCOMPARE(places.countFor(QStringLiteral("today")), 1);
        QCOMPARE(places.folders(), QStringList{QStringLiteral("Shuffle")});
        PlaceNotes board(&next);
        board.setPlace(QStringLiteral("folder:Shuffle"));
        QCOMPARE(board.count(), 1);
        QCOMPARE(board.index(0).data(PlaceNotes::TitleRole).toString(), Words);
    }
};

int main(int argc, char *argv[])
{
    if (argc == 4 && qstrcmp(argv[1], "--type") == 0) {
        // The typing copy: started by the test below, inside its home.
        QCoreApplication app(argc, argv);
        if (!qEnvironmentVariable("HOME").contains(QStringLiteral("gooseberry-test-"))) {
            return 3;
        }
        return typeAndWait(QString::fromLocal8Bit(argv[2]), QByteArray(argv[3]).toInt());
    }
    TestHome home;
    QCoreApplication app(argc, argv);
    KeepTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_keep.moc"
