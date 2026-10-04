// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

namespace Gooseberry::Trash {

// The desktop's trash in the person's home, as the freedesktop.org trash
// specification lays it out and Plasma's trash reads it.
QString homeTrash();

// Moves a file to the desktop's trash, recording where it came from so the
// trash can put it back. Returns where it went, or an empty string.
QString move(const QString &path);

// Puts a file back from where move() sent it.
bool restore(const QString &pathInTrash, const QString &path);

} // namespace Gooseberry::Trash
