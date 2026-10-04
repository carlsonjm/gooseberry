// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace Gooseberry {

// A checklist is the note's text with Markdown task lines in it, "- [ ] oats"
// and "- [x] oats", so any Markdown reader shows the same list. Every line of
// the text is a line here, items and plain lines alike, in order.
struct ChecklistLine {
    QString text; // Without the marker.
    bool item = false;
    bool checked = false;
};

namespace Checklist {

QList<ChecklistLine> lines(const QString &text);
// True when the text has at least one item.
bool contains(const QString &text);

// The text as a checklist: with more than one line the first stays as the
// list's heading, and every other line with words becomes an item. An empty
// text becomes one empty item.
QString from(const QString &text);
// The text with every item's marker taken away.
QString toPlain(const QString &text);

// One line changed, its marker kept as it was written.
QString withLineText(const QString &text, int line, const QString &lineText);
QString withChecked(const QString &text, int line, bool checked);
// A new empty item after the line, or at the start for -1.
QString withItemAfter(const QString &text, int line);
QString withoutLine(const QString &text, int line);

// The list in one line, as the board shows it: the heading, then the items
// joined by " · ".
QString heading(const QString &text);
QString summary(const QString &text);

} // namespace Checklist

} // namespace Gooseberry
