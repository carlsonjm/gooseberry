// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace Gooseberry {

// One point of a stroke, on the page, with how hard the pen pressed: 0 to 1.
struct InkPoint {
    qreal x = 0;
    qreal y = 0;
    qreal pressure = 0.5;
    bool operator==(const InkPoint &) const = default;
};

// One stroke: from the pen touching the page to it lifting.
struct InkStroke {
    QString colour = QStringLiteral("black");
    QList<InkPoint> points;
    bool operator==(const InkStroke &) const = default;

    // The ruled line the stroke sits on, by its middle.
    int row() const;
    // The stroke's outline: a shape as wide as the pen pressed at each point,
    // closed, in page units.
    QList<QPointF> outline() const;
    // True when a touch at this point, with this reach, falls on the stroke.
    bool touches(const QPointF &at, qreal reach) const;
};

// What the reader made of one ruled line, or the words typed to fix it.
struct InkReading {
    int row = 0;
    QString text;
    // The reader's runner-up words, so a word first read wrong can be found.
    QStringList also;
    // Typed by the person: never replaced by a later reading.
    bool fixed = false;
    // The strokes the reading was made from, so a changed line is read again.
    QString digest;
    bool operator==(const InkReading &) const = default;
};

// A note's handwriting: strokes on a ruled page of a fixed width, drawn larger
// or smaller to fit wherever it is shown, and what was read from each line.
// Kept as a drawing beside the note (docs/FORMAT.md § Ink).
struct Ink {
    // The page as the strokes are kept: the mock-up's page width and ruling.
    static constexpr qreal PageWidth = 716;
    static constexpr qreal RowHeight = 42;
    // The page is at least this many lines tall, and always has one empty
    // line below the writing to go on with.
    static constexpr int LeastRows = 4;

    QList<InkStroke> strokes;
    QList<InkReading> readings;

    bool isEmpty() const { return strokes.isEmpty(); }
    // The ruled lines in use, top to bottom.
    QList<int> rows() const;
    int rowCount() const;
    qreal height() const { return rowCount() * RowHeight; }
    QList<InkStroke> strokesIn(int row) const;
    // A short fingerprint of the strokes on a line.
    QString digest(int row) const;
    // Lines whose strokes have changed since they were read, or were never
    // read, and not fixed by hand.
    QList<int> unread() const;

    // The reading of the whole page, line by line, joined into one line of
    // words; and the runner-up words.
    QString readText() const;
    QString readAlso() const;
    // A line read. Ignored when the line has since changed or was fixed.
    bool setReading(int row, const QString &digest, const QString &text, const QStringList &also);
    // The person's own words for the lines written so far.
    void fix(const QString &text);
    // Readings of lines that no longer have strokes are let go.
    void dropEmptyReadings();

    // Strokes touched at this point are taken away and returned, in page
    // order, so they can be put back.
    QList<InkStroke> eraseAt(const QPointF &at, qreal reach);
    void restore(const QList<InkStroke> &taken);

    QByteArray toSvg() const;
    static Ink fromSvg(const QByteArray &svg);

    bool operator==(const Ink &) const = default;
};

// The three inks, by the names the drawing keeps, and their colours.
QStringList inkNames();
QString inkHex(const QString &name);

} // namespace Gooseberry
