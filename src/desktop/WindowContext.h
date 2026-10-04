// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Capture.h"

#include <QObject>
#include <QPersistentModelIndex>
#include <memory>

namespace TaskManager {
class WindowTasksModel;
class VirtualDesktopInfo;
}

namespace Gooseberry {

// What Robin is working on, as the desktop reports it: the last window in
// front that is not Gooseberry's own, and the current workspace. Where the
// desktop reports nothing, both stay empty and notes start Loose.
class WindowContext : public QObject
{
    Q_OBJECT

public:
    explicit WindowContext(const QString &ownAppId, QObject *parent = nullptr);
    ~WindowContext() override;

    CaptureContext current() const;

Q_SIGNALS:
    // Another window came to the front: Robin went back to the work.
    void workChanged();
    // The desktop said for the first time what is in front.
    void firstReported();

private:
    void activeChanged();
    void forgetClosed();

    QString m_ownAppId;
    std::unique_ptr<TaskManager::WindowTasksModel> m_windows;
    std::unique_ptr<TaskManager::VirtualDesktopInfo> m_desktops;
    QPersistentModelIndex m_active;
    QString m_title;
    QString m_appId;
    QString m_appName;
};

} // namespace Gooseberry
