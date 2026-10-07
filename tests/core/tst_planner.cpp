// SPDX-License-Identifier: GPL-2.0-or-later
// The planner's core: reminders, done, and checklists, kept in the note
// itself; each reminder shown once, through restarts; the planner's days and
// rows; and what Remind offers first. All in a home of the test's own.
#include "Board.h"
#include "Capture.h"
#include "Checklist.h"
#include "NoteStore.h"
#include "Planner.h"
#include "Reminders.h"
#include "TestHome.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace Gooseberry;

namespace {

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QDateTime toTheSecond(QDateTime time)
{
    time.setTime(QTime(time.time().hour(), time.time().minute(), time.time().second()));
    return time;
}

} // namespace

class PlannerTest : public QObject
{
    Q_OBJECT

public:
    explicit PlannerTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;
    QString m_folder;
    std::unique_ptr<NoteStore> m_store;

    QString make(const QString &text, const QDateTime &remind = {}, bool onOpen = false)
    {
        Note note;
        note.text = text;
        note.window = QStringLiteral("SpreadGesture.qml");
        note.app = QStringLiteral("org.kde.kate");
        note.stuck = true;
        note.remind = remind.isValid() ? toTheSecond(remind) : QDateTime();
        note.remindOnOpen = onOpen;
        return m_store->create(note);
    }

    void reopen()
    {
        m_store = std::make_unique<NoteStore>(m_folder);
        QVERIFY(m_store->open());
    }

private Q_SLOTS:
    void init()
    {
        static int round = 0;
        m_folder = m_home->path() + QStringLiteral("/planner-%1").arg(++round);
        QVERIFY(m_home->holds(m_folder));
        reopen();
    }

    void cleanup()
    {
        m_store.reset();
    }

    // The reminder, when it was shown and done are in the header, and read
    // back as written; a reminder this version cannot read is kept as is.
    void headerKeys()
    {
        const QDateTime at = toTheSecond(QDateTime::currentDateTime().addDays(1));
        const QString timed = make(QStringLiteral("Call about the cabin"), at);
        const QString opens = make(QStringLiteral("Check the flick"), {}, true);
        const QByteArray file = readFile(m_store->pathFor(timed));
        QVERIFY(file.contains(QByteArray("remind: " + at.toOffsetFromUtc(at.offsetFromUtc()).toString(Qt::ISODate).toUtf8() + "\n")));
        QVERIFY(!file.contains("reminded:"));
        QVERIFY(!file.contains("done:"));
        QVERIFY(readFile(m_store->pathFor(opens)).contains("remind: opens\n"));

        QVERIFY(setNoteDone(m_store.get(), timed, true));
        reopen();
        const auto read = m_store->note(timed);
        QCOMPARE(read->remind, at);
        QVERIFY(read->done.isValid());
        QVERIFY(m_store->note(opens)->remindOnOpen);
        QVERIFY(!m_store->note(opens)->remind.isValid());

        const Note odd = Note::parse("---\ngooseberry: 1\nremind: when the kettle boils\n---\nTea\n", QStringLiteral("x"), QDateTime::currentDateTime());
        QVERIFY(!odd.hasReminder());
        QVERIFY(odd.serialize().contains("remind: when the kettle boils\n"));
    }

    // A reminder whose time has come is shown once: the time it was shown is
    // in the note before it is told, and neither a restart nor another look
    // shows it again. One not yet due waits; a done note does not remind.
    void shownOnce()
    {
        const QString due = make(QStringLiteral("Due"), QDateTime::currentDateTime().addSecs(-60));
        const QString later = make(QStringLiteral("Later"), QDateTime::currentDateTime().addSecs(3600));
        const QString done = make(QStringLiteral("Done already"), QDateTime::currentDateTime().addSecs(-120));
        QVERIFY(setNoteDone(m_store.get(), done, true));

        auto reminders = std::make_unique<Reminders>(m_store.get());
        QSignalSpy shown(reminders.get(), &Reminders::due);
        QTRY_COMPARE(shown.count(), 1);
        QCOMPARE(shown.first().value(0).toString(), due);
        QVERIFY(readFile(m_store->pathFor(due)).contains("reminded: "));
        QVERIFY(!readFile(m_store->pathFor(later)).contains("reminded: "));
        reminders->check();
        QTest::qWait(100);
        QCOMPARE(shown.count(), 1);

        // Gooseberry starts again on the same folder.
        reminders.reset();
        reopen();
        reminders = std::make_unique<Reminders>(m_store.get());
        QSignalSpy again(reminders.get(), &Reminders::due);
        QTest::qWait(200);
        reminders->check();
        QCOMPARE(again.count(), 0);
        QCOMPARE(reminders->nextDue(), m_store->note(later)->remind);
    }

    // A reminder comes due while Gooseberry runs, on time.
    void onTime()
    {
        Reminders reminders(m_store.get());
        QSignalSpy shown(&reminders, &Reminders::due);
        const QDateTime at = QDateTime::currentDateTime().addSecs(2);
        const QString id = make(QStringLiteral("Soon"), at);
        QTest::qWait(500);
        QCOMPARE(shown.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(shown.count(), 1, 4000);
        QCOMPARE(shown.first().value(0).toString(), id);
        QVERIFY(QDateTime::currentDateTime() >= toTheSecond(at));
        QVERIFY(QDateTime::currentDateTime() < at.addSecs(2));
    }

    // Where nothing can show a reminder, it waits rather than being recorded
    // as shown, and is shown once it can be.
    void waitsUntilItCanBeShown()
    {
        Reminders reminders(m_store.get());
        reminders.setReady(false);
        QSignalSpy shown(&reminders, &Reminders::due);
        const QString id = make(QStringLiteral("Due"), QDateTime::currentDateTime().addSecs(-5));
        QTest::qWait(200);
        QCOMPARE(shown.count(), 0);
        QVERIFY(!m_store->note(id)->reminded.isValid());
        reminders.setReady(true);
        QCOMPARE(shown.count(), 1);
        QVERIFY(m_store->note(id)->reminded.isValid());
    }

    // "Next time this opens": shown when a window showing that document
    // opens, once, and not for another document or application.
    void nextTimeItOpens()
    {
        Reminders reminders(m_store.get());
        QSignalSpy shown(&reminders, &Reminders::due);
        const QString id = make(QStringLiteral("Check the flick"), {}, true);
        QTest::qWait(100);
        QCOMPARE(shown.count(), 0);
        reminders.documentOpened(QStringLiteral("Other.qml"), QStringLiteral("org.kde.kate"));
        reminders.documentOpened(QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.mozilla.firefox"));
        QCOMPARE(shown.count(), 0);
        reminders.documentOpened(QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"));
        QCOMPARE(shown.count(), 1);
        QCOMPARE(shown.first().value(0).toString(), id);
        reminders.documentOpened(QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"));
        QCOMPARE(shown.count(), 1);
    }

    // In 10 minutes is a new reminder, shown once more; Done marks it done.
    void againAndDone()
    {
        Reminders reminders(m_store.get());
        QSignalSpy shown(&reminders, &Reminders::due);
        const QString id = make(QStringLiteral("Due"), QDateTime::currentDateTime().addSecs(-5));
        QTRY_COMPARE(shown.count(), 1);
        const QDateTime before = QDateTime::currentDateTime();
        QVERIFY(reminders.remindAgainIn(id, 10));
        const auto note = m_store->note(id);
        QVERIFY(!note->reminded.isValid());
        QVERIFY(qAbs(note->remind.secsTo(before.addSecs(600))) <= 2);
        QVERIFY(reminders.setDone(id, true));
        QVERIFY(m_store->note(id)->done.isValid());
        QVERIFY(!reminders.setDone(id, true));
        QVERIFY(reminders.setDone(id, false));
        QVERIFY(!m_store->note(id)->done.isValid());
    }

    // Recording a reminder as shown is not a change the person made: the
    // note keeps its changed time.
    void shownIsNotAChange()
    {
        const QString id = make(QStringLiteral("Due"), QDateTime::currentDateTime().addSecs(-5));
        const QDateTime changed = m_store->note(id)->changed;
        QTest::qWait(1100);
        Reminders reminders(m_store.get());
        QSignalSpy shown(&reminders, &Reminders::due);
        QTRY_COMPARE(shown.count(), 1);
        QCOMPARE(m_store->note(id)->changed, changed);
    }

    // On the card: a reminder is kept with the note, at once; a new one is
    // shown again; one set before the first letter waits for it.
    void cardReminder()
    {
        Capture capture(m_store.get());
        capture.startNew({QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")});
        const QDateTime at = toTheSecond(QDateTime::currentDateTime().addDays(1));
        capture.setRemindAt(at);
        QVERIFY(!capture.kept());
        capture.setText(QStringLiteral("C"));
        QVERIFY(capture.kept());
        QCOMPARE(m_store->note(capture.noteId())->remind, at);

        // Shown, then set again: shown once more.
        auto note = *m_store->note(capture.noteId());
        note.reminded = QDateTime::currentDateTime();
        QVERIFY(m_store->save(note, NoteStore::Touch::Kept));
        QVERIFY(capture.hasReminder());
        capture.setRemindOnOpen();
        QVERIFY(m_store->note(capture.noteId())->remindOnOpen);
        QVERIFY(!m_store->note(capture.noteId())->reminded.isValid());
        QVERIFY(!m_store->note(capture.noteId())->remind.isValid());
        capture.clearReminder();
        QVERIFY(!m_store->note(capture.noteId())->hasReminder());
        QVERIFY(!readFile(m_store->pathFor(capture.noteId())).contains("remind"));

        // A note with no window has no "next time this opens".
        capture.startNewIn(QStringLiteral("inbox"));
        capture.setText(QStringLiteral("L"));
        capture.setRemindOnOpen();
        QVERIFY(!capture.hasReminder());
    }

    // A reminder shown while typing on the card waits for a pause is not
    // lost when the typing is written.
    void shownWhileTyping()
    {
        Capture capture(m_store.get());
        capture.setWaits(5000, 10000);
        capture.startNew({});
        capture.setText(QStringLiteral("D"));
        capture.setRemindAt(QDateTime::currentDateTime().addSecs(3600));
        capture.setText(QStringLiteral("Due soon"));
        QVERIFY(capture.waiting());
        Reminders reminders(m_store.get());
        auto note = *m_store->note(capture.noteId());
        note.remind = toTheSecond(QDateTime::currentDateTime().addSecs(-1));
        QVERIFY(m_store->save(note, NoteStore::Touch::Kept));
        QSignalSpy shown(&reminders, &Reminders::due);
        reminders.check();
        QCOMPARE(shown.count(), 1);
        capture.flush();
        const auto kept = m_store->note(capture.noteId());
        QCOMPARE(kept->text, QStringLiteral("Due soon"));
        QVERIFY(kept->reminded.isValid());
        reminders.check();
        QCOMPARE(shown.count(), 1);
    }

    // A checklist is Markdown task lines: made from text, ticked, added to
    // and taken back, the heading kept apart.
    void checklistText()
    {
        QCOMPARE(Checklist::from(QStringLiteral("Groceries\ncoffee\noats\n")),
                 QStringLiteral("Groceries\n- [ ] coffee\n- [ ] oats"));
        QCOMPARE(Checklist::from(QStringLiteral("coffee")), QStringLiteral("- [ ] coffee"));
        QCOMPARE(Checklist::from(QString()), QStringLiteral("- [ ] "));
        const QString list = QStringLiteral("Groceries\n- [ ] coffee\n* [X] oats\n- [ ] lemons");
        QVERIFY(Checklist::contains(list));
        const auto lines = Checklist::lines(list);
        QCOMPARE(lines.size(), 4);
        QVERIFY(!lines.at(0).item);
        QVERIFY(lines.at(2).item && lines.at(2).checked);
        QCOMPARE(lines.at(2).text, QStringLiteral("oats"));
        QCOMPARE(Checklist::withChecked(list, 1, true), QStringLiteral("Groceries\n- [x] coffee\n* [X] oats\n- [ ] lemons"));
        QCOMPARE(Checklist::withChecked(list, 2, false), QStringLiteral("Groceries\n- [ ] coffee\n* [ ] oats\n- [ ] lemons"));
        QCOMPARE(Checklist::withLineText(list, 2, QStringLiteral("rolled oats")),
                 QStringLiteral("Groceries\n- [ ] coffee\n* [X] rolled oats\n- [ ] lemons"));
        QCOMPARE(Checklist::withItemAfter(list, 2), QStringLiteral("Groceries\n- [ ] coffee\n* [X] oats\n* [ ] \n- [ ] lemons"));
        QCOMPARE(Checklist::withoutLine(list, 3), QStringLiteral("Groceries\n- [ ] coffee\n* [X] oats"));
        QCOMPARE(Checklist::toPlain(list), QStringLiteral("Groceries\ncoffee\noats\nlemons"));
        QCOMPARE(Checklist::heading(list), QStringLiteral("Groceries"));
        QCOMPARE(Checklist::summary(list), QStringLiteral("coffee · oats · lemons"));
        Note note;
        note.text = list;
        QCOMPARE(note.title(), QStringLiteral("Groceries"));
    }

    // On the card, a tick is kept at once; words in an item wait for the
    // pause, as typing does.
    void cardChecklist()
    {
        Capture capture(m_store.get());
        capture.setWaits(5000, 10000);
        capture.startNew({});
        capture.setText(QStringLiteral("Groceries\ncoffee\noats"));
        capture.flush();
        capture.makeChecklist();
        QVERIFY(capture.checklist());
        QCOMPARE(m_store->note(capture.noteId())->text, QStringLiteral("Groceries\n- [ ] coffee\n- [ ] oats"));
        capture.setLineChecked(2, true);
        QVERIFY(!capture.waiting());
        QCOMPARE(m_store->note(capture.noteId())->text, QStringLiteral("Groceries\n- [ ] coffee\n- [x] oats"));
        capture.addItemAfter(2);
        QCOMPARE(capture.lines().size(), 4);
        capture.setLineText(3, QStringLiteral("lemons"));
        QVERIFY(capture.waiting());
        capture.flush();
        QCOMPARE(m_store->note(capture.noteId())->text, QStringLiteral("Groceries\n- [ ] coffee\n- [x] oats\n- [ ] lemons"));
        capture.removeLine(1);
        QCOMPARE(m_store->note(capture.noteId())->text, QStringLiteral("Groceries\n- [x] oats\n- [ ] lemons"));
        capture.makePlain();
        QVERIFY(!capture.checklist());
        QCOMPARE(m_store->note(capture.noteId())->text, QStringLiteral("Groceries\noats\nlemons"));
    }

    // Today holds the notes planned for today and those written today with
    // no time; on the board, the timed ones stand on the planner instead.
    void todayPlace()
    {
        const QDate today = QDate::currentDate();
        const QString idea = make(QStringLiteral("An idea"));
        const QString planned = make(QStringLiteral("Planned today"), QDateTime(today, QTime(23, 59)));
        const QString tomorrow = make(QStringLiteral("Planned tomorrow"), QDateTime(today.addDays(1), QTime(9, 0)));
        QVERIFY(Places::contains(QStringLiteral("today"), *m_store->note(idea), today));
        QVERIFY(Places::contains(QStringLiteral("today"), *m_store->note(planned), today));
        QVERIFY(!Places::contains(QStringLiteral("today"), *m_store->note(tomorrow), today));
        PlaceNotes notes(m_store.get());
        notes.setPlace(QStringLiteral("today"));
        QCOMPARE(notes.count(), 1);
        QCOMPARE(notes.data(notes.index(0), PlaceNotes::IdRole).toString(), idea);
        notes.setPlace(QStringLiteral("window:SpreadGesture.qml"));
        QCOMPARE(notes.count(), 3);
    }

    // The planner: the strip from two days back to three ahead, the chosen
    // day's notes in time order with Done and the next one to come, then the
    // days ahead that have notes.
    void plannerDays()
    {
        const QDateTime now = QDateTime::currentDateTime();
        if (now.time() < QTime(0, 2) || now.time() > QTime(23, 57)) {
            QSKIP("Too near midnight to place notes on either side of now.");
        }
        const QDate today = now.date();
        const QString early = make(QStringLiteral("Record flick samples"), QDateTime(today, QTime(0, 0, 30)));
        const QString evening = make(QStringLiteral("Call about the cabin"), QDateTime(today, QTime(23, 59)));
        const QString soon = make(QStringLiteral("Ask about Spread"), now.addSecs(60).time() > now.time()
                                      ? now.addSecs(60) : QDateTime(today, QTime(23, 58)));
        const QString monday = make(QStringLiteral("Groceries\n- [ ] coffee"), QDateTime(today.addDays(2), QTime(9, 0)));
        make(QStringLiteral("Far off"), QDateTime(today.addDays(30), QTime(9, 0)));
        QVERIFY(setNoteDone(m_store.get(), early, true));

        Planner planner(m_store.get());
        QTRY_COMPARE(planner.days().size(), 2);
        const QVariantList strip = planner.strip();
        QCOMPARE(strip.size(), 6);
        QCOMPARE(strip.at(0).toMap().value(QStringLiteral("date")).toDate(), today.addDays(-2));
        QVERIFY(strip.at(2).toMap().value(QStringLiteral("today")).toBool());
        QVERIFY(strip.at(2).toMap().value(QStringLiteral("chosen")).toBool());
        QVERIFY(!strip.at(2).toMap().value(QStringLiteral("planned")).toBool());
        QVERIFY(strip.at(4).toMap().value(QStringLiteral("planned")).toBool());
        QVERIFY(!strip.at(3).toMap().value(QStringLiteral("planned")).toBool());

        const QVariantMap first = planner.days().at(0).toMap();
        QCOMPARE(first.value(QStringLiteral("date")).toDate(), today);
        const QVariantList rows = first.value(QStringLiteral("rows")).toList();
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.at(0).toMap().value(QStringLiteral("id")).toString(), early);
        QVERIFY(rows.at(0).toMap().value(QStringLiteral("done")).toBool());
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("id")).toString(), soon);
        QVERIFY(rows.at(1).toMap().value(QStringLiteral("next")).toBool());
        QCOMPARE(rows.at(2).toMap().value(QStringLiteral("id")).toString(), evening);
        QVERIFY(!rows.at(2).toMap().value(QStringLiteral("next")).toBool());
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("place")).toString(), QStringLiteral("SpreadGesture.qml"));

        const QVariantMap ahead = planner.days().at(1).toMap();
        QCOMPARE(ahead.value(QStringLiteral("date")).toDate(), today.addDays(2));
        const QVariantMap list = ahead.value(QStringLiteral("rows")).toList().at(0).toMap();
        QCOMPARE(list.value(QStringLiteral("id")).toString(), monday);
        QCOMPARE(list.value(QStringLiteral("title")).toString(), QStringLiteral("Groceries"));

        // Ticked on the planner, the next one moves on.
        QVERIFY(planner.setDone(soon, true));
        QTRY_VERIFY(planner.days().at(0).toMap().value(QStringLiteral("rows")).toList().at(2).toMap().value(QStringLiteral("next")).toBool());

        // Another day chosen: its notes, then those after it.
        planner.setDay(today.addDays(2));
        QCOMPARE(planner.days().at(0).toMap().value(QStringLiteral("date")).toDate(), today.addDays(2));
        QCOMPARE(planner.days().size(), 1);
        QVERIFY(planner.strip().at(4).toMap().value(QStringLiteral("chosen")).toBool());
        QVERIFY(planner.strip().at(2).toMap().value(QStringLiteral("planned")).toBool());
        planner.showToday();
        QCOMPARE(planner.day(), today);
    }

    // What Remind offers first, through the day.
    void quickTimesThroughTheDay()
    {
        const QDate day(2026, 10, 3);
        auto kinds = [](const QList<QuickTime> &times) {
            QStringList out;
            for (const auto &time : times) {
                out.append(time.kind);
            }
            return out;
        };
        const auto afternoon = quickTimes(QDateTime(day, QTime(14, 52)));
        QCOMPARE(kinds(afternoon), (QStringList{QStringLiteral("later"), QStringLiteral("evening"), QStringLiteral("tomorrow")}));
        QCOMPARE(afternoon.at(0).time, QDateTime(day, QTime(17, 0)));
        QCOMPARE(afternoon.at(1).time, QDateTime(day, QTime(19, 0)));
        QCOMPARE(afternoon.at(2).time, QDateTime(day.addDays(1), QTime(9, 0)));
        QCOMPARE(quickTimes(QDateTime(day, QTime(15, 0))).at(0).time, QDateTime(day, QTime(17, 0)));
        QCOMPARE(kinds(quickTimes(QDateTime(day, QTime(17, 30)))), (QStringList{QStringLiteral("evening"), QStringLiteral("tomorrow")}));
        QCOMPARE(kinds(quickTimes(QDateTime(day, QTime(21, 0)))), QStringList{QStringLiteral("tomorrow")});
    }
};

int main(int argc, char *argv[])
{
    TestHome home;
    QCoreApplication app(argc, argv);
    PlannerTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_planner.moc"
