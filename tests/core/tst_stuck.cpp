// SPDX-License-Identifier: GPL-2.0-or-later
// Which open windows have stuck notes, and which window a title bar or Spread
// means, as docs/DESKTOP.md § Stuck notes on the bus lays it out.
#include "StuckWindows.h"

#include <QTest>

using namespace Gooseberry;

namespace {

Note stuckNote(const QString &id, const QString &window, const QString &app, int minutes)
{
    Note note;
    note.id = id;
    note.text = QStringLiteral("Note %1\nmore").arg(id);
    note.colour = QStringLiteral("lake");
    note.stuck = true;
    note.window = window;
    note.app = app;
    note.changed = QDateTime(QDate(2026, 10, 7), QTime(9, 0)).addSecs(minutes * 60);
    return note;
}

OpenWindow openWindow(const QString &id, const QString &caption, const QString &app, const QString &window)
{
    OpenWindow open;
    open.ids = {id};
    open.caption = caption;
    open.app = app;
    open.window = window;
    open.geometry = QRect(100, 50, 800, 600);
    return open;
}

} // namespace

class StuckTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void placeRoundTrips()
    {
        Note note = stuckNote(QStringLiteral("a"), QStringLiteral("plan.md"), QStringLiteral("org.kde.kate"), 0);
        note.created = note.changed;
        note.place = QPointF(0.7125, 0.25);
        const QByteArray bytes = note.serialize();
        QVERIFY(bytes.contains("place: 0.713 0.250\n"));
        const Note read = Note::parse(bytes, note.id, {});
        QVERIFY(read.hasPlace());
        QCOMPARE(read.place, QPointF(0.713, 0.25));
        QVERIFY(read.extra.isEmpty());
    }

    void unplacedNoteWritesNoPlace()
    {
        Note note = stuckNote(QStringLiteral("a"), QStringLiteral("plan.md"), QStringLiteral("org.kde.kate"), 0);
        QVERIFY(!note.hasPlace());
        QVERIFY(!note.serialize().contains("place:"));
    }

    void placeOutOfRangeIsKeptAsWritten()
    {
        const QByteArray bytes = "---\ngooseberry: 2\ncreated: 2026-10-07T09:00:00-05:00\nchanged: 2026-10-07T09:00:00-05:00\n"
                                 "colour: butter\nplace: 1.5 x\ntucked: false\n---\nHi\n";
        const Note read = Note::parse(bytes, QStringLiteral("a"), {});
        QVERIFY(!read.hasPlace());
        QCOMPARE(read.extra.size(), 1);
        QCOMPARE(read.extra.constFirst().first, QStringLiteral("place"));
        QVERIFY(read.serialize().contains("place: 1.5 x\n"));
    }

    void entriesListOnlyWindowsWithNotes()
    {
        const QList<OpenWindow> windows{
            openWindow(QStringLiteral("{11111111-1111-1111-1111-111111111111}"), QStringLiteral("plan.md — Kate"),
                       QStringLiteral("org.kde.kate"), QStringLiteral("plan.md")),
            openWindow(QStringLiteral("{22222222-2222-2222-2222-222222222222}"), QStringLiteral("Dolphin"),
                       QStringLiteral("org.kde.dolphin"), QStringLiteral("Dolphin")),
        };
        Note older = stuckNote(QStringLiteral("older"), QStringLiteral("plan.md"), QStringLiteral("org.kde.kate"), 0);
        older.colour = QStringLiteral("butter");
        const Note newer = stuckNote(QStringLiteral("newer"), QStringLiteral("plan.md"), QStringLiteral("org.kde.kate"), 5);
        Note tucked = stuckNote(QStringLiteral("tucked"), QStringLiteral("plan.md"), QStringLiteral("org.kde.kate"), 9);
        tucked.tucked = true;
        Note unstuck = stuckNote(QStringLiteral("unstuck"), QStringLiteral("Dolphin"), QStringLiteral("org.kde.dolphin"), 9);
        unstuck.stuck = false;
        const Note otherApp = stuckNote(QStringLiteral("other"), QStringLiteral("plan.md"), QStringLiteral("org.kde.ghostwriter"), 9);

        const QVariantList entries = StuckWindows::entries(windows, {older, newer, tucked, unstuck, otherApp},
                                                           {windows.constFirst().key()});
        QCOMPARE(entries.size(), 1);
        const QVariantMap entry = entries.constFirst().toMap();
        QCOMPARE(entry.value(QStringLiteral("count")).toUInt(), 2u);
        QCOMPARE(entry.value(QStringLiteral("colour")).toString(), QStringLiteral("lake"));
        QCOMPARE(entry.value(QStringLiteral("colourHex")).toString(), colourHex(QStringLiteral("lake")));
        QCOMPARE(entry.value(QStringLiteral("caption")).toString(), QStringLiteral("plan.md — Kate"));
        QCOMPARE(entry.value(QStringLiteral("windowIds")).toStringList(), windows.constFirst().ids);
        QCOMPARE(entry.value(QStringLiteral("shown")).toBool(), true);
        const QVariantList notes = entry.value(QStringLiteral("notes")).toList();
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("newer"));
        QCOMPARE(notes.at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Note older"));
    }

    void findsByIdThenCaption()
    {
        const QList<OpenWindow> windows{
            openWindow(QStringLiteral("{11111111-1111-1111-1111-111111111111}"), QStringLiteral("plan.md — Kate"),
                       QStringLiteral("org.kde.kate"), QStringLiteral("plan.md")),
            openWindow(QStringLiteral("{22222222-2222-2222-2222-222222222222}"), QStringLiteral("plan.md — Kate <2>"),
                       QStringLiteral("org.kde.ghostwriter"), QStringLiteral("plan.md")),
            openWindow(QStringLiteral("{33333333-3333-3333-3333-333333333333}"), QStringLiteral("Konsole"),
                       QStringLiteral("org.kde.konsole"), QStringLiteral("Konsole")),
        };
        // By id, with or without braces and in any case.
        QCOMPARE(StuckWindows::find(windows, QStringLiteral("33333333-3333-3333-3333-333333333333"), {}, {}), 2);
        QCOMPARE(StuckWindows::find(windows, QStringLiteral("{11111111-1111-1111-1111-111111111111}"), {}, {}), 0);
        // By caption, the compositor's suffix taken off, the app choosing.
        QCOMPARE(StuckWindows::find(windows, {}, QStringLiteral("plan.md — Kate <2>"), QStringLiteral("ghostwriter")), 1);
        QCOMPARE(StuckWindows::find(windows, {}, QStringLiteral("plan.md — Kate"), QStringLiteral("org.kde.kate")), 0);
        // Two windows share the caption and the app says neither: none.
        QCOMPARE(StuckWindows::find(windows, {}, QStringLiteral("plan.md — Kate"), QStringLiteral("other")), -1);
        // A caption only one window has is that window, whatever its class.
        QCOMPARE(StuckWindows::find(windows, {}, QStringLiteral("Konsole"), QStringLiteral("konsole-x")), 2);
        QCOMPARE(StuckWindows::find(windows, QStringLiteral("{99999999-9999-9999-9999-999999999999}"),
                                    QStringLiteral("Nothing"), QStringLiteral("x")), -1);
    }

    void sameApp()
    {
        QVERIFY(StuckWindows::sameApp(QStringLiteral("org.kde.kate"), QStringLiteral("kate")));
        QVERIFY(StuckWindows::sameApp(QStringLiteral("Kate"), QStringLiteral("org.kde.kate")));
        QVERIFY(StuckWindows::sameApp(QStringLiteral("firefox"), QStringLiteral("Firefox")));
        QVERIFY(!StuckWindows::sameApp(QStringLiteral("org.kde.kate"), QStringLiteral("ate")));
        QVERIFY(!StuckWindows::sameApp({}, QStringLiteral("kate")));
    }
};

QTEST_GUILESS_MAIN(StuckTest)
#include "tst_stuck.moc"
