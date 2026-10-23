// SPDX-License-Identifier: GPL-2.0-or-later
// The quick-note card and the board, drawn off screen over a folder in the
// test's own home, and used by tapping and typing as a person would.
#include "Board.h"
#include "Capture.h"
#include "ContextBar.h"
#include "NoteStore.h"
#include "Planner.h"
#include "ReminderWords.h"
#include "TestHome.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QClipboard>
#include <algorithm>
#include <QGuiApplication>
#include <QLocale>
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
    Q_PROPERTY(QString backLabel MEMBER backLabel NOTIFY backLabelChanged)
    Q_PROPERTY(QObject *contextBar MEMBER contextBar CONSTANT)

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
    QString backLabel;
    QObject *contextBar = nullptr;
    int boards = 0;
    int wentBack = 0;

    void setBack(const QString &label)
    {
        backLabel = label;
        Q_EMIT backLabelChanged();
    }

Q_SIGNALS:
    void backLabelChanged();

public Q_SLOTS:
    void openNote(const QString &id) { opened = id; }
    void newNoteIn(const QString &place) { newIn = place; }
    void showBoard() { }
    void showCapture() { }
    void boardFromCard() { ++boards; }
    void goBack() { ++wentBack; }
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

// Stands in for Shuffle's Context Bar, as the shell gives it to the card.
class FakeBar : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int height MEMBER height CONSTANT)

public:
    int height = 0;
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
    Q_PROPERTY(QString writingId MEMBER m_writingId NOTIFY writingChanged)
    Q_PROPERTY(QString writingText MEMBER m_writingText NOTIFY writingTextChanged)
    Q_PROPERTY(bool folded MEMBER m_folded NOTIFY foldedChanged)

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
    }

    // The notes asked to be typed on, the words typed, and how often typing
    // was ended.
    QStringList written;
    QStringList typed;
    int stopped = 0;
    QList<bool> keys;
    QList<QSizeF> sizes;
    // Where the surface last took presses, how often it lost the keys and
    // how often its dot was tapped.
    QList<QRectF> pressable;
    int left = 0;
    int unfolded = 0;

    void setFolded(bool folded)
    {
        m_folded = folded;
        Q_EMIT foldedChanged();
    }

    // Words changed elsewhere while the note is typed on.
    void changeElsewhere(const QString &text)
    {
        m_writingText = text;
        Q_EMIT writingTextChanged();
    }

Q_SIGNALS:
    void writingChanged();
    void writingTextChanged();
    void foldedChanged();

public Q_SLOTS:
    void write(const QString &id)
    {
        written.append(id);
        m_writingId = id;
        m_writingText = m_notes.constFirst().toMap().value(QStringLiteral("text")).toString();
        Q_EMIT writingChanged();
        Q_EMIT writingTextChanged();
    }
    void setWritingText(const QString &text)
    {
        typed.append(text);
        m_writingText = text;
    }
    void stopWriting()
    {
        ++stopped;
        if (!m_writingId.isEmpty()) {
            m_writingId.clear();
            Q_EMIT writingChanged();
        }
    }
    void setPressable(const QVariantList &rects)
    {
        pressable.clear();
        for (const QVariant &rect : rects) {
            pressable.append(rect.toRectF());
        }
    }
    void keysLeft()
    {
        ++left;
        stopWriting();
    }
    void unfold()
    {
        ++unfolded;
        setFolded(false);
    }
    void place(const QString &, qreal, qreal) { }
    void setTakesKeys(bool takes) { keys.append(takes); }
    void resize(const QString &, qreal width, qreal height) { sizes.append(QSizeF(width, height)); }

private:
    QVariantList m_notes;
    QString m_writingId;
    QString m_writingText;
    bool m_folded = false;
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

// The topmost visible item under a scene point, for saying what a missed tap hit.
QQuickItem *topItemAt(QQuickItem *root, const QPointF &scenePos)
{
    // The overlay always holds Kirigami's notification layer, which lets
    // presses through; look under it.
    if (!root->isVisible() || root->opacity() <= 0
        || QString::fromLatin1(root->metaObject()->className()).startsWith(QStringLiteral("QQuickOverlay")))
        return nullptr;
    QList<QQuickItem *> kids = root->childItems();
    std::stable_sort(kids.begin(), kids.end(), [](QQuickItem *a, QQuickItem *b) { return a->z() < b->z(); });
    for (auto it = kids.crbegin(); it != kids.crend(); ++it) {
        if (QQuickItem *hit = topItemAt(*it, scenePos))
            return hit;
    }
    return root->contains(root->mapFromScene(scenePos)) ? root : nullptr;
}

QString describe(QQuickItem *item)
{
    QStringList chain;
    for (QQuickItem *up = item; up && chain.size() < 4; up = up->parentItem())
        chain << QStringLiteral("%1(%2)").arg(QString::fromLatin1(up->metaObject()->className()), up->objectName());
    return chain.join(QStringLiteral(" < "));
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

    QQuickItem *m_cursorEditor = nullptr;
    int *m_cursorWhenAsked = nullptr;

    QQuickItem *note() const { return m_view->rootObject(); }
    QQuickItem *named(const QString &name) const { return itemNamed(note(), name); }

private Q_SLOTS:
    // Where the note's cursor stood as the keys were asked for.
    void noteCursorWhenAsked()
    {
        if (m_cursorEditor && m_cursorWhenAsked)
            *m_cursorWhenAsked = m_cursorEditor->property("cursorPosition").toInt();
    }


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

    // The note's words fill the card inside 12 px margins, with the colour
    // a dot beside them, and, past a hairline, one 44 px line under them
    // holding where the note is kept, what it is stuck to, its controls and
    // All notes, which ends 12 px from the right edge.
    void pageFillsTheCard()
    {
        QQuickItem *page = named(QStringLiteral("page"));
        QVERIFY(page);
        QCOMPARE(page->property("radius").toReal(), 10.0);
        const QPointF pageAt = page->mapToItem(note(), QPointF());
        QCOMPARE(pageAt.x(), 12.0);
        QCOMPARE(pageAt.y(), 12.0);
        QCOMPARE(page->width(), note()->width() - 24);
        QQuickItem *tray = named(QStringLiteral("tray"));
        QCOMPARE(tray->height(), 44.0);
        const QPointF trayAt = tray->mapToItem(note(), QPointF());
        QCOMPARE(trayAt.y(), pageAt.y() + page->height() + 8 + 1 + 8);
        QCOMPARE(trayAt.y() + tray->height(), note()->height() - 12);
        QQuickItem *allNotes = named(QStringLiteral("allNotes"));
        QCOMPARE(allNotes->mapToItem(note(), QPointF()).x() + allNotes->width(), note()->width() - 12);
        QQuickItem *dot = named(QStringLiteral("colourDot"));
        QVERIFY(dot->isVisible());
        QCOMPARE(dot->parentItem(), page);
        QVERIFY(!named(QStringLiteral("colours"))->isVisible());
        // Nothing rarely needed is laid out at rest.
        QVERIFY(!named(QStringLiteral("moreMenu"))->isVisible());
        QCOMPARE(named(QStringLiteral("folderChip"))->property("text").toString(), QStringLiteral("Inbox"));
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
    // while a tap types on it there and a finger or the note's edge still
    // moves it; the surface takes the keys only while words are selected or
    // the note is typed on.
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
        QVERIFY(stuck.written.isEmpty());

        // A click lets the words go and gives the keys back; the next types
        // on the note where it stands, never on a card.
        QTest::qWait(400);
        tap(window, words);
        QTRY_COMPARE(stuck.keys, (QList<bool>{true, false}));
        QVERIFY(words->property("selectedText").toString().isEmpty());
        QVERIFY(stuck.written.isEmpty());
        QTest::qWait(400);
        tap(window, words);
        QCOMPARE(stuck.written, QStringList{QStringLiteral("n1")});
        QTRY_COMPARE(stuck.keys, (QList<bool>{true, false, true}));
        QVERIFY(!words->property("readOnly").toBool());
        stuck.stopWriting();
        QTRY_COMPARE(stuck.keys, (QList<bool>{true, false, true, false}));
        QVERIFY(words->property("readOnly").toBool());

        // A finger held on the code takes it whole; Copy copies it.
        QPointingDevice *finger = QTest::createTouchDevice();
        QTest::qWait(400);
        hold(window, finger, characterAt(words, text.indexOf(QStringLiteral("dckr")) + 3));
        QTRY_COMPARE(words->property("selectedText").toString(), QStringLiteral("dckr_pat_ABCDEF-ghijk"));
        QCOMPARE(stuck.written.size(), 1);
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
        // The corner is the one on the side the note has room to grow
        // toward, half past the paper; pulled away from the note, it grows.
        const auto outward = [stuckNote] {
            return stuckNote->property("gripLeft").toBool() ? -1 : 1;
        };
        const auto corner = [grip] {
            return grip->mapToScene(QPointF(grip->width() / 2, grip->height() / 2)).toPoint();
        };
        const int written = stuck.written.size();
        sweep(window, corner(), corner() + QPoint(80 * outward(), 60));
        QVERIFY(stuckNote->width() > fit.width());
        QVERIFY(stuckNote->height() > fit.height());
        QCOMPARE(stuck.sizes.size(), 1);
        QCOMPARE(stuck.sizes.constLast(), stuckNote->size());
        // Larger, the words have the room.
        QVERIFY(words->height() > fit.height() - 28);
        QTest::qWait(400);
        const QPoint held = corner();
        const int inward = -outward();
        QTest::touchEvent(window, finger).press(0, held, window);
        for (int step = 1; step <= 10; ++step) {
            QTest::qWait(10);
            QTest::touchEvent(window, finger).move(0, held + QPoint(4 * step * inward, -3 * step), window);
        }
        QTest::touchEvent(window, finger).release(0, held + QPoint(40 * inward, -30), window);
        QTRY_COMPARE(stuck.sizes.size(), 2);
        QCOMPARE(stuck.sizes.constLast(), stuckNote->size());
        QVERIFY(stuck.sizes.constLast().width() < stuck.sizes.constFirst().width());
        QTest::qWait(500);
        QCOMPARE(stuck.written.size(), written);
    }

    // A tap on a stuck note types on it where it stands: its words take the
    // cursor at their end, what is typed is handed on to be kept as the
    // quick note keeps it, words changed elsewhere reach it, and Esc ends it.
    void stuckNoteIsTypedWhereItStands()
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
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QQuickItem *words = itemNamed(window->contentItem(), QStringLiteral("stuckWords-n1"));
        QVERIFY(words);
        const QString text = words->property("text").toString();
        QVERIFY(words->property("readOnly").toBool());
        QTest::qWait(50);

        tap(window, words);
        QCOMPARE(stuck.written, QStringList{QStringLiteral("n1")});
        QTRY_VERIFY(words->hasActiveFocus());
        QVERIFY(!words->property("readOnly").toBool());
        QCOMPARE(words->property("cursorPosition").toInt(), text.size());
        QTRY_COMPARE(stuck.keys, QList<bool>{true});
        // A tap on the note while it is typed on asks nothing more.
        QTest::qWait(400);
        tap(window, words);
        QCOMPARE(stuck.written.size(), 1);

        // While typed on, a finger's tap on the words asks for the keys again
        // once it lifts, as on the quick note.
        QQuickItem *keys = itemNamed(window->contentItem(), QStringLiteral("stuckKeys-n1"));
        QVERIFY(keys);
        QSignalSpy asked(keys, SIGNAL(asked()));
        QPointingDevice *finger = QTest::createTouchDevice();
        const QPoint onTheWords = characterAt(words, 1);
        QTest::touchEvent(window, finger).press(0, onTheWords, window);
        QTest::qWait(60);
        QTest::touchEvent(window, finger).release(0, onTheWords, window);
        QTRY_COMPARE(asked.count(), 1);
        QCOMPARE(stuck.written.size(), 1);

        words->setProperty("cursorPosition", text.size());
        type(window, QStringLiteral(" ok"));
        QTRY_COMPARE(stuck.typed.constLast(), text + QStringLiteral(" ok"));
        QCOMPARE(words->property("text").toString(), text + QStringLiteral(" ok"));

        // Changed elsewhere, as on another device, the words on the window follow.
        stuck.changeElsewhere(QStringLiteral("From elsewhere"));
        QTRY_COMPARE(words->property("text").toString(), QStringLiteral("From elsewhere"));
        QCOMPARE(stuck.typed.constLast(), text + QStringLiteral(" ok"));

        // Esc ends it, the keys go back, and the words are the bus's again.
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(stuck.stopped >= 1);
        QTRY_COMPARE(stuck.keys, (QList<bool>{true, false}));
        QTRY_VERIFY(words->property("readOnly").toBool());
    }

    // Folded, the notes are one dot at the top note's top-right corner, a
    // 44 px target and the only place the surface takes presses; a tap on it
    // opens them again.
    void stuckNotesFoldToADot()
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
        QQuickItem *dot = itemNamed(window->contentItem(), QStringLiteral("stuckDot"));
        QVERIFY(stuckNote);
        QVERIFY(dot);
        QVERIFY(stuckNote->isVisible());
        QVERIFY(!dot->isVisible());
        // The note, and its resize corner, which reaches past the paper.
        QTRY_COMPARE(stuck.pressable.size(), 2);
        const QRectF note(stuckNote->position(), stuckNote->size());
        QCOMPARE(stuck.pressable.constLast().size(), QSizeF(44, 44));
        QVERIFY(stuck.pressable.constLast().bottom() > note.bottom());

        stuck.setFolded(true);
        QTRY_VERIFY(dot->isVisible());
        QVERIFY(!stuckNote->isVisible());
        QCOMPARE(dot->size(), QSizeF(44, 44));
        QCOMPARE(dot->position(), QPointF(note.right() - 44, note.top()));
        QTRY_COMPARE(stuck.pressable, QList<QRectF>{QRectF(dot->position(), dot->size())});

        QTest::qWait(50);
        tap(window, dot);
        QCOMPARE(stuck.unfolded, 1);
        QTRY_VERIFY(stuckNote->isVisible());
        QVERIFY(!dot->isVisible());
        QTRY_COMPARE(stuck.pressable.size(), 2);
        QCOMPARE(stuck.pressable.constFirst().topLeft(), note.topLeft());
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

    // A finger's tap on the words asks for the on-screen keys again once it
    // has lifted, with the cursor on the line the finger landed on: the text
    // asks as the finger comes down, with the cursor still where it was, and
    // a desktop that reads the cursor's line keeps the keys down for that
    // ask. A mouse asks for nothing; neither does a checklist item a mouse
    // clicks, while a finger on one asks the same way.
    void aTapOnTheWordsAsksForTheKeysWhereItLands()
    {
        m_capture->setText(QStringLiteral("Lisbon\nCall the apartment host\nFado night"));
        auto *editor = note()->property("editor").value<QQuickItem *>();
        QTRY_VERIFY(editor->hasActiveFocus());
        editor->setProperty("cursorPosition", editor->property("length"));
        QQuickItem *keys = named(QStringLiteral("keysOnTap"));
        QVERIFY(keys);
        const QString text = m_capture->text();
        const int lineStart = text.indexOf(QStringLiteral("Call"));
        const int lineEnd = text.indexOf(QStringLiteral("\nFado"));
        int cursorWhenAsked = -1;
        QSignalSpy asked(keys, SIGNAL(asked()));
        connect(keys, SIGNAL(asked()), this, SLOT(noteCursorWhenAsked()));
        m_cursorEditor = editor;
        m_cursorWhenAsked = &cursorWhenAsked;

        QPointingDevice *finger = QTest::createTouchDevice();
        const QPoint onTheLine = characterAt(editor, lineStart + 5);
        QTest::touchEvent(m_view.get(), finger).press(0, onTheLine, m_view.get());
        QTest::qWait(60);
        QTest::touchEvent(m_view.get(), finger).release(0, onTheLine, m_view.get());
        QTRY_COMPARE(asked.count(), 1);
        QVERIFY2(cursorWhenAsked >= lineStart && cursorWhenAsked <= lineEnd, qPrintable(QString::number(cursorWhenAsked)));

        QTest::mouseClick(m_view.get(), Qt::LeftButton, {}, characterAt(editor, 2));
        QTest::qWait(100);
        QCOMPARE(asked.count(), 1);

        // A checklist's items are words of their own, asked for the same way.
        m_view->resize(806, 640);
        tap(m_view.get(), named(QStringLiteral("checklist")));
        QVERIFY(m_capture->checklist());
        QQuickItem *pad = named(QStringLiteral("checklistPad"));
        QTRY_VERIFY(pad->isVisible());
        QQuickItem *item = itemNamed(itemNamed(pad, QStringLiteral("line-1")), QStringLiteral("lineText"));
        QVERIFY(item);
        QQuickItem *itemKeys = nullptr;
        for (QQuickItem *child : item->childItems()) {
            if (QString::fromLatin1(child->metaObject()->className()).startsWith(QStringLiteral("KeysOnTap")))
                itemKeys = child;
        }
        QVERIFY(itemKeys);
        QSignalSpy itemAsked(itemKeys, SIGNAL(asked()));
        const QPoint onTheItem = characterAt(item, 3);
        QTest::touchEvent(m_view.get(), finger).press(0, onTheItem, m_view.get());
        QTest::qWait(60);
        QTest::touchEvent(m_view.get(), finger).release(0, onTheItem, m_view.get());
        QTRY_COMPARE(itemAsked.count(), 1);
        QVERIFY(item->hasActiveFocus());
    }

    void everyTargetIsBigEnoughToTouch()
    {
        m_capture->setText(QStringLiteral("kept, so Tuck away and Remove show"));
        const auto buttons = buttonsIn(note());
        QVERIFY(buttons.size() >= 7);
        for (QQuickItem *button : buttons) {
            QVERIFY2(button->height() >= 44 && button->width() >= 44,
                     qPrintable(QStringLiteral("%1 is %2×%3").arg(button->objectName()).arg(button->width()).arg(button->height())));
        }
    }

    // The colour's dot lays the five colours out; one of them chooses and
    // puts them away.
    void colourIsTheDot()
    {
        picture(m_view.get(), QStringLiteral("note-empty"));
        tap(m_view.get(), named(QStringLiteral("colourDot")));
        QVERIFY(named(QStringLiteral("colours"))->isVisible());
        picture(m_view.get(), QStringLiteral("note-colours"));
        tap(m_view.get(), named(QStringLiteral("colour-lichen")));
        QVERIFY(!named(QStringLiteral("colours"))->isVisible());
        QCOMPARE(m_capture->colour(), QStringLiteral("lichen"));
        type(m_view.get(), QStringLiteral("Groceries"));
        QCOMPARE(m_store->note(m_capture->noteId())->colour, QStringLiteral("lichen"));
        picture(m_view.get(), QStringLiteral("note-written"));
    }

    // One line, already filled in: the folder, and the window in front.
    // Each opens its choices with a tap, and one more chooses.
    void folderAndWindowAreChips()
    {
        QCOMPARE(named(QStringLiteral("folderChip"))->property("text").toString(), QStringLiteral("Inbox"));
        QCOMPARE(named(QStringLiteral("stuckChip"))->property("text").toString(), QStringLiteral("SpreadGesture.qml"));
        QVERIFY(!named(QStringLiteral("folderChoices"))->isVisible());

        tap(m_view.get(), named(QStringLiteral("folderChip")));
        QVERIFY(named(QStringLiteral("folderChoices"))->isVisible());
        QVERIFY(named(QStringLiteral("folder-inbox"))->property("checked").toBool());
        picture(m_view.get(), QStringLiteral("note-folders"));
        tap(m_view.get(), named(QStringLiteral("folder-Shuffle")));
        QCOMPARE(m_capture->folder(), QStringLiteral("Shuffle"));
        QVERIFY(!named(QStringLiteral("folderChoices"))->isVisible());
        QCOMPARE(named(QStringLiteral("folderChip"))->property("text").toString(), QStringLiteral("Shuffle"));
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
        QCOMPARE(named(QStringLiteral("stuckChip"))->property("text").toString(), QStringLiteral("shuffleforplasma.com"));

        // Not stuck, it keeps its folder.
        tap(m_view.get(), named(QStringLiteral("stuckChip")));
        tap(m_view.get(), named(QStringLiteral("dontStick")));
        QVERIFY(!m_capture->stuck());
        QVERIFY(!m_store->note(m_capture->noteId())->stuck);
        QCOMPARE(m_store->note(m_capture->noteId())->folder, QStringLiteral("Shuffle"));
        QCOMPARE(named(QStringLiteral("stuckChip"))->property("text").toString(), QStringLiteral("Not stuck"));
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

    // The note listens: a day offers a reminder, lines begun with "- " a
    // checklist and a #word a folder, the full stop after it left out, each
    // taken with a tap, after which it is offered no more.
    void wordsOffer()
    {
        m_capture->setText(QStringLiteral("Call Sam tomorrow\n- parking\n- deposit\nKeep it in #cabin."));
        QTRY_VERIFY(named(QStringLiteral("offers"))->isVisible());
        QVERIFY(!named(QStringLiteral("listensHint"))->isVisible());
        QQuickItem *remind = named(QStringLiteral("offer-remind"));
        const QDateTime tomorrow(QDate::currentDate().addDays(1), QTime(9, 0));
        QCOMPARE(remind->property("text").toString(), QStringLiteral("Remind %1").arg(ReminderWords::label(tomorrow, false)));
        picture(m_view.get(), QStringLiteral("note-offers"));
        tap(m_view.get(), remind);
        QCOMPARE(m_capture->remindAt(), tomorrow);
        QTRY_VERIFY(!named(QStringLiteral("offer-remind")));

        tap(m_view.get(), named(QStringLiteral("offer-folder")));
        QCOMPARE(m_capture->folder(), QStringLiteral("Cabin"));
        QTRY_VERIFY(!named(QStringLiteral("offer-folder")));

        tap(m_view.get(), named(QStringLiteral("offer-checklist")));
        QVERIFY(m_capture->checklist());
        QVERIFY(!m_capture->text().contains(QStringLiteral("- - ")));
        QVERIFY(m_capture->text().contains(QStringLiteral("- [ ] parking")));
        QTRY_VERIFY(!named(QStringLiteral("offers"))->isVisible());
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
        // The day shown is the day of the time picked: late in the evening
        // the hour on carries it past midnight, two days on.
        const QDateTime picked = start.addDays(1).addSecs(75 * 60);
        const qint64 daysOn = QDate::currentDate().daysTo(picked.date());
        QCOMPARE(named(QStringLiteral("pickedDay"))->property("text").toString(),
                 daysOn == 1 ? QStringLiteral("Tomorrow") : QLocale().toString(picked.date(), QStringLiteral("dddd")));
        tap(m_view.get(), named(QStringLiteral("setTime")));
        QCOMPARE(m_capture->remindAt(), picked);
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

    // More holds what a kept note rarely needs: Copy all, Pin, Tuck away
    // and Remove. Before the first letter there is nothing to act on.
    void moreOnlyForAKeptNote()
    {
        QVERIFY(!named(QStringLiteral("more"))->isVisible());
        type(m_view.get(), QStringLiteral("x"));
        QVERIFY(named(QStringLiteral("more"))->isVisible());
        QVERIFY(!named(QStringLiteral("tuckAway"))->isVisible());
        tap(m_view.get(), named(QStringLiteral("more")));
        QVERIFY(named(QStringLiteral("moreMenu"))->isVisible());
        QVERIFY(named(QStringLiteral("tuckAway"))->isVisible());
        picture(m_view.get(), QStringLiteral("note-more"));
        tap(m_view.get(), named(QStringLiteral("copyAll")));
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("x"));
        QVERIFY(!named(QStringLiteral("moreMenu"))->isVisible());
        tap(m_view.get(), named(QStringLiteral("more")));
        auto *pin = named(QStringLiteral("pin"));
        QCOMPARE(pin->property("text").toString(), QStringLiteral("Pin to the panel"));
        tap(m_view.get(), pin);
        QVERIFY(m_capture->pinned());
        QCOMPARE(m_store->pinnedNote()->id, m_capture->noteId());
        tap(m_view.get(), named(QStringLiteral("more")));
        QCOMPARE(pin->property("text").toString(), QStringLiteral("Unpin from the panel"));
        tap(m_view.get(), pin);
        QVERIFY(!m_store->pinnedNote());
        tap(m_view.get(), named(QStringLiteral("more")));
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
        QCOMPARE(card->property("radius").toReal(), 10.0);
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

    // Under Shuffle the card's surface covers the Context Bar too. Keys tall
    // enough to lift the card to the top leave it a gutter below the bar, as
    // above the keys, and it shortens rather than reach over the bar.
    void cardKeepsAGutterBelowTheContextBar()
    {
        Places places(m_store.get());
        PlaceNotes notes(m_store.get());
        FakeShell shell(m_store.get(), m_capture.get(), &places, &notes);
        FakeBar bar;
        bar.height = 32;
        shell.contextBar = &bar;
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
        // Without the keys the card rests where it does without a bar.
        // The card's place and height settle on frames of their own.
        QTRY_COMPARE(card->y(), 133.0);
        QTRY_COMPARE(card->height(), 471.0);
        window->setProperty("keysRect", QRectF(0, 436, 1260, 300));
        QTRY_COMPARE(card->height(), 384.0);
        QCOMPARE(card->y(), 42.0);
        QCOMPARE(card->y() + card->height(), 426.0);
        picture(window, QStringLiteral("card-keys-bar"));
        window->setProperty("keysRect", QRectF());
        QTRY_COMPARE(card->y(), 133.0);
    }

    // The Context Bar's height, from its clock extent; anything but a
    // presenting bar of the known schema is no bar.
    void contextBarHeightFromItsExtent()
    {
        QCOMPARE(ContextBar::heightFrom(R"({"schema":"co.goodinput.shuffle.clock-extent","version":1,"output":"eDP-1",)"
                                        R"("presenting":true,"bar":{"height":32}})"),
                 32);
        QCOMPARE(ContextBar::heightFrom(R"({"schema":"co.goodinput.shuffle.clock-extent","version":1,)"
                                        R"("presenting":false,"bar":{"height":32}})"),
                 0);
        QCOMPARE(ContextBar::heightFrom(R"({"schema":"co.goodinput.shuffle.clock-extent","version":2,)"
                                        R"("presenting":true,"bar":{"height":32}})"),
                 0);
        QCOMPARE(ContextBar::heightFrom(R"({"schema":"other","version":1,"presenting":true,"bar":{"height":32}})"), 0);
        QCOMPARE(ContextBar::heightFrom("not json"), 0);
    }

    // All notes opens the board as Gooseberry's own window, and the card
    // goes. A note opened from a search's Notes has a bar on top leading back
    // there, which Esc takes too; Esc on any other puts the card away.
    void cardOpensTheBoardAndGoesBack()
    {
        m_capture->setText(QStringLiteral("Flick threshold"));
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
        QVERIFY(!itemNamed(card, QStringLiteral("backBar"))->isVisible());

        tap(window, itemNamed(card, QStringLiteral("allNotes")));
        QCOMPARE(shell.boards, 1);
        QCOMPARE(card->width(), 806.0);

        // From a search's Notes: the bar leads back, and so does Esc.
        shell.setBack(QStringLiteral("Notes"));
        QQuickItem *back = itemNamed(card, QStringLiteral("back"));
        QTRY_VERIFY(back->isVisible());
        QVERIFY(back->height() >= 44);
        QVERIFY(itemNamed(card, QStringLiteral("page"))->mapToItem(card, QPointF()).y() >= back->mapToItem(card, QPointF()).y() + 30);
        picture(window, QStringLiteral("card-back"));
        tap(window, back);
        QCOMPARE(shell.wentBack, 1);
        QSignalSpy finished(m_capture.get(), &Capture::finished);
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(shell.wentBack, 2);
        QCOMPARE(finished.count(), 0);
        shell.setBack({});
        QTRY_VERIFY(!itemNamed(card, QStringLiteral("backBar"))->isVisible());
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(finished.count(), 1);

        // A tap on the card is the card's; a tap beside it puts it away.
        QMetaObject::invokeMethod(window, "open");
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(int(card->x()) + 120, int(card->y()) + 36));
        QCOMPARE(finished.count(), 1);
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(60, 60));
        QCOMPARE(finished.count(), 2);
    }

    // Opened from a search's Notes, the card comes up as the search goes,
    // and the desktop may hand the keyboard back to the window under them:
    // the card asks for it back and stays. Once it has settled, the keyboard
    // moving elsewhere is the person going back to the work, and it goes.
    void cardKeepsTheKeyboardAsItOpens()
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
        QSignalSpy finished(m_capture.get(), &Capture::finished);
        QMetaObject::invokeMethod(window, "open");
        QVERIFY(QTest::qWaitForWindowActive(window));

        QWindow work;
        work.resize(400, 300);
        work.show();
        work.requestActivate();
        QTRY_VERIFY(window->isActive());
        QCOMPARE(finished.count(), 0);
        QVERIFY(window->property("shown").toBool());

        QTest::qWait(1100);
        work.requestActivate();
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!window->property("shown").toBool());
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
        // On a wide board a row also tells when the note changed.
        QVERIFY(itemNamed(find(QStringLiteral("note-") + onWindow), QStringLiteral("when"))->isVisible());

        // Tapping a note opens it on the card.
        tap(board, find(QStringLiteral("note-") + loose));
        QCOMPARE(shell.opened, loose);

        // Tuck away from the note, and undo it from the message that follows.
        // Opening or tucking a note lays the rows out again, so a row is
        // found afresh each time.
        QString lastTuck;
        auto tuckRow = [&](const QString &id) {
            QTest::qWait(200);
            QQuickItem *tuck = itemNamed(find(QStringLiteral("note-") + id), QStringLiteral("tuck"));
            QVERIFY(tuck);
            // Scroll the row to the middle of what the list shows, clear of
            // the header above it and New note over its end, at any size.
            QQuickItem *list = nullptr;
            for (QQuickItem *up = tuck; up && !list; up = up->parentItem()) {
                if (up->inherits("QQuickFlickable"))
                    list = up;
            }
            QVERIFY(list);
            auto *content = list->property("contentItem").value<QQuickItem *>();
            QVERIFY(content);
            const qreal rowY = tuck->mapToItem(content, QPointF(0, tuck->height() / 2)).y();
            const qreal most = qMax<qreal>(0, list->property("contentHeight").toReal() - list->height());
            list->setProperty("contentY", qBound<qreal>(0, rowY - list->height() / 2, most));
            QTest::qWait(50);
            tuck = itemNamed(find(QStringLiteral("note-") + id), QStringLiteral("tuck"));
            QVERIFY(tuck);
            const QRectF shown = list->mapRectToScene(QRectF(0, 0, list->width(), list->height()));
            const QRectF button = tuck->mapRectToScene(QRectF(0, 0, tuck->width(), tuck->height()));
            QVERIFY2(shown.contains(button), qPrintable(QStringLiteral("tuck %1,%2 outside the list %3,%4 to %5,%6")
                                                            .arg(button.x()).arg(button.y()).arg(shown.left()).arg(shown.top())
                                                            .arg(shown.right()).arg(shown.bottom())));
            QQuickItem *newNote = find(QStringLiteral("newNote"));
            QVERIFY(!newNote->mapRectToScene(QRectF(0, 0, newNote->width(), newNote->height())).intersects(button));
            const QPointF at = tuck->mapToScene({tuck->width() / 2, tuck->height() / 2});
            // Rest the pointer off the list, so nothing under it is hovered.
            QTest::mouseMove(board, QPoint(2, 2));
            QTest::qWait(50);
            lastTuck = QStringLiteral("tuck at %1,%2 enabled %3 opacity %4; under it: %5")
                           .arg(at.x()).arg(at.y()).arg(tuck->isEnabled()).arg(tuck->opacity())
                           .arg(describe(topItemAt(board->contentItem(), at)));
            tap(board, tuck);
        };
        tuckRow(onWindow);
        QVERIFY2(m_store->note(onWindow)->tucked, qPrintable(lastTuck));
        QTRY_VERIFY(find(QStringLiteral("noticeAction"))->isVisible());
        tap(board, find(QStringLiteral("noticeAction")));
        QVERIFY(!m_store->note(onWindow)->tucked);

        // Tuck it away again, then bring it back from Tucked away.
        QTRY_VERIFY(find(QStringLiteral("note-") + onWindow));
        tuckRow(onWindow);
        QVERIFY2(m_store->note(onWindow)->tucked, qPrintable(lastTuck));
        tap(board, find(QStringLiteral("place-tucked")));
        QCOMPARE(notes.place(), QStringLiteral("tucked"));
        QTRY_VERIFY(find(QStringLiteral("note-") + onWindow));
        tap(board, find(QStringLiteral("note-") + onWindow));
        QVERIFY(!m_store->note(onWindow)->tucked);

        // New note in a place goes there.
        tap(board, find(QStringLiteral("place-window:SpreadGesture.qml")));
        // New note floats in the board's bottom right corner.
        QQuickItem *newNote = find(QStringLiteral("newNote"));
        const QPointF corner = newNote->mapToScene({newNote->width(), newNote->height()});
        QCOMPARE(corner.x(), board->width() - 24);
        QCOMPARE(corner.y(), board->height() - 24);
        // Under the pointer it lights, with a ring round it.
        QQuickItem *ring = itemNamed(newNote, QStringLiteral("ring"));
        QVERIFY(ring);
        QCOMPARE(ring->opacity(), 0.0);
        QTest::mouseMove(board, newNote->mapToScene({newNote->width() / 2, newNote->height() / 2}).toPoint());
        QTRY_COMPARE(ring->opacity(), 1.0);
        tap(board, newNote);
        QCOMPARE(shell.newIn, QStringLiteral("window:SpreadGesture.qml"));

        // The places in one row along the top, under the search: Today,
        // Inbox first among the folders, New folder last, then the windows
        // and Tucked away.
        QQuickItem *inbox = find(QStringLiteral("place-inbox"));
        QQuickItem *home = find(QStringLiteral("place-folder:Home"));
        QQuickItem *shuffle = find(QStringLiteral("place-folder:Shuffle"));
        QQuickItem *newFolder = find(QStringLiteral("place-newfolder"));
        QQuickItem *window = find(QStringLiteral("place-window:SpreadGesture.qml"));
        QVERIFY(find(QStringLiteral("place-today"))->x() < inbox->x());
        QVERIFY(inbox->x() < home->x() && home->x() < shuffle->x() && shuffle->x() < newFolder->x());
        QVERIFY(newFolder->x() < window->x() && window->x() < find(QStringLiteral("place-tucked"))->x());
        QCOMPARE(inbox->y(), window->y());
        // With folders made, New folder is a + after them.
        QVERIFY(newFolder->property("plus").toBool());
        QCOMPARE(newFolder->width(), 44.0);
        QQuickItem *searchField = find(QStringLiteral("search"));
        QVERIFY(inbox->mapToScene({0, 0}).y() > searchField->mapToScene({0, searchField->height()}).y());

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

        // New folder: one tap makes it and shows it, its name ready to type
        // over in place; a second tap makes another, numbered.
        tap(board, find(QStringLiteral("place-newfolder")));
        QVERIFY(m_store->hasFolder(QStringLiteral("New folder")));
        QCOMPARE(notes.place(), QStringLiteral("folder:New folder"));
        QQuickItem *name = find(QStringLiteral("renameField"));
        QVERIFY(name->isVisible());
        QTRY_VERIFY(name->hasActiveFocus());
        QCOMPARE(name->property("selectedText").toString(), QStringLiteral("New folder"));
        type(board, QStringLiteral("Tablet"));
        QTest::keyClick(board, Qt::Key_Return);
        QVERIFY(m_store->hasFolder(QStringLiteral("Tablet")));
        QVERIFY(!m_store->hasFolder(QStringLiteral("New folder")));
        QCOMPARE(notes.place(), QStringLiteral("folder:Tablet"));
        QVERIFY(!name->isVisible());
        QTRY_VERIFY(find(QStringLiteral("place-folder:Tablet")));
        QVERIFY(m_store->makeFolder(QStringLiteral("New folder")));
        // The row is laid out again for the folder before New folder is tapped.
        QTRY_VERIFY(find(QStringLiteral("place-folder:New folder")));
        QTest::qWait(50);
        tap(board, find(QStringLiteral("place-newfolder")));
        QVERIFY(m_store->hasFolder(QStringLiteral("New folder 2")));
        QTest::keyClick(board, Qt::Key_Escape);
        QVERIFY(m_store->removeFolder(QStringLiteral("New folder 2")).has_value());
        QVERIFY(m_store->removeFolder(QStringLiteral("New folder")).has_value());
        QTRY_VERIFY(!find(QStringLiteral("place-folder:New folder")));
        QTest::qWait(50);
        tap(board, find(QStringLiteral("place-folder:Tablet")));
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

        // Words no note has, and Enter, start a note with them in Inbox.
        tap(board, searchField);
        type(board, QStringLiteral("Zebra crossing"));
        QTRY_COMPARE(notes.count(), 0);
        QTest::keyClick(board, Qt::Key_Return);
        QCOMPARE(shell.newIn, QStringLiteral("inbox"));
        QCOMPARE(m_capture->text(), QStringLiteral("Zebra crossing"));
        QCOMPARE(searchField->property("text").toString(), QString());
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
        QCOMPARE(find(QStringLiteral("ideasHeading"))->property("text").toString(), QStringLiteral("Ideas · no date needed"));
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
    // No tooltips under a resting pointer: they would take the next press
    // to close themselves, whatever desktop the tests run on. Controls that
    // ask for hover themselves, such as the floating New note, still have it.
    qputenv("QT_QUICK_CONTROLS_HOVER_ENABLED", "0");
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
