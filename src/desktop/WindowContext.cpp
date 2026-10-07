// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowContext.h"

#include "Log.h"

#include <taskmanager/abstracttasksmodel.h>
#include <taskmanager/virtualdesktopinfo.h>
#include <taskmanager/windowtasksmodel.h>

#include <KWindowSystem>

#include <QTimer>

namespace Gooseberry {

using TaskManager::AbstractTasksModel;

WindowContext::WindowContext(const QString &ownAppId, QObject *parent)
    : QObject(parent)
    , m_ownAppId(ownAppId)
{
    // Plasma's window list answers only on a desktop session it knows.
    if (!KWindowSystem::isPlatformWayland() && !KWindowSystem::isPlatformX11()) {
        qCDebug(DESKTOP) << "no desktop session: notes start in their folder, unstuck";
        return;
    }
    m_windows = std::make_unique<TaskManager::WindowTasksModel>();
    m_desktops = std::make_unique<TaskManager::VirtualDesktopInfo>();
    connect(m_windows.get(), &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
                if (roles.isEmpty() || roles.contains(AbstractTasksModel::IsActive) || roles.contains(Qt::DisplayRole)) {
                    activeChanged();
                }
                if (roles.isEmpty() || roles.contains(Qt::DisplayRole) || roles.contains(AbstractTasksModel::AppId)) {
                    noticeDocuments();
                }
                Q_EMIT windowsChanged();
            });
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved}) {
        connect(m_windows.get(), signal, this, &WindowContext::windowsChanged);
    }
    connect(m_windows.get(), &QAbstractItemModel::modelReset, this, &WindowContext::windowsChanged);
    connect(m_windows.get(), &QAbstractItemModel::rowsInserted, this, &WindowContext::activeChanged);
    connect(m_windows.get(), &QAbstractItemModel::modelReset, this, &WindowContext::activeChanged);
    connect(m_windows.get(), &QAbstractItemModel::rowsRemoved, this, &WindowContext::forgetClosed);
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved}) {
        connect(m_windows.get(), signal, this, &WindowContext::noticeDocuments);
    }
    connect(m_windows.get(), &QAbstractItemModel::modelReset, this, &WindowContext::noticeDocuments);
    activeChanged();
    noticeDocuments();
    // The desktop reports the windows already open one by one as Gooseberry
    // starts; they were open before it was. Only what opens after they have
    // been told is news.
    QTimer::singleShot(3000, this, [this] {
        m_documentsKnown = true;
        noticeDocuments();
    });
}

void WindowContext::noticeDocuments()
{
    QSet<QPair<QString, QString>> documents;
    for (int row = 0; row < m_windows->rowCount(); ++row) {
        const QModelIndex index = m_windows->index(row, 0);
        QString appId = index.data(AbstractTasksModel::AppId).toString();
        if (appId.endsWith(QLatin1String(".desktop"))) {
            appId.chop(8);
        }
        const QString title = index.data(Qt::DisplayRole).toString();
        if (appId == m_ownAppId || title.isEmpty()) {
            continue;
        }
        documents.insert({appId, documentName(title, index.data(AbstractTasksModel::AppName).toString())});
    }
    const QSet<QPair<QString, QString>> before = std::exchange(m_documents, documents);
    if (!m_documentsKnown) {
        return;
    }
    for (const auto &[app, window] : std::as_const(documents)) {
        if (!before.contains({app, window})) {
            qCDebug(DESKTOP) << "opened:" << window << "in" << app;
            Q_EMIT documentOpened(window, app);
        }
    }
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
        // A new title in the same window is not a change of work, and nor is
        // the desktop's first report of what is in front, which can arrive
        // after a card opened on the first tap.
        if (QPersistentModelIndex(index) != m_active) {
            const bool known = m_active.isValid();
            m_active = index;
            if (known) {
                Q_EMIT workChanged();
            } else {
                Q_EMIT firstReported();
            }
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

QVariantList WindowContext::openWindows() const
{
    QVariantList list;
    if (!m_windows) {
        return list;
    }
    const CaptureContext front = current();
    QSet<QPair<QString, QString>> seen;
    for (int row = 0; row < m_windows->rowCount(); ++row) {
        const QModelIndex index = m_windows->index(row, 0);
        QString appId = index.data(AbstractTasksModel::AppId).toString();
        if (appId.endsWith(QLatin1String(".desktop"))) {
            appId.chop(8);
        }
        const QString title = index.data(Qt::DisplayRole).toString();
        if (appId == m_ownAppId || title.isEmpty()) {
            continue;
        }
        const QString appName = index.data(AbstractTasksModel::AppName).toString();
        const QString window = documentName(title, appName);
        if (seen.contains({appId, window})) {
            continue;
        }
        seen.insert({appId, window});
        const bool inFront = window == front.window && appId == front.app;
        const QVariantMap entry{{QStringLiteral("window"), window},
                                {QStringLiteral("app"), appId},
                                {QStringLiteral("appName"), appName},
                                {QStringLiteral("front"), inFront}};
        if (inFront) {
            list.prepend(entry);
        } else {
            list.append(entry);
        }
    }
    return list;
}

QList<OpenWindow> WindowContext::windows() const
{
    QList<OpenWindow> list;
    if (!m_windows) {
        return list;
    }
    for (int row = 0; row < m_windows->rowCount(); ++row) {
        const QModelIndex index = m_windows->index(row, 0);
        OpenWindow window;
        window.app = index.data(AbstractTasksModel::AppId).toString();
        if (window.app.endsWith(QLatin1String(".desktop"))) {
            window.app.chop(8);
        }
        window.caption = index.data(Qt::DisplayRole).toString();
        if (window.app == m_ownAppId || window.caption.isEmpty()) {
            continue;
        }
        window.window = documentName(window.caption, index.data(AbstractTasksModel::AppName).toString());
        // On Wayland the compositor's id for the window; on X11 its number.
        for (const QVariant &id : index.data(AbstractTasksModel::WinIdList).toList()) {
            const QString text = id.toString();
            if (!text.isEmpty()) {
                window.ids.append(text);
            }
        }
        window.geometry = index.data(AbstractTasksModel::Geometry).toRect();
        window.active = index.data(AbstractTasksModel::IsActive).toBool();
        list.append(window);
    }
    return list;
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
