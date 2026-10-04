// SPDX-License-Identifier: GPL-2.0-or-later
#include "InkCanvas.h"

#include "Capture.h"

#include <QPainter>

namespace Gooseberry {

namespace {

QPainterPath pathOf(const InkStroke &stroke)
{
    QPainterPath path;
    path.addPolygon(QPolygonF(stroke.outline()));
    path.closeSubpath();
    return path;
}

} // namespace

InkCanvas::InkCanvas(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::widthChanged, this, &InkCanvas::pageChanged);
}

QObject *InkCanvas::capture() const
{
    return m_capture;
}

void InkCanvas::setCapture(QObject *capture)
{
    auto *found = qobject_cast<Capture *>(capture);
    if (found == m_capture) {
        return;
    }
    if (m_capture) {
        disconnect(m_capture, nullptr, this, nullptr);
    }
    m_capture = found;
    if (m_capture) {
        connect(m_capture, &Capture::inkChanged, this, &InkCanvas::inkChanged);
    }
    inkChanged();
    Q_EMIT captureChanged();
}

qreal InkCanvas::pageScale() const
{
    return width() > 0 ? width() / Ink::PageWidth : 1;
}

qreal InkCanvas::pageHeight() const
{
    return (m_capture ? m_capture->ink().height() : Ink::LeastRows * Ink::RowHeight) * pageScale();
}

void InkCanvas::inkChanged()
{
    const qsizetype count = m_capture ? m_capture->ink().strokes.size() : 0;
    // A stroke being written grows the list by one and changes only its last
    // stroke; anything else is drawn afresh.
    if (count < m_cachedStrokes || count > m_cachedStrokes + 1) {
        m_cachedStrokes = -1;
    }
    Q_EMIT pageChanged();
    update();
}

void InkCanvas::paint(QPainter *painter)
{
    const qreal scale = pageScale();
    painter->setRenderHint(QPainter::Antialiasing);
    if (m_ruled) {
        painter->setPen(QPen(QColor(0, 0, 0, 20), 1));
        for (qreal y = Ink::RowHeight * scale; y < height(); y += Ink::RowHeight * scale) {
            painter->drawLine(QPointF(0, std::round(y) - 0.5), QPointF(width(), std::round(y) - 0.5));
        }
    }
    if (!m_capture) {
        return;
    }
    const QList<InkStroke> &strokes = m_capture->ink().strokes;
    if (m_cachedStrokes < 0) {
        m_paths.clear();
        m_colours.clear();
        m_cachedStrokes = 0;
    }
    // Every stroke but the last is finished, and drawn from what is kept.
    while (m_cachedStrokes < strokes.size() - 1) {
        m_paths.append(pathOf(strokes.at(m_cachedStrokes)));
        m_colours.append(QColor(inkHex(strokes.at(m_cachedStrokes).colour)));
        ++m_cachedStrokes;
    }
    painter->save();
    painter->scale(scale, scale);
    painter->setPen(Qt::NoPen);
    for (qsizetype i = 0; i < m_paths.size(); ++i) {
        painter->setBrush(m_colours.at(i));
        painter->drawPath(m_paths.at(i));
    }
    if (!strokes.isEmpty()) {
        painter->setBrush(QColor(inkHex(strokes.last().colour)));
        painter->drawPath(pathOf(strokes.last()));
    }
    painter->restore();
}

} // namespace Gooseberry
