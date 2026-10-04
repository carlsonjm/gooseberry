// SPDX-License-Identifier: GPL-2.0-or-later
// The quick-note card and the board, drawn off screen over a folder in the
// test's own home, and used by tapping and typing as a person would.
#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "TestHome.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QIcon>
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

public:
    FakeShell(NoteStore *store, Capture *capture, Places *places, PlaceNotes *notes)
        : m_capture(capture)
        , m_places(places)
        , m_notes(notes)
        , m_store(store)
    {
    }

    QString opened;
    QString newIn;

public Q_SLOTS:
    void openNote(const QString &id) { opened = id; }
    void newNoteIn(const QString &place) { newIn = place; }
    void showBoard() { }
    void showCapture() { }

private:
    QObject *m_capture;
    QObject *m_places;
    QObject *m_notes;
    QObject *m_store;
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

    QQuickItem *note() const { return m_view->rootObject(); }
    QQuickItem *named(const QString &name) const { return itemNamed(note(), name); }

private Q_SLOTS:
    void init()
    {
        // A mistake in a screen shows only as a warning; here it fails the test.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("ReferenceError|TypeError|Unable to assign|is not a type|Cannot read property")));
        static int round = 0;
        const QString folder = m_home->path() + QStringLiteral("/screens-%1").arg(++round);
        QVERIFY(m_home->holds(folder));
        m_store = std::make_unique<NoteStore>(folder);
        QVERIFY(m_store->open());
        m_capture = std::make_unique<Capture>(m_store.get());
        m_capture->startNew({QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")});

        m_view = std::make_unique<QQuickView>();
        KLocalization::setupLocalizedContext(m_view->engine());
        m_view->setInitialProperties({{QStringLiteral("capture"), QVariant::fromValue<QObject *>(m_capture.get())},
                                      {QStringLiteral("projects"), QStringList{QStringLiteral("Shuffle"), QStringLiteral("Home")}}});
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
    // note opened there, Back to the note or Esc brings the note back, and a
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

        // Back to the note, then Esc twice.
        tap(window, itemNamed(card, QStringLiteral("allNotes")));
        QTRY_VERIFY(itemNamed(card, QStringLiteral("backToNote"))->isVisible());
        tap(window, itemNamed(card, QStringLiteral("backToNote")));
        QVERIFY(!window->property("expanded").toBool());
        tap(window, itemNamed(card, QStringLiteral("allNotes")));
        QSignalSpy finished(m_capture.get(), &Capture::finished);
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!window->property("expanded").toBool());
        QCOMPARE(finished.count(), 0);
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(finished.count(), 1);

        // A tap on the card is the card's; a tap beside it puts it away.
        QMetaObject::invokeMethod(window, "open");
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(int(card->x()) + 300, int(card->y()) + 20));
        QCOMPARE(finished.count(), 1);
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(60, 60));
        QCOMPARE(finished.count(), 2);
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
        QMetaObject::invokeMethod(board, "present");
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
};

int main(int argc, char *argv[])
{
    TestHome home;
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("gooseberry");
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
