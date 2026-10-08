// SPDX-License-Identifier: GPL-2.0-or-later
#include "Product.h"

#include <QStandardPaths>

namespace Gooseberry {

namespace {
bool s_insideShuffle = false;
}

QString productName()
{
    return s_insideShuffle ? QStringLiteral("Notes") : QStringLiteral("Gooseberry");
}

void setInsideShuffle(bool inside)
{
    s_insideShuffle = inside;
}

bool insideShuffle()
{
    return s_insideShuffle;
}

bool shuffleInstalled()
{
    // Shuffle's bottom surface is the one part every Shuffle install has.
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   QStringLiteral("plasma/plasmoids/co.goodinput.shuffle.bottomsurface"),
                                   QStandardPaths::LocateDirectory)
                .isEmpty();
}

} // namespace Gooseberry
