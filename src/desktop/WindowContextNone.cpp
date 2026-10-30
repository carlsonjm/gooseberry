// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowContext.h"

// Built when Plasma's window list is left out, as in the Flatpak, where the
// sandbox keeps other programs' windows out of sight. Nothing is reported, so
// every note starts in its folder, unstuck, as on a desktop that reports
// nothing.

// The window list's types, never made here; the members holding them stay
// empty.
namespace TaskManager {
class WindowTasksModel
{
};
class VirtualDesktopInfo
{
};
}

namespace Gooseberry {

WindowContext::WindowContext(const QString &ownAppId, QObject *parent)
    : QObject(parent)
    , m_ownAppId(ownAppId)
{
}

WindowContext::~WindowContext() = default;

CaptureContext WindowContext::current() const
{
    return {};
}

QVariantList WindowContext::openWindows() const
{
    return {};
}

QList<OpenWindow> WindowContext::windows() const
{
    return {};
}

void WindowContext::activeChanged()
{
}

void WindowContext::forgetClosed()
{
}

void WindowContext::noticeDocuments()
{
}

} // namespace Gooseberry
