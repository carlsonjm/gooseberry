// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

namespace Gooseberry {

// What the person sees the program called. Where Shuffle is installed it is
// Notes; on any other Plasma desktop it is Gooseberry (docs/DECISIONS.md). Ids,
// the notes folder and the desktop file keep Gooseberry's own name either way.
QString productName();
void setInsideShuffle(bool inside);
bool insideShuffle();

// Whether Shuffle's own desktop is installed for this person. Gooseberry only
// looks; it never depends on Shuffle.
bool shuffleInstalled();

} // namespace Gooseberry
