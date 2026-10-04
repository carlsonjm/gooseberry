// SPDX-License-Identifier: GPL-2.0-or-later
#include "Note.h"

#include <QTest>

using namespace Gooseberry;

class NoteTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip()
    {
        Note note;
        note.id = QStringLiteral("2026-10-04-144112-abcd");
        note.text = QStringLiteral("Flick threshold feels short.\n\nMeasure it: \"1400\" is a guess.\n");
        note.colour = QStringLiteral("lake");
        note.belongs = Belongs::Window;
        note.window = QStringLiteral("SpreadGesture.qml");
        note.app = QStringLiteral("org.kde.kate");
        note.project = QStringLiteral("Shuffle: \"the suite\"");
        note.workspace = QStringLiteral("Desk");
        note.created = QDateTime(QDate(2026, 10, 4), QTime(14, 41, 12), QTimeZone::fromSecondsAheadOfUtc(-5 * 3600));
        note.changed = note.created.addSecs(65);
        note.tucked = true;

        const Note read = Note::parse(note.serialize(), note.id, {});
        QCOMPARE(read.text, note.text);
        QCOMPARE(read.colour, note.colour);
        QCOMPARE(read.belongs, note.belongs);
        QCOMPARE(read.window, note.window);
        QCOMPARE(read.app, note.app);
        QCOMPARE(read.project, note.project);
        QCOMPARE(read.workspace, note.workspace);
        QCOMPARE(read.created, note.created);
        QCOMPARE(read.changed, note.changed);
        QCOMPARE(read.tucked, true);
        QCOMPARE(read.format, NoteFormat);
    }

    void headerIsWrittenAsDocumented()
    {
        Note note;
        note.text = QStringLiteral("Groceries");
        note.created = QDateTime(QDate(2026, 10, 4), QTime(9, 5), QTimeZone::UTC);
        note.changed = note.created;
        const QString written = QString::fromUtf8(note.serialize());
        QCOMPARE(written,
                 QStringLiteral("---\n"
                                "gooseberry: 1\n"
                                "created: 2026-10-04T09:05:00Z\n"
                                "changed: 2026-10-04T09:05:00Z\n"
                                "colour: butter\n"
                                "belongs: loose\n"
                                "tucked: false\n"
                                "---\n"
                                "Groceries"));
    }

    void unknownKeysSurviveAnEdit()
    {
        const QByteArray file = "---\ngooseberry: 1\nremind: 2026-10-05T09:00:00Z\ncolour: lichen\nink-words: \"coffee oats\"\n---\nGroceries\n";
        Note note = Note::parse(file, QStringLiteral("a"), {});
        note.text += QStringLiteral("lemons\n");
        const QString written = QString::fromUtf8(note.serialize());
        QVERIFY(written.contains(QStringLiteral("remind: 2026-10-05T09:00:00Z\n")));
        QVERIFY(written.contains(QStringLiteral("ink-words: \"coffee oats\"\n")));
        QVERIFY(written.endsWith(QStringLiteral("---\nGroceries\nlemons\n")));
    }

    void fileWithoutHeaderIsALooseNote()
    {
        const QDateTime when(QDate(2026, 9, 1), QTime(8, 0));
        const Note note = Note::parse("# Wallpaper idea\nPalisade Head at dusk", QStringLiteral("idea"), when);
        QCOMPARE(note.belongs, Belongs::Loose);
        QCOMPARE(note.text, QStringLiteral("# Wallpaper idea\nPalisade Head at dusk"));
        QCOMPARE(note.created, when);
        QCOMPARE(note.title(), QStringLiteral("Wallpaper idea"));
        QCOMPARE(note.placeKey(), QStringLiteral("loose"));
    }

    void unclosedHeaderIsText()
    {
        const Note note = Note::parse("---\nnot a header, just a rule\n", QStringLiteral("x"), {});
        QCOMPARE(note.text, QStringLiteral("---\nnot a header, just a rule\n"));
    }

    void windowsLineEndingsAndMarkAreRead()
    {
        const Note note = Note::parse("\xEF\xBB\xBF---\r\ngooseberry: 1\r\nbelongs: project\r\nproject: Home\r\n---\r\nCall about the cabin\r\n",
                                      QStringLiteral("x"), {});
        QCOMPARE(note.belongs, Belongs::Project);
        QCOMPARE(note.project, QStringLiteral("Home"));
        QCOMPARE(note.text, QStringLiteral("Call about the cabin\n"));
    }

    void newerFormatIsReadOnly()
    {
        const Note note = Note::parse("---\ngooseberry: 2\n---\nfrom the future", QStringLiteral("x"), {});
        QVERIFY(note.newerFormat());
        QCOMPARE(note.text, QStringLiteral("from the future"));
    }

    void titleIsTheFirstWords()
    {
        Note note;
        note.text = QStringLiteral("\n\n  - [ ] coffee\n- oats");
        QCOMPARE(note.title(), QStringLiteral("coffee"));
        note.text = QStringLiteral("   \n");
        QCOMPARE(note.title(), QString());
    }

    void placeFollowsBelonging()
    {
        Note note;
        note.window = QStringLiteral("Bug 412");
        note.project = QStringLiteral("Shuffle");
        note.workspace = QStringLiteral("Desk");
        QCOMPARE(note.placeKey(), QStringLiteral("loose"));
        note.belongs = Belongs::Window;
        QCOMPARE(note.placeKey(), QStringLiteral("window:Bug 412"));
        note.belongs = Belongs::Project;
        QCOMPARE(note.placeKey(), QStringLiteral("project:Shuffle"));
        note.belongs = Belongs::Workspace;
        QCOMPARE(note.placeKey(), QStringLiteral("workspace:Desk"));
        note.project.clear();
        note.belongs = Belongs::Project;
        QCOMPARE(note.placeKey(), QStringLiteral("loose"));
    }

    void documentNameFromTitle_data()
    {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("app");
        QTest::addColumn<QString>("document");
        QTest::newRow("kate") << QStringLiteral("SpreadGesture.qml — Kate") << QStringLiteral("Kate") << QStringLiteral("SpreadGesture.qml");
        QTest::newRow("unsaved") << QStringLiteral("SpreadGesture.qml * — Kate") << QStringLiteral("Kate") << QStringLiteral("SpreadGesture.qml");
        QTest::newRow("firefox") << QStringLiteral("Bug 412 · GitHub — Mozilla Firefox") << QStringLiteral("Firefox")
                                 << QStringLiteral("Bug 412 · GitHub");
        QTest::newRow("hyphen") << QStringLiteral("notes.txt - gedit") << QStringLiteral("gedit") << QStringLiteral("notes.txt");
        QTest::newRow("hyphen kept") << QStringLiteral("Re: plan - draft") << QStringLiteral("Mail") << QStringLiteral("Re: plan - draft");
        QTest::newRow("unknown app") << QStringLiteral("SpreadGesture.qml — Kate") << QString() << QStringLiteral("SpreadGesture.qml");
        QTest::newRow("dash in document") << QStringLiteral("Plan — draft 2 — Writer") << QStringLiteral("Writer") << QStringLiteral("Plan — draft 2");
        QTest::newRow("only app") << QString() << QStringLiteral("Konsole") << QStringLiteral("Konsole");
    }

    void documentNameFromTitle()
    {
        QFETCH(QString, title);
        QFETCH(QString, app);
        QFETCH(QString, document);
        QCOMPARE(documentName(title, app), document);
    }

    void colours()
    {
        QCOMPARE(colourNames(), (QStringList{QStringLiteral("butter"), QStringLiteral("rhyolite"), QStringLiteral("lake"),
                                             QStringLiteral("lichen"), QStringLiteral("stone")}));
        QCOMPARE(colourHex(QStringLiteral("lake")), QStringLiteral("#9ED6CB"));
        QCOMPARE(colourHex(QStringLiteral("plum")), QStringLiteral("#F2D98A"));
    }
};

QTEST_GUILESS_MAIN(NoteTest)
#include "tst_note.moc"
