// SPDX-License-Identifier: GPL-2.0-or-later
#include "Notifier.h"

#include "Checklist.h"
#include "Log.h"
#include "NoteStore.h"
#include "Product.h"
#include "ReminderWords.h"
#include "Reminders.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QGuiApplication>

namespace Gooseberry {

namespace {

const QString Service = QStringLiteral("org.freedesktop.Notifications");
const QString Path = QStringLiteral("/org/freedesktop/Notifications");
const QString Interface = QStringLiteral("org.freedesktop.Notifications");

QString appId()
{
    const QString id = QGuiApplication::desktopFileName();
    return id.isEmpty() ? QStringLiteral("io.github.carlsonjm.Gooseberry") : id;
}

} // namespace

Notifier::Notifier(NoteStore *store, Reminders *reminders, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_reminders(reminders)
    , m_watcher(new QDBusServiceWatcher(Service, QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForOwnerChange, this))
{
    connect(m_reminders, &Reminders::due, this, &Notifier::show);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &Notifier::updateReady);
    QDBusConnection::sessionBus().connect(Service, Path, Interface, QStringLiteral("ActionInvoked"), this,
                                          SLOT(actionInvoked(uint, QString)));
    QDBusConnection::sessionBus().connect(Service, Path, Interface, QStringLiteral("NotificationClosed"), this,
                                          SLOT(closed(uint, uint)));
    updateReady();
}

void Notifier::updateReady()
{
    // Something shows notifications, or the bus starts it when asked.
    auto *bus = QDBusConnection::sessionBus().interface();
    bool ready = false;
    if (bus) {
        ready = bus->isServiceRegistered(Service).value() || bus->activatableServiceNames().value().contains(Service);
    }
    qCDebug(DESKTOP) << "notifications can be shown:" << ready;
    m_reminders->setReady(ready);
}

void Notifier::show(const QString &noteId)
{
    const auto note = m_store->note(noteId);
    if (!note) {
        return;
    }
    QString summary = note->title();
    if (Checklist::contains(note->text) && !Checklist::heading(note->text).isEmpty()) {
        summary = Checklist::heading(note->text);
    } else if (Checklist::contains(note->text)) {
        summary = Checklist::summary(note->text);
    }
    if (summary.isEmpty()) {
        summary = i18n("A note");
    }
    const QString when = note->remindOnOpen ? note->window : ReminderWords::label(note->remind, false);
    const QString body = when.isEmpty() || note->placeLabel() == when ? note->placeLabel()
                                                                      : i18nc("when · where", "%1 · %2", when, note->placeLabel());

    QDBusMessage notify = QDBusMessage::createMethodCall(Service, Path, Interface, QStringLiteral("Notify"));
    const QStringList actions = {
        QStringLiteral("default"), i18n("Open"),
        QStringLiteral("done"), i18n("Done"),
        QStringLiteral("again"), i18n("In 10 minutes"),
    };
    const QVariantMap hints = {
        {QStringLiteral("desktop-entry"), appId()},
        {QStringLiteral("urgency"), QVariant::fromValue(uchar(1))},
    };
    notify.setArguments({QGuiApplication::applicationDisplayName().isEmpty() ? productName()
                                                                              : QGuiApplication::applicationDisplayName(),
                         uint(0), appId(), summary, body.toHtmlEscaped(), actions, hints,
                         // The desktop's own time on screen, then its history:
                         // nothing here has to be closed.
                         int(-1)});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(notify), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, noteId] {
        const QDBusPendingReply<uint> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            qCWarning(DESKTOP) << "a reminder could not be shown:" << reply.error().message();
            return;
        }
        m_shown.insert(reply.value(), noteId);
    });
    qCDebug(DESKTOP) << "reminder shown for" << noteId;
}

void Notifier::actionInvoked(uint notification, const QString &action)
{
    const auto found = m_shown.constFind(notification);
    if (found == m_shown.constEnd()) {
        return;
    }
    const QString noteId = *found;
    if (action == QLatin1String("done")) {
        m_reminders->setDone(noteId, true);
    } else if (action == QLatin1String("again")) {
        m_reminders->remindAgainIn(noteId, AgainMinutes);
    } else if (action == QLatin1String("default")) {
        Q_EMIT openRequested(noteId);
    }
}

void Notifier::closed(uint notification, uint)
{
    m_shown.remove(notification);
}

} // namespace Gooseberry
