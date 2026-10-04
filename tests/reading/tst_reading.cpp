// SPDX-License-Identifier: GPL-2.0-or-later
// Reading handwriting, on the computer: the real reader, run by ONNX Runtime
// on a stand-in model of the same shape as the real one (make-fake-reader.py),
// draws a line and reads it, with its runner-up words; and the reading of
// notes, line by line and out of sight, keeps what it read in each note, never
// replaces a fix, reads a changed line again, and lets the reader rest. The
// real model's accuracy is judged on the tablet; this proves everything round
// it. All in a home of the test's own.
#include "Capture.h"
#include "NoteStore.h"
#include "Reading.h"
#include "TestHome.h"
#include "TrOcrReader.h"

#include <QGuiApplication>
#include <QMutex>
#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <memory>

using namespace Gooseberry;

namespace {

InkStroke stroke(int row, qreal from, qreal to)
{
    InkStroke s;
    const qreal y = row * Ink::RowHeight + Ink::RowHeight / 2;
    for (qreal x = from; x <= to; x += 4) {
        s.points.append({x, y + std::sin(x / 6) * 6, 0.6});
    }
    return s;
}

// Reads every line as the words it is told, counting what it is asked, and
// can be held mid-line to change the note under it.
class FakeReader : public InkReader
{
public:
    std::optional<Read> read(const QList<InkStroke> &line) override
    {
        ++asked;
        while (hold) {
            QThread::msleep(5);
        }
        if (line.isEmpty() || fail) {
            return std::nullopt;
        }
        QMutexLocker lock(&mutex);
        return Read{words, also};
    }
    void rest() override { ++rested; }

    QMutex mutex;
    QString words = QStringLiteral("measure flack velocity");
    QStringList also = {QStringLiteral("flick")};
    std::atomic<int> asked = 0;
    std::atomic<int> rested = 0;
    std::atomic<bool> hold = false;
    std::atomic<bool> fail = false;
};

} // namespace

class ReadingTest : public QObject
{
    Q_OBJECT

public:
    explicit ReadingTest(TestHome *home)
        : m_home(home)
    {
    }

private:
    TestHome *m_home;
    std::unique_ptr<NoteStore> m_store;

    // A note with handwriting on the given lines.
    QString inkNote(const QList<int> &rows)
    {
        Capture capture(m_store.get());
        capture.startNew({});
        for (const int row : rows) {
            const InkStroke s = stroke(row, 20, 160);
            capture.beginStroke(s.points.first().x, s.points.first().y, 0.6, QStringLiteral("black"));
            for (const InkPoint &point : s.points) {
                capture.extendStroke(point.x, point.y, point.pressure);
            }
            capture.endStroke();
        }
        capture.finish();
        return capture.noteId();
    }

private Q_SLOTS:
    void init()
    {
        static int round = 0;
        const QString folder = m_home->path() + QStringLiteral("/reading-%1").arg(++round);
        QVERIFY(m_home->holds(folder));
        m_store = std::make_unique<NoteStore>(folder);
        QVERIFY(m_store->open());
    }

    void cleanup()
    {
        m_store.reset();
    }

    // The line drawn for reading: black on white, the writing all in it.
    void lineAsAPicture()
    {
        InkStroke blue = stroke(0, 20, 160);
        blue.colour = QStringLiteral("blue");
        const QImage picture = TrOcrReader::picture({blue});
        QVERIFY(picture.width() > picture.height());
        QCOMPARE(picture.pixelColor(0, 0), QColor(Qt::white));
        bool dark = false;
        for (int x = 0; x < picture.width() && !dark; ++x) {
            dark = qGray(picture.pixel(x, picture.height() / 2)) < 60;
        }
        QVERIFY(dark);
    }

    // The real reader on the stand-in model: the line is read through the
    // encoder and decoder, its pieces made into words, and the next-best
    // reading gives the runner-up words.
    void readerReadsALine()
    {
        const QString folder = QStringLiteral(FAKE_READER);
        QVERIFY(TrOcrReader::installedAt(folder));
        QVERIFY(!TrOcrReader::installedAt(m_home->path()));
        TrOcrReader reader(folder);
        const auto read = reader.read({stroke(0, 20, 160), stroke(0, 200, 300)});
        QVERIFY(read.has_value());
        QCOMPARE(read->text, QStringLiteral("measure flick velocity"));
        QVERIFY(read->also.contains(QStringLiteral("flack")));
        // Rested, it loads again when next asked.
        reader.rest();
        QCOMPARE(reader.read({stroke(0, 20, 160)})->text, QStringLiteral("measure flick velocity"));
        QVERIFY(!reader.read({}).has_value());
        // No model: nothing read, nothing broken.
        TrOcrReader none(m_home->path());
        QVERIFY(!none.read({stroke(0, 20, 160)}).has_value());
    }

    // Each line of a note is read once and kept in the drawing and the note,
    // without counting as a change the person made.
    void notesAreReadLineByLine()
    {
        const QString id = inkNote({0, 2});
        const QDateTime changed = m_store->note(id)->changed;
        auto *fake = new FakeReader;
        Reading reading(m_store.get(), std::unique_ptr<InkReader>(fake));
        QVERIFY(reading.available());
        QSignalSpy idle(&reading, &Reading::idle);
        QTest::qWait(1100);
        reading.request(id);
        QTRY_VERIFY(idle.count() >= 1 && !reading.busy());
        QCOMPARE(fake->asked.load(), 2);
        const auto note = m_store->note(id);
        QCOMPARE(note->read, QStringLiteral("measure flack velocity measure flack velocity"));
        QCOMPARE(note->readAlso, QStringLiteral("flick"));
        QCOMPARE(note->changed, changed);
        QVERIFY(m_store->ink(id).unread().isEmpty());

        // Asked again, nothing is read again.
        reading.request(id);
        QTRY_VERIFY(!reading.busy());
        QCOMPARE(fake->asked.load(), 2);
    }

    // A fix is never read over; a line written after it is read.
    void fixesStay()
    {
        const QString id = inkNote({0});
        Capture capture(m_store.get());
        QVERIFY(capture.open(id));
        capture.fixReading(QStringLiteral("measure flick velocity"));
        const InkStroke more = stroke(3, 20, 160);
        capture.beginStroke(more.points.first().x, more.points.first().y, 0.6, QStringLiteral("black"));
        for (const InkPoint &point : more.points) {
            capture.extendStroke(point.x, point.y, point.pressure);
        }
        capture.endStroke();
        capture.finish();

        auto *fake = new FakeReader;
        fake->words = QStringLiteral("today");
        fake->also = {};
        Reading reading(m_store.get(), std::unique_ptr<InkReader>(fake));
        reading.request(id);
        QTRY_VERIFY(!reading.busy());
        QCOMPARE(fake->asked.load(), 1);
        QCOMPARE(m_store->note(id)->read, QStringLiteral("measure flick velocity today"));
    }

    // A line changed while it was being read is not given the old reading;
    // it is read again as it now is.
    void changedWhileRead()
    {
        const QString id = inkNote({0});
        auto *fake = new FakeReader;
        fake->hold = true;
        Reading reading(m_store.get(), std::unique_ptr<InkReader>(fake));
        reading.request(id);
        QTRY_COMPARE(fake->asked.load(), 1);
        Capture capture(m_store.get());
        QVERIFY(capture.open(id));
        const InkStroke more = stroke(0, 200, 300);
        capture.beginStroke(more.points.first().x, more.points.first().y, 0.6, QStringLiteral("black"));
        capture.extendStroke(more.points.last().x, more.points.last().y, 0.6);
        capture.endStroke();
        capture.flush();
        {
            QMutexLocker lock(&fake->mutex);
            fake->words = QStringLiteral("measure flick velocity now");
        }
        fake->hold = false;
        QTRY_VERIFY(!reading.busy());
        QCOMPARE(fake->asked.load(), 2);
        QCOMPARE(m_store->note(id)->read, QStringLiteral("measure flick velocity now"));
    }

    // After a start every note with unread lines is read; a reader that
    // cannot read a note leaves it for another time instead of trying over
    // and over; without a reader nothing happens; and the reader rests when
    // there is nothing to read.
    void unreadAfterAStart()
    {
        const QString first = inkNote({0});
        const QString second = inkNote({1});
        Capture typed(m_store.get());
        typed.startNew({});
        typed.setText(QStringLiteral("Typed"));
        typed.finish();

        auto *failing = new FakeReader;
        failing->fail = true;
        {
            Reading reading(m_store.get(), std::unique_ptr<InkReader>(failing));
            reading.requestUnread();
            QTRY_VERIFY(!reading.busy());
            QCOMPARE(failing->asked.load(), 2);
        }

        Reading none(m_store.get(), nullptr);
        QVERIFY(!none.available());
        none.requestUnread();
        QVERIFY(!none.busy());

        auto *fake = new FakeReader;
        Reading reading(m_store.get(), std::unique_ptr<InkReader>(fake));
        reading.setRestAfter(100);
        reading.requestUnread();
        QTRY_VERIFY(!reading.busy());
        QVERIFY(!m_store->note(first)->read.isEmpty());
        QVERIFY(!m_store->note(second)->read.isEmpty());
        QTRY_COMPARE(fake->rested.load(), 1);
    }
};

int main(int argc, char *argv[])
{
    TestHome home;
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ReadingTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reading.moc"
