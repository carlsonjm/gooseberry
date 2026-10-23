// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Note.h"

namespace Gooseberry {

class NoteStore;

// The note a brand new notes folder starts with: what Gooseberry can do, in
// a few plain lines, with a day and a #word in it so the note shows its own
// offers. It is an ordinary note, kept in Inbox and stuck to nothing.
Note welcomeNote();

// Keeps the welcome note when the store's folder is fresh, and only then.
// Returns its id, or an empty string when none was kept.
QString welcomeIfFresh(NoteStore &store);

} // namespace Gooseberry
