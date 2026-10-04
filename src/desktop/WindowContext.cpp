// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowContext.h"

#include "Log.h"

#include <taskmanager/abstracttasksmodel.h>
#include <taskmanager/virtualdesktopinfo.h>
#include <taskmanager/windowtasksmodel.h>

#include <KWindowSystem>

namespace Gooseberry {

using TaskManager::AbstractTasksModel;

WindowContext::WindowContext(const QString &ownAppId, QObject *parent)
    : QObject(parent)
    , m_ownAppId(ownAppId)
{
    // Plasma's window list answers only on a desktop session it knows.
    if (!KWindowSystem::isPlatformWayland() && !KWindowSystem::isPlatformX11()) {
        qCDebug(DESKTOP) << "no desktop session: notes start Loose";
        return;
    }
    m_windows = std::make_unique<TaskManager::WindowTasksModel>();
    m_desktops = std::make_unique<TaskManager::VirtualDesktopInfo>();
    connect(m_windows.get(), &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
                if (roles.isEmpty() || roles.contains(AbstractTasksModel::IsActive) || roles.contains(Qt::DisplayRole)) {
                    activeChanged();
                }
            });
    connect(m_windows.get(), &QAbstractItemModel::rowsInserted, this, &WindowContext::activeChanged);
    connect(m_windows.get(), &QAbstractItemModel::modelReset, this, &WindowContext::activeChanged);
    connect(m_windows.get(), &QAbstractItemModel::rowsRemoved, this, &WindowContext::forgetClosed);
    activeChanged();
}

WindowContext::~WindowContext() = default;

void WindowContext::activeChanged()
{
    if (!m_windows) {
        return;
    }
    for (int row = 0; row < m_windows->rowCount(); ++row) {
        const QModelIndex index = m_windows->index(row, 0);
        if (!index.data(AbstractTasksModel::IsActive).toBool()) {
            continue;
        }
        QString appId = index.data(AbstractTasksModel::AppId).toString();
        // Plasma names some applications by their desktop file, ending included.
        if (appId.endsWith(QLatin1String(".desktop"))) {
            appId.chop(8);
        }
        // Gooseberry's own board is never what a note is about; the window
        // before it stays the one in front.
        if (appId == m_ownAppId) {
            return;
        }
        if (appId != m_appId || index.data(Qt::DisplayRole).toString() != m_title) {
            qCDebug(DESKTOP) << "window in front:" << appId << index.data(Qt::DisplayRole).toString() << "of" << m_windows->rowCount();
        }
        m_appId = appId;
        m_appName = index.data(AbstractTasksModel::AppName).toString();
        m_title = index.data(Qt::DisplayRole).toString();
        // A new title in the same window is not a change of work.
        if (QPersistentModelIndex(index) != m_active) {
            m_active = index;
            Q_EMIT workChanged();
        }
        return;
    }
}

void WindowContext::forgetClosed()
{
    // A note is not offered to a window that has since closed.
    for (int row = 0; row < m_windows->rowCount(); ++row) {
        const QModelIndex index = m_windows->index(row, 0);
        if (index.data(Qt::DisplayRole).toString() == m_title && index.data(AbstractTasksModel::AppId).toString().startsWith(m_appId)) {
            return;
        }
    }
    m_title.clear();
    m_appId.clear();
    m_appName.clear();
    activeChanged();
}

CaptureContext WindowContext::current() const
{
    CaptureContext context;
    if (!m_windows) {
        return context;
    }
    if (!m_title.isEmpty()) {
        context.window = documentName(m_title, m_appName);
        context.app = m_appId;
    }
    const QVariantList ids = m_desktops->desktopIds();
    const QStringList names = m_desktops->desktopNames();
    const qsizetype at = ids.indexOf(m_desktops->currentDesktop());
    if (at >= 0 && at < names.size()) {
        context.workspace = names.at(at);
    }
    return context;
}

} // namespace Gooseberry
