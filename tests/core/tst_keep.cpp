// SPDX-License-Identifier: GPL-2.0-or-later
// A note survives a crash, a logout and a restart. A second copy of this test
// types a note letter by letter into its own folder and says each letter once
// the call that typed it has returned; the first copy then ends it the way a
// crash or a logout would, and reads the folder as Gooseberry would on the
// next start.
#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "TestHome.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTest>

#include <csignal>
#include <cstdio>
#include <unistd.h>

using namespace Gooseberry;

namespace {

const QString Words = QStringLiteral("Flick threshold feels short. Measure it.");

int typeAndWait(const QString &folder, int letters)
{
    NoteStore store(folder);
    if (!store.open()) {
        return 2;
    }
    Capture capture(&store);
    capture.startNew({QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")});
    capture.setColour(QStringLiteral("rhyolite"));
    capture.setBelongs(QStringLiteral("project"), QStringLiteral("Shuffle"));
    for (int i = 1; i <= letters; ++i) {
        capture.setText(Words.left(i));
        std::printf("%d\n", i);
        std::fflush(stdout);
    }
    // Still running, the sheet still up, when the end comes.
    for (;;) {
        pause();
    }
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

    QString typeInChild(const QString &name, int letters, bool crash)
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
        const QByteArray last = QByteArray::number(letters).append('\n');
        while (!said.contains(last)) {
            if (!child.waitForReadyRead(10000)) {
                return {};
            }
            said += child.readAllStandardOutput();
        }
        if (crash) {
            child.kill(); // As a crash: no chance to finish anything.
        } else {
            child.terminate(); // As a logout or shutdown asks a program to end.
        }
        child.waitForFinished();
        return folder;
    }

private Q_SLOTS:
    void survivesACrashFromTheFirstLetter()
    {
        const QString folder = typeInChild(QStringLiteral("crash-1"), 1, true);
        QVERIFY(m_home->holds(folder));
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        QCOMPARE(next.notes().first().text, QStringLiteral("F"));
    }

    void survivesACrashMidSentence()
    {
        const int letters = 23;
        const QString folder = typeInChild(QStringLiteral("crash-23"), letters, true);
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        const Note note = next.notes().first();
        QCOMPARE(note.text, Words.left(letters));
        QCOMPARE(note.colour, QStringLiteral("rhyolite"));
        QCOMPARE(note.belongs, Belongs::Project);
        QCOMPARE(note.project, QStringLiteral("Shuffle"));
        QCOMPARE(note.window, QStringLiteral("SpreadGesture.qml"));
        // Nothing half-written is left behind beside it.
        const QStringList left = QDir(folder).entryList(QDir::Files | QDir::Hidden);
        QCOMPARE(left, (QStringList{QStringLiteral(".gooseberry"), note.id + QStringLiteral(".md")}));
    }

    void survivesALogout()
    {
        const QString folder = typeInChild(QStringLiteral("logout"), int(Words.size()), false);
        NoteStore next(folder);
        QVERIFY(next.open());
        QCOMPARE(next.notes().size(), 1);
        QCOMPARE(next.notes().first().text, Words);
    }

    void comesBackOnTheBoardAfterARestart()
    {
        const QString folder = typeInChild(QStringLiteral("restart"), int(Words.size()), true);
        NoteStore next(folder);
        QVERIFY(next.open());
        Places places(&next);
        QCOMPARE(places.countFor(QStringLiteral("project:Shuffle")), 1);
        QCOMPARE(places.countFor(QStringLiteral("today")), 1);
        QCOMPARE(places.projects(), QStringList{QStringLiteral("Shuffle")});
        PlaceNotes board(&next);
        board.setPlace(QStringLiteral("project:Shuffle"));
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
