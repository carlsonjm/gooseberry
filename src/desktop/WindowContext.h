// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Capture.h"

#include <QObject>
#include <QPersistentModelIndex>
#include <QSet>
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
    // A window showing this document appeared, or a window came to show it,
    // where none did before. Windows already open when Gooseberry started
    // are not news.
    void documentOpened(const QString &window, const QString &app);

private:
    void activeChanged();
    void forgetClosed();
    void noticeDocuments();

    QString m_ownAppId;
    std::unique_ptr<TaskManager::WindowTasksModel> m_windows;
    std::unique_ptr<TaskManager::VirtualDesktopInfo> m_desktops;
    QPersistentModelIndex m_active;
    QString m_title;
    QString m_appId;
    QString m_appName;
    // Every document shown in a window, as application and name.
    QSet<QPair<QString, QString>> m_documents;
    bool m_documentsKnown = false;
};

} // namespace Gooseberry
