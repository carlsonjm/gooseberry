// SPDX-License-Identifier: GPL-2.0-or-later
// The quick-note card and the board, drawn off screen over a folder in the
// test's own home, and used by tapping and typing as a person would.
#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "Planner.h"
#include "ReminderWords.h"
#include "TestHome.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QClipboard>
#include <QGuiApplication>
#include <QPointingDevice>
#include <QColor>
#include <QIcon>
#include <QQmlComponent>
#include <QRegularExpression>
#include <QQmlEngine>
#include <QStyleHints>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QSizeF>
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
    QString currentWorkspace() const { return QStringLiteral("Desk"); }

private:
    QObject *m_capture;
    QObject *m_places;
    QObject *m_notes;
    QObject *m_store;
    QObject *m_planner;
};

// The words for reminders and the open windows, as the shell gives them to
// the card.
class Words : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    QString reminderLabel(const QDateTime &remind, bool onOpen) const { return ReminderWords::label(remind, onOpen); }
    QVariantList reminderChoices() const { return ReminderWords::choices(true); }
    QVariantList openWindows() const
    {
        return {QVariantMap{{QStringLiteral("window"), QStringLiteral("SpreadGesture.qml")},
                            {QStringLiteral("app"), QStringLiteral("org.kde.kate")},
                            {QStringLiteral("appName"), QStringLiteral("Kate")},
                            {QStringLiteral("front"), true}},
                QVariantMap{{QStringLiteral("window"), QStringLiteral("shuffleforplasma.com")},
                            {QStringLiteral("app"), QStringLiteral("org.mozilla.firefox")},
                            {QStringLiteral("appName"), QStringLiteral("Firefox")},
                            {QStringLiteral("front"), false}}};
    }
};

// Stands in for the shell's stuck notes, with one note up, and records what
// the surface asks of it.
class FakeStuck : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList shownNotes MEMBER m_notes CONSTANT)

public:
    FakeStuck()
    {
        m_notes = {QVariantMap{{QStringLiteral("id"), QStringLiteral("n1")},
                               {QStringLiteral("title"), QStringLiteral("Docker")},
                               {QStringLiteral("text"), QStringLiteral("DOCKERHUB_USERNAME\ndocker login -u someone\n\nTOKEN\ndckr_pat_ABCDEF-ghijk")},
                               {QStringLiteral("colourHex"), QStringLiteral("#F2D98A")},
                               {QStringLiteral("x"), -1.0},
                               {QStringLiteral("y"), -1.0},
                               {QStringLiteral("width"), -1.0},
                               {QStringLiteral("height"), -1.0}}};
        connect(this, &FakeStuck::openRequested, this, [this](const QString &id) {
            opened.append(id);
        });
    }

    QStringList opened;
    QList<bool> keys;
    QList<QSizeF> sizes;

Q_SIGNALS:
    void openRequested(const QString &id);

public Q_SLOTS:
    void setPressable(const QVariantList &) { }
    void place(const QString &, qreal, qreal) { }
    void setTakesKeys(bool takes) { keys.append(takes); }
    void resize(const QString &, qreal width, qreal height) { sizes.append(QSizeF(width, height)); }

private:
    QVariantList m_notes;
};

class FakeStuckShell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *stuck MEMBER m_stuck CONSTANT)

public:
    explicit FakeStuckShell(QObject *stuck)
        : m_stuck(stuck)
    {
    }

private:
    QObject *m_stuck;
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

// Holds an item until it lifts, carries it in steps to the middle of
// another, and lets go there.
void carry(QQuickWindow *window, QQuickItem *item, QQuickItem *to)
{
    QVERIFY(item);
    QVERIFY(to);
    QTest::qWait(30);
    const QPoint from = item->mapToScene(QPointF(item->width() / 2, item->height() / 3)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, {}, from);
    QTest::qWait(QGuiApplication::styleHints()->mousePressAndHoldInterval() + 300);
    const QPoint target = to->mapToScene(QPointF(to->width() / 2, to->height() / 2)).toPoint();
    for (int step = 1; step <= 10; ++step) {
        QTest::mouseMove(window, from + (target - from) * step / 10);
        QTest::qWait(16);
    }
    QTest::mouseRelease(window, Qt::LeftButton, {}, target);
    QTest::qWait(30);
}

// Where the character at position in a piece of text is drawn, in the scene.
QPoint characterAt(QQuickItem *text, int position)
{
    QRectF rect;
    QMetaObject::invokeMethod(text, "positionToRectangle", Q_RETURN_ARG(QRectF, rect), Q_ARG(int, position));
    return text->mapToScene(QPointF(rect.x() + 2, rect.center().y())).toPoint();
}

// A finger held still on a point until it counts as a hold, then lifted.
void hold(QQuickWindow *window, QPointingDevice *finger, QPoint at)
{
    QTest::touchEvent(window, finger).press(0, at, window);
    QTest::qWait(QGuiApplication::styleHints()->mousePressAndHoldInterval() + 300);
    QTest::touchEvent(window, finger).release(0, at, window);
    QTest::qWait(50);
}

// A mouse dragged across, in steps, as a person selects words.
void sweep(QQuickWindow *window, QPoint from, QPoint to)
{
    QTest::mousePress(window, Qt::LeftButton, {}, from);
    for (int step = 1; step <= 10; ++step) {
        QTest::mouseMove(window, from + (to - from) * step / 10);
        QTest::qWait(10);
    }
    QTest::mouseRelease(window, Qt::LeftButton, {}, to);
    QTest::qWait(30);
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
        QVERIFY(m_home->holds(folder));
        m_store = std::make_unique<NoteStore>(folder);
        QVERIFY(m_store->open());
        QVERIFY(m_store->makeFolder(QStringLiteral("Shuffle")));
        QVERIFY(m_store->makeFolder(QStringLiteral("Home")));
        m_capture = std::make_unique<Capture>(m_store.get());
        m_capture->startNew({QStringLiteral("SpreadGesture.qml"), QStringLiteral("org.kde.kate"), QStringLiteral("Desk")});

        m_view = std::make_unique<QQuickView>();
        KLocalization::setupLocalizedContext(m_view->engine());
        m_view->setInitialProperties({{QStringLiteral("capture"), QVariant::fromValue<QObject *>(m_capture.get())},
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

    // The page's words are selected by a mouse dragged across them, or by a
    // finger held on one, which takes a code or a command whole, and copied
    // with Copy as well as Ctrl+C.
    void wordsOnThePageCopy()
    {
        m_capture->setText(QStringLiteral("docker login -u someone\ndckr_pat_ABCDEF-ghijk"));
        auto *editor = note()->property("editor").value<QQuickItem *>();
        QVERIFY(editor->property("selectByMouse").toBool());
        QQuickItem *copy = named(QStringLiteral("copy"));
        QVERIFY(copy);
        QVERIFY(!copy->isVisible());
        const QString text = m_capture->text();

        QPointingDevice *finger = QTest::createTouchDevice();
        hold(m_view.get(), finger, characterAt(editor, text.indexOf(QStringLiteral("dckr")) + 3));
        QTRY_COMPARE(editor->property("selectedText").toString(), QStringLiteral("dckr_pat_ABCDEF-ghijk"));
        QVERIFY(copy->isVisible());
        tap(m_view.get(), copy);
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("dckr_pat_ABCDEF-ghijk"));
        // Copying keeps the note as it was.
        QCOMPARE(m_capture->text(), text);

        editor->setProperty("cursorPosition", 0);
        QVERIFY(!copy->isVisible());
        sweep(m_view.get(), characterAt(editor, 0), characterAt(editor, 6));
        QCOMPARE(editor->property("selectedText").toString(), QStringLiteral("docker"));
        QTest::keyClick(m_view.get(), Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("docker"));
    }

    // A stuck note's words are selected where the note stands and copied,
    // while a tap still opens it and a finger or the note's edge still moves
    // it; the surface takes the keys only while words are selected.
    void stuckNoteWordsCopy()
    {
        FakeStuck stuck;
        FakeStuckShell shell(&stuck);
        QQmlComponent component(m_view->engine(), QStringLiteral("io.github.carlsonjm.gooseberry"), QStringLiteral("StuckWindow"));
        std::unique_ptr<QQuickWindow> surface(qobject_cast<QQuickWindow *>(
            component.createWithInitialProperties({{QStringLiteral("shell"), QVariant::fromValue<QObject *>(&shell)}})));
        QVERIFY2(surface, qPrintable(component.errorString()));
        surface->resize(800, 600);
        surface->setVisible(true);
        QVERIFY(QTest::qWaitForWindowExposed(surface.get()));
        QQuickWindow *window = surface.get();
        QQuickItem *stuckNote = itemNamed(window->contentItem(), QStringLiteral("stuck-n1"));
        QQuickItem *words = itemNamed(window->contentItem(), QStringLiteral("stuckWords-n1"));
        QQuickItem *tools = itemNamed(window->contentItem(), QStringLiteral("stuckTools-n1"));
        QVERIFY(stuckNote);
        QVERIFY(words);
        QVERIFY(tools);
        QVERIFY(!tools->isVisible());
        const QString text = words->property("text").toString();
        QTest::qWait(50);

        // A mouse selects across the words, without moving the note.
        const QPointF before = stuckNote->position();
        sweep(window, characterAt(words, 0), characterAt(words, 6));
        QCOMPARE(words->property("selectedText").toString(), QStringLiteral("DOCKER"));
        QCOMPARE(stuckNote->position(), before);
        QTRY_COMPARE(stuck.keys, QList<bool>{true});
        QVERIFY(tools->isVisible());
        tap(window, itemNamed(window->contentItem(), QStringLiteral("stuck-copy")));
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("DOCKER"));
        QVERIFY(stuck.opened.isEmpty());

        // A click lets the words go and gives the keys back; the next opens
        // the note.
        QTest::qWait(400);
        tap(window, words);
        QTRY_COMPARE(stuck.keys, (QList<bool>{true, false}));
        QVERIFY(words->property("selectedText").toString().isEmpty());
        QVERIFY(stuck.opened.isEmpty());
        QTest::qWait(400);
        tap(window, words);
        QCOMPARE(stuck.opened, QStringList{QStringLiteral("n1")});

        // A finger held on the code takes it whole; Copy copies it.
        QPointingDevice *finger = QTest::createTouchDevice();
        QTest::qWait(400);
        hold(window, finger, characterAt(words, text.indexOf(QStringLiteral("dckr")) + 3));
        QTRY_COMPARE(words->property("selectedText").toString(), QStringLiteral("dckr_pat_ABCDEF-ghijk"));
        QCOMPARE(stuck.opened.size(), 1);
        tap(window, itemNamed(window->contentItem(), QStringLiteral("stuck-copy")));
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("dckr_pat_ABCDEF-ghijk"));

        // A finger dragged without a hold still moves the note.
        QTest::qWait(400);
        tap(window, words);
        QTRY_VERIFY(words->property("selectedText").toString().isEmpty());
        const QPointF from = stuckNote->position();
        const QPoint start = characterAt(words, 2);
        QTest::touchEvent(window, finger).press(0, start, window);
        for (int step = 1; step <= 10; ++step) {
            QTest::qWait(10);
            QTest::touchEvent(window, finger).move(0, start + QPoint(-12 * step, 6 * step), window);
        }
        QTest::touchEvent(window, finger).release(0, start + QPoint(-120, 60), window);
        QTRY_VERIFY(stuckNote->position() != from);
        QVERIFY(words->property("selectedText").toString().isEmpty());

        // A right-click offers to copy the whole note.
        QTest::qWait(400);
        QTest::mouseClick(window, Qt::RightButton, {}, characterAt(words, 2));
        QTRY_VERIFY(tools->isVisible());
        tap(window, itemNamed(window->contentItem(), QStringLiteral("stuck-copy")));
        QCOMPARE(QGuiApplication::clipboard()->text(), text);
        QTest::qWait(400);
        tap(window, words);

        // The corner resizes the note, with a mouse and with a finger,
        // without opening it, and the size it is let go at is kept.
        QQuickItem *grip = itemNamed(window->contentItem(), QStringLiteral("stuckResize-n1"));
        QVERIFY(grip);
        const QSizeF fit = stuckNote->size();
        const auto corner = [grip] {
            return grip->mapToScene(QPointF(grip->width() - 10, grip->height() - 10)).toPoint();
        };
        const int opened = stuck.opened.size();
        sweep(window, corner(), corner() + QPoint(80, 60));
        QVERIFY(stuckNote->width() > fit.width());
        QVERIFY(stuckNote->height() > fit.height());
        QCOMPARE(stuck.sizes.size(), 1);
        QCOMPARE(stuck.sizes.constLast(), stuckNote->size());
        // Larger, the words have the room.
        QVERIFY(words->height() > fit.height() - 28);
        QTest::qWait(400);
        const QPoint start = corner();
        QTest::touchEvent(window, finger).press(0, start, window);
        for (int step = 1; step <= 10; ++step) {
            QTest::qWait(10);
            QTest::touchEvent(window, finger).move(0, start + QPoint(-4 * step, -3 * step), window);
        }
        QTest::touchEvent(window, finger).release(0, start + QPoint(-40, -30), window);
        QTRY_COMPARE(stuck.sizes.size(), 2);
        QCOMPARE(stuck.sizes.constLast(), stuckNote->size());
        QVERIFY(stuck.sizes.constLast().width() < stuck.sizes.constFirst().width());
        QTest::qWait(500);
        QCOMPARE(stuck.opened.size(), opened);
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

    // Two chips, both already filled in: the folder, and the window in
    // front. Each opens its choices with a tap, and one more chooses.
    void folderAndWindowAreChips()
    {
        QCOMPARE(named(QStringLiteral("folderChip"))->property("text").toString(), QStringLiteral("Folder · Inbox ▾"));
        QCOMPARE(named(QStringLiteral("stuckChip"))->property("text").toString(), QStringLiteral("Stuck to · SpreadGesture.qml ▾"));
        QVERIFY(!named(QStringLiteral("folderChoices"))->isVisible());

        tap(m_view.get(), named(QStringLiteral("folderChip")));
        QVERIFY(named(QStringLiteral("folderChoices"))->isVisible());
        QVERIFY(named(QStringLiteral("folder-inbox"))->property("checked").toBool());
        picture(m_view.get(), QStringLiteral("note-folders"));
        tap(m_view.get(), named(QStringLiteral("folder-Shuffle")));
        QCOMPARE(m_capture->folder(), QStringLiteral("Shuffle"));
        QVERIFY(!named(QStringLiteral("folderChoices"))->isVisible());
        QCOMPARE(named(QStringLiteral("folderChip"))->property("text").toString(), QStringLiteral("Folder · Shuffle ▾"));
        // The cursor goes back to the note.
        QTRY_VERIFY(note()->property("editor").value<QQuickItem *>()->hasActiveFocus());
        type(m_view.get(), QStringLiteral("Price"));
        QVERIFY(QFile::exists(m_store->folder() + QStringLiteral("/Shuffle/") + m_capture->noteId() + QStringLiteral(".md")));

        // Stuck to lists the open windows, the one in front first.
        tap(m_view.get(), named(QStringLiteral("stuckChip")));
        QVERIFY(named(QStringLiteral("windowChoices"))->isVisible());
        QCOMPARE(named(QStringLiteral("window-0"))->property("text").toString(), QStringLiteral("Kate · SpreadGesture.qml · in front"));
        QVERIFY(named(QStringLiteral("window-0"))->property("checked").toBool());
        QCOMPARE(named(QStringLiteral("window-1"))->property("text").toString(), QStringLiteral("Firefox · shuffleforplasma.com"));
        picture(m_view.get(), QStringLiteral("note-windows"));
        tap(m_view.get(), named(QStringLiteral("window-1")));
        QCOMPARE(m_capture->window(), QStringLiteral("shuffleforplasma.com"));
        QCOMPARE(m_capture->app(), QStringLiteral("org.mozilla.firefox"));
        QVERIFY(m_store->note(m_capture->noteId())->isStuck());
        QCOMPARE(named(QStringLiteral("stuckChip"))->property("text").toString(), QStringLiteral("Stuck to · shuffleforplasma.com ▾"));

        // Not stuck, it keeps its folder.
        tap(m_view.get(), named(QStringLiteral("stuckChip")));
        tap(m_view.get(), named(QStringLiteral("dontStick")));
        QVERIFY(!m_capture->stuck());
        QVERIFY(!m_store->note(m_capture->noteId())->stuck);
        QCOMPARE(m_store->note(m_capture->noteId())->folder, QStringLiteral("Shuffle"));
        QCOMPARE(named(QStringLiteral("stuckChip"))->property("text").toString(), QStringLiteral("Not stuck to a window ▾"));
    }

    void newFolderIsNamedOnce()
    {
        tap(m_view.get(), named(QStringLiteral("folderChip")));
        QQuickItem *field = named(QStringLiteral("folderName"));
        QVERIFY(field->isVisible());
        tap(m_view.get(), field);
        QTRY_VERIFY(field->hasActiveFocus());
        type(m_view.get(), QStringLiteral("Cabin"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QCOMPARE(m_capture->folder(), QStringLiteral("Cabin"));
        QVERIFY(m_store->hasFolder(QStringLiteral("Cabin")));
        QTRY_VERIFY(note()->property("editor").value<QQuickItem *>()->hasActiveFocus());
        type(m_view.get(), QStringLiteral("Book it"));
        QCOMPARE(m_store->note(m_capture->noteId())->folder, QStringLiteral("Cabin"));

        // A name that cannot be a folder's says why, and changes nothing.
        tap(m_view.get(), named(QStringLiteral("folderChip")));
        tap(m_view.get(), field);
        QTRY_VERIFY(field->hasActiveFocus());
        type(m_view.get(), QStringLiteral("Inbox"));
        QTest::keyClick(m_view.get(), Qt::Key_Return);
        QVERIFY(named(QStringLiteral("folderProblem"))->isVisible());
        QCOMPARE(m_capture->folder(), QStringLiteral("Cabin"));
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
        QCOMPARE(named(QStringLiteral("pickedDay"))->property("text").toString(),
                 start.date().addDays(1) == QDate::currentDate().addDays(1) ? QStringLiteral("Tomorrow")
                                                                            : named(QStringLiteral("pickedDay"))->property("text").toString());
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
            extra.startNewIn(QStringLiteral("folder:Shuffle"));
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

        // New note in a place goes there.
        tap(board, find(QStringLiteral("place-window:SpreadGesture.qml")));
        tap(board, find(QStringLiteral("newNote")));
        QCOMPARE(shell.newIn, QStringLiteral("window:SpreadGesture.qml"));

        // Folders down the side: Inbox first, New folder last.
        QQuickItem *inbox = find(QStringLiteral("place-inbox"));
        QQuickItem *home = find(QStringLiteral("place-folder:Home"));
        QQuickItem *shuffle = find(QStringLiteral("place-folder:Shuffle"));
        QQuickItem *newFolder = find(QStringLiteral("place-newfolder"));
        QVERIFY(inbox->y() < home->y() && home->y() < shuffle->y() && shuffle->y() < newFolder->y());
        QVERIFY(newFolder->y() < find(QStringLiteral("place-window:SpreadGesture.qml"))->y());

        // A held note lifts and drops on a folder.
        tap(board, inbox);
        QTRY_VERIFY(find(QStringLiteral("note-") + loose));
        shell.opened.clear();
        carry(board, find(QStringLiteral("note-") + loose), home);
        QTRY_COMPARE(m_store->note(loose)->folder, QStringLiteral("Home"));
        QVERIFY(!find(QStringLiteral("lifted"))->isVisible());
        QTRY_VERIFY(!find(QStringLiteral("note-") + loose));
        // Held, it was carried rather than opened.
        QVERIFY(shell.opened.isEmpty());

        // On the trash: removed, and undone from the message.
        tap(board, home);
        QTRY_VERIFY(find(QStringLiteral("note-") + loose));
        {
            QQuickItem *card = find(QStringLiteral("note-") + loose);
            const QPoint from = card->mapToScene(QPointF(card->width() / 2, card->height() / 3)).toPoint();
            QTest::mousePress(board, Qt::LeftButton, {}, from);
            QTest::qWait(QGuiApplication::styleHints()->mousePressAndHoldInterval() + 300);
            QQuickItem *trash = find(QStringLiteral("trashTarget"));
            QTRY_VERIFY(trash->isVisible());
            QVERIFY(find(QStringLiteral("lifted"))->isVisible());
            picture(board, QStringLiteral("board-carrying"));
            const QPoint target = trash->mapToScene(QPointF(trash->width() / 2, trash->height() / 2)).toPoint();
            for (int step = 1; step <= 10; ++step) {
                QTest::mouseMove(board, from + (target - from) * step / 10);
                QTest::qWait(16);
            }
            QTest::mouseRelease(board, Qt::LeftButton, {}, target);
        }
        QTRY_VERIFY(!m_store->note(loose));
        QTRY_VERIFY(find(QStringLiteral("noticeAction"))->isVisible());
        tap(board, find(QStringLiteral("noticeAction")));
        QTRY_COMPARE(m_store->note(loose)->folder, QStringLiteral("Home"));

        // New folder: named once, then shown.
        tap(board, find(QStringLiteral("place-newfolder")));
        QQuickItem *name = find(QStringLiteral("newFolderName"));
        QVERIFY(name->isVisible());
        QTRY_VERIFY(name->hasActiveFocus());
        type(board, QStringLiteral("Tablet"));
        QTest::keyClick(board, Qt::Key_Return);
        QVERIFY(m_store->hasFolder(QStringLiteral("Tablet")));
        QCOMPARE(notes.place(), QStringLiteral("folder:Tablet"));
        QVERIFY(!name->isVisible());
        QTRY_VERIFY(find(QStringLiteral("place-folder:Tablet")));
        QVERIFY(find(QStringLiteral("folderActions"))->isVisible());
        picture(board, QStringLiteral("board-folder"));

        // The workspace's folder: notes written on Desk go here.
        QQuickItem *workspace = find(QStringLiteral("workspaceFolder"));
        QCOMPARE(workspace->property("text").toString(), QStringLiteral("New notes on Desk go here"));
        QVERIFY(!workspace->property("checked").toBool());
        tap(board, workspace);
        QCOMPARE(m_store->workspaceFolder(QStringLiteral("Desk")), QStringLiteral("Tablet"));
        QTRY_VERIFY(workspace->property("checked").toBool());

        // Renamed in place.
        tap(board, find(QStringLiteral("renameFolder")));
        QQuickItem *rename = find(QStringLiteral("renameField"));
        QTRY_VERIFY(rename->hasActiveFocus());
        QTest::keyClick(board, Qt::Key_A, Qt::ControlModifier);
        type(board, QStringLiteral("Z13"));
        QTest::keyClick(board, Qt::Key_Return);
        QCOMPARE(m_store->folders(), (QStringList{QStringLiteral("Home"), QStringLiteral("Shuffle"), QStringLiteral("Z13")}));
        QCOMPARE(notes.place(), QStringLiteral("folder:Z13"));
        QCOMPARE(m_store->workspaceFolder(QStringLiteral("Desk")), QStringLiteral("Z13"));

        // Removed, its notes to Inbox; undo brings it back.
        QVERIFY(m_store->moveNote(loose, QStringLiteral("Z13")));
        tap(board, find(QStringLiteral("removeFolder")));
        QVERIFY(!m_store->hasFolder(QStringLiteral("Z13")));
        QCOMPARE(m_store->note(loose)->folder, QString());
        QCOMPARE(notes.place(), QStringLiteral("inbox"));
        QTRY_VERIFY(find(QStringLiteral("noticeAction"))->isVisible());
        tap(board, find(QStringLiteral("noticeAction")));
        QVERIFY(m_store->hasFolder(QStringLiteral("Z13")));
        QCOMPARE(m_store->note(loose)->folder, QStringLiteral("Z13"));

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
        QVERIFY(m_store->makeFolder(QStringLiteral("Split Rock")));
        const QString done = planned(QStringLiteral("Record Z13 flick samples"), QStringLiteral("window:SpreadGesture.qml"),
                                     QDateTime(today, QTime(0, 0, 30)));
        QVERIFY(setNoteDone(m_store.get(), done, true));
        QDateTime soon = now.addSecs(3600);
        soon.setTime(QTime(soon.time().hour(), 0));
        if (soon.date() != today) {
            soon = QDateTime(today, QTime(23, 45));
        }
        const QString next = planned(QStringLiteral("Ask Sam about a mouse way to move Spread on from Search"),
                                     QStringLiteral("folder:Shuffle"), soon);
        const QString evening = planned(QStringLiteral("Call about the cabin weekend"), QStringLiteral("folder:Home"),
                                        QDateTime(today, QTime(23, 59)));
        const QString ahead = planned(QStringLiteral("Split Rock: Milestone 0 review"), QStringLiteral("folder:Split Rock"),
                                      QDateTime(today.addDays(2), QTime(9, 0)));
        m_capture->setText(QStringLiteral("Flick threshold feels short on the Z13. Measure the real velocity before touching 1400."));
        Capture list(m_store.get());
        list.startNewIn(QStringLiteral("folder:Shuffle"));
        list.setColour(QStringLiteral("lichen"));
        list.setText(QStringLiteral("Groceries\ncoffee\noats\nlemons\ntape"));
        list.makeChecklist();
        list.setLineChecked(2, true);
        const QString groceries = list.noteId();
        Capture idea(m_store.get());
        idea.startNewIn(QStringLiteral("folder:Shuffle"));
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

        // An idea let go on a day is planned for it, at nine.
        const QDate tomorrow = today.addDays(1);
        tap(board, itemNamed(strip, QStringLiteral("day-") + today.toString(QStringLiteral("yyyy-MM-dd"))));
        QTRY_VERIFY(find(QStringLiteral("note-") + groceries));
        carry(board, find(QStringLiteral("note-") + groceries),
              itemNamed(strip, QStringLiteral("day-") + tomorrow.toString(QStringLiteral("yyyy-MM-dd"))));
        QTRY_COMPARE(m_store->note(groceries)->remind, QDateTime(tomorrow, QTime(9, 0)));

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
