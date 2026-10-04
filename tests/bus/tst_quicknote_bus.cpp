// SPDX-License-Identifier: GPL-2.0-or-later
// The quick note's session-bus interface, used as a desktop search would use
// it: by a client that is not Gooseberry and loads none of its code. The test
// starts the real Gooseberry on a private bus, in a home of its own, drives the
// quick note through the bus alone, and reads the note files from disk to see
// what was kept.
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

const QString Service = QStringLiteral("io.github.carlsonjm.gooseberry");
const QString Path = QStringLiteral("/QuickNote");
const QString Interface = QStringLiteral("io.github.carlsonjm.Gooseberry.QuickNote");

// A note file read as text: its header keys and the text under them.
struct NoteFile {
    QMap<QString, QString> header;
    QString text;
    bool exists = false;
};

NoteFile readNote(const QString &path)
{
    NoteFile note;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return note;
    }
    note.exists = true;
    const QString content = QString::fromUtf8(file.readAll());
    const qsizetype end = content.indexOf(QLatin1String("\n---\n"), 3);
    if (!content.startsWith(QLatin1String("---\n")) || end < 0) {
        note.text = content;
        return note;
    }
    for (const QString &line : content.mid(4, end - 4).split(QLatin1Char('\n'))) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon > 0) {
            QString value = line.mid(colon + 1).trimmed();
            if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
                value = value.mid(1, value.size() - 2);
            }
            note.header.insert(line.left(colon).trimmed(), value);
        }
    }
    note.text = content.mid(end + 5);
    return note;
}

// A dictionary inside the reply, however the bus delivered it.
QVariantMap asMap(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>()) {
        return qdbus_cast<QVariantMap>(value.value<QDBusArgument>());
    }
    return value.toMap();
}

QVariantList asList(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>()) {
        return qdbus_cast<QVariantList>(value.value<QDBusArgument>());
    }
    return value.toList();
}

} // namespace

class QuickNoteBusTest : public QObject
{
    Q_OBJECT

public:
    explicit QuickNoteBusTest(const QString &home)
        : m_home(home)
    {
    }

private:
    QString m_home;
    QProcess m_gooseberry;
    std::unique_ptr<QDBusInterface> m_note;

    QString folder() const { return m_home + QStringLiteral("/Documents/Gooseberry"); }
    QString pathOf(const QString &id) const { return folder() + QLatin1Char('/') + id + QStringLiteral(".md"); }

    QVariantMap call(const QString &method, const QVariantList &arguments = {})
    {
        const QDBusMessage reply = m_note->callWithArgumentList(QDBus::Block, method, arguments);
        if (reply.type() != QDBusMessage::ReplyMessage) {
            qWarning().noquote() << method << reply.errorMessage();
            return {};
        }
        return asMap(reply.arguments().value(0));
    }

private Q_SLOTS:
    void initTestCase()
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
        environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
        environment.remove(QStringLiteral("DISPLAY"));
        m_gooseberry.setProcessEnvironment(environment);
        m_gooseberry.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_gooseberry.start(QStringLiteral(GOOSEBERRY_BINARY), {QStringLiteral("--background")});
        QVERIFY(m_gooseberry.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QDBusConnection::sessionBus().interface()->isServiceRegistered(Service), 15000);
        m_note = std::make_unique<QDBusInterface>(Service, Path, Interface);
        QVERIFY2(m_note->isValid(), qPrintable(m_note->lastError().message()));
    }

    void cleanupTestCase()
    {
        m_gooseberry.terminate();
        m_gooseberry.waitForFinished(5000);
    }

    void protocolVersion()
    {
        const QDBusReply<uint> version = m_note->call(QStringLiteral("ProtocolVersion"));
        QVERIFY(version.isValid());
        QCOMPARE(version.value(), 1u);
    }

    // Start gives an empty note with everything a pad needs to draw it, and
    // nothing is kept until it has text.
    void startOffersAnEmptyNote()
    {
        const QVariantMap state = call(QStringLiteral("Start"));
        QVERIFY(state.value(QStringLiteral("open")).toBool());
        QVERIFY(!state.value(QStringLiteral("kept")).toBool());
        QCOMPARE(state.value(QStringLiteral("text")).toString(), QString());
        QCOMPARE(state.value(QStringLiteral("colour")).toString(), QStringLiteral("butter"));
        QCOMPARE(state.value(QStringLiteral("colours")).toStringList(),
                 (QStringList{QStringLiteral("butter"), QStringLiteral("rhyolite"), QStringLiteral("lake"),
                              QStringLiteral("lichen"), QStringLiteral("stone")}));
        QCOMPARE(state.value(QStringLiteral("colourHexes")).toStringList().value(2), QStringLiteral("#9ED6CB"));
        QCOMPARE(state.value(QStringLiteral("belongs")).toString(), QStringLiteral("loose"));
        QStringList kinds;
        for (const QVariant &choice : asList(state.value(QStringLiteral("choices")))) {
            kinds.append(asMap(choice).value(QStringLiteral("kind")).toString());
        }
        QCOMPARE(kinds, (QStringList{QStringLiteral("workspace"), QStringLiteral("loose")}));
        QVERIFY(QDir(folder()).entryList({QStringLiteral("*.md")}, QDir::Files).isEmpty());
    }

    // Each change is on disk when its call returns, and Start again resumes
    // the same note.
    void changesAreKeptAsTheyCome()
    {
        QVariantMap state = call(QStringLiteral("SetText"), {QStringLiteral("F")});
        QVERIFY(state.value(QStringLiteral("kept")).toBool());
        const QString id = state.value(QStringLiteral("id")).toString();
        QVERIFY(!id.isEmpty());
        QCOMPARE(readNote(pathOf(id)).text, QStringLiteral("F"));

        call(QStringLiteral("SetText"), {QStringLiteral("Flick threshold")});
        QCOMPARE(readNote(pathOf(id)).text, QStringLiteral("Flick threshold"));

        state = call(QStringLiteral("SetColour"), {QStringLiteral("lake")});
        QCOMPARE(state.value(QStringLiteral("colour")).toString(), QStringLiteral("lake"));
        QCOMPARE(readNote(pathOf(id)).header.value(QStringLiteral("colour")), QStringLiteral("lake"));

        state = call(QStringLiteral("SetBelongs"), {QStringLiteral("project"), QStringLiteral("Shuffle")});
        QCOMPARE(state.value(QStringLiteral("belongs")).toString(), QStringLiteral("project"));
        const NoteFile file = readNote(pathOf(id));
        QCOMPARE(file.header.value(QStringLiteral("belongs")), QStringLiteral("project"));
        QCOMPARE(file.header.value(QStringLiteral("project")), QStringLiteral("Shuffle"));
        bool chosen = false;
        for (const QVariant &choice : asList(state.value(QStringLiteral("choices")))) {
            const QVariantMap map = asMap(choice);
            chosen |= map.value(QStringLiteral("kind")).toString() == QLatin1String("project")
                && map.value(QStringLiteral("project")).toString() == QLatin1String("Shuffle")
                && map.value(QStringLiteral("chosen")).toBool();
        }
        QVERIFY(chosen);

        // Nonsense is refused, and changes nothing.
        QVERIFY(m_note->call(QStringLiteral("SetColour"), QStringLiteral("plum")).type() == QDBusMessage::ErrorMessage);
        QVERIFY(m_note->call(QStringLiteral("SetBelongs"), QStringLiteral("drawer"), QString()).type()
                == QDBusMessage::ErrorMessage);
        QCOMPARE(readNote(pathOf(id)).header.value(QStringLiteral("colour")), QStringLiteral("lake"));

        const QVariantMap resumed = call(QStringLiteral("Start"));
        QCOMPARE(resumed.value(QStringLiteral("id")).toString(), id);
        QCOMPARE(resumed.value(QStringLiteral("text")).toString(), QStringLiteral("Flick threshold"));
    }

    // Written elsewhere while the pad is open, the note comes back as Changed.
    void changesFromElsewhereAreTold()
    {
        const QString id = call(QStringLiteral("State")).value(QStringLiteral("id")).toString();
        QSignalSpy changed(m_note.get(), SIGNAL(Changed(QVariantMap)));
        QFile file(pathOf(id));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        content.replace(QLatin1String("\n---\nFlick threshold"), QLatin1String("\n---\nFlick threshold, measured"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content.toUtf8());
        file.close();
        QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 5000);
        const QVariantMap state = asMap(changed.last().value(0));
        QCOMPARE(state.value(QStringLiteral("text")).toString(), QStringLiteral("Flick threshold, measured"));
    }

    // Done ends the note; the next Start begins another.
    void doneEndsTheNote()
    {
        const QString id = call(QStringLiteral("State")).value(QStringLiteral("id")).toString();
        const QVariantMap state = call(QStringLiteral("Done"));
        QVERIFY(!state.value(QStringLiteral("open")).toBool());
        QVERIFY(readNote(pathOf(id)).exists);
        const QVariantMap next = call(QStringLiteral("Start"));
        QVERIFY(next.value(QStringLiteral("open")).toBool());
        QCOMPARE(next.value(QStringLiteral("text")).toString(), QString());
        QVERIFY(next.value(QStringLiteral("id")).toString() != id);
        call(QStringLiteral("Done"));
    }

    void tuckAwayKeepsItOutOfSight()
    {
        call(QStringLiteral("Start"));
        const QString id = call(QStringLiteral("SetText"), {QStringLiteral("Groceries")}).value(QStringLiteral("id")).toString();
        const QVariantMap state = call(QStringLiteral("TuckAway"));
        QVERIFY(!state.value(QStringLiteral("open")).toBool());
        QCOMPARE(readNote(pathOf(id)).header.value(QStringLiteral("tucked")), QStringLiteral("true"));
    }

    // Remove sends the note to the desktop's trash, and UndoRemove brings it back.
    void removeGoesToTheTrash()
    {
        call(QStringLiteral("Start"));
        const QString id = call(QStringLiteral("SetText"), {QStringLiteral("Wallpaper idea")}).value(QStringLiteral("id")).toString();
        call(QStringLiteral("Remove"));
        QVERIFY(!QFile::exists(pathOf(id)));
        const QString trashed = m_home + QStringLiteral("/share/Trash/files/") + id + QStringLiteral(".md");
        QCOMPARE(readNote(trashed).text, QStringLiteral("Wallpaper idea"));
        QVERIFY(QFile::exists(m_home + QStringLiteral("/share/Trash/info/") + id + QStringLiteral(".md.trashinfo")));
        QVERIFY(call(QStringLiteral("UndoRemove")).value(QStringLiteral("restored")).toBool());
        QCOMPARE(readNote(pathOf(id)).text, QStringLiteral("Wallpaper idea"));
    }

    // All notes: the board opens as a window, and BoardShown says, with the
    // caller's token, when it has drawn.
    void boardSaysWhenItHasArrived()
    {
        QSignalSpy shown(m_note.get(), SIGNAL(BoardShown(QString)));
        const QDir notes(folder());
        const QString id = notes.entryList({QStringLiteral("*.md")}, QDir::Files).value(0).chopped(3);
        const QDBusReply<bool> accepted = m_note->call(QStringLiteral("OpenBoard"), id, QStringLiteral("token-1"));
        QVERIFY(accepted.isValid() && accepted.value());
        QTRY_VERIFY_WITH_TIMEOUT(shown.count() >= 1, 10000);
        QCOMPARE(shown.first().value(0).toString(), QStringLiteral("token-1"));
    }

    // Added in version 1 for the planner, ignorable by a caller that does
    // not know them: a reminder, from the same choices as the card's Remind,
    // and a checklist ticked line by line. Each is on disk when the call
    // returns.
    void remindersAndChecklists()
    {
        QVariantMap state = call(QStringLiteral("Start"));
        QCOMPARE(state.value(QStringLiteral("remind")).toString(), QString());
        QVERIFY(!state.value(QStringLiteral("checklist")).toBool());
        // From Milestone 2: a note typed in the search has no handwriting.
        QVERIFY(state.contains(QStringLiteral("ink")) && state.value(QStringLiteral("ink")).toString().isEmpty());
        QVERIFY(state.contains(QStringLiteral("read")) && state.value(QStringLiteral("read")).toString().isEmpty());
        QStringList kinds;
        for (const QVariant &choice : asList(state.value(QStringLiteral("remindChoices")))) {
            const QVariantMap map = asMap(choice);
            kinds.append(map.value(QStringLiteral("kind")).toString());
            QVERIFY(!map.value(QStringLiteral("label")).toString().isEmpty());
            if (map.value(QStringLiteral("kind")) == QLatin1String("tomorrow")) {
                QCOMPARE(QDateTime::fromString(map.value(QStringLiteral("time")).toString(), Qt::ISODate),
                         QDateTime(QDate::currentDate().addDays(1), QTime(9, 0)));
            }
        }
        QVERIFY(kinds.contains(QStringLiteral("tomorrow")));
        QCOMPARE(kinds.last(), QStringLiteral("pick"));
        // Written on no window, so no "next time this opens".
        QVERIFY(!kinds.contains(QStringLiteral("opens")));

        state = call(QStringLiteral("SetText"), {QStringLiteral("Groceries\ncoffee\noats")});
        const QString id = state.value(QStringLiteral("id")).toString();
        const QDateTime tomorrow(QDate::currentDate().addDays(1), QTime(9, 0));
        const QString iso = tomorrow.toOffsetFromUtc(tomorrow.offsetFromUtc()).toString(Qt::ISODate);
        state = call(QStringLiteral("SetReminder"), {iso});
        QCOMPARE(state.value(QStringLiteral("remind")).toString(), iso);
        QVERIFY(state.value(QStringLiteral("remindLabel")).toString().startsWith(QStringLiteral("Tomorrow ")));
        QCOMPARE(readNote(pathOf(id)).header.value(QStringLiteral("remind")), iso);

        for (const QString &wrong : {QStringLiteral("opens"), QStringLiteral("soon")}) {
            const QDBusMessage refused = m_note->call(QStringLiteral("SetReminder"), wrong);
            QCOMPARE(refused.type(), QDBusMessage::ErrorMessage);
        }
        QCOMPARE(readNote(pathOf(id)).header.value(QStringLiteral("remind")), iso);
        state = call(QStringLiteral("SetReminder"), {QString()});
        QCOMPARE(state.value(QStringLiteral("remind")).toString(), QString());
        QVERIFY(!readNote(pathOf(id)).header.contains(QStringLiteral("remind")));

        state = call(QStringLiteral("SetChecklist"), {true});
        QVERIFY(state.value(QStringLiteral("checklist")).toBool());
        QCOMPARE(readNote(pathOf(id)).text, QStringLiteral("Groceries\n- [ ] coffee\n- [ ] oats"));
        const QVariantList lines = asList(state.value(QStringLiteral("lines")));
        QCOMPARE(lines.size(), 3);
        QVERIFY(!asMap(lines.at(0)).value(QStringLiteral("item")).toBool());
        QCOMPARE(asMap(lines.at(2)).value(QStringLiteral("text")).toString(), QStringLiteral("oats"));
        state = call(QStringLiteral("SetLineChecked"), {2u, true});
        QVERIFY(asMap(asList(state.value(QStringLiteral("lines"))).at(2)).value(QStringLiteral("checked")).toBool());
        QCOMPARE(readNote(pathOf(id)).text, QStringLiteral("Groceries\n- [ ] coffee\n- [x] oats"));
        QCOMPARE(m_note->call(QStringLiteral("SetLineChecked"), 0u, true).type(), QDBusMessage::ErrorMessage);
        QCOMPARE(m_note->call(QStringLiteral("SetLineChecked"), 9u, true).type(), QDBusMessage::ErrorMessage);
        state = call(QStringLiteral("SetChecklist"), {false});
        QCOMPARE(readNote(pathOf(id)).text, QStringLiteral("Groceries\ncoffee\noats"));
        call(QStringLiteral("Done"));
    }

    // Gooseberry still running and well after all of it.
    void stillRunning()
    {
        QCOMPARE(m_gooseberry.state(), QProcess::Running);
    }
};

int main(int argc, char *argv[])
{
    // Only on a bus of the test's own, as tests/CMakeLists.txt starts it.
    if (qEnvironmentVariable("GOOSEBERRY_PRIVATE_BUS") != QLatin1String("1")) {
        fputs("Runs only on the private bus its test entry starts.\n", stderr);
        return 77;
    }
    QTemporaryDir home(QDir::tempPath() + QStringLiteral("/gooseberry-bus-XXXXXX"));
    if (!home.isValid()) {
        return 1;
    }
    QCoreApplication app(argc, argv);
    QuickNoteBusTest test(home.path());
    return QTest::qExec(&test, argc, argv);
}

#include "tst_quicknote_bus.moc"
