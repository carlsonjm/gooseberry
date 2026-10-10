// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace Gooseberry {

// The note format version this program writes (docs/FORMAT.md). Notes of
// the first format are read too, and written in this one when they change.
inline constexpr int NoteFormat = 2;

// The board's name for the folder a note lands in when none is chosen: the
// notes folder itself.
QString inboxLabel();

// One note as it is kept on disk: a header and the Markdown text under it.
struct Note {
    QString id; // The file name without ".md".
    QString text;
    QString colour = QStringLiteral("butter");
    // The folder the note is kept in, by name; empty for Inbox. It is where
    // the file is, never a header key.
    QString folder;
    // Stuck to its window: the note comes back on it when it opens again.
    bool stuck = false;
    QString window; // The document or window the note was written on or stuck to.
    QString app; // That window's application id.
    QString workspace;
    // Where the note sits on its window when the window's notes are shown:
    // its top-left corner as fractions of the window's width and height, so
    // it keeps its place in proportion when the window is resized. Negative
    // when it has never been placed.
    QPointF place{-1, -1};
    // How big the note is drawn over its window, in the desktop's pixels, as
    // it was last resized there. Empty when it never was; it is then drawn at
    // the size every note starts at.
    QSizeF size;
    QDateTime created;
    QDateTime changed;
    bool tucked = false;
    // A reminder: at a time, which also puts the note on the planner, or the
    // next time its window or document opens. At most one of them is set.
    QDateTime remind;
    bool remindOnOpen = false;
    // When the reminder was shown. A reminder is shown once; setting a new
    // one clears this.
    QDateTime reminded;
    // When the note was marked done on the planner.
    QDateTime done;
    int format = NoteFormat;
    // The project a note of the first format belonged to, which names the
    // folder it is moved into. Never written.
    QString formerProject;
    // Header keys this version does not know, kept in order so a newer
    // program's fields survive an edit made here.
    QList<QPair<QString, QString>> extra;

    // True when the note was written by a newer format: shown, never changed.
    bool newerFormat() const { return format > NoteFormat; }

    bool hasReminder() const { return remind.isValid() || remindOnOpen; }
    // A reminder waiting to be shown.
    bool reminderWaiting() const { return hasReminder() && !reminded.isValid() && !done.isValid(); }
    // The day the note stands on the planner; invalid for a note with no time.
    QDate plannedDay() const { return remind.isValid() ? remind.toLocalTime().date() : QDate(); }

    // The first words: the first line with text, without Markdown markers.
    QString title() const;

    // The folder the note is kept in, in the words the board uses.
    QString placeLabel() const;
    // The board place of that folder: "inbox" or "folder:<name>".
    QString placeKey() const;
    // The window the note is stuck to, as the board names its place; empty
    // when it is stuck to none.
    QString stuckKey() const;
    bool isStuck() const { return stuck && !window.isEmpty(); }
    bool hasPlace() const { return place.x() >= 0 && place.y() >= 0; }
    bool hasSize() const { return size.width() > 0 && size.height() > 0; }
    // The smallest and largest size a note is kept at on its window.
    static constexpr int SmallestSide = 80;
    static constexpr int LargestSide = 4000;
    // Where the note is, in a few words: the window it is stuck to, or its
    // folder.
    QString whereLabel() const;

    QByteArray serialize() const;
    // Reads a note file. A file with no header is a note in its folder, unstuck; its times
    // come from fallbackTime.
    static Note parse(const QByteArray &bytes, const QString &id, const QDateTime &fallbackTime);
};

// The document part of a window title: "SpreadGesture.qml — Kate" is
// "SpreadGesture.qml". Unsaved-change markers are dropped so the label stays
// the same while the document is edited.
QString documentName(const QString &windowTitle, const QString &appName);

// The header's way of writing a text value, and of reading one back.
QString headerQuoted(const QString &value);
QString headerUnquoted(const QString &raw);

// The note colours, by the names the header stores, and the colour used when
// a header names one this version does not know.
QStringList colourNames();
QString colourHex(const QString &name);

} // namespace Gooseberry
