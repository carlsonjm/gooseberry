// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

namespace Gooseberry {

class Capture;

// The ink page on the card: the ruled lines and the note's strokes, drawn as
// they are written, at whatever width the page has. Input is the screen's;
// this only draws.
class InkCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *capture READ capture WRITE setCapture NOTIFY captureChanged)
    Q_PROPERTY(bool ruled MEMBER m_ruled NOTIFY ruledChanged)
    // Screen pixels to a page unit, and the page's height at this width.
    Q_PROPERTY(qreal scale READ pageScale NOTIFY pageChanged)
    Q_PROPERTY(qreal pageHeight READ pageHeight NOTIFY pageChanged)

public:
    explicit InkCanvas(QQuickItem *parent = nullptr);

    QObject *capture() const;
    void setCapture(QObject *capture);
    qreal pageScale() const;
    qreal pageHeight() const;

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void captureChanged();
    void ruledChanged();
    void pageChanged();

private:
    void inkChanged();

    Capture *m_capture = nullptr;
    bool m_ruled = true;
    // Finished strokes drawn once, kept until the ink changes other than by
    // the stroke being written.
    QList<QPainterPath> m_paths;
    QList<QColor> m_colours;
    qsizetype m_cachedStrokes = -1;
};

} // namespace Gooseberry
