// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Note.h"

#include <QList>
#include <QRect>
#include <QSet>
#include <QStringList>
#include <QVariantList>

namespace Gooseberry {

// One window open on the desktop, as the desktop's window list reports it.
struct OpenWindow {
    // The desktop's ids for the window: on Wayland the compositor's own id,
    // a UUID with braces; on X11 the window id in decimal.
    QStringList ids;
    QString caption; // The title as the desktop reports it.
    QString app; // The application, by desktop file name without ".desktop".
    QString window; // The document it shows, as a note's window names it.
    QRect geometry; // Where it is on the desktop, its frame included.
    bool active = false;

    // Tells this window apart from the others while it is open.
    QString key() const { return ids.isEmpty() ? app + QLatin1Char('\n') + caption : ids.constFirst(); }
};

// Which open windows have notes stuck to them, as the desktop's title bar
// and Spread are told (docs/DESKTOP.md § Stuck notes on the bus).
namespace StuckWindows {

// The notes stuck to the window and not tucked away, the one changed last
// first.
QList<Note> notesOn(const OpenWindow &window, const QList<Note> &notes);

// One entry for each window with notes, as the bus gives it; shownKeys are
// the keys of the windows whose notes are up.
QVariantList entries(const QList<OpenWindow> &windows, const QList<Note> &notes, const QSet<QString> &shownKeys);

// The window a caller means: by one of its ids when it is given and open,
// otherwise by its caption and application. -1 when none matches. Only
// windows for which accept is true are considered.
qsizetype find(const QList<OpenWindow> &windows, const QString &windowId, const QString &caption, const QString &app);

// A title without the " <2>" the compositor adds to tell same-titled
// windows apart.
QString bareCaption(const QString &caption);
// Two application names the same, as a title bar and the window list may
// spell them: without case, or one ending with the other after a dot.
bool sameApp(const QString &a, const QString &b);

} // namespace StuckWindows

} // namespace Gooseberry
