// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QLoggingCategory>

// What the desktop told Gooseberry, for finding out why a note started where
// it did. Off unless QT_LOGGING_RULES="gooseberry.desktop.debug=true".
Q_DECLARE_LOGGING_CATEGORY(DESKTOP)
