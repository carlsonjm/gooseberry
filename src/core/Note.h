// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>

namespace Gooseberry {

// The note format version this program reads and writes (docs/FORMAT.md).
inline constexpr int NoteFormat = 1;

enum class Belongs {
    Loose,
    Window,
    Project,
    Workspace,
};

QString belongsName(Belongs belongs);
Belongs belongsFromName(const QString &name);

// One note as it is kept on disk: a header and the Markdown text under it.
struct Note {
    QString id; // The file name without ".md".
    QString text;
    QString colour = QStringLiteral("butter");
    Belongs belongs = Belongs::Loose;
    QString window; // The document or window the note was written on.
    QString app; // That window's application id.
    QString project;
    QString workspace;
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

    // What the note belongs to, in the words the board uses.
    QString placeLabel() const;
    // The board place this note sits in when it is not tucked away.
    QString placeKey() const;

    QByteArray serialize() const;
    // Reads a note file. A file with no header is a Loose note; its times
    // come from fallbackTime.
    static Note parse(const QByteArray &bytes, const QString &id, const QDateTime &fallbackTime);
};

// The document part of a window title: "SpreadGesture.qml — Kate" is
// "SpreadGesture.qml". Unsaved-change markers are dropped so the label stays
// the same while the document is edited.
QString documentName(const QString &windowTitle, const QString &appName);

// The note colours, by the names the header stores, and the colour used when
// a header names one this version does not know.
QStringList colourNames();
QString colourHex(const QString &name);

} // namespace Gooseberry
