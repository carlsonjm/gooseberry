// SPDX-License-Identifier: GPL-2.0-or-later
// Reminders as the desktop shows them: the real Gooseberry on a bus of the
// test's own, in a home of its own, with the test standing in for whatever
// shows the desktop's notifications. Each reminder reaches it once, on time,
// through restarts; Done and In 10 minutes answer from the notification; and
// with nothing to show notifications, reminders wait rather than being lost.
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace {

const QString Notifications = QStringLiteral("org.freedesktop.Notifications");
const QString NotificationsPath = QStringLiteral("/org/freedesktop/Notifications");

QString isoTime(const QDateTime &time)
{
    QDateTime second = time;
    second.setTime(QTime(time.time().hour(), time.time().minute(), time.time().second()));
    return second.toOffsetFromUtc(second.offsetFromUtc()).toString(Qt::ISODate);
}

QString header(const QString &file, const QString &key)
{
    QFile note(file);
    if (!note.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QStringList lines = QString::fromUtf8(note.readAll()).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(key + QLatin1Char(':'))) {
            return line.mid(key.size() + 1).trimmed();
        }
        if (line == QLatin1String("---") && &line != &lines.first()) {
            break;
        }
    }
    return {};
}

} // namespace

// What shows the desktop's notifications, as the freedesktop.org interface
// offers it: every notification asked for is kept to look at.
class FakeNotifications : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    struct Shown {
        uint id;
        QString app;
        QString icon;
        QString summary;
        QString body;
        QStringList actions;
        QVariantMap hints;
        int timeout;
        QDateTime at;
    };
    QList<Shown> shown;

public Q_SLOTS:
    Q_SCRIPTABLE uint Notify(const QString &app, uint replaces, const QString &icon, const QString &summary,
                             const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout)
    {
        Q_UNUSED(replaces)
        const uint id = uint(shown.size()) + 1;
        shown.append({id, app, icon, summary, body, actions, hints, timeout, QDateTime::currentDateTime()});
        return id;
    }
    Q_SCRIPTABLE QStringList GetCapabilities() { return {QStringLiteral("actions"), QStringLiteral("body")}; }
    Q_SCRIPTABLE void CloseNotification(uint) { }
};

class RemindersBusTest : public QObject
{
    Q_OBJECT

public:
    explicit RemindersBusTest(const QString &home)
        : m_home(home)
    {
    }

private:
    QString m_home;
    QProcess m_gooseberry;
    FakeNotifications m_notifications;

    QString folder() const { return m_home + QStringLiteral("/Documents/Gooseberry"); }
    // Each note is kept in the Shuffle folder.
    QString pathOf(const QString &id) const { return folder() + QStringLiteral("/Shuffle/") + id + QStringLiteral(".md"); }

    void writeNote(const QString &id, const QString &text, const QString &remind)
    {
        QDir().mkpath(folder() + QStringLiteral("/Shuffle"));
        QFile file(pathOf(id));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QString now = isoTime(QDateTime::currentDateTime());
        file.write(QStringLiteral("---\ngooseberry: 2\ncreated: %1\nchanged: %1\ncolour: lake\n"
                                  "tucked: false\nremind: %2\n---\n%3\n")
                       .arg(now, remind, text)
                       .toUtf8());
    }

    void start()
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), m_home);
        environment.insert(QStringLiteral("XDG_DATA_HOME"), m_home + QStringLiteral("/share"));
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), m_home + QStringLiteral("/config"));
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), m_home + QStringLiteral("/cache"));
        environment.insert(QStringLiteral("XDG_STATE_HOME"), m_home + QStringLiteral("/state"));
        environment.insert(QStringLiteral("GOOSEBERRY_FOLDER"), folder());
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
        environment.insert(QStringLiteral("LANG"), QStringLiteral("en_US.UTF-8"));
        environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
        environment.remove(QStringLiteral("DISPLAY"));
        m_gooseberry.setProcessEnvironment(environment);
        m_gooseberry.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_gooseberry.start(QStringLiteral(GOOSEBERRY_BINARY), {QStringLiteral("--background")});
        QVERIFY(m_gooseberry.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(
            QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("io.github.carlsonjm.gooseberry")), 15000);
    }

    void stop()
    {
        m_gooseberry.terminate();
        QVERIFY(m_gooseberry.waitForFinished(5000));
    }

    void act(uint notification, const QString &action)
    {
        QDBusMessage signal = QDBusMessage::createSignal(NotificationsPath, Notifications, QStringLiteral("ActionInvoked"));
        signal.setArguments({notification, action});
        QVERIFY(QDBusConnection::sessionBus().send(signal));
    }

    int shownFor(const QString &summary) const
    {
        int count = 0;
        for (const auto &shown : m_notifications.shown) {
            count += shown.summary == summary ? 1 : 0;
        }
        return count;
    }

    const FakeNotifications::Shown *find(const QString &summary) const
    {
        for (const auto &shown : m_notifications.shown) {
            if (shown.summary == summary) {
                return &shown;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    void cleanupTestCase()
    {
        if (m_gooseberry.state() != QProcess::NotRunning) {
            stop();
        }
    }

    // With nothing on the bus to show notifications, a due reminder waits,
    // unrecorded; once something can show it, it is shown.
    void waitsForNotifications()
    {
        writeNote(QStringLiteral("2026-10-04-090000-aaaa"), QStringLiteral("Ask about Spread on Search"),
                  isoTime(QDateTime::currentDateTime().addSecs(-60)));
        writeNote(QStringLiteral("2026-10-04-090000-cccc"), QStringLiteral("Far off"),
                  isoTime(QDateTime::currentDateTime().addSecs(3600)));
        start();
        QTest::qWait(1500);
        QVERIFY(header(pathOf(QStringLiteral("2026-10-04-090000-aaaa")), QStringLiteral("reminded")).isEmpty());

        QVERIFY(QDBusConnection::sessionBus().registerObject(NotificationsPath, &m_notifications,
                                                             QDBusConnection::ExportScriptableSlots));
        QVERIFY(QDBusConnection::sessionBus().registerService(Notifications));
        QTRY_COMPARE_WITH_TIMEOUT(shownFor(QStringLiteral("Ask about Spread on Search")), 1, 5000);
        QVERIFY(!header(pathOf(QStringLiteral("2026-10-04-090000-aaaa")), QStringLiteral("reminded")).isEmpty());
    }

    // The notification: the note's first words, when and where it belongs,
    // Open, Done and In 10 minutes, under Gooseberry's own entry, and kept
    // up only as long as the desktop keeps its own.
    void whatItSays()
    {
        const auto *shown = find(QStringLiteral("Ask about Spread on Search"));
        QVERIFY(shown);
        QVERIFY2(shown->body.startsWith(QStringLiteral("Today ")) || shown->body.startsWith(QStringLiteral("Yesterday ")),
                 qPrintable(shown->body));
        QVERIFY(shown->body.endsWith(QStringLiteral(" · Shuffle")));
        QCOMPARE(shown->actions, (QStringList{QStringLiteral("default"), QStringLiteral("Open"), QStringLiteral("done"),
                                              QStringLiteral("Done"), QStringLiteral("again"), QStringLiteral("In 10 minutes")}));
        QCOMPARE(shown->hints.value(QStringLiteral("desktop-entry")).toString(), QStringLiteral("io.github.carlsonjm.Gooseberry"));
        QCOMPARE(shown->icon, QStringLiteral("io.github.carlsonjm.Gooseberry"));
        QCOMPARE(shown->timeout, -1);
    }

    // A reminder due while Gooseberry runs is shown on time, once.
    void onTime()
    {
        const QDateTime due = QDateTime::currentDateTime().addSecs(3);
        writeNote(QStringLiteral("2026-10-04-090000-bbbb"), QStringLiteral("Call about the cabin"), isoTime(due));
        QTest::qWait(1000);
        QCOMPARE(shownFor(QStringLiteral("Call about the cabin")), 0);
        QTRY_COMPARE_WITH_TIMEOUT(shownFor(QStringLiteral("Call about the cabin")), 1, 6000);
        const auto *shown = find(QStringLiteral("Call about the cabin"));
        QVERIFY(shown->at >= due.addMSecs(-1000));
        QVERIFY2(shown->at <= due.addMSecs(2000), qPrintable(shown->at.toString(Qt::ISODateWithMs)));
        QTest::qWait(1000);
        QCOMPARE(shownFor(QStringLiteral("Call about the cabin")), 1);
    }

    // Done, from the notification, marks the note done; In 10 minutes sets a
    // new reminder ten minutes on, to be shown once more.
    void doneAndAgain()
    {
        act(find(QStringLiteral("Call about the cabin"))->id, QStringLiteral("done"));
        QTRY_VERIFY(!header(pathOf(QStringLiteral("2026-10-04-090000-bbbb")), QStringLiteral("done")).isEmpty());

        const QDateTime before = QDateTime::currentDateTime();
        act(find(QStringLiteral("Ask about Spread on Search"))->id, QStringLiteral("again"));
        const QString path = pathOf(QStringLiteral("2026-10-04-090000-aaaa"));
        QTRY_VERIFY(header(path, QStringLiteral("reminded")).isEmpty());
        const QDateTime again = QDateTime::fromString(header(path, QStringLiteral("remind")), Qt::ISODate);
        QVERIFY(qAbs(again.secsTo(before.addSecs(600))) <= 3);
    }

    // Gooseberry started again shows nothing it has shown already.
    void notAgainAfterRestart()
    {
        const auto before = m_notifications.shown.size();
        stop();
        start();
        QTest::qWait(2500);
        QCOMPARE(m_notifications.shown.size(), before);
    }

    // Gooseberry's own trail is all in the test's home.
    void stillRunning()
    {
        QCOMPARE(m_gooseberry.state(), QProcess::Running);
        QVERIFY(QFile::exists(folder() + QStringLiteral("/.gooseberry")));
    }
};

int main(int argc, char *argv[])
{
    // Only on a bus of the test's own, as tests/CMakeLists.txt starts it.
    if (qEnvironmentVariable("GOOSEBERRY_PRIVATE_BUS") != QLatin1String("1")) {
        fputs("Runs only on the private bus its test entry starts.\n", stderr);
        return 77;
    }
    QTemporaryDir home(QDir::tempPath() + QStringLiteral("/gooseberry-reminders-XXXXXX"));
    if (!home.isValid()) {
        return 1;
    }
    QCoreApplication app(argc, argv);
    RemindersBusTest test(home.path());
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reminders_bus.moc"
