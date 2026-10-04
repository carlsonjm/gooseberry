// SPDX-License-Identifier: GPL-2.0-or-later
// Handwriting kept: strokes as a drawing beside the note, with their ink and
// pressure, read back as written; the eraser taking whole strokes and putting
// them back; what was read from each line, a fix that is never replaced, and
// lines read again when they change; the card keeping ink as it keeps typing;
// and ink found by the board's search. All in a home of the test's own.
#include "Board.h"
#include "Capture.h"
#include "Ink.h"
#include "NoteStore.h"
#include "TestHome.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QSignalSpy>
#include <QTest>
#include <QXmlStreamReader>

#include <memory>

using namespace Gooseberry;

namespace {

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A stroke across part of a ruled line, pressing as given.
InkStroke stroke(int row, qreal from, qreal to, qreal pressure = 0.5, const QString &ink = QStringLiteral("black"))
{
    InkStroke s;
    s.colour = ink;
    const qreal y = row * Ink::RowHeight + Ink::RowHeight / 2;
    for (qreal x = from; x <= to; x += 4) {
        s.points.append({x, y + std::sin(x / 6) * 6, pressure});
    }
    return s;
}

qreal widthOf(const QList<QPointF> &outline)
{
    qreal top = outline.first().y();
    qreal bottom = top;
    for (const QPointF &point : outline) {
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return bottom - top;
}

// Writes a stroke on the card, as the pen would.
void write(Capture &capture, int row, qreal from, qreal to, const QString &ink = QStringLiteral("black"))
{
    const InkStroke s = stroke(row, from, to, 0.6, ink);
    capture.beginStroke(s.points.first().x, s.points.first().y, 0.6, ink);
    for (qsizetype i = 1; i < s.points.size(); ++i) {
        capture.extendStroke(s.points.at(i).x, s.points.at(i).y, 0.6);
    }
    capture.endStroke();
}

} // namespace

class InkTest : public QObject
{
    Q_OBJECT

public:
    explicit InkTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;
    QString m_folder;
    std::unique_ptr<NoteStore> m_store;

    void reopen()
    {
        m_store = std::make_unique<NoteStore>(m_folder);
        QVERIFY(m_store->open());
    }

private Q_SLOTS:
    void init()
    {
        static int round = 0;
        m_folder = m_home->path() + QStringLiteral("/ink-%1").arg(++round);
        QVERIFY(m_home->holds(m_folder));
        reopen();
    }

    void cleanup()
    {
        m_store.reset();
    }

    // The drawing is an ordinary picture: each stroke a filled shape in its
    // ink, the page as tall as its lines, with the points and pressure kept
    // so it reads back exactly as it was kept.
    void drawingReadsBack()
    {
        Ink ink;
        ink.strokes = {stroke(0, 20, 120, 0.2), stroke(0, 140, 200, 0.9, QStringLiteral("blue")),
                       stroke(2, 20, 90, 0.5, QStringLiteral("red"))};
        ink.setReading(0, ink.digest(0), QStringLiteral("measure flick"), {QStringLiteral("flack")});
        const QByteArray svg = ink.toSvg();

        // Plain SVG, as any viewer expects: no prefix on its own elements.
        QVERIFY2(svg.contains("<svg xmlns=\"http://www.w3.org/2000/svg\""), svg.left(300).constData());
        QVERIFY(!svg.contains("<n1:"));
        QDomDocument document;
        QVERIFY(document.setContent(svg, QDomDocument::ParseOption::UseNamespaceProcessing));
        const QDomElement root = document.documentElement();
        QCOMPARE(root.tagName(), QStringLiteral("svg"));
        QCOMPARE(root.namespaceURI(), QStringLiteral("http://www.w3.org/2000/svg"));
        QCOMPARE(root.attribute(QStringLiteral("width")), QStringLiteral("716"));
        // As tall as the writing: its third line, and a little below it.
        const int tall = root.attribute(QStringLiteral("height")).toInt();
        QVERIFY2(tall > 2 * 42 + 21 && tall < 3 * 42 + 21, qPrintable(QString::number(tall)));
        const QDomNodeList paths = document.elementsByTagName(QStringLiteral("path"));
        QCOMPARE(paths.size(), 3);
        QStringList fills;
        for (int i = 0; i < paths.size(); ++i) {
            const QDomElement path = paths.at(i).toElement();
            fills.append(path.attribute(QStringLiteral("fill")));
            QVERIFY(path.attribute(QStringLiteral("d")).startsWith(QLatin1Char('M')));
            QVERIFY(path.attribute(QStringLiteral("d")).endsWith(QLatin1Char('Z')));
        }
        QVERIFY(fills.contains(QStringLiteral("#1A1A1A")));
        QVERIFY(fills.contains(QStringLiteral("#2F6FB0")));
        QVERIFY(fills.contains(QStringLiteral("#B4442F")));
        QCOMPARE(document.elementsByTagName(QStringLiteral("desc")).at(0).toElement().text(), QStringLiteral("measure flick"));

        const Ink back = Ink::fromSvg(svg);
        QCOMPARE(back.strokes.size(), 3);
        QCOMPARE(back.strokes.at(1).colour, QStringLiteral("blue"));
        QCOMPARE(back.strokes.at(2).row(), 2);
        QCOMPARE(back.strokes.at(0).points.first().pressure, 0.2);
        QCOMPARE(back.readText(), QStringLiteral("measure flick"));
        QCOMPARE(back.readAlso(), QStringLiteral("flack"));
        // Kept to a tenth of a point, the same strokes are the same line.
        QCOMPARE(back.digest(0), ink.digest(0));
        QVERIFY(back.unread() == QList<int>{2});
        QCOMPARE(back.toSvg(), Ink::fromSvg(back.toSvg()).toSvg());
    }

    // Pressing harder draws a wider line; a touch that hardly moves is a dot.
    void pressureShapesTheLine()
    {
        QVERIFY(widthOf(stroke(0, 0, 0, 1.0).outline()) > widthOf(stroke(0, 0, 0, 0.0).outline()) + 2);
        InkStroke light;
        light.points = {{10, 20, 0.1}, {200, 20, 0.1}};
        InkStroke hard;
        hard.points = {{10, 20, 1.0}, {200, 20, 1.0}};
        QVERIFY(widthOf(hard.outline()) > widthOf(light.outline()) * 2);
        InkStroke dot;
        dot.points = {{50, 50, 0.5}};
        QVERIFY(dot.outline().size() >= 8);
        QVERIFY(dot.touches({50, 50}, 1));
    }

    // The page has room below the writing, and at least four lines.
    void pageGrowsWithTheWriting()
    {
        Ink ink;
        QCOMPARE(ink.rowCount(), 4);
        ink.strokes = {stroke(5, 10, 50)};
        QCOMPARE(ink.rowCount(), 7);
        QCOMPARE(ink.rows(), QList<int>{5});
    }

    // The eraser takes each whole stroke it touches, and only those; they
    // can be put back.
    void eraserTakesWholeStrokes()
    {
        Ink ink;
        ink.strokes = {stroke(0, 20, 120), stroke(0, 300, 400), stroke(1, 20, 120)};
        const QList<InkStroke> taken = ink.eraseAt({60, Ink::RowHeight / 2}, 6);
        QCOMPARE(taken.size(), 1);
        QCOMPARE(ink.strokes.size(), 2);
        QVERIFY(ink.eraseAt({250, Ink::RowHeight / 2}, 6).isEmpty());
        ink.restore(taken);
        QCOMPARE(ink.strokes.size(), 3);
    }

    // Each line is read once; a line that changes is read again; a fix is
    // the person's own words and no later reading replaces it, while lines
    // written after it are read as usual.
    void readingsAndFixes()
    {
        Ink ink;
        ink.strokes = {stroke(0, 20, 120), stroke(1, 20, 120)};
        QCOMPARE(ink.unread(), (QList<int>{0, 1}));
        QVERIFY(ink.setReading(0, ink.digest(0), QStringLiteral("measure flack"), {QStringLiteral("flick")}));
        QVERIFY(ink.setReading(1, ink.digest(1), QStringLiteral("velocity"), {}));
        QVERIFY(ink.unread().isEmpty());
        QCOMPARE(ink.readText(), QStringLiteral("measure flack velocity"));
        QCOMPARE(ink.readAlso(), QStringLiteral("flick"));

        // A stroke added to the first line: it is read again, and a reading
        // of the line as it was is not taken.
        const QString before = ink.digest(0);
        ink.strokes.append(stroke(0, 200, 260));
        QCOMPARE(ink.unread(), QList<int>{0});
        QVERIFY(!ink.setReading(0, before, QStringLiteral("old"), {}));

        ink.fix(QStringLiteral("measure flick velocity"));
        QVERIFY(ink.unread().isEmpty());
        QCOMPARE(ink.readText(), QStringLiteral("measure flick velocity"));
        QVERIFY(!ink.setReading(0, ink.digest(0), QStringLiteral("measure flack"), {}));
        QCOMPARE(ink.readText(), QStringLiteral("measure flick velocity"));

        ink.strokes.append(stroke(3, 20, 120));
        QCOMPARE(ink.unread(), QList<int>{3});
        QVERIFY(ink.setReading(3, ink.digest(3), QStringLiteral("today"), {}));
        QCOMPARE(ink.readText(), QStringLiteral("measure flick velocity today"));

        // A line erased whole lets its reading go.
        ink.strokes.removeLast();
        ink.dropEmptyReadings();
        QCOMPARE(ink.readText(), QStringLiteral("measure flick velocity"));
    }

    // On the card: the first stroke keeps a new note at once, drawing and
    // note together; later strokes wait for the pen to pause, as typing
    // does; the note names its drawing, and is kept with no words at all.
    void cardKeepsInkAsItKeepsTyping()
    {
        Capture capture(m_store.get());
        capture.setWaits(5000, 10000);
        capture.startNew({});
        write(capture, 0, 20, 120);
        QVERIFY(capture.kept());
        const QString id = capture.noteId();
        QVERIFY(QFile::exists(m_store->inkPathFor(id)));
        QCOMPARE(m_store->note(id)->ink, id + QStringLiteral(".svg"));
        QCOMPARE(m_store->ink(id).strokes.size(), 1);
        QVERIFY(readFile(m_store->pathFor(id)).contains(QByteArray(QByteArray("ink: \"") + id.toUtf8() + ".svg\"\n")));

        write(capture, 0, 140, 200, QStringLiteral("red"));
        QVERIFY(capture.waiting());
        QCOMPARE(m_store->ink(id).strokes.size(), 1);
        capture.flush();
        QCOMPARE(m_store->ink(id).strokes.size(), 2);
        QCOMPARE(m_store->ink(id).strokes.at(1).colour, QStringLiteral("red"));

        // Ink alone is a note: finishing keeps it.
        capture.finish();
        QVERIFY(m_store->note(id).has_value());
        QVERIFY(QFile::exists(m_store->inkPathFor(id)));

        // Opened again, the ink is there to go on with.
        QVERIFY(capture.open(id));
        QVERIFY(capture.hasInk());
        QCOMPARE(capture.ink().strokes.size(), 2);
    }

    // The eraser on the card: whole strokes go, Undo puts them back, and a
    // page erased bare leaves a note with nothing in it, which goes to the
    // trash with its drawing when it is finished, as an emptied note does.
    void cardEraser()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        write(capture, 0, 20, 120);
        write(capture, 1, 20, 120);
        capture.flush();
        const QString id = capture.noteId();
        capture.eraseAt(60, Ink::RowHeight / 2);
        QCOMPARE(capture.endErase(), 1);
        QVERIFY(capture.canUndoErase());
        QCOMPARE(m_store->ink(id).strokes.size(), 1);
        capture.undoErase();
        QCOMPARE(m_store->ink(id).strokes.size(), 2);
        QVERIFY(!capture.canUndoErase());

        capture.eraseAt(60, Ink::RowHeight / 2);
        capture.eraseAt(60, Ink::RowHeight * 1.5);
        QCOMPARE(capture.endErase(), 2);
        QVERIFY(!capture.hasInk());
        QVERIFY(!QFile::exists(m_store->inkPathFor(id)));
        QVERIFY(m_store->note(id)->ink.isEmpty());
        capture.finish();
        QVERIFY(!m_store->note(id).has_value());
        QVERIFY(QDir(m_home->trash() + QStringLiteral("/files")).entryList().contains(id + QStringLiteral(".md")));
    }

    // Removed, a note's drawing goes to the trash with it, and Undo brings
    // both back.
    void drawingGoesAndComesBackWithItsNote()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        capture.setText(QStringLiteral("Sketch"));
        write(capture, 0, 20, 120);
        capture.flush();
        const QString id = capture.noteId();
        capture.remove();
        QVERIFY(!QFile::exists(m_store->inkPathFor(id)));
        QVERIFY(!QFile::exists(m_store->pathFor(id)));
        QVERIFY(capture.undoRemove());
        QVERIFY(QFile::exists(m_store->inkPathFor(id)));
        QCOMPARE(m_store->ink(id).strokes.size(), 1);
    }

    // A fix on the card is kept at once, in the drawing and in the note.
    void cardFix()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        write(capture, 0, 20, 120);
        capture.fixReading(QStringLiteral("measure flick velocity"));
        const QString id = capture.noteId();
        QCOMPARE(capture.readText(), QStringLiteral("measure flick velocity"));
        QCOMPARE(m_store->note(id)->read, QStringLiteral("measure flick velocity"));
        QVERIFY(m_store->ink(id).readings.first().fixed);
        QCOMPARE(m_store->note(id)->title(), QStringLiteral("measure flick velocity"));
    }

    // A reading saved while strokes on the card wait for a pause is kept
    // when they are written.
    void readingWhileWriting()
    {
        Capture capture(m_store.get());
        capture.setWaits(5000, 10000);
        capture.startNew({});
        write(capture, 0, 20, 120);
        const QString id = capture.noteId();
        write(capture, 2, 20, 120);
        QVERIFY(capture.waiting());
        Ink onDisk = m_store->ink(id);
        QVERIFY(onDisk.setReading(0, onDisk.digest(0), QStringLiteral("flick"), {}));
        QVERIFY(m_store->saveInk(*m_store->note(id), onDisk, NoteStore::Touch::Kept));
        capture.flush();
        QCOMPARE(m_store->ink(id).strokes.size(), 2);
        QCOMPARE(m_store->note(id)->read, QStringLiteral("flick"));
    }

    // The board's search finds handwriting by what was read, and by the
    // reader's runner-up words.
    void searchFindsHandwriting()
    {
        Capture capture(m_store.get());
        capture.startNew({});
        write(capture, 0, 20, 120);
        const QString id = capture.noteId();
        Ink ink = m_store->ink(id);
        ink.setReading(0, ink.digest(0), QStringLiteral("measure flack velocity"), {QStringLiteral("flick")});
        QVERIFY(m_store->saveInk(*m_store->note(id), ink, NoteStore::Touch::Kept));
        PlaceNotes notes(m_store.get());
        for (const QString &word : {QStringLiteral("velocity"), QStringLiteral("flick")}) {
            notes.setSearch(word);
            QCOMPARE(notes.count(), 1);
            QCOMPARE(notes.data(notes.index(0), PlaceNotes::ReadRole).toString(), QStringLiteral("measure flack velocity"));
            QVERIFY(notes.data(notes.index(0), PlaceNotes::InkRole).toString().contains(id + QStringLiteral(".svg#")));
        }
        notes.setSearch(QStringLiteral("kettle"));
        QCOMPARE(notes.count(), 0);
    }
};

int main(int argc, char *argv[])
{
    TestHome home;
    QCoreApplication app(argc, argv);
    InkTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ink.moc"
