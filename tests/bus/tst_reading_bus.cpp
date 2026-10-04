// SPDX-License-Identifier: GPL-2.0-or-later
// Reading handwriting in the real Gooseberry, on a bus and in a home of the
// test's own, with the stand-in reader (tests/reading/make-fake-reader.py):
// handwriting kept but never read, as after Gooseberry gains its reader or a
// note arrives from elsewhere, is read a little after Gooseberry starts, and
// what it says is kept in the note where the file index and Search look.
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class ReadingBusTest : public QObject
{
    Q_OBJECT

public:
    explicit ReadingBusTest(const QString &home)
        : m_home(home)
    {
    }

private:
    QString m_home;
    QProcess m_gooseberry;

    QString folder() const { return m_home + QStringLiteral("/Documents/Gooseberry"); }

    QByteArray note(const QString &id) const
    {
        QFile file(folder() + QLatin1Char('/') + id + QStringLiteral(".md"));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private Q_SLOTS:
    void cleanupTestCase()
    {
        m_gooseberry.terminate();
        m_gooseberry.waitForFinished(5000);
    }

    void unreadHandwritingIsReadAfterAStart()
    {
        const QString id = QStringLiteral("2026-10-04-090000-ink1");
        QVERIFY(QDir().mkpath(folder()));
        // A line of handwriting, as Gooseberry keeps it, never read.
        QFile drawing(folder() + QLatin1Char('/') + id + QStringLiteral(".svg"));
        QVERIFY(drawing.open(QIODevice::WriteOnly));
        drawing.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:gooseberry=\"https://github.com/carlsonjm/gooseberry/ink\" "
                      "width=\"716\" height=\"42\" viewBox=\"0 0 716 42\" gooseberry:format=\"1\">\n"
                      " <title>Handwriting</title>\n"
                      " <g gooseberry:row=\"0\">\n"
                      "  <path fill=\"#1A1A1A\" d=\"M20 20 L200 22 Z\" gooseberry:ink=\"black\" "
                      "gooseberry:points=\"20,20,0.5 80,24,0.6 140,18,0.6 200,22,0.5\"/>\n"
                      " </g>\n"
                      "</svg>\n");
        drawing.close();
        QFile file(folder() + QLatin1Char('/') + id + QStringLiteral(".md"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("---\ngooseberry: 1\ncreated: 2026-10-04T09:00:00-05:00\nchanged: 2026-10-04T09:00:00-05:00\n"
                   "colour: butter\nbelongs: loose\ntucked: false\nink: \"2026-10-04-090000-ink1.svg\"\n---\n");
        file.close();

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), m_home);
        environment.insert(QStringLiteral("XDG_DATA_HOME"), m_home + QStringLiteral("/share"));
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), m_home + QStringLiteral("/config"));
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), m_home + QStringLiteral("/cache"));
        environment.insert(QStringLiteral("XDG_STATE_HOME"), m_home + QStringLiteral("/state"));
        environment.insert(QStringLiteral("GOOSEBERRY_FOLDER"), folder());
        environment.insert(QStringLiteral("GOOSEBERRY_READER_DIR"), QStringLiteral(FAKE_READER));
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
        environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
        environment.remove(QStringLiteral("DISPLAY"));
        m_gooseberry.setProcessEnvironment(environment);
        // The reader's own start-up chatter is not this test's to show.
        m_gooseberry.setProcessChannelMode(QProcess::MergedChannels);
        m_gooseberry.setStandardOutputFile(QProcess::nullDevice());
        m_gooseberry.start(QStringLiteral(GOOSEBERRY_BINARY), {QStringLiteral("--background")});
        QVERIFY(m_gooseberry.waitForStarted());

        QTRY_VERIFY_WITH_TIMEOUT(note(id).contains("read: \"measure flick velocity\"\n"), 20000);
        QVERIFY(note(id).contains("read-also: \"flack\"\n"));
        // Reading is not a change the person made.
        QVERIFY(note(id).contains("changed: 2026-10-04T09:00:00-05:00\n"));
        QCOMPARE(m_gooseberry.state(), QProcess::Running);
    }
};

int main(int argc, char *argv[])
{
    if (qEnvironmentVariable("GOOSEBERRY_PRIVATE_BUS") != QLatin1String("1")) {
        fputs("Runs only on the private bus its test entry starts.\n", stderr);
        return 77;
    }
    QTemporaryDir home(QDir::tempPath() + QStringLiteral("/gooseberry-reading-XXXXXX"));
    if (!home.isValid()) {
        return 1;
    }
    QCoreApplication app(argc, argv);
    ReadingBusTest test(home.path());
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reading_bus.moc"
