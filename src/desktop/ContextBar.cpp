// SPDX-License-Identifier: GPL-2.0-or-later
#include "ContextBar.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace Gooseberry {

ContextBar::ContextBar(QObject *parent)
    : QObject(parent)
{
}

int ContextBar::heightFrom(const QByteArray &payload)
{
    const QJsonObject extent = QJsonDocument::fromJson(payload).object();
    if (extent.value(QStringLiteral("schema")).toString() != QStringLiteral("co.goodinput.shuffle.clock-extent")
        || extent.value(QStringLiteral("version")).toInt() != 1 || !extent.value(QStringLiteral("presenting")).toBool()) {
        return 0;
    }
    return std::max(0, extent.value(QStringLiteral("bar")).toObject().value(QStringLiteral("height")).toInt());
}

void ContextBar::read(const QString &outputName)
{
    ask(outputName, ++m_generation, 250, false);
}

// The bar's shell can be slow to answer just after sign-in. An unanswered read
// keeps the height already known and asks once more, longer; only an answer,
// or no bar on the bus at all, changes it.
void ContextBar::ask(const QString &outputName, quint64 generation, int timeout, bool retried)
{
    QDBusMessage request = QDBusMessage::createMethodCall(QStringLiteral("co.goodinput.ContextBar"), QStringLiteral("/ContextBar"),
                                                          QStringLiteral("co.goodinput.ContextBar"), QStringLiteral("clockExtent"));
    request << outputName;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, timeout), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, outputName, generation, retried] {
        watcher->deleteLater();
        if (generation != m_generation) {
            return;
        }
        const QDBusPendingReply<QString> reply = *watcher;
        if (reply.isError() && reply.error().type() != QDBusError::ServiceUnknown) {
            if (!retried) {
                ask(outputName, generation, 1500, true);
            }
            return;
        }
        const int height = reply.isError() ? 0 : heightFrom(reply.value().toUtf8());
        if (height != m_height) {
            m_height = height;
            Q_EMIT heightChanged();
        }
    });
}

} // namespace Gooseberry
