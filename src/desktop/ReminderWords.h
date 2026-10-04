// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDateTime>
#include <QVariantList>

namespace Gooseberry {

// The words for a reminder, the same on the card, in the reminder itself and
// for a search that hosts the quick note.
namespace ReminderWords {

// "Today 4:00 PM", "Tomorrow 9:00 AM", "Monday 9:00 AM", "5 November 9:00 AM",
// or "Next time this opens".
QString label(const QDateTime &remind, bool onOpen, const QDateTime &now = QDateTime::currentDateTime());

// What Remind offers, in order: each a map of kind ("later", "evening",
// "tomorrow", "opens" or "pick"), label and, for a time, the time.
// "Next time this opens" only where the note was written on a window.
QVariantList choices(bool hasWindow, const QDateTime &now = QDateTime::currentDateTime());

} // namespace ReminderWords

} // namespace Gooseberry
