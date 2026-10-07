// SPDX-License-Identifier: GPL-2.0-or-later
#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "TestHome.h"

#include <QCoreApplication>
#include <QDirIterator>
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

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

QStringList noteFiles(const QString &folder)
{
    return QDir(folder).entryList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
}

} // namespace

class StoreTest : public QObject
{
    Q_OBJECT

public:
    explicit StoreTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;
    QString m_folder;
    std::unique_ptr<NoteStore> m_store;

    CaptureContext kate() const
    {
        return {QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")};
    }

private Q_SLOTS:
    void init()
    {
        static int round = 0;
        m_folder = m_home->path() + QStringLiteral("/notes-%1").arg(++round);
        QVERIFY(m_home->holds(m_folder));
        m_store = std::make_unique<NoteStore>(m_folder);
        QVERIFY2(m_store->open(), qPrintable(m_store->lastError()));
    }

    void cleanup()
    {
        m_store.reset();
    }

    void defaultFolderIsInDocuments()
    {
        // Under test the folder variable points into the test's home; without
        // it the folder is Gooseberry in the Documents folder.
        QCOMPARE(NoteStore::defaultFolder(), m_home->notesFolder());
        qunsetenv("GOOSEBERRY_FOLDER");
        const QString fallback = NoteStore::defaultFolder();
        qputenv("GOOSEBERRY_FOLDER", m_home->notesFolder().toLocal8Bit());
        QVERIFY(m_home->holds(fallback));
        QVERIFY(fallback.endsWith(QStringLiteral("/Gooseberry")));
    }

    void openMakesTheFolderAndItsMarker()
    {
        QVERIFY(QFileInfo(m_folder).isDir());
        QCOMPARE(readFile(m_folder + QStringLiteral("/.gooseberry")), QByteArray("format: 2\n"));
        QVERIFY(noteFiles(m_folder).isEmpty());
    }

    void keptFromTheFirstLetter()
    {
        Capture capture(m_store.get());
        capture.startNew(kate());
        QVERIFY(!capture.kept());
        QVERIFY(capture.stuck());
        QCOMPARE(capture.folder(), QString());

        // Choosing a colour first keeps nothing: there is no note yet.
        capture.setColour(QStringLiteral("lake"));
        QVERIFY(noteFiles(m_folder).isEmpty());

        capture.setText(QStringLiteral("F"));
        QVERIFY(capture.kept());
        const QStringList files = noteFiles(m_folder);
        QCOMPARE(files.size(), 1);
        const Note onDisk = Note::parse(readFile(m_folder + QLatin1Char('/') + files.first()), capture.noteId(), {});
        QCOMPARE(onDisk.text, QStringLiteral("F"));
        QCOMPARE(onDisk.colour, QStringLiteral("lake"));
        QVERIFY(onDisk.isStuck());
        QCOMPARE(onDisk.window, QStringLiteral("SpreadGesture.qml"));
        QCOMPARE(onDisk.app, QStringLiteral("org.kde.kate"));
        QCOMPARE(onDisk.workspace, QStringLiteral("Desk"));
        QVERIFY(files.first().startsWith(QDate::currentDate().toString(Qt::ISODate)));

        // Later letters wait for the writing to pause: typed in a run, they
        // are written once, not once per letter.
        const auto textOnDisk = [&] {
            return Note::parse(readFile(m_store->pathFor(capture.noteId())), {}, {}).text;
        };
        const QString words = QStringLiteral("Flick threshold");
        for (int i = 2; i <= words.size(); ++i) {
            capture.setText(words.left(i));
        }
        QVERIFY(capture.waiting());
        QCOMPARE(textOnDisk(), QStringLiteral("F"));
        QTRY_COMPARE_WITH_TIMEOUT(textOnDisk(), words, Capture::PauseMs * 4);
        QVERIFY(!capture.waiting());
        QCOMPARE(noteFiles(m_folder).size(), 1);

        // Done writes what is waiting at once.
        capture.setText(words + QStringLiteral(" now"));
        capture.finish();
        QCOMPARE(textOnDisk(), words + QStringLiteral(" now"));
    }

    void longTypingIsWrittenAsItGoes()
    {
        Capture capture(m_store.get());
        capture.setWaits(200, 600);
        capture.startNew({});
        const QString words = QStringLiteral("Flick threshold feels short");
        QString lastWritten;
        for (int i = 1; i <= words.size(); ++i) {
            capture.setText(words.left(i));
            QTest::qWait(40); // Faster than the pause: the writing never rests.
            lastWritten = Note::parse(readFile(m_store->pathFor(capture.noteId())), {}, {}).text;
        }
        // Written on the way, not only at the first letter.
        QVERIFY2(lastWritten.size() > 1 && words.startsWith(lastWritten), qPrintable(lastWritten));
    }

    void anyOtherChangeWritesTheTypingAtOnce()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("F"));
        capture.setText(QStringLiteral("Flick"));
        capture.setColour(QStringLiteral("lake"));
        const Note onDisk = Note::parse(readFile(m_store->pathFor(capture.noteId())), {}, {});
        QCOMPARE(onDisk.text, QStringLiteral("Flick"));
        QCOMPARE(onDisk.colour, QStringLiteral("lake"));
        QVERIFY(!capture.waiting());
    }

    void removingWhileTypingWaitsLeavesNothing()
    {
        Capture capture(m_store.get());
        capture.setWaits(100, 600);
        capture.startNew({});
        capture.setText(QStringLiteral("F"));
        capture.setText(QStringLiteral("Flick"));
        capture.remove();
        QTest::qWait(300); // Past the pause, when waiting typing would be written.
        QVERIFY(noteFiles(m_folder).isEmpty());
    }

    void blankIsNotANote()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        QVERIFY(!capture.stuck());
        QCOMPARE(capture.folderLabel(), QStringLiteral("Inbox"));
        capture.setText(QStringLiteral("   \n"));
        QVERIFY(!capture.kept());
        capture.finish();
        QVERIFY(noteFiles(m_folder).isEmpty());
    }

    void emptiedNoteGoesToTheTrash()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("x"));
        const QString id = capture.noteId();
        capture.setText(QString());
        QVERIFY(capture.kept());
        capture.finish();
        QVERIFY(noteFiles(m_folder).isEmpty());
        QVERIFY(QFile::exists(m_home->trash() + QStringLiteral("/files/") + id + QStringLiteral(".md")));
    }

    void emptiedNoteIsNotLeftBehindByANewOne()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("x"));
        capture.setText(QString());
        // The button is tapped again rather than the sheet finished.
        capture.startNew({});
        QVERIFY(noteFiles(m_folder).isEmpty());
    }

    void folderAndStuckAreOneTapEach()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Shuffle")));
        Capture capture(m_store.get());
        capture.startNew(kate());
        capture.setText(QStringLiteral("Ask about a mouse way"));
        const QString id = capture.noteId();
        QVERIFY(QFile::exists(m_folder + QLatin1Char('/') + id + QStringLiteral(".md")));

        // Kept in a folder: the file moves into it, and stays stuck.
        capture.setFolder(QStringLiteral("Shuffle"));
        const QString inFolder = m_folder + QStringLiteral("/Shuffle/") + id + QStringLiteral(".md");
        QVERIFY(QFile::exists(inFolder));
        QVERIFY(!QFile::exists(m_folder + QLatin1Char('/') + id + QStringLiteral(".md")));
        QCOMPARE(m_store->pathFor(id), inFolder);
        Note note = Note::parse(readFile(inFolder), {}, {});
        QVERIFY(note.isStuck());
        QCOMPARE(note.window, QStringLiteral("SpreadGesture.qml"));

        // Unstuck, it keeps its folder and remembers the window.
        capture.setStuck({});
        note = Note::parse(readFile(inFolder), {}, {});
        QVERIFY(!note.stuck);
        QCOMPARE(note.window, QStringLiteral("SpreadGesture.qml"));
        QCOMPARE(m_store->note(id)->folder, QStringLiteral("Shuffle"));

        // Stuck to another open window.
        capture.setStuck(QStringLiteral("Price card"), QStringLiteral("org.mozilla.firefox"));
        note = Note::parse(readFile(inFolder), {}, {});
        QVERIFY(note.isStuck());
        QCOMPARE(note.window, QStringLiteral("Price card"));
        QCOMPARE(note.app, QStringLiteral("org.mozilla.firefox"));

        // A folder that is not there changes nothing; Inbox brings it back.
        capture.setFolder(QStringLiteral("Nowhere"));
        QCOMPARE(capture.folder(), QStringLiteral("Shuffle"));
        capture.setFolder({});
        QVERIFY(QFile::exists(m_folder + QLatin1Char('/') + id + QStringLiteral(".md")));
        QVERIFY(m_store->note(id)->isStuck());

        // A new folder is made from the card and the note goes into it.
        QCOMPARE(capture.makeFolder(QStringLiteral("  Tablet  ")), QString());
        QCOMPARE(capture.folder(), QStringLiteral("Tablet"));
        QVERIFY(QFile::exists(m_folder + QStringLiteral("/Tablet/") + id + QStringLiteral(".md")));
        QVERIFY(!capture.makeFolder(QStringLiteral("inbox")).isEmpty());
    }

    void folderChosenFirstWaitsForTheFirstLetter()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Groceries")));
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setFolder(QStringLiteral("Groceries"));
        QVERIFY(QDir(m_folder + QStringLiteral("/Groceries")).isEmpty());
        capture.setText(QStringLiteral("oats"));
        QVERIFY(QFile::exists(m_folder + QStringLiteral("/Groceries/") + capture.noteId() + QStringLiteral(".md")));
    }

    void windowChoiceNeedsAWindow()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setStuck({});
        QVERIFY(!capture.stuck());
    }

    void foldersAreKeptUntilRemoved()
    {
        QSignalSpy folders(m_store.get(), &NoteStore::foldersChanged);
        QVERIFY(m_store->makeFolder(QStringLiteral("Shuffle launch")));
        QCOMPARE(folders.count(), 1);
        QVERIFY(QFileInfo(m_folder + QStringLiteral("/Shuffle launch")).isDir());
        QVERIFY(!m_store->makeFolder(QStringLiteral("Shuffle launch")));
        QVERIFY(!m_store->makeFolder(QStringLiteral("a/b")));
        QVERIFY(!m_store->makeFolder(QStringLiteral(".hidden")));
        QVERIFY(!m_store->makeFolder(QStringLiteral("INBOX")));
        QVERIFY(!m_store->makeFolder(QStringLiteral("   ")));

        // Empty, it is still a place on the board, read again from disk.
        NoteStore again(m_folder);
        QVERIFY(again.open());
        QCOMPARE(again.folders(), QStringList{QStringLiteral("Shuffle launch")});
        Places places(m_store.get());
        QStringList keys;
        for (int row = 0; row < places.rowCount(); ++row) {
            keys.append(places.index(row).data(Places::KeyRole).toString());
        }
        QVERIFY(keys.contains(QStringLiteral("folder:Shuffle launch")));
        QCOMPARE(places.countFor(QStringLiteral("folder:Shuffle launch")), 0);

        // A folder made in a file manager is seen; a hidden one is not.
        QDir(m_folder).mkdir(QStringLiteral("Groceries"));
        QDir(m_folder).mkdir(QStringLiteral(".stfolder"));
        QTRY_COMPARE(m_store->folders(), (QStringList{QStringLiteral("Groceries"), QStringLiteral("Shuffle launch")}));
    }

    void renamingAFolderKeepsItsNotes()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Launch")));
        Note note;
        note.text = QStringLiteral("Refund wording");
        note.folder = QStringLiteral("Launch");
        const QString id = m_store->create(note);
        const QDateTime changed = m_store->note(id)->changed;
        QVERIFY(m_store->setWorkspaceFolder(QStringLiteral("Desk"), QStringLiteral("Launch")));

        QVERIFY(!m_store->renameFolder(QStringLiteral("Launch"), QStringLiteral("Inbox")));
        QVERIFY(m_store->renameFolder(QStringLiteral("Launch"), QStringLiteral("Shuffle launch")));
        QCOMPARE(m_store->folders(), QStringList{QStringLiteral("Shuffle launch")});
        QCOMPARE(m_store->note(id)->folder, QStringLiteral("Shuffle launch"));
        QCOMPARE(m_store->note(id)->changed, changed);
        QVERIFY(QFile::exists(m_folder + QStringLiteral("/Shuffle launch/") + id + QStringLiteral(".md")));
        QCOMPARE(m_store->workspaceFolder(QStringLiteral("Desk")), QStringLiteral("Shuffle launch"));
    }

    void removingAFolderSendsItsNotesToInbox()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Tablet")));
        Note note;
        note.text = QStringLiteral("Battery baseline");
        note.folder = QStringLiteral("Tablet");
        note.stuck = true;
        note.window = QStringLiteral("Konsole");
        const QString id = m_store->create(note);
        writeFile(m_folder + QStringLiteral("/Tablet/") + id + QStringLiteral(".svg"), "<svg/>");
        writeFile(m_folder + QStringLiteral("/Tablet/photo.png"), "not a note");
        QVERIFY(m_store->setWorkspaceFolder(QStringLiteral("Desk"), QStringLiteral("Tablet")));

        Places places(m_store.get());
        const QVariantMap removed = places.removeFolder(QStringLiteral("Tablet"));
        QCOMPARE(removed.value(QStringLiteral("ids")).toStringList(), QStringList{id});
        QVERIFY(m_store->folders().isEmpty());
        // The note, and its ink, are in Inbox, still stuck; the rest of the
        // folder is in the trash, not deleted.
        QVERIFY(QFile::exists(m_folder + QLatin1Char('/') + id + QStringLiteral(".md")));
        QVERIFY(QFile::exists(m_folder + QLatin1Char('/') + id + QStringLiteral(".svg")));
        QVERIFY(m_store->note(id)->isStuck());
        QVERIFY(!QFileInfo::exists(m_folder + QStringLiteral("/Tablet")));
        QVERIFY(QFile::exists(m_home->trash() + QStringLiteral("/files/Tablet/photo.png")));
        QCOMPARE(m_store->workspaceFolder(QStringLiteral("Desk")), QString());

        // Undo makes the folder again and moves its notes back.
        QVERIFY(places.undoRemoveFolder(removed));
        QCOMPARE(m_store->note(id)->folder, QStringLiteral("Tablet"));
        QCOMPARE(m_store->workspaceFolder(QStringLiteral("Desk")), QStringLiteral("Tablet"));

        // An empty folder simply goes.
        QVERIFY(m_store->makeFolder(QStringLiteral("Empty")));
        QVERIFY(m_store->removeFolder(QStringLiteral("Empty")).has_value());
        QVERIFY(!QFileInfo::exists(m_folder + QStringLiteral("/Empty")));
    }

    void aWorkspaceGivesNewNotesItsFolder()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Shuffle launch")));
        QVERIFY(m_store->setWorkspaceFolder(QStringLiteral("Desk"), QStringLiteral("Shuffle launch")));
        QVERIFY(!m_store->setWorkspaceFolder(QStringLiteral("Desk"), QStringLiteral("Nowhere")));
        QVERIFY(readFile(m_folder + QStringLiteral("/.workspaces")).contains("\"Desk\": \"Shuffle launch\""));

        Capture capture(m_store.get());
        capture.startNew(kate());
        QCOMPARE(capture.folder(), QStringLiteral("Shuffle launch"));
        capture.setText(QStringLiteral("Price card says $30"));
        QVERIFY(QFile::exists(m_folder + QStringLiteral("/Shuffle launch/") + capture.noteId() + QStringLiteral(".md")));

        // Another workspace uses Inbox.
        capture.startNew({QString(), QString(), QStringLiteral("Couch")});
        QCOMPARE(capture.folder(), QString());

        // Read again from disk.
        NoteStore again(m_folder);
        QVERIFY(again.open());
        QCOMPARE(again.workspaceFolder(QStringLiteral("Desk")), QStringLiteral("Shuffle launch"));
        QVERIFY(m_store->setWorkspaceFolder(QStringLiteral("Desk"), {}));
        QCOMPARE(m_store->workspaceFolder(QStringLiteral("Desk")), QString());
    }

    void aMoveOnTheBoardIsNotUndoneByTheCard()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Home")));
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("Cabin weekend"));
        const QString id = capture.noteId();
        capture.setText(QStringLiteral("Cabin weekend, bring"));
        QVERIFY(capture.waiting());

        // Dropped on a folder while typing waits: the typing is written in
        // the folder, never back in Inbox.
        Places places(m_store.get());
        QVERIFY(places.moveNote(id, QStringLiteral("folder:Home")));
        capture.flush();
        QTRY_COMPARE(capture.folder(), QStringLiteral("Home"));
        QVERIFY(!QFile::exists(m_folder + QLatin1Char('/') + id + QStringLiteral(".md")));
        QCOMPARE(Note::parse(readFile(m_folder + QStringLiteral("/Home/") + id + QStringLiteral(".md")), {}, {}).text,
                 QStringLiteral("Cabin weekend, bring"));
        QVERIFY(places.moveNote(id, QStringLiteral("inbox")));
        QVERIFY(!places.moveNote(id, QStringLiteral("folder:Nowhere")));
    }

    void firstLayoutIsBroughtUpToDate()
    {
        const QString old = m_home->path() + QStringLiteral("/first-layout");
        QVERIFY(QDir().mkpath(old));
        writeFile(old + QStringLiteral("/.gooseberry"), "format: 1\n");
        const QByteArray header = "---\ngooseberry: 1\ncreated: 2026-10-04T14:41:12-05:00\nchanged: 2026-10-04T14:43:05-05:00\n"
                                  "colour: lake\n";
        writeFile(old + QStringLiteral("/a.md"), header + "belongs: project\nproject: \"Shuffle\"\ntucked: false\n---\nRefund wording\n");
        writeFile(old + QStringLiteral("/a.svg"), "<svg/>");
        writeFile(old + QStringLiteral("/b.md"), header + "belongs: window\nwindow: \"Bug 412\"\napp: \"org.kde.kate\"\n"
                                                 "project: \"Shuffle\"\ntucked: false\n---\nRepro first\n");
        writeFile(old + QStringLiteral("/c.md"), header + "belongs: project\nproject: \"Notes/Search\"\ntucked: false\n---\nSlash\n");
        writeFile(old + QStringLiteral("/d.md"), header + "belongs: workspace\nworkspace: \"Desk\"\ntucked: false\n---\nDesk tidy\n");

        NoteStore store(old);
        QVERIFY(store.open());
        QCOMPARE(readFile(old + QStringLiteral("/.gooseberry")), QByteArray("format: 2\n"));
        QCOMPARE(store.folders(), (QStringList{QStringLiteral("Notes-Search"), QStringLiteral("Shuffle")}));
        // A project's notes are in its folder, with their ink, and their
        // changed time is the person's, not the update's.
        QVERIFY(QFile::exists(old + QStringLiteral("/Shuffle/a.md")));
        QVERIFY(QFile::exists(old + QStringLiteral("/Shuffle/a.svg")));
        QCOMPARE(store.note(QStringLiteral("a"))->folder, QStringLiteral("Shuffle"));
        QCOMPARE(store.note(QStringLiteral("a"))->changed, QDateTime::fromString(QStringLiteral("2026-10-04T14:43:05-05:00"), Qt::ISODate));
        QVERIFY(readFile(old + QStringLiteral("/Shuffle/a.md")).contains("gooseberry: 2\n"));
        QVERIFY(QFile::exists(old + QStringLiteral("/Notes-Search/c.md")));
        // A window's note is stuck to it and stays in Inbox; so does the
        // workspace's.
        QVERIFY(QFile::exists(old + QStringLiteral("/b.md")));
        QVERIFY(store.note(QStringLiteral("b"))->isStuck());
        QVERIFY(QFile::exists(old + QStringLiteral("/d.md")));
        QCOMPARE(store.note(QStringLiteral("d"))->folder, QString());
    }

    void restoredNoteGoesBackToItsFolder()
    {
        QVERIFY(m_store->makeFolder(QStringLiteral("Home")));
        Note note;
        note.text = QStringLiteral("Cabin");
        note.folder = QStringLiteral("Home");
        const QString id = m_store->create(note);
        PlaceNotes board(m_store.get());
        const QString inTrash = board.remove(id);
        QVERIFY(!inTrash.isEmpty());
        QVERIFY(!m_store->note(id));
        QVERIFY(board.restore(id, inTrash));
        QCOMPARE(m_store->note(id)->folder, QStringLiteral("Home"));
    }

    void droppedOnADayIsPlanned()
    {
        Note note;
        note.text = QStringLiteral("Call about the laptop");
        const QString id = m_store->create(note);
        PlaceNotes board(m_store.get());
        const QDate tomorrow = QDate::currentDate().addDays(1);
        QVERIFY(!board.remindOf(id).isValid());
        QVERIFY(board.planOn(id, tomorrow));
        QCOMPARE(m_store->note(id)->remind, QDateTime(tomorrow, QTime(9, 0)));
        // Another day keeps the time of day.
        QVERIFY(board.setRemind(id, QDateTime(tomorrow, QTime(14, 30))));
        QVERIFY(board.planOn(id, tomorrow.addDays(1)));
        QCOMPARE(m_store->note(id)->remind, QDateTime(tomorrow.addDays(1), QTime(14, 30)));
        // Undo takes it off the planner.
        QVERIFY(board.setRemind(id, {}));
        QVERIFY(!m_store->note(id)->hasReminder());
    }

    void removeSendsToTheTrashAndUndoBringsBack()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("Wallpaper idea"));
        const QString id = capture.noteId();
        writeFile(m_store->inkPathFor(id), "<svg/>");

        QSignalSpy removed(m_store.get(), &NoteStore::noteRemoved);
        capture.remove();
        QCOMPARE(removed.count(), 1);
        QVERIFY(!QFile::exists(m_store->pathFor(id)));
        QVERIFY(!QFile::exists(m_store->inkPathFor(id)));

        // Into the desktop's trash, with the record the trash keeps, never
        // deleted outright.
        const QString trash = m_home->trash();
        QVERIFY(m_home->holds(trash));
        QCOMPARE(Note::parse(readFile(trash + QStringLiteral("/files/") + id + QStringLiteral(".md")), {}, {}).text,
                 QStringLiteral("Wallpaper idea"));
        QVERIFY(QFile::exists(trash + QStringLiteral("/files/") + id + QStringLiteral(".svg")));
        const QByteArray info = readFile(trash + QStringLiteral("/info/") + id + QStringLiteral(".md.trashinfo"));
        QVERIFY(info.contains("Path="));

        QVERIFY(capture.undoRemove());
        QVERIFY(m_store->note(id).has_value());
        QCOMPARE(m_store->note(id)->text, QStringLiteral("Wallpaper idea"));
        QVERIFY(!QFile::exists(trash + QStringLiteral("/info/") + id + QStringLiteral(".md.trashinfo")));
    }

    void tuckAwayAndBringBack()
    {
        Capture capture(m_store.get());
        capture.startNew(kate());
        capture.setText(QStringLiteral("What if Bento remembered notes per pane?"));
        const QString id = capture.noteId();

        Places places(m_store.get());
        PlaceNotes board(m_store.get());
        QCoreApplication::processEvents();
        QCOMPARE(places.countFor(QStringLiteral("window:SpreadGesture.qml")), 1);
        QCOMPARE(places.countFor(QStringLiteral("tucked")), 0);

        capture.tuckAway();
        QVERIFY(Note::parse(readFile(m_store->pathFor(id)), {}, {}).tucked);
        QCoreApplication::processEvents();
        QCOMPARE(places.countFor(QStringLiteral("tucked")), 1);
        // Gone from where it was and from Today: it waits only under Tucked away.
        QCOMPARE(places.countFor(QStringLiteral("window:SpreadGesture.qml")), 0);
        QCOMPARE(places.countFor(QStringLiteral("today")), 0);
        board.setPlace(QStringLiteral("tucked"));
        QCOMPARE(board.count(), 1);

        QVERIFY(board.bringBack(id));
        QVERIFY(!Note::parse(readFile(m_store->pathFor(id)), {}, {}).tucked);
        QCOMPARE(board.count(), 0);
        QCoreApplication::processEvents();
        QCOMPARE(places.countFor(QStringLiteral("window:SpreadGesture.qml")), 1);

        QVERIFY(board.tuckAway(id));
        QCOMPARE(board.count(), 1);
    }

    void boardGathersFoldersAndWindows()
    {
        for (const auto &name : {QStringLiteral("Shuffle"), QStringLiteral("Split Rock"), QStringLiteral("Home"), QStringLiteral("Empty")}) {
            QVERIFY(m_store->makeFolder(name));
        }
        auto add = [this](const QString &text, const QString &folder, const QString &window, int daysAgo = 0) {
            Note note;
            note.text = text;
            note.folder = folder;
            note.window = window;
            note.stuck = !window.isEmpty();
            note.workspace = QStringLiteral("Desk");
            note.created = QDateTime::currentDateTime().addDays(-daysAgo);
            const QString id = m_store->create(note);
            QVERIFY(!id.isEmpty());
            if (daysAgo > 0) {
                // An old note: its changed time is rewritten as on the day it was written.
                Note old = *m_store->note(id);
                old.changed = old.created;
                QFile file(m_store->pathFor(id));
                QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
                file.write(old.serialize());
            }
        };
        add(QStringLiteral("Groceries"), {}, {});
        add(QStringLiteral("Wallpaper"), {}, {}, 3);
        add(QStringLiteral("Flick threshold"), {}, QStringLiteral("SpreadGesture.qml"));
        add(QStringLiteral("Notes on a corner"), QStringLiteral("Shuffle"), QStringLiteral("SpreadGesture.qml"), 2);
        add(QStringLiteral("README for Notes"), QStringLiteral("Split Rock"), {});
        add(QStringLiteral("Cabin weekend"), QStringLiteral("Home"), {});
        m_store->rescan();

        Places places(m_store.get());
        QCOMPARE(places.countFor(QStringLiteral("inbox")), 3);
        QCOMPARE(places.countFor(QStringLiteral("today")), 4);
        QCOMPARE(places.countFor(QStringLiteral("tucked")), 0);
        QCOMPARE(places.countFor(QStringLiteral("window:SpreadGesture.qml")), 2);
        QCOMPARE(places.countFor(QStringLiteral("folder:Shuffle")), 1);
        QCOMPARE(places.countFor(QStringLiteral("folder:Empty")), 0);

        QStringList keys;
        QStringList sections;
        for (int row = 0; row < places.rowCount(); ++row) {
            keys.append(places.index(row).data(Places::KeyRole).toString());
            sections.append(places.index(row).data(Places::SectionRole).toString());
        }
        // Today; Inbox first and New folder last among the folders; the
        // windows with notes stuck to them; Tucked away.
        QCOMPARE(keys, (QStringList{QStringLiteral("today"), QStringLiteral("inbox"), QStringLiteral("folder:Empty"),
                                    QStringLiteral("folder:Home"), QStringLiteral("folder:Shuffle"),
                                    QStringLiteral("folder:Split Rock"), QStringLiteral("newfolder"),
                                    QStringLiteral("window:SpreadGesture.qml"), QStringLiteral("tucked")}));
        QCOMPARE(sections, (QStringList{QString(), QStringLiteral("folders"), QStringLiteral("folders"),
                                        QStringLiteral("folders"), QStringLiteral("folders"), QStringLiteral("folders"),
                                        QStringLiteral("folders"), QStringLiteral("windows"), QStringLiteral("end")}));
        QCOMPARE(places.folders().size(), 4);

        PlaceNotes board(m_store.get());
        QCOMPARE(board.place(), QStringLiteral("today"));
        QCOMPARE(board.count(), 4);
        board.setPlace(QStringLiteral("inbox"));
        QCOMPARE(board.count(), 3);
        // The most recently changed first.
        QCOMPARE(board.index(2).data(PlaceNotes::TextRole).toString(), QStringLiteral("Wallpaper"));
        QStringList stuckTo;
        for (int row = 0; row < board.count(); ++row) {
            stuckTo.append(board.index(row).data(PlaceNotes::StuckToRole).toString());
        }
        QVERIFY(stuckTo.contains(QStringLiteral("SpreadGesture.qml")));
        board.setPlace(QStringLiteral("folder:Split Rock"));
        QCOMPARE(board.count(), 1);
        board.setPlace(QStringLiteral("window:SpreadGesture.qml"));
        QCOMPARE(board.count(), 2);

        // Search looks through every note, whatever place is chosen, and
        // finds a note by its folder or window too.
        board.setSearch(QStringLiteral("WALLPAPER"));
        QCOMPARE(board.count(), 1);
        board.setSearch(QStringLiteral("home"));
        QCOMPARE(board.count(), 1);
        board.setSearch(QStringLiteral("spreadgesture"));
        QCOMPARE(board.count(), 2);
        board.setSearch(QString());
        QCOMPARE(board.count(), 2);

        // A window's place goes when its last note is unstuck.
        for (const Note &note : m_store->notes()) {
            if (note.isStuck()) {
                Note unstuck = note;
                unstuck.stuck = false;
                QVERIFY(m_store->save(unstuck));
            }
        }
        QTRY_COMPARE(places.countFor(QStringLiteral("window:SpreadGesture.qml")), 0);
        QCOMPARE(places.rowCount(), 8);
    }

    void boardFollowsTypingRowByRow()
    {
        PlaceNotes board(m_store.get());
        board.setPlace(QStringLiteral("inbox"));
        QSignalSpy reset(&board, &QAbstractItemModel::modelReset);
        QSignalSpy inserted(&board, &QAbstractItemModel::rowsInserted);
        QSignalSpy changed(&board, &QAbstractItemModel::dataChanged);

        Capture first(m_store.get());
        first.startNew({});
        first.setText(QStringLiteral("one"));
        Capture second(m_store.get());
        second.startNew({});
        second.setText(QStringLiteral("two"));
        QCOMPARE(board.count(), 2);
        QCOMPARE(inserted.count(), 2);

        QTest::qWait(1100); // Change times are kept to the second.
        first.setText(QStringLiteral("one more"));
        QTRY_COMPARE_WITH_TIMEOUT(board.index(0).data(PlaceNotes::TextRole).toString(), QStringLiteral("one more"),
                                  Capture::PauseMs * 4);
        QVERIFY(changed.count() >= 1);
        QCOMPARE(reset.count(), 0);
    }

    void changesFromOtherProgramsAreSeen()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("Typed here"));
        const QString id = capture.noteId();

        QSignalSpy added(m_store.get(), &NoteStore::noteAdded);
        QSignalSpy changed(m_store.get(), &NoteStore::noteChanged);
        QSignalSpy removed(m_store.get(), &NoteStore::noteRemoved);

        // A note dropped in by hand or by a sync tool, with no header.
        writeFile(m_folder + QStringLiteral("/from-elsewhere.md"), "Written in another editor\n");
        QTRY_COMPARE(added.count(), 1);
        QCOMPARE(m_store->note(QStringLiteral("from-elsewhere"))->folder, QString());
        QVERIFY(!m_store->note(QStringLiteral("from-elsewhere"))->isStuck());

        // A note moved into a folder in a file manager is seen there.
        QDir(m_folder).mkdir(QStringLiteral("Moved"));
        QVERIFY(QFile::rename(m_folder + QStringLiteral("/from-elsewhere.md"), m_folder + QStringLiteral("/Moved/from-elsewhere.md")));
        QTRY_COMPARE(m_store->note(QStringLiteral("from-elsewhere"))->folder, QStringLiteral("Moved"));

        // The open note edited elsewhere: the sheet shows the new text.
        Note edited = *m_store->note(id);
        edited.text = QStringLiteral("Edited elsewhere");
        writeFile(m_store->pathFor(id), edited.serialize());
        QTRY_COMPARE(capture.text(), QStringLiteral("Edited elsewhere"));

        // Removed elsewhere while open: the text stays on the sheet and the
        // next letter keeps it again.
        QFile::remove(m_store->pathFor(id));
        QTRY_COMPARE(removed.count(), 1);
        QVERIFY(!capture.kept());
        QCOMPARE(capture.text(), QStringLiteral("Edited elsewhere"));
        capture.setText(QStringLiteral("Edited elsewhere!"));
        QVERIFY(capture.kept());
        QVERIFY(QFile::exists(m_store->pathFor(capture.noteId())));

        // Gooseberry's own saves are not taken for changes from elsewhere.
        changed.clear();
        capture.setText(QStringLiteral("Edited elsewhere!!"));
        capture.flush();
        QTest::qWait(400);
        QCOMPARE(changed.count(), 1);
    }

    void missingFolderIsMadeAgain()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("before"));
        QVERIFY(QDir(m_folder).removeRecursively());
        capture.startNew({});
        capture.setText(QStringLiteral("after the folder went"));
        QVERIFY2(capture.kept(), qPrintable(capture.problem()));
        QCOMPARE(noteFiles(m_folder).size(), 1);
    }

    void unwritableFolderSaysSo()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        QFile::setPermissions(m_folder, QFile::ReadOwner | QFile::ExeOwner);
        capture.setText(QStringLiteral("cannot be kept"));
        const bool failed = !capture.kept();
        QFile::setPermissions(m_folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        if (!failed) {
            QSKIP("Running with permission to write anywhere");
        }
        QVERIFY(!capture.problem().isEmpty());
        // The text is held, and the next change keeps it.
        capture.setText(QStringLiteral("cannot be kept yet"));
        QVERIFY(capture.kept());
        QVERIFY(capture.problem().isEmpty());
    }

    void newerFolderIsLeftAlone()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("kept by version one"));
        const QString id = capture.noteId();
        writeFile(m_folder + QStringLiteral("/.gooseberry"), "format: 3\n");

        NoteStore later(m_folder);
        QVERIFY(later.open());
        QVERIFY(later.readOnly());
        QCOMPARE(later.note(id)->text, QStringLiteral("kept by version one"));
        Capture reader(&later);
        QVERIFY(reader.open(id));
        QVERIFY(reader.readOnly());
        reader.setText(QStringLiteral("changed"));
        QVERIFY(!later.trash(id).has_value());
        QVERIFY(!later.makeFolder(QStringLiteral("Anything")));
        QCOMPARE(Note::parse(readFile(later.pathFor(id)), {}, {}).text, QStringLiteral("kept by version one"));
    }

    void newerNoteIsLeftAlone()
    {
        writeFile(m_folder + QStringLiteral("/future.md"), "---\ngooseberry: 3\n---\nfrom the future\n");
        m_store->rescan();
        Capture capture(m_store.get());
        QVERIFY(capture.open(QStringLiteral("future")));
        QVERIFY(capture.readOnly());
        capture.setText(QStringLiteral("changed"));
        QCOMPARE(readFile(m_folder + QStringLiteral("/future.md")), QByteArray("---\ngooseberry: 3\n---\nfrom the future\n"));
    }

    void otherFilesAreLeftAlone()
    {
        writeFile(m_folder + QStringLiteral("/photo.png"), "not a note");
        QDir(m_folder).mkpath(QStringLiteral(".stversions/deep"));
        writeFile(m_folder + QStringLiteral("/.stversions/inside.md"), "a sync tool's copy");
        QDir(m_folder).mkpath(QStringLiteral("old/deeper"));
        writeFile(m_folder + QStringLiteral("/old/deeper/inside.md"), "two levels down");
        m_store->rescan();
        QVERIFY(m_store->notes().isEmpty());
        QCOMPARE(m_store->folders(), QStringList{QStringLiteral("old")});
    }
};

int main(int argc, char *argv[])
{
    TestHome home;
    QCoreApplication app(argc, argv);
    StoreTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_store.moc"
