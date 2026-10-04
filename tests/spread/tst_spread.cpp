// SPDX-License-Identifier: GPL-2.0-or-later
// The quick-note card in Kadunce's card workspace, against a stand-in Kadunce
// on a bus of the test's own. The stand-in answers on Kadunce's name and
// object, from a thread of its own as the compositor would, and records what
// it is asked; the real card and board are drawn off screen in a home of the
// test's own. Whether Kadunce itself moves its cards as it says can only be
// seen on a tablet; this proves Gooseberry's side of every exchange.
#include "NoteStore.h"
#include "Shell.h"
#include "SpreadGuest.h"
#include "TestHome.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(io_github_carlsonjm_gooseberryPlugin)

using namespace Gooseberry;

namespace {

const QString KWin = QStringLiteral("org.kde.KWin");
const QString KadunceObject = QStringLiteral("/Kadunce");
const QString KadunceInterface = QStringLiteral("studio.warbler.Kadunce");
const QString FakeConnection = QStringLiteral("fake-kadunce");
const QString BoardId = QStringLiteral("io.github.carlsonjm.Gooseberry.desktop");

QJsonObject jsonRect(const QRect &rect)
{
    return {{QStringLiteral("x"), rect.x()}, {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()}};
}

QQuickItem *itemNamed(QQuickItem *root, const QString &name)
{
    const auto children = root->childItems();
    for (QQuickItem *child : children) {
        if (child->objectName() == name) {
            return child;
        }
        if (QQuickItem *found = itemNamed(child, name)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

// Kadunce as Gooseberry sees it on the bus: the calls the card makes, answered
// as the test sets, and every call kept to look at afterwards.
class FakeKadunce : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "studio.warbler.Kadunce")

public:
    struct Call {
        QString method;
        QVariantList arguments;
        QString sender;
    };

    // What Kadunce shows and how it answers; set from the test.
    struct Setup {
        QString presentation = QStringLiteral("cardLine");
        QString selected = QStringLiteral("w-work");
        int protocol = 1; // 0: an older Kadunce, without the method
        bool accept = true;
        QString output;
        QRect card{200, 150, 400, 260};
        QRect active{40, 40, 720, 520};
        bool expand = true;
        bool prepare = true;
    };

    void reset(const QString &output)
    {
        QMutexLocker lock(&m_mutex);
        m_setup = Setup();
        m_setup.output = output;
        m_calls.clear();
    }
    void change(const std::function<void(Setup &)> &edit)
    {
        QMutexLocker lock(&m_mutex);
        edit(m_setup);
    }
    QList<Call> calls() const
    {
        QMutexLocker lock(&m_mutex);
        return m_calls;
    }
    QStringList methods() const
    {
        QStringList names;
        for (const Call &call : calls()) {
            names.append(call.method);
        }
        return names;
    }
    Call last(const QString &method) const
    {
        const QList<Call> all = calls();
        for (auto it = all.crbegin(); it != all.crend(); ++it) {
            if (it->method == method) {
                return *it;
            }
        }
        return {};
    }

public Q_SLOTS:
    Q_SCRIPTABLE QString workspaceContext()
    {
        record(QStringLiteral("workspaceContext"), {});
        QMutexLocker lock(&m_mutex);
        const QJsonObject stage{{QStringLiteral("presentation"), m_setup.presentation},
                                {QStringLiteral("selectedCardId"), m_setup.selected}};
        const QJsonArray applications{
            QJsonObject{{QStringLiteral("windowId"), QStringLiteral("w-work")}, {QStringLiteral("appId"), QStringLiteral("org.kde.kate")}},
            QJsonObject{{QStringLiteral("windowId"), QStringLiteral("w-board")}, {QStringLiteral("appId"), QStringLiteral("io.github.carlsonjm.Gooseberry")}}};
        const QJsonObject root{{QStringLiteral("schema"), QStringLiteral("studio.warbler.kadunce.workspace-context")},
                               {QStringLiteral("version"), 1},
                               {QStringLiteral("cardStage"), stage},
                               {QStringLiteral("applications"), applications}};
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    Q_SCRIPTABLE int companionGuestProtocolVersion()
    {
        record(QStringLiteral("companionGuestProtocolVersion"), {});
        QMutexLocker lock(&m_mutex);
        if (m_setup.protocol == 0) {
            sendErrorReply(QDBusError::UnknownMethod, QStringLiteral("No such method"));
            return 0;
        }
        return m_setup.protocol;
    }

    Q_SCRIPTABLE QString beginCompanionGuest(const QString &owner, const QString &path, const QString &interface)
    {
        record(QStringLiteral("beginCompanionGuest"), {owner, path, interface});
        QMutexLocker lock(&m_mutex);
        QJsonObject reply{{QStringLiteral("protocol"), 1}, {QStringLiteral("accepted"), m_setup.accept}};
        if (m_setup.accept) {
            reply.insert(QStringLiteral("output"), m_setup.output);
            reply.insert(QStringLiteral("card"), jsonRect(m_setup.card));
            reply.insert(QStringLiteral("active"), jsonRect(m_setup.active));
            reply.insert(QStringLiteral("presentationCapability"), 1);
        }
        return QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    Q_SCRIPTABLE bool setLauncherGuestExpanded(bool expanded)
    {
        record(QStringLiteral("setLauncherGuestExpanded"), {expanded});
        QMutexLocker lock(&m_mutex);
        return m_setup.expand;
    }

    Q_SCRIPTABLE bool prepareLauncherGuestLaunch(const QStringList &applicationIds, const QString &requestToken)
    {
        record(QStringLiteral("prepareLauncherGuestLaunch"), {applicationIds, requestToken});
        QMutexLocker lock(&m_mutex);
        return m_setup.prepare;
    }

    Q_SCRIPTABLE void cancelLauncherGuestLaunch() { record(QStringLiteral("cancelLauncherGuestLaunch"), {}); }
    Q_SCRIPTABLE void endLauncherGuest() { record(QStringLiteral("endLauncherGuest"), {}); }

private:
    void record(const QString &method, const QVariantList &arguments)
    {
        QMutexLocker lock(&m_mutex);
        m_calls.append({method, arguments, message().service()});
    }

    mutable QMutex m_mutex;
    Setup m_setup;
    QList<Call> m_calls;
};

class SpreadTest : public QObject
{
    Q_OBJECT

public:
    explicit SpreadTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;
    QThread m_thread;
    FakeKadunce *m_kadunce = nullptr;
    NoteStore *m_store = nullptr;
    QQmlEngine *m_engine = nullptr;
    Shell *m_shell = nullptr;

    QDBusConnection fakeBus() const { return QDBusConnection(FakeConnection); }

    // Kadunce running, or not.
    void setKadunce(bool running)
    {
        auto *bus = fakeBus().interface();
        if (running && !bus->isServiceRegistered(KWin)) {
            QVERIFY(fakeBus().registerService(KWin));
        } else if (!running && bus->isServiceRegistered(KWin)) {
            QVERIFY(fakeBus().unregisterService(KWin));
        }
        QTRY_COMPARE(QDBusConnection::sessionBus().interface()->isServiceRegistered(KWin).value(), running);
    }

    QQuickWindow *window(const char *name) const
    {
        const auto windows = QGuiApplication::topLevelWindows();
        for (QWindow *candidate : windows) {
            if (candidate->objectName() == QLatin1String(name)) {
                return qobject_cast<QQuickWindow *>(candidate);
            }
        }
        return nullptr;
    }
    QQuickWindow *card() const { return window("quickNoteWindow"); }
    QQuickWindow *board() const { return window("boardWindow"); }
    QQuickItem *cardItem() const { return card()->property("card").value<QQuickItem *>(); }
    QRect cardGeometry() const
    {
        QQuickItem *item = cardItem();
        return QRect(qRound(item->x()), qRound(item->y()), qRound(item->width()), qRound(item->height()));
    }

    // Kadunce's own call to the card's object, as it makes it.
    void callCard(const QString &method, const QVariantList &arguments = {})
    {
        const FakeKadunce::Call begin = m_kadunce->last(QStringLiteral("beginCompanionGuest"));
        QVERIFY(!begin.method.isEmpty());
        QDBusMessage call = QDBusMessage::createMethodCall(begin.arguments.at(0).toString(), begin.arguments.at(1).toString(),
                                                           begin.arguments.at(2).toString(), method);
        call.setArguments(arguments);
        fakeBus().asyncCall(call);
    }

    void openCard()
    {
        m_shell->showCapture();
        QVERIFY(card());
        QTRY_VERIFY(card()->isVisible());
        QTRY_VERIFY(card()->isActive());
        QVERIFY(QTest::qWaitForWindowExposed(card()));
        // The card grows in; a tap lands where it has come to rest.
        QTest::qWait(400);
        for (const char key : {'M', 'i', 'l', 'k'}) {
            QTest::keyClick(card(), key, key == 'M' ? Qt::ShiftModifier : Qt::NoModifier);
        }
    }

    void tapAllNotes()
    {
        QQuickItem *button = itemNamed(cardItem(), QStringLiteral("allNotes"));
        QVERIFY(button);
        QTRY_VERIFY(button->isVisible() && button->opacity() > 0);
        const QPoint centre = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(card(), Qt::LeftButton, {}, centre);
        QTRY_VERIFY(card()->property("expanded").toBool());
    }

    QRegion expectedCardMask() const
    {
        return QRegion(QRect(200, 150, 400, 260).adjusted(-16, -16, 16, 16));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        QVERIFY(QDBusConnection::connectToBus(QDBusConnection::SessionBus, FakeConnection).isConnected());
        m_kadunce = new FakeKadunce;
        m_kadunce->moveToThread(&m_thread);
        m_thread.start();
        QVERIFY(fakeBus().registerObject(KadunceObject, m_kadunce, QDBusConnection::ExportScriptableSlots));

        m_store = new NoteStore(m_home->notesFolder(), this);
        QVERIFY(m_store->open());
        m_engine = new QQmlEngine(this);
        KLocalization::setupLocalizedContext(m_engine);
        m_shell = new Shell(m_store, m_engine, this);
        QVERIFY(m_shell->guest()->publish());
    }

    void init()
    {
        m_kadunce->reset(QGuiApplication::primaryScreen()->name());
        m_shell->setHandOffWait(10000);
    }

    void cleanup()
    {
        // Every case leaves the card and the board put away.
        if (card() && card()->isVisible()) {
            m_shell->capture()->finish();
            QTRY_VERIFY(!card()->isVisible());
        }
        if (board()) {
            board()->hide();
        }
        QVERIFY(!m_shell->handingOff());
        QVERIFY(!m_shell->guest()->active());
        QVERIFY(m_home->holds(m_home->notesFolder()));
    }

    void cleanupTestCase()
    {
        delete m_shell;
        m_shell = nullptr;
        fakeBus().unregisterObject(KadunceObject);
        m_thread.quit();
        m_thread.wait();
        delete m_kadunce;
        QDBusConnection::disconnectFromBus(FakeConnection);
    }

    // Without Kadunce the card is as it always was: centred, and All notes
    // shows the board in the card itself.
    void withoutKadunce()
    {
        setKadunce(false);
        openCard();
        QVERIFY(!m_shell->guest()->active());
        QVERIFY(card()->mask().isEmpty());
        tapAllNotes();
        QVERIFY(!m_shell->handingOff());
        QVERIFY(!board() || !board()->isVisible());
        QTest::qWait(300);
        QVERIFY(card()->isVisible());
        QVERIFY(m_kadunce->calls().isEmpty());
    }

    // A Kadunce that does not offer companion guests, one that offers a
    // version this Gooseberry does not know, one that keeps Spread's centre,
    // and one showing neither Spread nor an Active card: the card stands on
    // its own, as without Kadunce.
    void standsAlone_data()
    {
        QTest::addColumn<QString>("presentation");
        QTest::addColumn<int>("protocol");
        QTest::addColumn<bool>("accept");
        QTest::addColumn<bool>("asked");
        QTest::newRow("older Kadunce") << QStringLiteral("cardLine") << 0 << true << false;
        QTest::newRow("newer protocol") << QStringLiteral("cardLine") << 2 << true << false;
        QTest::newRow("centre refused") << QStringLiteral("cardLine") << 1 << false << true;
        QTest::newRow("desktop shown") << QStringLiteral("desktop") << 1 << true << false;
    }

    void standsAlone()
    {
        QFETCH(QString, presentation);
        QFETCH(int, protocol);
        QFETCH(bool, accept);
        QFETCH(bool, asked);
        setKadunce(true);
        m_kadunce->change([&](FakeKadunce::Setup &setup) {
            setup.presentation = presentation;
            setup.protocol = protocol;
            setup.accept = accept;
        });
        openCard();
        QVERIFY(!m_shell->guest()->active());
        QVERIFY(card()->mask().isEmpty());
        QCOMPARE(m_kadunce->methods().contains(QStringLiteral("beginCompanionGuest")), asked);
        tapAllNotes();
        QVERIFY(!m_shell->handingOff());
        QVERIFY(!board() || !board()->isVisible());
        m_shell->capture()->finish();
        QTRY_VERIFY(!card()->isVisible());
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("endLauncherGuest")));
    }

    // In Spread the card asks for the centre with its own bus name and
    // object, and is drawn where Kadunce says; outside it, presses are
    // Spread's. All notes grows it into the Active card's room, opens the
    // board, and once Kadunce says the board has arrived the card fades.
    void allNotesInSpread()
    {
        setKadunce(true);
        openCard();
        const FakeKadunce::Call begin = m_kadunce->last(QStringLiteral("beginCompanionGuest"));
        QCOMPARE(begin.arguments.value(0).toString(), QDBusConnection::sessionBus().baseService());
        QCOMPARE(begin.arguments.value(1).toString(), QStringLiteral("/CompanionGuest"));
        QCOMPARE(begin.arguments.value(2).toString(), QStringLiteral("io.github.carlsonjm.Gooseberry.CompanionGuest"));
        QVERIFY(m_kadunce->methods().indexOf(QStringLiteral("companionGuestProtocolVersion"))
                < m_kadunce->methods().indexOf(QStringLiteral("beginCompanionGuest")));
        QVERIFY(m_shell->guest()->active());
        QTRY_COMPARE(cardGeometry(), QRect(200, 150, 400, 260));
        QCOMPARE(card()->mask(), expectedCardMask());

        QSignalSpy shown(m_shell, &Shell::boardShown);
        tapAllNotes();
        QVERIFY(m_shell->handingOff());
        QTRY_VERIFY(m_kadunce->methods().contains(QStringLiteral("setLauncherGuestExpanded")));
        const FakeKadunce::Call expand = m_kadunce->last(QStringLiteral("setLauncherGuestExpanded"));
        QCOMPARE(expand.arguments.value(0).toBool(), true);
        // Kadunce takes the call only from the bus name that began the guest.
        QCOMPARE(expand.sender, begin.arguments.value(0).toString());
        QTRY_COMPARE(cardGeometry(), QRect(40, 40, 720, 520));
        QCOMPARE(card()->mask(), QRegion(QRect(40, 40, 720, 520)));

        QTRY_VERIFY(m_kadunce->methods().contains(QStringLiteral("prepareLauncherGuestLaunch")));
        const FakeKadunce::Call prepare = m_kadunce->last(QStringLiteral("prepareLauncherGuestLaunch"));
        QCOMPARE(prepare.arguments.value(0).toStringList(), QStringList{BoardId});
        const QString token = prepare.arguments.value(1).toString();
        QVERIFY(!token.isEmpty());
        QTRY_VERIFY(board() && board()->isVisible());
        QTRY_VERIFY(shown.count() >= 1);
        // The board has drawn; the card still waits for Kadunce.
        QTest::qWait(300);
        QVERIFY(card()->isVisible());
        QVERIFY(!card()->property("fading").toBool());

        // A word for another request changes nothing.
        callCard(QStringLiteral("completeGuestLaunch"), {QStringLiteral("another")});
        QTest::qWait(200);
        QVERIFY(!card()->property("fading").toBool());

        callCard(QStringLiteral("completeGuestLaunch"), {token});
        QTRY_VERIFY(card()->property("fading").toBool());
        // It fades where it stood, not where it would stand alone.
        QCOMPARE(cardGeometry(), QRect(40, 40, 720, 520));
        QTRY_VERIFY(!card()->isVisible());
        QVERIFY(board()->isVisible());
        QVERIFY(!m_shell->handingOff());
        // Kadunce ended the guest with the hand-off; nothing is asked back.
        QTest::qWait(200);
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("endLauncherGuest")));
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("cancelLauncherGuestLaunch")));

        // The next card asks again, with the whole surface to start from.
        openCard();
        QCOMPARE(m_kadunce->methods().count(QStringLiteral("beginCompanionGuest")), 2);
        QCOMPARE(card()->mask(), expectedCardMask());
        QTRY_COMPARE(cardGeometry(), QRect(200, 150, 400, 260));
    }

    // The card put away in Spread gives the centre back, once.
    void putAwayInSpread()
    {
        setKadunce(true);
        openCard();
        QVERIFY(m_shell->guest()->active());
        QTest::keyClick(card(), Qt::Key_Escape);
        QTRY_VERIFY(!card()->isVisible());
        QTRY_COMPARE(m_kadunce->methods().count(QStringLiteral("endLauncherGuest")), 1);
        QVERIFY(card()->mask().isEmpty());
        QTest::qWait(200);
        QCOMPARE(m_kadunce->methods().count(QStringLiteral("endLauncherGuest")), 1);
    }

    // Another guest took the centre: the card closes, the note kept, and
    // nothing is asked back of Kadunce.
    void dismissed()
    {
        setKadunce(true);
        openCard();
        QVERIFY(m_shell->guest()->active());
        callCard(QStringLiteral("dismissGuest"));
        QTRY_VERIFY(!card()->isVisible());
        QVERIFY(!m_shell->guest()->active());
        QVERIFY(card()->mask().isEmpty());
        QTest::qWait(200);
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("endLauncherGuest")));
        const auto notes = m_store->notes();
        bool kept = false;
        for (const auto &note : notes) {
            kept = kept || note.text.contains(QLatin1String("Milk"));
        }
        QVERIFY(kept);
    }

    // Kadunce refuses the Active card's room: the guest ends, and the card
    // shows the board itself, as without Kadunce.
    void growRefused()
    {
        setKadunce(true);
        m_kadunce->change([](FakeKadunce::Setup &setup) {
            setup.expand = false;
        });
        openCard();
        tapAllNotes();
        QTRY_COMPARE(m_kadunce->methods().count(QStringLiteral("endLauncherGuest")), 1);
        QTRY_VERIFY(!m_shell->handingOff());
        QVERIFY(!m_shell->guest()->active());
        QVERIFY(card()->mask().isEmpty());
        QTest::qWait(400);
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("prepareLauncherGuestLaunch")));
        QVERIFY(!board() || !board()->isVisible());
        QVERIFY(card()->isVisible());
    }

    // Kadunce will not wait for the board: the guest ends, the board opens,
    // and the card fades where it stood once the board has drawn.
    void launchRefused()
    {
        setKadunce(true);
        m_kadunce->change([](FakeKadunce::Setup &setup) {
            setup.prepare = false;
        });
        openCard();
        tapAllNotes();
        QTRY_VERIFY(board() && board()->isVisible());
        QTRY_COMPARE(m_kadunce->methods().count(QStringLiteral("endLauncherGuest")), 1);
        QTRY_VERIFY(card()->property("fading").toBool());
        QCOMPARE(cardGeometry(), QRect(40, 40, 720, 520));
        QTRY_VERIFY(!card()->isVisible());
        QTest::qWait(200);
        QCOMPARE(m_kadunce->methods().count(QStringLiteral("endLauncherGuest")), 1);
    }

    // Kadunce never says the board arrived: after the wait the launch is
    // called off, the centre given back, and the grown card stays.
    void launchNeverArrives()
    {
        setKadunce(true);
        m_shell->setHandOffWait(500);
        openCard();
        tapAllNotes();
        QTRY_VERIFY(m_kadunce->methods().contains(QStringLiteral("prepareLauncherGuestLaunch")));
        QTRY_VERIFY_WITH_TIMEOUT(m_kadunce->methods().contains(QStringLiteral("endLauncherGuest")), 3000);
        QVERIFY(m_kadunce->methods().contains(QStringLiteral("cancelLauncherGuestLaunch")));
        QVERIFY(!m_shell->handingOff());
        QVERIFY(card()->isVisible());
        QVERIFY(card()->property("expanded").toBool());
        QVERIFY(!card()->property("fading").toBool());
    }

    // Over an Active card there is no guest. All notes grows the card and
    // opens the board; the card stays up until Kadunce reports the board as
    // the selected card, then fades.
    void allNotesOverActiveCard()
    {
        setKadunce(true);
        m_kadunce->change([](FakeKadunce::Setup &setup) {
            setup.presentation = QStringLiteral("active");
        });
        openCard();
        QVERIFY(!m_shell->guest()->active());
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("companionGuestProtocolVersion")));
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("beginCompanionGuest")));
        QVERIFY(card()->mask().isEmpty());
        const QRect resting = cardGeometry();

        tapAllNotes();
        QVERIFY(m_shell->handingOff());
        QTRY_VERIFY(board() && board()->isVisible());
        QTRY_VERIFY(cardGeometry().width() > resting.width());

        // Something else changed; the work window is still the one selected.
        fakeBus().send(QDBusMessage::createSignal(KadunceObject, KadunceInterface, QStringLiteral("workspaceContextChanged")));
        QTest::qWait(300);
        QVERIFY(card()->isVisible());
        QVERIFY(!card()->property("fading").toBool());

        m_kadunce->change([](FakeKadunce::Setup &setup) {
            setup.selected = QStringLiteral("w-board");
        });
        fakeBus().send(QDBusMessage::createSignal(KadunceObject, KadunceInterface, QStringLiteral("workspaceContextChanged")));
        QTRY_VERIFY(card()->property("fading").toBool());
        QTRY_VERIFY(!card()->isVisible());
        QVERIFY(board()->isVisible());
        QVERIFY(!m_shell->handingOff());
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("setLauncherGuestExpanded")));
        QVERIFY(!m_kadunce->methods().contains(QStringLiteral("endLauncherGuest")));
    }

    // Over an Active card, the board never seen in its place: the grown card
    // stays, showing the board itself.
    void overActiveCardGivesUp()
    {
        setKadunce(true);
        m_kadunce->change([](FakeKadunce::Setup &setup) {
            setup.presentation = QStringLiteral("active");
        });
        m_shell->setHandOffWait(500);
        openCard();
        tapAllNotes();
        QVERIFY(m_shell->handingOff());
        QTRY_VERIFY_WITH_TIMEOUT(!m_shell->handingOff(), 3000);
        QVERIFY(card()->isVisible());
        QVERIFY(card()->property("expanded").toBool());
        QVERIFY(!card()->property("fading").toBool());
        // A report after giving up is no longer the card's to act on.
        m_kadunce->change([](FakeKadunce::Setup &setup) {
            setup.selected = QStringLiteral("w-board");
        });
        fakeBus().send(QDBusMessage::createSignal(KadunceObject, KadunceInterface, QStringLiteral("workspaceContextChanged")));
        QTest::qWait(300);
        QVERIFY(!card()->property("fading").toBool());
    }
};

int main(int argc, char *argv[])
{
    // Only on a bus of the test's own, as tests/CMakeLists.txt starts it.
    if (qEnvironmentVariable("GOOSEBERRY_PRIVATE_BUS") != QLatin1String("1")) {
        fputs("Runs only on the private bus its test entry starts.\n", stderr);
        return 77;
    }
    const QByteArray bus = qgetenv("DBUS_SESSION_BUS_ADDRESS");
    TestHome home;
    // The test's own bus, which reaches nothing of a person's desktop.
    qputenv("DBUS_SESSION_BUS_ADDRESS", bus);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.carlsonjm.Gooseberry"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Gooseberry"));
    KLocalizedString::setApplicationDomain("gooseberry");
    SpreadTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_spread.moc"
