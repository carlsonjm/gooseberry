// SPDX-License-Identifier: GPL-2.0-or-later
// The quick-note card and the board, drawn off screen over a folder in the
// test's own home, and used by tapping and typing as a person would.
#include "Board.h"
#include "Capture.h"
#include "Ink.h"
#include "NoteStore.h"
#include "Planner.h"
#include "ReminderWords.h"
#include "TestHome.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QQmlComponent>
#include <QRegularExpression>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <memory>

Q_IMPORT_QML_PLUGIN(io_github_carlsonjm_gooseberryPlugin)

using namespace Gooseberry;

// Stands in for the desktop shell, which needs a running desktop, and records
// what the board asks of it.
class FakeShell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *capture MEMBER m_capture CONSTANT)
    Q_PROPERTY(QObject *places MEMBER m_places CONSTANT)
    Q_PROPERTY(QObject *notes MEMBER m_notes CONSTANT)
    Q_PROPERTY(QObject *store MEMBER m_store CONSTANT)
    Q_PROPERTY(QObject *planner MEMBER m_planner CONSTANT)

public:
    FakeShell(NoteStore *store, Capture *capture, Places *places, PlaceNotes *notes)
        : m_capture(capture)
        , m_places(places)
        , m_notes(notes)
        , m_store(store)
        , m_planner(new Planner(store, this))
    {
    }

    QString opened;
    QString newIn;

public Q_SLOTS:
    void openNote(const QString &id) { opened = id; }
    void newNoteIn(const QString &place) { newIn = place; }
    void showBoard() { }
    void showCapture() { }
    QString applicationName() const { return QStringLiteral("Gooseberry"); }
    QString reminderLabel(const QDateTime &remind, bool onOpen) const { return ReminderWords::label(remind, onOpen); }
    QVariantList reminderChoices() const { return ReminderWords::choices(true); }

private:
    QObject *m_capture;
    QObject *m_places;
    QObject *m_notes;
    QObject *m_store;
    QObject *m_planner;
};

// The words for reminders, as the shell gives them to the card.
class Words : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    QString reminderLabel(const QDateTime &remind, bool onOpen) const { return ReminderWords::label(remind, onOpen); }
    QVariantList reminderChoices() const { return ReminderWords::choices(true); }
};

namespace {

// Every item drawn under root, including those a list made, as a person sees them.
QList<QQuickItem *> itemsUnder(QQuickItem *root)
{
    QList<QQuickItem *> found;
    const auto children = root->childItems();
    for (QQuickItem *child : children) {
        found.append(child);
        found += itemsUnder(child);
    }
    return found;
}

QQuickItem *itemNamed(QQuickItem *root, const QString &name)
{
    for (QQuickItem *item : itemsUnder(root)) {
        if (item->objectName() == name) {
            return item;
        }
    }
    return nullptr;
}

QList<QQuickItem *> buttonsIn(QQuickItem *root)
{
    QList<QQuickItem *> found;
    for (QQuickItem *item : itemsUnder(root)) {
        if (item->inherits("QQuickAbstractButton") && item->isVisible() && item->opacity() > 0) {
            found.append(item);
        }
    }
    return found;
}

void type(QWindow *window, const QString &text)
{
    for (const QChar c : text) {
        QTest::keyClick(window, c.toLatin1(), c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier);
    }
}

// With GOOSEBERRY_SCREENSHOTS set to a folder, each screen is saved there as
// drawn, to compare with the mock-up by eye.
void picture(QQuickWindow *window, const QString &name)
{
    const QString folder = qEnvironmentVariable("GOOSEBERRY_SCREENSHOTS");
    if (!folder.isEmpty()) {
        QTest::qWait(400);
        window->grabWindow().save(folder + QLatin1Char('/') + name + QStringLiteral(".png"));
    }
}

// A pen, and its eraser end, as the tablet reports them: a pen of its own for
// each case, as each case has a card of its own.
int penRound = 0;

const QPointingDevice *penDevice(QPointingDevice::PointerType type)
{
    static QHash<int, QPointingDevice *> devices;
    const int key = penRound * 2 + (type == QPointingDevice::PointerType::Eraser ? 1 : 0);
    if (!devices.contains(key)) {
        const qint64 id = 9000 + penRound;
        devices.insert(key, new QPointingDevice(QStringLiteral("test pen %1").arg(key), id, QInputDevice::DeviceType::Stylus, type,
                                                QInputDevice::Capability::Position | QInputDevice::Capability::Pressure
                                                    | QInputDevice::Capability::Hover,
                                                1, 2, QString(), QPointingDeviceUniqueId::fromNumericId(id)));
    }
    return devices.value(key);
}

void penEvent(QQuickWindow *window, QEvent::Type type, const QPointF &at, qreal pressure,
              QPointingDevice::PointerType end = QPointingDevice::PointerType::Pen)
{
    const bool down = type == QEvent::TabletPress || (type == QEvent::TabletMove && pressure > 0);
    QTabletEvent event(type, penDevice(end), at, window->mapToGlobal(at), pressure, 0, 0, 0, 0, 0, Qt::NoModifier,
                       type == QEvent::TabletMove ? Qt::NoButton : Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton);
    QCoreApplication::sendEvent(window, &event);
}

// A stroke by the pen across an item, from one fraction of it to another,
// pressing as given.
void penStroke(QQuickWindow *window, QQuickItem *item, QPointF from, QPointF to, qreal pressure,
               QPointingDevice::PointerType end = QPointingDevice::PointerType::Pen)
{
    auto at = [item](QPointF f) { return item->mapToScene(QPointF(item->width() * f.x(), item->height() * f.y())); };
    penEvent(window, QEvent::TabletMove, at(from) - QPointF(0, 4), 0, end);
    penEvent(window, QEvent::TabletPress, at(from), pressure, end);
    for (int step = 1; step <= 12; ++step) {
        penEvent(window, QEvent::TabletMove, at(from + (to - from) * step / 12.0), pressure, end);
        QTest::qWait(5);
    }
    penEvent(window, QEvent::TabletRelease, at(to), 0, end);
    // Lifted away, out of the screen's reach.
    penEvent(window, QEvent::TabletLeaveProximity, at(to), 0, end);
}

// A stroke by a finger across an item.
void fingerStroke(QQuickWindow *window, QQuickItem *item, QPointF from, QPointF to)
{
    static QPointingDevice *finger = QTest::createTouchDevice();
    auto at = [item](QPointF f) {
        return item->mapToScene(QPointF(item->width() * f.x(), item->height() * f.y())).toPoint();
    };
    QTest::touchEvent(window, finger).press(0, at(from), window);
    for (int step = 1; step <= 12; ++step) {
        QTest::touchEvent(window, finger).move(0, at(from + (to - from) * step / 12.0), window);
        QTest::qWait(5);
    }
    QTest::touchEvent(window, finger).release(0, at(to), window);
}

void tap(QQuickWindow *window, QQuickItem *item)
{
    QVERIFY(item);
    // Notes are laid out in columns once the list settles; tap where they end up.
    QTest::qWait(30);
    const QPoint centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, {}, centre);
}

} // namespace

class ScreensTest : public QObject
{
    Q_OBJECT

public:
    explicit ScreensTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;
    std::unique_ptr<NoteStore> m_store;
    std::unique_ptr<Capture> m_capture;
    std::unique_ptr<QQuickView> m_view;
    Words m_words;

    QQuickItem *note() const { return m_view->rootObject(); }
    QQuickItem *named(const QString &name) const { return itemNamed(note(), name); }

private Q_SLOTS:
    void init()
    {
        // A mistake in a screen shows only as a warning; here it fails the test.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("ReferenceError|TypeError|Unable to assign|is not a type|Cannot read property")));
        static int round = 0;
        const QString folder = m_home->path() + QStringLiteral("/screens-%1").arg(++round);
        penRound = round;
        QVERIFY(m_home->holds(folder));
        m_store = std::make_unique<NoteStore>(folder);
        QVERIFY(m_store->open());
        m_capture = std::make_unique<Capture>(m_store.get());
        m_capture->startNew({QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")});

        m_view = std::make_unique<QQuickView>();
        KLocalization::setupLocalizedContext(m_view->engine());
        m_view->setInitialProperties({{QStringLiteral("capture"), QVariant::fromValue<QObject *>(m_capture.get())},
                                      {QStringLiteral("projects"), QStringList{QStringLiteral("Shuffle"), QStringLiteral("Home")}},
                                      {QStringLiteral("words"), QVariant::fromValue<QObject *>(&m_words)}});
        m_view->loadFromModule(QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("QuickNote"));
        QVERIFY2(m_view->status() == QQuickView::Ready, qPrintable(m_view->errors().value(0).toString()));
        // The card's size on a tablet: the search's, 64 % of the room.
        m_view->setResizeMode(QQuickView::SizeRootObjectToView);
        m_view->resize(806, 471);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view.get()));
        QMetaObject::invokeMethod(note(), "focusText");
    }

    void cleanup()
    {
        m_view.reset();
        m_capture.reset();
        m_store.reset();
    }

    // The approved layout: an 8 px card with a 1 px outline; a 44 px header
    // with the yellow tile and the application's name at the left, the
    // colours centred and a 30 px All notes pill 14 px from the right edge;
    // the note pad 14 px under it, across the width inside 22 px margins.
    void headerFollowsTheApprovedLayout()
    {
        QQuickItem *header = named(QStringLiteral("header"));
        QVERIFY(header);
        QCOMPARE(header->height(), 44.0);
        QQuickItem *tile = named(QStringLiteral("appTile"));
        QCOMPARE(tile->width(), 32.0);
        QCOMPARE(tile->height(), 32.0);
        QCOMPARE(tile->property("radius").toReal(), 8.0);
        QCOMPARE(tile->property("color").value<QColor>(), QColor(QStringLiteral("#F2D98A")));
        QCOMPARE(tile->mapToItem(note(), QPointF()).x(), 22.0);
        QQuickItem *name = named(QStringLiteral("appName"));
        QVERIFY(name->isVisible());
        QCOMPARE(name->property("text").toString(), QStringLiteral("Gooseberry"));
        QQuickItem *colours = named(QStringLiteral("colours"));
        const QPointF coloursAt = colours->mapToItem(note(), QPointF());
        QCOMPARE(coloursAt.x() + colours->width() / 2, note()->width() / 2);
        QQuickItem *allNotes = named(QStringLiteral("allNotes"));
        const QPointF allNotesAt = allNotes->mapToItem(note(), QPointF());
        QCOMPARE(allNotesAt.x() + allNotes->width(), note()->width() - 14);
        QCOMPARE(itemNamed(allNotes, QStringLiteral("face"))->height(), 30.0);
        QVERIFY(allNotes->height() >= 44);
        QQuickItem *pad = note()->property("editor").value<QQuickItem *>();
        QQuickItem *page = pad->parentItem();
        while (page && page->property("radius").toReal() != 14.0) {
            page = page->parentItem();
        }
        QVERIFY(page);
        const QPointF pageAt = page->mapToItem(note(), QPointF());
        const QPointF headerAt = header->mapToItem(note(), QPointF());
        QCOMPARE(pageAt.y(), headerAt.y() + 44 + 14);
        QCOMPARE(pageAt.x(), 22.0);
        QCOMPARE(page->width(), note()->width() - 44);
    }

    void cursorIsReadyAndTypingKeeps()
    {
        auto *editor = note()->property("editor").value<QQuickItem *>();
        QVERIFY(editor);
        QTRY_VERIFY(editor->hasActiveFocus());
        QVERIFY(!m_capture->kept());

        type(m_view.get(), QStringLiteral("F"));
        QVERIFY(m_capture->kept());
        QCOMPARE(m_store->note(m_capture->noteId())->text, QStringLiteral("F"));
        type(m_view.get(), QStringLiteral("lick"));
        // The rest is written when the typing pauses.
        const auto written = [this] {
            QFile file(m_store->pathFor(m_capture->noteId()));
            return file.open(QIODevice::ReadOnly) && file.readAll().endsWith("---\nFlick");
        };
        QTRY_VERIFY_WITH_TIMEOUT(written(), Capture::PauseMs * 4);
    }

    void everyTargetIsBigEnoughToTouch()
    {
        m_capture->setText(QStringLiteral("kept, so Tuck away and Remove show"));
        const auto buttons = buttonsIn(note());
        QVERIFY(buttons.size() >= 12);
        for (QQuickItem *button : buttons) {
            QVERIFY2(button->height() >= 44 && button->width() >= 44,
                     qPrintable(QStringLiteral("%1 is %2×%3").arg(button->objectName()).arg(button->width()).arg(button->height())));
        }
    }

    void colourIsOneTap()
    {
        picture(m_view.get(), QStringLiteral("note-empty"));
        tap(m_view.get(), named(QStringLiteral("colour-lichen")));
        QCOMPARE(m_capture->colour(), QStringLiteral("lichen"));
        type(m_view.get(), QStringLiteral("Groceries"));
        QCOMPARE(m_store->note(m_capture->noteId())->colour, QStringLiteral("lichen"));
        picture(m_view.get(), QStringLiteral("note-written"));
    }

    void belongsToIsOneTap()
    {
        QVERIFY(named(QStringLiteral("belongs-window"))->isVisible());
        QVERIFY(named(QStringLiteral("belongs-window"))->property("checked").toBool());
        QCOMPARE(named(QStringLiteral("belongs-window"))->property("text").toString(), QStringLiteral("This window · SpreadGesture.qml"));
        QCOMPARE(named(QStringLiteral("belongs-project"))->property("text").toString(), QStringLiteral("Project · Shuffle"));
        QCOMPARE(named(QStringLiteral("belongs-workspace"))->property("text").toString(), QStringLiteral("Workspace · Desk"));

        tap(m_view.get(), named(QStringLiteral("belongs-project")));
        QCOMPARE(m_capture->belongs(), QStringLiteral("project"));
        QCOMPARE(m_capture->project(), QStringLiteral("Shuffle"));
        tap(m_view.get(), named(QStringLiteral("belongs-loose")));
        QCOMPARE(m_capture->belongs(), QStringLiteral("loose"));

        // Another project: one tap to open the list, one to choose.
        tap(m_view.get(), named(QStringLiteral("chooseProject")));
        QQuickItem *home = nullptr;
        for (QQuickItem *button : buttonsIn(note())) {
            if (button->property("text").toString() == QLatin1String("Home")) {
                home = button;
            }
        }
        QVERIFY(home);
        tap(m_view.get(), home);
        QCOMPARE(m_capture->project(), QStringLiteral("Home"));
        // The cursor goes back to the note.
        QTRY_VERIFY(note()->property("editor").value<QQuickItem *>()->hasActiveFocus());
    }

    void newProjectIsNamedOnce()
    {
        tap(m_view.get(), named(QStringLiteral("chooseProject")));
        QQuickItem *field = named(QStringLiteral("projectName"));
        QVERIFY(field->isVisible());
        picture(m_view.get(), QStringLiteral("note-projects"));
        tap(m_view.get(), field);
        QTRY_VERIFY(field->hasActiveFocus());
        type(m_view.get(), QStringLiteral("Cabin"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QCOMPARE(m_capture->belongs(), QStringLiteral("project"));
        QCOMPARE(m_capture->project(), QStringLiteral("Cabin"));
        QTRY_VERIFY(note()->property("editor").value<QQuickItem *>()->hasActiveFocus());
        type(m_view.get(), QStringLiteral("Book it"));
        QCOMPARE(m_store->note(m_capture->noteId())->project, QStringLiteral("Cabin"));
    }

    // Remind offers the quick times, "next time this opens" and Pick a
    // time; a choice is kept with the note at once, and the pill then says
    // when. The cross takes the reminder away.
    void remindQuickTimes()
    {
        type(m_view.get(), QStringLiteral("Call about the cabin"));
        tap(m_view.get(), named(QStringLiteral("remind")));
        QTRY_VERIFY(named(QStringLiteral("reminderChoices"))->isVisible());
        const QVariantList choices = ReminderWords::choices(true);
        for (const QVariant &choice : choices) {
            QQuickItem *pill = named(QStringLiteral("remind-") + choice.toMap().value(QStringLiteral("kind")).toString());
            QVERIFY(pill && pill->isVisible());
            QCOMPARE(pill->property("text").toString(), choice.toMap().value(QStringLiteral("label")).toString());
        }
        picture(m_view.get(), QStringLiteral("note-remind"));
        tap(m_view.get(), named(QStringLiteral("remind-tomorrow")));
        const QDateTime tomorrow(QDate::currentDate().addDays(1), QTime(9, 0));
        QCOMPARE(m_capture->remindAt(), tomorrow);
        QCOMPARE(m_store->note(m_capture->noteId())->remind, tomorrow);
        QVERIFY(!named(QStringLiteral("reminderChoices"))->isVisible());
        QCOMPARE(named(QStringLiteral("remind"))->property("text").toString(), ReminderWords::label(tomorrow, false));
        QVERIFY(named(QStringLiteral("remind"))->property("text").toString().startsWith(QStringLiteral("Tomorrow ")));
        picture(m_view.get(), QStringLiteral("note-reminder-set"));

        tap(m_view.get(), named(QStringLiteral("remind")));
        tap(m_view.get(), named(QStringLiteral("remind-opens")));
        QVERIFY(m_store->note(m_capture->noteId())->remindOnOpen);
        QCOMPARE(named(QStringLiteral("remind"))->property("text").toString(), QStringLiteral("Next time this opens"));

        tap(m_view.get(), named(QStringLiteral("noReminder")));
        QVERIFY(!m_store->note(m_capture->noteId())->hasReminder());
        QCOMPARE(named(QStringLiteral("remind"))->property("text").toString(), QStringLiteral("Remind"));
        QVERIFY(!named(QStringLiteral("noReminder"))->isVisible());
    }

    // Pick a time: a day on and an hour and a quarter later than the next
    // whole hour, then Set.
    void pickATime()
    {
        type(m_view.get(), QStringLiteral("Split Rock review"));
        tap(m_view.get(), named(QStringLiteral("remind")));
        tap(m_view.get(), named(QStringLiteral("remind-pick")));
        QTRY_VERIFY(named(QStringLiteral("timePicker"))->isVisible());
        QDateTime start = QDateTime::currentDateTime().addSecs(3600);
        start.setTime(QTime(start.time().hour(), 0));
        picture(m_view.get(), QStringLiteral("note-pick"));
        tap(m_view.get(), named(QStringLiteral("dayOn")));
        tap(m_view.get(), named(QStringLiteral("hourOn")));
        tap(m_view.get(), named(QStringLiteral("minutesOn")));
        // The day the steps landed on, in the picker's words: late in the
        // evening the hour stepped on is already the day after.
        const QDate landed = start.addDays(1).addSecs(75 * 60).date();
        const qint64 ahead = QDate::currentDate().daysTo(landed);
        QCOMPARE(named(QStringLiteral("pickedDay"))->property("text").toString(),
                 ahead == 1 ? QStringLiteral("Tomorrow") : QLocale().toString(landed, QStringLiteral("dddd")));
        tap(m_view.get(), named(QStringLiteral("setTime")));
        QCOMPARE(m_capture->remindAt(), start.addDays(1).addSecs(75 * 60));
        QVERIFY(!named(QStringLiteral("timePicker"))->isVisible());
        // Nothing before now can be picked.
        tap(m_view.get(), named(QStringLiteral("remind")));
        tap(m_view.get(), named(QStringLiteral("remind-pick")));
        for (int i = 0; i < 3; ++i) {
            tap(m_view.get(), named(QStringLiteral("dayBack")));
            tap(m_view.get(), named(QStringLiteral("hourBack")));
        }
        tap(m_view.get(), named(QStringLiteral("setTime")));
        QVERIFY(m_capture->remindAt() > QDateTime::currentDateTime());
    }

    // Checklist turns the lines into items under their heading, each ticked
    // where it stands; Return starts the next item, backspace in an empty
    // one takes it away, and Checklist again turns it back into text.
    void checklistTicksInPlace()
    {
        // With the keys down the card has room for the whole list.
        m_view->resize(806, 640);
        type(m_view.get(), QStringLiteral("Groceries"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        type(m_view.get(), QStringLiteral("coffee"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        type(m_view.get(), QStringLiteral("oats"));
        tap(m_view.get(), named(QStringLiteral("checklist")));
        QVERIFY(m_capture->checklist());
        QCOMPARE(m_store->note(m_capture->noteId())->text, QStringLiteral("Groceries\n- [ ] coffee\n- [ ] oats"));
        QQuickItem *pad = named(QStringLiteral("checklistPad"));
        QTRY_VERIFY(pad->isVisible());
        QVERIFY(!itemNamed(itemNamed(pad, QStringLiteral("line-0")), QStringLiteral("tick"))->isVisible());
        QQuickItem *oats = itemNamed(pad, QStringLiteral("line-2"));
        QVERIFY(itemNamed(oats, QStringLiteral("tick"))->isVisible());
        QCOMPARE(itemNamed(oats, QStringLiteral("tick"))->width(), 44.0);

        tap(m_view.get(), itemNamed(oats, QStringLiteral("tick")));
        QCOMPARE(m_store->note(m_capture->noteId())->text, QStringLiteral("Groceries\n- [ ] coffee\n- [x] oats"));
        QTRY_VERIFY(itemNamed(oats, QStringLiteral("lineText"))->property("font").value<QFont>().strikeOut());

        // The cursor is in the last item; Return starts another.
        QTRY_VERIFY(itemNamed(oats, QStringLiteral("lineText"))->hasActiveFocus());
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QTRY_VERIFY(itemNamed(itemNamed(pad, QStringLiteral("line-3")), QStringLiteral("lineText"))->hasActiveFocus());
        type(m_view.get(), QStringLiteral("lemons"));
        m_capture->flush();
        QCOMPARE(m_store->note(m_capture->noteId())->text, QStringLiteral("Groceries\n- [ ] coffee\n- [x] oats\n- [ ] lemons"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QTRY_COMPARE(m_capture->lines().size(), 5);
        picture(m_view.get(), QStringLiteral("note-checklist"));
        QTest::keyClick(m_view.get(), Qt::Key_Backspace);
        QTRY_COMPARE(m_capture->lines().size(), 4);
        QTRY_VERIFY(itemNamed(itemNamed(pad, QStringLiteral("line-3")), QStringLiteral("lineText"))->hasActiveFocus());

        // On a short page the line being written scrolls into sight.
        m_view->resize(806, 471);
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QQuickItem *last = itemNamed(pad, QStringLiteral("line-4"));
        QTRY_VERIFY(last && itemNamed(last, QStringLiteral("lineText"))->hasActiveFocus());
        const QPointF lastAt = last->mapToItem(pad, QPointF());
        QTRY_VERIFY(last->mapToItem(pad, QPointF()).y() + last->height() <= pad->height() + 1);
        Q_UNUSED(lastAt)
        QTest::keyClick(m_view.get(), Qt::Key_Backspace);
        QTRY_COMPARE(m_capture->lines().size(), 4);

        tap(m_view.get(), named(QStringLiteral("checklist")));
        QVERIFY(!m_capture->checklist());
        QCOMPARE(m_store->note(m_capture->noteId())->text, QStringLiteral("Groceries\ncoffee\noats\nlemons"));
        QVERIFY(!pad->isVisible());
    }

    // Pen: the ruled page under the words takes ink, as wide as the pen
    // presses, in the ink chosen. The first stroke keeps the note at once,
    // drawing and all; the words stay above.
    void penWritesUnderTheWords()
    {
        m_view->resize(806, 640);
        type(m_view.get(), QStringLiteral("Flick"));
        tap(m_view.get(), named(QStringLiteral("pen")));
        QQuickItem *page = named(QStringLiteral("inkCanvas"));
        QTRY_VERIFY(page && page->isVisible());
        // The card makes room for the page before it is written on.
        QTest::qWait(300);
        QVERIFY(named(QStringLiteral("palette"))->isVisible());
        QVERIFY(named(QStringLiteral("typed"))->isVisible());
        // The words stay above the page.
        QTRY_VERIFY(named(QStringLiteral("typed"))->mapToScene(QPointF()).y() + named(QStringLiteral("typed"))->height()
                    <= page->mapToScene(QPointF()).y());

        penStroke(m_view.get(), page, {0.05, 0.2}, {0.4, 0.22}, 0.15);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 1);
        const QString id = m_capture->noteId();
        // The note was kept by its first letter; the stroke waits for the
        // pen to pause, as typing does.
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_store->inkPathFor(id)), Capture::PauseMs * 4);
        QCOMPARE(m_capture->ink().strokes.first().points.first().pressure, 0.15);

        tap(m_view.get(), named(QStringLiteral("ink-blue")));
        penStroke(m_view.get(), page, {0.05, 0.4}, {0.4, 0.42}, 0.95);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 2);
        QCOMPARE(m_capture->ink().strokes.at(1).colour, QStringLiteral("blue"));
        QVERIFY(m_capture->ink().strokes.at(1).points.at(3).pressure > 0.9);
        m_capture->flush();
        QCOMPARE(m_store->ink(id).strokes.size(), 2);
        picture(m_view.get(), QStringLiteral("note-pen"));
        for (const QString &name : {QStringLiteral("ink-black"), QStringLiteral("ink-blue"), QStringLiteral("ink-red"),
                                    QStringLiteral("eraser")}) {
            QCOMPARE(named(name)->width(), 44.0);
            QCOMPARE(named(name)->height(), 44.0);
        }
    }

    // A finger writes in Pen while no pen is near; with the pen near the
    // screen, a finger on the page writes nothing.
    void fingerWritesUntilThePenIsNear()
    {
        m_view->resize(806, 640);
        tap(m_view.get(), named(QStringLiteral("pen")));
        QQuickItem *page = named(QStringLiteral("inkCanvas"));
        QTRY_VERIFY(page && page->isVisible());
        // The card makes room for the page before it is written on.
        QTest::qWait(300);
        fingerStroke(m_view.get(), page, {0.1, 0.2}, {0.5, 0.25});
        QTRY_COMPARE(m_capture->ink().strokes.size(), 1);

        // The pen comes near, hovering over the page.
        const QPointF over = page->mapToScene(QPointF(page->width() * 0.7, page->height() * 0.3));
        penEvent(m_view.get(), QEvent::TabletEnterProximity, over, 0);
        penEvent(m_view.get(), QEvent::TabletMove, over, 0);
        penEvent(m_view.get(), QEvent::TabletMove, over + QPointF(3, 0), 0);
        QTRY_VERIFY(named(QStringLiteral("inkPage"))->property("penNear").toBool());
        fingerStroke(m_view.get(), page, {0.1, 0.5}, {0.5, 0.55});
        QTest::qWait(100);
        QCOMPARE(m_capture->ink().strokes.size(), 1);
    }

    // The pen's eraser end takes the whole stroke it touches, and Undo, in
    // the message that follows, puts it back; Eraser in the palette does the
    // same with the pen's point.
    void eraserTakesWholeStrokes()
    {
        m_view->resize(806, 640);
        tap(m_view.get(), named(QStringLiteral("pen")));
        QQuickItem *page = named(QStringLiteral("inkCanvas"));
        QTRY_VERIFY(page && page->isVisible());
        // The card makes room for the page before it is written on.
        QTest::qWait(300);
        penStroke(m_view.get(), page, {0.05, 0.2}, {0.5, 0.2}, 0.5);
        penStroke(m_view.get(), page, {0.05, 0.6}, {0.5, 0.6}, 0.5);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 2);

        penStroke(m_view.get(), page, {0.2, 0.15}, {0.2, 0.25}, 0.5, QPointingDevice::PointerType::Eraser);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 1);
        QQuickItem *undo = named(QStringLiteral("noticeAction"));
        QTRY_VERIFY(undo && undo->isVisible());
        picture(m_view.get(), QStringLiteral("note-erased"));
        tap(m_view.get(), undo);
        QCOMPARE(m_capture->ink().strokes.size(), 2);

        tap(m_view.get(), named(QStringLiteral("eraser")));
        penStroke(m_view.get(), page, {0.3, 0.55}, {0.3, 0.65}, 0.5);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 1);
        tap(m_view.get(), named(QStringLiteral("ink-black")));
        penStroke(m_view.get(), page, {0.05, 0.6}, {0.5, 0.6}, 0.5);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 2);
    }

    // Under the ink, what it was read as; tapped, the right words can be
    // typed, and they are kept as the person's own.
    void readingCanBeFixed()
    {
        m_view->resize(806, 640);
        tap(m_view.get(), named(QStringLiteral("pen")));
        QQuickItem *page = named(QStringLiteral("inkCanvas"));
        QTRY_VERIFY(page && page->isVisible());
        // The card makes room for the page before it is written on.
        QTest::qWait(300);
        penStroke(m_view.get(), page, {0.05, 0.2}, {0.5, 0.2}, 0.5);
        QTRY_COMPARE(m_capture->ink().strokes.size(), 1);
        const QString id = m_capture->noteId();
        Ink ink = m_store->ink(id);
        const int line = ink.rows().first();
        QVERIFY(ink.setReading(line, ink.digest(line), QStringLiteral("measure flack velocity"), {}));
        QVERIFY(m_store->saveInk(*m_store->note(id), ink, NoteStore::Touch::Kept));
        QTRY_COMPARE(m_capture->readText(), QStringLiteral("measure flack velocity"));
        QQuickItem *readAs = named(QStringLiteral("readAs"));
        QTRY_VERIFY(readAs->isVisible());
        picture(m_view.get(), QStringLiteral("note-read-as"));
        tap(m_view.get(), readAs);
        QQuickItem *field = named(QStringLiteral("fixReading"));
        QTRY_VERIFY(field->isVisible() && field->hasActiveFocus());
        QCOMPARE(field->property("text").toString(), QStringLiteral("measure flack velocity"));
        QMetaObject::invokeMethod(field, "selectAll");
        type(m_view.get(), QStringLiteral("measure flick velocity"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QCOMPARE(m_capture->readText(), QStringLiteral("measure flick velocity"));
        QCOMPARE(m_store->note(id)->read, QStringLiteral("measure flick velocity"));
        QVERIFY(m_store->ink(id).readings.first().fixed);
        QTRY_VERIFY(readAs->isVisible());
    }

    // Out of Pen, the ink shows under the words, and a pen touching it
    // switches to Pen.
    void penTouchSwitchesToPen()
    {
        m_view->resize(806, 640);
        tap(m_view.get(), named(QStringLiteral("pen")));
        QQuickItem *page = named(QStringLiteral("inkCanvas"));
        QTRY_VERIFY(page && page->isVisible());
        // The card makes room for the page before it is written on.
        QTest::qWait(300);
        penStroke(m_view.get(), page, {0.05, 0.2}, {0.5, 0.2}, 0.5);
        tap(m_view.get(), named(QStringLiteral("type")));
        QVERIFY(!note()->property("writing").toBool());
        QTRY_VERIFY(page->isVisible());
        QVERIFY(!named(QStringLiteral("palette"))->isVisible());
        const QPointF on = page->mapToScene(QPointF(page->width() * 0.6, page->height() * 0.5));
        penEvent(m_view.get(), QEvent::TabletPress, on, 0.5);
        penEvent(m_view.get(), QEvent::TabletRelease, on, 0);
        QTRY_VERIFY(note()->property("writing").toBool());
    }

    void doneFinishes()
    {
        QSignalSpy finished(m_capture.get(), &Capture::finished);
        type(m_view.get(), QStringLiteral("x"));
        tap(m_view.get(), named(QStringLiteral("done")));
        QCOMPARE(finished.count(), 1);
        QVERIFY(m_store->note(m_capture->noteId()).has_value());
    }

    void actionsOnlyForAKeptNote()
    {
        QVERIFY(!named(QStringLiteral("tuckAway"))->isVisible());
        QVERIFY(!named(QStringLiteral("remove"))->isVisible());
        type(m_view.get(), QStringLiteral("x"));
        QVERIFY(named(QStringLiteral("tuckAway"))->isVisible());
        tap(m_view.get(), named(QStringLiteral("remove")));
        QVERIFY(m_store->notes().isEmpty());
        QVERIFY(QFile::exists(m_home->trash()));
    }

    // The card on a tablet's free room: the search's size, centred, the
    // cursor in the note. With the keys up it rises only as far as it must,
    // then shortens; it never lies under them.
    void cardFitsTheRoomAndTheKeys()
    {
        Places places(m_store.get());
        PlaceNotes notes(m_store.get());
        FakeShell shell(m_store.get(), m_capture.get(), &places, &notes);
        QQmlEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent component(&engine, QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("CaptureWindow"));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(&shell)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        window->resize(1260, 736);
        QMetaObject::invokeMethod(window, "open");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *card = window->property("card").value<QQuickItem *>();
        QVERIFY(card);
        QTRY_COMPARE(card->width(), 806.0);
        QCOMPARE(card->height(), 471.0);
        QCOMPARE(card->x(), 227.0);
        QCOMPARE(card->y(), 133.0);
        QCOMPARE(card->property("radius").toReal(), 8.0);
        QCOMPARE(card->property("border").value<QObject *>()->property("width").toReal(), 1.0);
        auto *quick = window->property("note").value<QQuickItem *>();
        QTRY_VERIFY(quick->property("editor").value<QQuickItem *>()->hasActiveFocus());
        picture(window, QStringLiteral("card"));

        // Low keys leave the card where it is.
        window->setProperty("keysRect", QRectF(0, 650, 1260, 86));
        QTest::qWait(300);
        QCOMPARE(card->y(), 133.0);
        QCOMPARE(card->height(), 471.0);
        // Taller keys lift it just clear of them.
        window->setProperty("keysRect", QRectF(0, 560, 1260, 176));
        QTRY_COMPARE(card->y(), 79.0);
        QCOMPARE(card->height(), 471.0);
        // Taller still: it reaches the top of the room, then shortens.
        window->setProperty("keysRect", QRectF(0, 436, 1260, 300));
        QTRY_COMPARE(card->height(), 416.0);
        QCOMPARE(card->y(), 10.0);
        QVERIFY(card->y() + card->height() <= 436);
        picture(window, QStringLiteral("card-keys"));
        for (QQuickItem *button : buttonsIn(card)) {
            QVERIFY2(button->height() >= 44 && button->width() >= 44, qPrintable(button->objectName()));
        }
        window->setProperty("keysRect", QRectF());
        QTRY_COMPARE(card->y(), 133.0);
    }

    // All notes grows the card into the board, as Apps grows the search; a
    // note opened there or Esc brings the note back, and a
    // second Esc or a tap outside the card puts it away.
    void cardGrowsIntoTheBoard()
    {
        m_capture->setText(QStringLiteral("Flick threshold"));
        const QString written = m_capture->noteId();
        Places places(m_store.get());
        PlaceNotes notes(m_store.get());
        FakeShell shell(m_store.get(), m_capture.get(), &places, &notes);
        QQmlEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent component(&engine, QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("CaptureWindow"));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(&shell)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        window->resize(1260, 736);
        QMetaObject::invokeMethod(window, "open");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *card = window->property("card").value<QQuickItem *>();
        QTRY_COMPARE(card->width(), 806.0);

        tap(window, itemNamed(card, QStringLiteral("allNotes")));
        QVERIFY(window->property("expanded").toBool());
        QTRY_COMPARE(card->width(), 1240.0);
        QTRY_COMPARE(card->height(), 716.0);
        QCOMPARE(card->y(), 10.0);
        QTRY_VERIFY(itemNamed(card, QStringLiteral("note-") + written));
        QVERIFY(itemNamed(card, QStringLiteral("place-today"))->isVisible());
        picture(window, QStringLiteral("card-board"));
        for (QQuickItem *button : buttonsIn(window->property("board").value<QQuickItem *>())) {
            QVERIFY2(button->height() >= 44 && button->width() >= 44, qPrintable(button->objectName()));
        }

        // A note tapped on the board opens on the card, back at its size.
        tap(window, itemNamed(card, QStringLiteral("note-") + written));
        QCOMPARE(shell.opened, written);
        QMetaObject::invokeMethod(window, "open"); // As the shell does for it.
        QVERIFY(!window->property("expanded").toBool());
        QTRY_COMPARE(card->width(), 806.0);

        // At full size there is no Back button: Esc brings the note back, and
        // a second Esc puts the card away.
        tap(window, itemNamed(card, QStringLiteral("allNotes")));
        QVERIFY(window->property("expanded").toBool());
        QVERIFY(!itemNamed(card, QStringLiteral("backToNote")));
        QSignalSpy finished(m_capture.get(), &Capture::finished);
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!window->property("expanded").toBool());
        QCOMPARE(finished.count(), 0);
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(finished.count(), 1);

        // A tap on the card is the card's; a tap beside it puts it away.
        QMetaObject::invokeMethod(window, "open");
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(int(card->x()) + 120, int(card->y()) + 36));
        QCOMPARE(finished.count(), 1);
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(60, 60));
        QCOMPARE(finished.count(), 2);
    }

    // The board shows handwriting as it was written, marked Handwritten; a
    // search finds it by what it was read as, or a runner-up word, and says
    // what it was read as with the word marked.
    void boardShowsHandwriting()
    {
        Capture written(m_store.get());
        written.startNewIn(QStringLiteral("project:Shuffle"));
        written.setColour(QStringLiteral("rhyolite"));
        for (int row = 0; row < 2; ++row) {
            const qreal y = row * Ink::RowHeight + Ink::RowHeight / 2;
            written.beginStroke(20, y, 0.5, QStringLiteral("black"));
            for (qreal x = 24; x < 300; x += 6) {
                written.extendStroke(x, y + std::sin(x / 9) * 8, 0.5 + 0.3 * std::sin(x / 40));
            }
            written.endStroke();
        }
        written.finish();
        const QString id = written.noteId();
        Ink ink = m_store->ink(id);
        QVERIFY(ink.setReading(0, ink.digest(0), QStringLiteral("measure flack"), {QStringLiteral("flick")}));
        QVERIFY(ink.setReading(1, ink.digest(1), QStringLiteral("velocity"), {}));
        QVERIFY(m_store->saveInk(*m_store->note(id), ink, NoteStore::Touch::Kept));

        Places places(m_store.get());
        PlaceNotes notes(m_store.get());
        FakeShell shell(m_store.get(), m_capture.get(), &places, &notes);
        QQmlEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent component(&engine, QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("BoardWindow"));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(&shell)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *board = qobject_cast<QQuickWindow *>(object.get());
        QMetaObject::invokeMethod(board, "present", Q_ARG(QVariant, QVariant()));
        QVERIFY(QTest::qWaitForWindowExposed(board));
        board->resize(1260, 716);
        auto find = [board](const QString &name) { return itemNamed(board->contentItem(), name); };
        tap(board, find(QStringLiteral("place-project:Shuffle")));
        QTRY_VERIFY(find(QStringLiteral("note-") + id));
        QQuickItem *card = find(QStringLiteral("note-") + id);
        QQuickItem *drawing = itemNamed(card, QStringLiteral("drawing"));
        QVERIFY(drawing->isVisible());
        QTRY_COMPARE(drawing->property("status").toInt(), 1); // Ready: the drawing is a picture any viewer draws.
        QTRY_VERIFY(drawing->height() > 20);
        QVERIFY(itemNamed(card, QStringLiteral("handwritten"))->isVisible());
        QVERIFY(!itemNamed(card, QStringLiteral("readAs"))->isVisible());
        picture(board, QStringLiteral("board-ink"));

        QQuickItem *search = find(QStringLiteral("search"));
        tap(board, search);
        type(board, QStringLiteral("flick"));
        QTRY_VERIFY(find(QStringLiteral("note-") + id));
        card = find(QStringLiteral("note-") + id);
        QQuickItem *readAs = itemNamed(card, QStringLiteral("readAs"));
        QTRY_VERIFY(readAs->isVisible());
        QCOMPARE(readAs->property("text").toString(), QStringLiteral("Read as “measure flack velocity”, or “<b><u>flick</u></b>”"));
        picture(board, QStringLiteral("board-ink-search"));
        search->setProperty("text", QStringLiteral("veloc"));
        QTRY_COMPARE(itemNamed(find(QStringLiteral("note-") + id), QStringLiteral("readAs"))->property("text").toString(),
                     QStringLiteral("Read as “measure flack <b><u>veloc</u></b>ity”"));
    }

    void boardGathersAndActs()
    {
        m_capture->setText(QStringLiteral("Flick threshold"));
        const QString onWindow = m_capture->noteId();
        Capture other(m_store.get());
        other.startNew({});
        other.setText(QStringLiteral("Groceries"));
        const QString loose = other.noteId();
        const QList<QPair<QString, QString>> more = {
            {QStringLiteral("Notes stack on a card's corner in Spread: count, not content."), QStringLiteral("lake")},
            {QStringLiteral("Wallpaper idea: Palisade Head at dusk, rhyolite red."), QStringLiteral("stone")},
            {QStringLiteral("What if Bento remembered which notes were open per pane?"), QStringLiteral("butter")},
            {QStringLiteral("Call about the cabin weekend"), QStringLiteral("rhyolite")},
        };
        for (const auto &[text, colour] : more) {
            Capture extra(m_store.get());
            extra.startNewIn(QStringLiteral("project:Shuffle"));
            extra.setColour(colour);
            extra.setText(text);
        }

        Places places(m_store.get());
        PlaceNotes notes(m_store.get());
        FakeShell shell(m_store.get(), m_capture.get(), &places, &notes);
        QQmlEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent component(&engine, QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("BoardWindow"));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(&shell)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *board = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(board);
        QMetaObject::invokeMethod(board, "present", Q_ARG(QVariant, QVariant()));
        QVERIFY(QTest::qWaitForWindowExposed(board));
        QCOMPARE(notes.place(), QStringLiteral("today"));
        board->resize(1260, 716);
        picture(board, QStringLiteral("board-today"));

        auto find = [board](const QString &name) { return itemNamed(board->contentItem(), name); };
        QTRY_VERIFY(find(QStringLiteral("note-") + onWindow));
        QVERIFY(find(QStringLiteral("place-window:SpreadGesture.qml")));

        // Tapping a note opens it on the card.
        tap(board, find(QStringLiteral("note-") + loose));
        QCOMPARE(shell.opened, loose);

        // Tuck away from the note, and undo it from the message that follows.
        tap(board, itemNamed(find(QStringLiteral("note-") + onWindow), QStringLiteral("tuck")));
        QVERIFY(m_store->note(onWindow)->tucked);
        QTRY_VERIFY(find(QStringLiteral("noticeAction"))->isVisible());
        tap(board, find(QStringLiteral("noticeAction")));
        QVERIFY(!m_store->note(onWindow)->tucked);

        // Tuck it away again, then bring it back from Tucked away.
        QTRY_VERIFY(find(QStringLiteral("note-") + onWindow));
        tap(board, itemNamed(find(QStringLiteral("note-") + onWindow), QStringLiteral("tuck")));
        QVERIFY(m_store->note(onWindow)->tucked);
        tap(board, find(QStringLiteral("place-tucked")));
        QCOMPARE(notes.place(), QStringLiteral("tucked"));
        QTRY_VERIFY(find(QStringLiteral("note-") + onWindow));
        tap(board, find(QStringLiteral("note-") + onWindow));
        QVERIFY(!m_store->note(onWindow)->tucked);

        // New note in a place belongs there.
        tap(board, find(QStringLiteral("place-window:SpreadGesture.qml")));
        tap(board, find(QStringLiteral("newNote")));
        QCOMPARE(shell.newIn, QStringLiteral("window:SpreadGesture.qml"));

        // Every target on the board is big enough to touch.
        for (QQuickItem *button : buttonsIn(board->contentItem())) {
            QVERIFY2(button->height() >= 44 && button->width() >= 44,
                     qPrintable(QStringLiteral("%1 '%2' is %3×%4").arg(QString::fromLatin1(button->metaObject()->className()),
                                                                         button->property("text").toString())
                                    .arg(button->width()).arg(button->height())));
        }
    }

    // Today on the board, as the mock-up draws it: the day strip, the
    // planner with the next note marked and a done one struck through, the
    // days ahead, and the ideas with no date beside it. A tick marks a note
    // done; a row opens its note; another day shows its own notes.
    void plannerOnTheBoard()
    {
        const QDateTime now = QDateTime::currentDateTime();
        if (now.time() < QTime(0, 2) || now.time() > QTime(23, 30)) {
            QSKIP("Too near midnight to plan notes on either side of now.");
        }
        const QDate today = now.date();
        auto planned = [this](const QString &text, const QString &place, const QDateTime &at, const QString &colour = QStringLiteral("butter")) {
            Capture capture(m_store.get());
            capture.startNewIn(place);
            capture.setColour(colour);
            capture.setText(text);
            capture.setRemindAt(at);
            capture.finish();
            return capture.noteId();
        };
        const QString done = planned(QStringLiteral("Record Z13 flick samples"), QStringLiteral("window:SpreadGesture.qml"),
                                     QDateTime(today, QTime(0, 0, 30)));
        QVERIFY(setNoteDone(m_store.get(), done, true));
        QDateTime soon = now.addSecs(3600);
        soon.setTime(QTime(soon.time().hour(), 0));
        if (soon.date() != today) {
            soon = QDateTime(today, QTime(23, 45));
        }
        const QString next = planned(QStringLiteral("Ask Sam about a mouse way to move Spread on from Search"),
                                     QStringLiteral("project:Shuffle"), soon);
        const QString evening = planned(QStringLiteral("Call about the cabin weekend"), QStringLiteral("project:Home"),
                                        QDateTime(today, QTime(23, 59)));
        const QString ahead = planned(QStringLiteral("Split Rock: Milestone 0 review"), QStringLiteral("project:Split Rock"),
                                      QDateTime(today.addDays(2), QTime(9, 0)));
        m_capture->setText(QStringLiteral("Flick threshold feels short on the Z13. Measure the real velocity before touching 1400."));
        Capture list(m_store.get());
        list.startNewIn(QStringLiteral("project:Shuffle"));
        list.setColour(QStringLiteral("lichen"));
        list.setText(QStringLiteral("Groceries\ncoffee\noats\nlemons\ntape"));
        list.makeChecklist();
        list.setLineChecked(2, true);
        const QString groceries = list.noteId();
        Capture idea(m_store.get());
        idea.startNewIn(QStringLiteral("project:Shuffle"));
        idea.setColour(QStringLiteral("lake"));
        idea.setText(QStringLiteral("Notes stack on a card's corner in Spread: count, not content."));

        Places places(m_store.get());
        PlaceNotes notes(m_store.get());
        FakeShell shell(m_store.get(), m_capture.get(), &places, &notes);
        QQmlEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent component(&engine, QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("BoardWindow"));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(&shell)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *board = qobject_cast<QQuickWindow *>(object.get());
        QMetaObject::invokeMethod(board, "present", Q_ARG(QVariant, QVariant()));
        QVERIFY(QTest::qWaitForWindowExposed(board));
        board->resize(1260, 716);
        auto find = [board](const QString &name) { return itemNamed(board->contentItem(), name); };
        QTRY_VERIFY(find(QStringLiteral("plan-") + next));
        picture(board, QStringLiteral("board-planner"));

        QCOMPARE(find(QStringLiteral("heading"))->property("text").toString(), QLocale().toString(today, QStringLiteral("d MMMM")));
        QQuickItem *strip = find(QStringLiteral("dayStrip"));
        QVERIFY(strip->isVisible());
        QQuickItem *todayButton = itemNamed(strip, QStringLiteral("day-") + today.toString(QStringLiteral("yyyy-MM-dd")));
        QVERIFY(todayButton);
        QCOMPARE(todayButton->width(), 64.0);
        QCOMPARE(todayButton->height(), 64.0);
        QVERIFY(itemNamed(itemNamed(strip, QStringLiteral("day-") + today.addDays(2).toString(QStringLiteral("yyyy-MM-dd"))),
                          QStringLiteral("planned"))->isVisible());
        QVERIFY(!itemNamed(todayButton, QStringLiteral("planned"))->isVisible());

        // The planner column: today's notes in order, then the day ahead.
        QList<QString> headings;
        for (QQuickItem *item : itemsUnder(find(QStringLiteral("planner")))) {
            if (item->objectName() == QLatin1String("plannerHeading")) {
                headings.append(item->property("text").toString());
            }
        }
        QCOMPARE(headings.value(0), QStringLiteral("PLANNER · TODAY"));
        QCOMPARE(headings.size(), 2);
        QQuickItem *doneRow = find(QStringLiteral("plan-") + done);
        QQuickItem *nextRow = find(QStringLiteral("plan-") + next);
        QQuickItem *eveningRow = find(QStringLiteral("plan-") + evening);
        QVERIFY(find(QStringLiteral("plan-") + ahead));
        QVERIFY(doneRow->y() < nextRow->y() && nextRow->y() < eveningRow->y());
        QCOMPARE(itemNamed(doneRow, QStringLiteral("planTime"))->property("text").toString(), QStringLiteral("Done"));
        QVERIFY(itemNamed(doneRow, QStringLiteral("planTitle"))->property("font").value<QFont>().strikeOut());
        QCOMPARE(nextRow->property("border").value<QObject *>()->property("width").toReal(), 1.0);
        QCOMPARE(eveningRow->property("border").value<QObject *>()->property("width").toReal(), 0.0);
        QCOMPARE(itemNamed(nextRow, QStringLiteral("planTime"))->property("color").value<QColor>(), QColor(QStringLiteral("#F2A65A")));

        // Ideas beside it: no note with a time among them; the checklist as
        // its heading and items.
        QVERIFY(find(QStringLiteral("ideasHeading"))->isVisible());
        QCOMPARE(find(QStringLiteral("ideasHeading"))->property("text").toString(), QStringLiteral("IDEAS · NO DATE NEEDED"));
        QVERIFY(find(QStringLiteral("note-") + groceries));
        QVERIFY(!find(QStringLiteral("note-") + next));
        QQuickItem *groceriesCard = find(QStringLiteral("note-") + groceries);
        QCOMPARE(itemNamed(groceriesCard, QStringLiteral("checklistHeading"))->property("text").toString(), QStringLiteral("Groceries"));
        QCOMPARE(itemNamed(groceriesCard, QStringLiteral("checklistItems"))->property("text").toString(),
                 QStringLiteral("coffee · <s>oats</s> · lemons · tape"));

        // The tick marks a note done, and again marks it not done.
        tap(board, itemNamed(eveningRow, QStringLiteral("planDone")));
        QVERIFY(m_store->note(evening)->done.isValid());
        QTRY_COMPARE(itemNamed(find(QStringLiteral("plan-") + evening), QStringLiteral("planTime"))->property("text").toString(),
                     QStringLiteral("Done"));
        tap(board, itemNamed(find(QStringLiteral("plan-") + evening), QStringLiteral("planDone")));
        QVERIFY(!m_store->note(evening)->done.isValid());

        // A row opens its note.
        QTRY_VERIFY(find(QStringLiteral("plan-") + next));
        tap(board, itemNamed(find(QStringLiteral("plan-") + next), QStringLiteral("planTitle")));
        QCOMPARE(shell.opened, next);

        // Another day: its own notes, under its own name.
        tap(board, itemNamed(strip, QStringLiteral("day-") + today.addDays(2).toString(QStringLiteral("yyyy-MM-dd"))));
        QTRY_COMPARE(find(QStringLiteral("heading"))->property("text").toString(),
                     QLocale().toString(today.addDays(2), QStringLiteral("d MMMM")));
        QTRY_VERIFY(!find(QStringLiteral("plan-") + next));
        QVERIFY(find(QStringLiteral("plan-") + ahead));
        picture(board, QStringLiteral("board-planner-ahead"));

        // Every target big enough to touch.
        for (QQuickItem *button : buttonsIn(board->contentItem())) {
            QVERIFY2(button->height() >= 44 && button->width() >= 44, qPrintable(button->objectName()));
        }

        // A narrow window puts the day strip on a line of its own.
        board->resize(800, 716);
        QTRY_VERIFY(find(QStringLiteral("dayStripBelow"))->isVisible());
        QVERIFY(!find(QStringLiteral("dayStrip"))->isVisible());
    }
};

int main(int argc, char *argv[])
{
    TestHome home;
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("gooseberry");
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Gooseberry"));
    // The desktop names the icon theme and where themes are; off screen,
    // Breeze from the system's icon folders stands in for it.
    QIcon::setThemeSearchPaths(QIcon::themeSearchPaths()
                               + QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"),
                                                           QStandardPaths::LocateDirectory));
    if (QIcon::themeName().isEmpty() || QIcon::themeName() == QLatin1String("hicolor")) {
        QIcon::setThemeName(QStringLiteral("breeze"));
    }
    ScreensTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_screens.moc"
