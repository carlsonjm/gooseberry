// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace Gooseberry {

// How tall Shuffle's Context Bar stands along the top of a display, read from
// its clockExtent on the session bus (Shuffle's CONTEXT-BAR-CONTRACT.md
// § Clock extent), as the desktop's search reads it. The quick-note card's
// surface covers the whole display, so with the keys up the card keeps a
// gutter below the bar as it keeps one above the keys. Without a bar, on any
// other desktop, the height is 0.
class ContextBar : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int height READ height NOTIFY heightChanged)

public:
    explicit ContextBar(QObject *parent = nullptr);

    int height() const { return m_height; }

    // Reads the bar on the display of that name, or on the first display
    // with one where the name is empty.
    void read(const QString &outputName);

    // The bar's height from a clockExtent payload; 0 for one that is not
    // presenting, of another schema or version, or unreadable.
    static int heightFrom(const QByteArray &payload);

Q_SIGNALS:
    void heightChanged();

private:
    void ask(const QString &outputName, quint64 generation, int timeout, bool retried);

    int m_height = 0;
    quint64 m_generation = 0;
};

} // namespace Gooseberry
