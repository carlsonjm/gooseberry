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
        QCOMPARE(readFile(m_folder + QStringLiteral("/.gooseberry")), QByteArray("format: 1\n"));
        QVERIFY(noteFiles(m_folder).isEmpty());
    }

    void keptFromTheFirstLetter()
    {
        Capture capture(m_store.get());
        capture.startNew(kate());
        QVERIFY(!capture.kept());
        QCOMPARE(capture.belongs(), QStringLiteral("window"));

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
        QCOMPARE(onDisk.belongs, Belongs::Window);
        QCOMPARE(onDisk.window, QStringLiteral("SpreadGesture.qml"));
        QCOMPARE(onDisk.app, QStringLiteral("org.kde.kate"));
        QCOMPARE(onDisk.workspace, QStringLiteral("Desk"));
        QVERIFY(files.first().startsWith(QDate::currentDate().toString(Qt::ISODate)));

        // Every later letter is on disk when the call that typed it returns.
        const QString words = QStringLiteral("Flick threshold");
        for (int i = 2; i <= words.size(); ++i) {
            capture.setText(words.left(i));
            QCOMPARE(Note::parse(readFile(m_store->pathFor(capture.noteId())), {}, {}).text, words.left(i));
        }
        QCOMPARE(noteFiles(m_folder).size(), 1);
    }

    void blankIsNotANote()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        QCOMPARE(capture.belongs(), QStringLiteral("loose"));
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

    void belongingIsOneTap()
    {
        Capture capture(m_store.get());
        capture.startNew(kate());
        capture.setText(QStringLiteral("Ask about a mouse way"));
        const QString path = m_store->pathFor(capture.noteId());

        capture.setBelongs(QStringLiteral("project"), QStringLiteral("  Shuffle "));
        Note note = Note::parse(readFile(path), {}, {});
        QCOMPARE(note.belongs, Belongs::Project);
        QCOMPARE(note.project, QStringLiteral("Shuffle"));
        // Where it was written stays recorded.
        QCOMPARE(note.window, QStringLiteral("SpreadGesture.qml"));

        capture.setBelongs(QStringLiteral("workspace"));
        QCOMPARE(Note::parse(readFile(path), {}, {}).belongs, Belongs::Workspace);
        capture.setBelongs(QStringLiteral("loose"));
        QCOMPARE(Note::parse(readFile(path), {}, {}).belongs, Belongs::Loose);
        capture.setBelongs(QStringLiteral("window"));
        QCOMPARE(Note::parse(readFile(path), {}, {}).belongs, Belongs::Window);

        // A project needs a name; an empty one changes nothing.
        capture.setBelongs(QStringLiteral("project"), QStringLiteral("  "));
        QCOMPARE(capture.belongs(), QStringLiteral("window"));
    }

    void windowChoiceNeedsAWindow()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setBelongs(QStringLiteral("window"));
        QCOMPARE(capture.belongs(), QStringLiteral("loose"));
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

    void boardGathersWithoutFiling()
    {
        auto add = [this](const QString &text, Belongs belongs, const QString &name, int daysAgo = 0) {
            Note note;
            note.text = text;
            note.belongs = belongs;
            note.window = belongs == Belongs::Window ? name : QString();
            note.project = belongs == Belongs::Project ? name : QString();
            note.workspace = belongs == Belongs::Workspace ? name : QStringLiteral("Desk");
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
        add(QStringLiteral("Groceries"), Belongs::Loose, {});
        add(QStringLiteral("Wallpaper"), Belongs::Loose, {}, 3);
        add(QStringLiteral("Flick threshold"), Belongs::Window, QStringLiteral("SpreadGesture.qml"));
        add(QStringLiteral("Notes on a corner"), Belongs::Project, QStringLiteral("Shuffle"), 2);
        add(QStringLiteral("README for Notes"), Belongs::Project, QStringLiteral("Split Rock"));
        add(QStringLiteral("Cabin weekend"), Belongs::Project, QStringLiteral("Home"));
        add(QStringLiteral("Desk tidy"), Belongs::Workspace, QStringLiteral("Desk"));
        m_store->rescan();

        Places places(m_store.get());
        QCOMPARE(places.countFor(QStringLiteral("loose")), 2);
        QCOMPARE(places.countFor(QStringLiteral("today")), 5);
        QCOMPARE(places.countFor(QStringLiteral("tucked")), 0);
        QCOMPARE(places.countFor(QStringLiteral("window:SpreadGesture.qml")), 1);
        QCOMPARE(places.countFor(QStringLiteral("project:Shuffle")), 1);
        QCOMPARE(places.countFor(QStringLiteral("workspace:Desk")), 1);

        QStringList keys;
        QStringList sections;
        for (int row = 0; row < places.rowCount(); ++row) {
            keys.append(places.index(row).data(Places::KeyRole).toString());
            sections.append(places.index(row).data(Places::SectionRole).toString());
        }
        QCOMPARE(keys, (QStringList{QStringLiteral("loose"), QStringLiteral("today"), QStringLiteral("tucked"),
                                    QStringLiteral("window:SpreadGesture.qml"), QStringLiteral("project:Home"),
                                    QStringLiteral("project:Shuffle"), QStringLiteral("project:Split Rock"),
                                    QStringLiteral("workspace:Desk")}));
        QCOMPARE(sections.mid(3), (QStringList{QStringLiteral("windows"), QStringLiteral("projects"), QStringLiteral("projects"),
                                           QStringLiteral("projects"), QStringLiteral("workspaces")}));
        // The project chip offers the most recently used first.
        QCOMPARE(places.projects().last(), QStringLiteral("Shuffle"));

        PlaceNotes board(m_store.get());
        QCOMPARE(board.place(), QStringLiteral("today"));
        QCOMPARE(board.count(), 5);
        board.setPlace(QStringLiteral("loose"));
        QCOMPARE(board.count(), 2);
        // The most recently changed first.
        QCOMPARE(board.index(0).data(PlaceNotes::TextRole).toString(), QStringLiteral("Groceries"));
        board.setPlace(QStringLiteral("project:Split Rock"));
        QCOMPARE(board.count(), 1);

        // Search looks through every note, whatever place is chosen.
        board.setSearch(QStringLiteral("WALLPAPER"));
        QCOMPARE(board.count(), 1);
        board.setSearch(QStringLiteral("home"));
        QCOMPARE(board.count(), 1);
        board.setSearch(QString());
        QCOMPARE(board.count(), 1);
    }

    void boardFollowsTypingRowByRow()
    {
        PlaceNotes board(m_store.get());
        board.setPlace(QStringLiteral("loose"));
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
        QCOMPARE(board.index(0).data(PlaceNotes::TextRole).toString(), QStringLiteral("one more"));
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
        QCOMPARE(m_store->note(QStringLiteral("from-elsewhere"))->belongs, Belongs::Loose);

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
        writeFile(m_folder + QStringLiteral("/.gooseberry"), "format: 2\n");

        NoteStore later(m_folder);
        QVERIFY(later.open());
        QVERIFY(later.readOnly());
        QCOMPARE(later.note(id)->text, QStringLiteral("kept by version one"));
        Capture reader(&later);
        QVERIFY(reader.open(id));
        QVERIFY(reader.readOnly());
        reader.setText(QStringLiteral("changed"));
        QVERIFY(!later.trash(id).has_value());
        QCOMPARE(Note::parse(readFile(later.pathFor(id)), {}, {}).text, QStringLiteral("kept by version one"));
    }

    void newerNoteIsLeftAlone()
    {
        writeFile(m_folder + QStringLiteral("/future.md"), "---\ngooseberry: 2\n---\nfrom the future\n");
        m_store->rescan();
        Capture capture(m_store.get());
        QVERIFY(capture.open(QStringLiteral("future")));
        QVERIFY(capture.readOnly());
        capture.setText(QStringLiteral("changed"));
        QCOMPARE(readFile(m_folder + QStringLiteral("/future.md")), QByteArray("---\ngooseberry: 2\n---\nfrom the future\n"));
    }

    void otherFilesAreLeftAlone()
    {
        writeFile(m_folder + QStringLiteral("/photo.png"), "not a note");
        QDir(m_folder).mkdir(QStringLiteral("old"));
        writeFile(m_folder + QStringLiteral("/old/inside.md"), "a note in a subfolder");
        m_store->rescan();
        QVERIFY(m_store->notes().isEmpty());
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
