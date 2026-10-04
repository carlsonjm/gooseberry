// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QHash>
#include <QObject>

class QDBusServiceWatcher;

namespace Gooseberry {

class NoteStore;
class Reminders;

// Shows each due reminder as one of the desktop's standard notifications
// (the freedesktop.org notification interface on the session bus), so
// whatever shows notifications on this desktop shows it, and keeps it in its
// history. Tapping it opens the note; Done marks it done; In 10 minutes
// reminds once more ten minutes on. Where nothing on the bus shows
// notifications, reminders wait until something does.
class Notifier : public QObject
{
    Q_OBJECT

public:
    static constexpr int AgainMinutes = 10;

    Notifier(NoteStore *store, Reminders *reminders, QObject *parent = nullptr);

Q_SIGNALS:
    void openRequested(const QString &noteId);

private Q_SLOTS:
    void actionInvoked(uint notification, const QString &action);
    void closed(uint notification, uint reason);

private:
    void show(const QString &noteId);
    void updateReady();

    NoteStore *m_store;
    Reminders *m_reminders;
    QDBusServiceWatcher *m_watcher;
    // Notifications shown, and the note each is for.
    QHash<uint, QString> m_shown;
};

} // namespace Gooseberry
