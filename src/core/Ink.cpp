// SPDX-License-Identifier: GPL-2.0-or-later
#include "Ink.h"

#include <QCryptographicHash>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>

namespace Gooseberry {

namespace {

const QString Svg = QStringLiteral("http://www.w3.org/2000/svg");
const QString Own = QStringLiteral("https://github.com/carlsonjm/gooseberry/ink");

struct InkColour {
    const char *name;
    const char *hex;
};

// The mock-up's three inks, in the order the palette offers them.
constexpr InkColour Inks[] = {
    {"black", "#1A1A1A"},
    {"blue", "#2F6FB0"},
    {"red", "#B4442F"},
};

// How wide the pen draws, from its lightest touch to its hardest press.
constexpr qreal ThinnestWidth = 1.6;
constexpr qreal WidestWidth = 4.6;

qreal widthAt(qreal pressure)
{
    return ThinnestWidth + (WidestWidth - ThinnestWidth) * std::clamp(pressure, 0.0, 1.0);
}

QString number(qreal value, int decimals)
{
    QString text = QString::number(value, 'f', decimals);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

QString pointsText(const InkStroke &stroke)
{
    QStringList parts;
    parts.reserve(stroke.points.size());
    for (const InkPoint &point : stroke.points) {
        parts.append(number(point.x, 1) + QLatin1Char(',') + number(point.y, 1) + QLatin1Char(',')
                     + number(point.pressure, 2));
    }
    return parts.join(QLatin1Char(' '));
}

QList<InkPoint> pointsFrom(const QString &text)
{
    QList<InkPoint> points;
    const auto parts = QStringView(text).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        const auto values = part.split(QLatin1Char(','));
        if (values.size() < 2) {
            continue;
        }
        InkPoint point;
        point.x = values.at(0).toDouble();
        point.y = values.at(1).toDouble();
        point.pressure = values.size() > 2 ? values.at(2).toDouble() : 0.5;
        points.append(point);
    }
    return points;
}

QString pathData(const QList<QPointF> &outline)
{
    if (outline.isEmpty()) {
        return {};
    }
    QString data = QStringLiteral("M") + number(outline.first().x(), 1) + QLatin1Char(' ') + number(outline.first().y(), 1);
    for (qsizetype i = 1; i < outline.size(); ++i) {
        data += QStringLiteral(" L") + number(outline.at(i).x(), 1) + QLatin1Char(' ') + number(outline.at(i).y(), 1);
    }
    return data + QStringLiteral(" Z");
}

qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const qreal length = QPointF::dotProduct(ab, ab);
    qreal t = length > 0 ? QPointF::dotProduct(p - a, ab) / length : 0;
    t = std::clamp(t, 0.0, 1.0);
    const QPointF nearest = a + t * ab;
    return std::hypot(p.x() - nearest.x(), p.y() - nearest.y());
}

QStringList words(const QString &text)
{
    return text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

} // namespace

int InkStroke::row() const
{
    if (points.isEmpty()) {
        return 0;
    }
    qreal top = points.first().y;
    qreal bottom = top;
    for (const InkPoint &point : points) {
        top = std::min(top, point.y);
        bottom = std::max(bottom, point.y);
    }
    return std::max(0, int(std::floor((top + bottom) / 2 / Ink::RowHeight)));
}

QList<QPointF> InkStroke::outline() const
{
    QList<QPointF> shape;
    if (points.isEmpty()) {
        return shape;
    }
    // A dot, or a touch that hardly moved: a round blot.
    qreal travel = 0;
    for (qsizetype i = 1; i < points.size(); ++i) {
        travel += std::hypot(points.at(i).x - points.at(i - 1).x, points.at(i).y - points.at(i - 1).y);
    }
    if (points.size() == 1 || travel < 0.5) {
        const qreal radius = widthAt(points.first().pressure) / 2 + 0.4;
        for (int step = 0; step < 10; ++step) {
            const qreal angle = step * M_PI / 5;
            shape.append({points.first().x + radius * std::cos(angle), points.first().y + radius * std::sin(angle)});
        }
        return shape;
    }

    QList<QPointF> left;
    QList<QPointF> right;
    for (qsizetype i = 0; i < points.size(); ++i) {
        const InkPoint &before = points.at(std::max<qsizetype>(0, i - 1));
        const InkPoint &after = points.at(std::min<qsizetype>(points.size() - 1, i + 1));
        qreal dx = after.x - before.x;
        qreal dy = after.y - before.y;
        const qreal length = std::hypot(dx, dy);
        if (length < 1e-6) {
            dx = 1;
            dy = 0;
        } else {
            dx /= length;
            dy /= length;
        }
        const qreal half = widthAt(points.at(i).pressure) / 2;
        left.append({points.at(i).x - dy * half, points.at(i).y + dx * half});
        right.append({points.at(i).x + dy * half, points.at(i).y - dx * half});
    }
    // Round ends: half a circle round each end of the line.
    auto cap = [](const InkPoint &end, const QPointF &from, qreal half, QList<QPointF> &into) {
        const qreal start = std::atan2(from.y() - end.y, from.x() - end.x);
        for (int step = 1; step < 6; ++step) {
            const qreal angle = start + step * M_PI / 6;
            into.append({end.x + half * std::cos(angle), end.y + half * std::sin(angle)});
        }
    };
    shape += left;
    cap(points.last(), left.last(), widthAt(points.last().pressure) / 2, shape);
    for (auto it = right.crbegin(); it != right.crend(); ++it) {
        shape.append(*it);
    }
    cap(points.first(), right.first(), widthAt(points.first().pressure) / 2, shape);
    return shape;
}

bool InkStroke::touches(const QPointF &at, qreal reach) const
{
    for (qsizetype i = 0; i < points.size(); ++i) {
        const QPointF a(points.at(i).x, points.at(i).y);
        const QPointF b = i + 1 < points.size() ? QPointF(points.at(i + 1).x, points.at(i + 1).y) : a;
        if (distanceToSegment(at, a, b) <= reach + widthAt(points.at(i).pressure) / 2) {
            return true;
        }
    }
    return false;
}

QList<int> Ink::rows() const
{
    QList<int> found;
    for (const InkStroke &stroke : strokes) {
        const int row = stroke.row();
        if (!found.contains(row)) {
            found.append(row);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

int Ink::rowCount() const
{
    int lowest = -1;
    for (const InkStroke &stroke : strokes) {
        for (const InkPoint &point : stroke.points) {
            lowest = std::max(lowest, int(std::floor(point.y / RowHeight)));
        }
    }
    return std::max(LeastRows, lowest + 2);
}

QList<InkStroke> Ink::strokesIn(int row) const
{
    QList<InkStroke> found;
    for (const InkStroke &stroke : strokes) {
        if (stroke.row() == row) {
            found.append(stroke);
        }
    }
    return found;
}

QString Ink::digest(int row) const
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const InkStroke &stroke : strokesIn(row)) {
        hash.addData(pointsText(stroke).toUtf8());
        hash.addData("\n");
    }
    return QString::fromLatin1(hash.result().toHex().left(12));
}

QList<int> Ink::unread() const
{
    QList<int> found;
    for (const int row : rows()) {
        const auto reading = std::find_if(readings.cbegin(), readings.cend(), [row](const InkReading &r) {
            return r.row == row;
        });
        if (reading == readings.cend() || (!reading->fixed && reading->digest != digest(row))) {
            found.append(row);
        }
    }
    return found;
}

QString Ink::readText() const
{
    QList<InkReading> ordered = readings;
    std::sort(ordered.begin(), ordered.end(), [](const InkReading &a, const InkReading &b) {
        return a.row < b.row;
    });
    QStringList parts;
    for (const InkReading &reading : std::as_const(ordered)) {
        if (!reading.text.trimmed().isEmpty()) {
            parts.append(reading.text.simplified());
        }
    }
    return parts.join(QLatin1Char(' '));
}

QString Ink::readAlso() const
{
    const QStringList read = words(readText().toLower());
    QStringList also;
    for (const InkReading &reading : readings) {
        for (const QString &word : reading.also) {
            const QString plain = word.simplified();
            if (!plain.isEmpty() && !read.contains(plain.toLower()) && !also.contains(plain)) {
                also.append(plain);
            }
        }
    }
    return also.join(QLatin1Char(' '));
}

bool Ink::setReading(int row, const QString &rowDigest, const QString &text, const QStringList &also)
{
    if (digest(row) != rowDigest || !rows().contains(row)) {
        return false;
    }
    for (InkReading &reading : readings) {
        if (reading.row != row) {
            continue;
        }
        if (reading.fixed) {
            return false;
        }
        reading.text = text.simplified();
        reading.also = also;
        reading.digest = rowDigest;
        return true;
    }
    readings.append({row, text.simplified(), also, false, rowDigest});
    return true;
}

void Ink::fix(const QString &text)
{
    const QList<int> used = rows();
    readings.clear();
    for (const int row : used) {
        readings.append({row, row == used.first() ? text.simplified() : QString(), {}, true, digest(row)});
    }
}

void Ink::dropEmptyReadings()
{
    const QList<int> used = rows();
    readings.erase(std::remove_if(readings.begin(), readings.end(),
                                  [&used](const InkReading &reading) {
                                      return !used.contains(reading.row);
                                  }),
                   readings.end());
}

QList<InkStroke> Ink::eraseAt(const QPointF &at, qreal reach)
{
    QList<InkStroke> taken;
    for (auto it = strokes.begin(); it != strokes.end();) {
        if (it->touches(at, reach)) {
            taken.append(*it);
            it = strokes.erase(it);
        } else {
            ++it;
        }
    }
    return taken;
}

void Ink::restore(const QList<InkStroke> &taken)
{
    strokes += taken;
}

QByteArray Ink::toSvg() const
{
    QByteArray out;
    QXmlStreamWriter xml(&out);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(1);
    xml.writeStartDocument();
    // SVG as the drawing's own namespace, unprefixed, as viewers expect it.
    xml.writeDefaultNamespace(Svg);
    xml.writeNamespace(Own, QStringLiteral("gooseberry"));
    xml.writeStartElement(Svg, QStringLiteral("svg"));
    // The drawing is as tall as the writing, so it shows without the empty
    // lines the page keeps below it to go on with.
    qreal lowest = RowHeight;
    for (const InkStroke &stroke : strokes) {
        for (const QPointF &point : stroke.outline()) {
            lowest = std::max(lowest, point.y() + 6);
        }
    }
    const QString drawn = number(std::ceil(lowest), 0);
    xml.writeAttribute(QStringLiteral("width"), number(PageWidth, 0));
    xml.writeAttribute(QStringLiteral("height"), drawn);
    xml.writeAttribute(QStringLiteral("viewBox"), QStringLiteral("0 0 %1 %2").arg(number(PageWidth, 0), drawn));
    xml.writeAttribute(Own, QStringLiteral("format"), QStringLiteral("1"));
    xml.writeTextElement(Svg, QStringLiteral("title"), QStringLiteral("Handwriting"));
    if (!readText().isEmpty()) {
        // The reading, for anything that finds drawings by what they say.
        xml.writeTextElement(Svg, QStringLiteral("desc"), readText());
    }
    for (const int row : rows()) {
        xml.writeStartElement(Svg, QStringLiteral("g"));
        xml.writeAttribute(Own, QStringLiteral("row"), QString::number(row));
        const auto reading = std::find_if(readings.cbegin(), readings.cend(), [row](const InkReading &r) {
            return r.row == row;
        });
        if (reading != readings.cend()) {
            xml.writeAttribute(Own, QStringLiteral("read"), reading->text);
            if (!reading->also.isEmpty()) {
                xml.writeAttribute(Own, QStringLiteral("also"), reading->also.join(QLatin1Char(' ')));
            }
            if (reading->fixed) {
                xml.writeAttribute(Own, QStringLiteral("fixed"), QStringLiteral("true"));
            }
            xml.writeAttribute(Own, QStringLiteral("digest"), reading->digest);
        }
        for (const InkStroke &stroke : strokes) {
            if (stroke.row() != row) {
                continue;
            }
            xml.writeEmptyElement(Svg, QStringLiteral("path"));
            xml.writeAttribute(QStringLiteral("fill"), inkHex(stroke.colour));
            xml.writeAttribute(QStringLiteral("d"), pathData(stroke.outline()));
            xml.writeAttribute(Own, QStringLiteral("ink"), stroke.colour);
            xml.writeAttribute(Own, QStringLiteral("points"), pointsText(stroke));
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    return out;
}

Ink Ink::fromSvg(const QByteArray &svg)
{
    Ink ink;
    QXmlStreamReader xml(svg);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement) {
            continue;
        }
        const auto attributes = xml.attributes();
        if (xml.name() == QLatin1String("g") && attributes.hasAttribute(Own, QStringLiteral("row"))
            && attributes.hasAttribute(Own, QStringLiteral("digest"))) {
            InkReading reading;
            reading.row = attributes.value(Own, QStringLiteral("row")).toInt();
            reading.text = attributes.value(Own, QStringLiteral("read")).toString();
            reading.also = words(attributes.value(Own, QStringLiteral("also")).toString());
            reading.fixed = attributes.value(Own, QStringLiteral("fixed")) == QLatin1String("true");
            reading.digest = attributes.value(Own, QStringLiteral("digest")).toString();
            ink.readings.append(reading);
        } else if (xml.name() == QLatin1String("path") && attributes.hasAttribute(Own, QStringLiteral("points"))) {
            InkStroke stroke;
            const QString colour = attributes.value(Own, QStringLiteral("ink")).toString();
            stroke.colour = colour.isEmpty() ? QStringLiteral("black") : colour;
            stroke.points = pointsFrom(attributes.value(Own, QStringLiteral("points")).toString());
            if (!stroke.points.isEmpty()) {
                ink.strokes.append(stroke);
            }
        }
    }
    return ink;
}

QStringList inkNames()
{
    QStringList names;
    for (const auto &ink : Inks) {
        names.append(QString::fromLatin1(ink.name));
    }
    return names;
}

QString inkHex(const QString &name)
{
    for (const auto &ink : Inks) {
        if (name == QLatin1String(ink.name)) {
            return QString::fromLatin1(ink.hex);
        }
    }
    return QString::fromLatin1(Inks[0].hex);
}

} // namespace Gooseberry
