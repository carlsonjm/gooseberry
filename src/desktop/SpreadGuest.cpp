// SPDX-License-Identifier: GPL-2.0-or-later
#include "SpreadGuest.h"

#include "Log.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegion>
#include <QScreen>
#include <QWindow>

namespace Gooseberry {

namespace {

const QString Kadunce = QStringLiteral("org.kde.KWin");
const QString KadunceObject = QStringLiteral("/Kadunce");
const QString KadunceInterface = QStringLiteral("co.goodinput.Kadunce");
const QString OwnInterface = QStringLiteral("io.github.carlsonjm.Gooseberry.CompanionGuest");

// Room round the card a finger may land in and still be the card's.
constexpr int InputSafety = 16;

QDBusMessage kadunce(const QString &method)
{
    return QDBusMessage::createMethodCall(Kadunce, KadunceObject, KadunceInterface, method);
}

QRect rectFrom(const QJsonValue &value)
{
    const QJsonObject rect = value.toObject();
    return QRect(rect.value(QStringLiteral("x")).toInt(), rect.value(QStringLiteral("y")).toInt(),
                 rect.value(QStringLiteral("width")).toInt(), rect.value(QStringLiteral("height")).toInt());
}

// Kadunce's workspace snapshot, or an empty object where Kadunce is not
// running or answers with a schema this does not know.
QJsonObject workspaceContext()
{
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(
        kadunce(QStringLiteral("workspaceContext")), QDBus::Block, 350);
    if (!reply.isValid()) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(reply.value().toUtf8()).object();
    if (root.value(QStringLiteral("schema")).toString() != QLatin1String("co.goodinput.kadunce.workspace-context")
        || root.value(QStringLiteral("version")).toInt() != 1) {
        return {};
    }
    return root;
}

} // namespace

SpreadGuest::SpreadGuest(QObject *parent)
    : QObject(parent)
{
}

bool SpreadGuest::publish()
{
    return QDBusConnection::sessionBus().registerObject(QString::fromLatin1(Path), this,
                                                        QDBusConnection::ExportScriptableSlots);
}

bool SpreadGuest::begin(QWindow *window)
{
    m_window = window;
    if (m_active) {
        return true;
    }
    auto *bus = QDBusConnection::sessionBus().interface();
    if (!bus || !bus->isServiceRegistered(Kadunce)) {
        return false;
    }
    const QJsonObject context = workspaceContext();
    const QJsonObject stage = context.value(QStringLiteral("cardStage")).toObject();
    const QString presentation = stage.value(QStringLiteral("presentation")).toString();
    if (presentation != QLatin1String("cardLine")) {
        return false;
    }
    const QDBusReply<int> protocol = QDBusConnection::sessionBus().call(
        kadunce(QStringLiteral("companionGuestProtocolVersion")), QDBus::Block, 350);
    if (!protocol.isValid() || protocol.value() != Protocol) {
        qCDebug(DESKTOP) << "Kadunce offers no companion guest this version knows";
        return false;
    }
    QDBusMessage request = kadunce(QStringLiteral("beginCompanionGuest"));
    request.setArguments({QDBusConnection::sessionBus().baseService(), QString::fromLatin1(Path), OwnInterface});
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(request, QDBus::Block, 700);
    if (!reply.isValid()) {
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(reply.value().toUtf8()).object();
    if (root.value(QStringLiteral("protocol")).toInt() != Protocol || !root.value(QStringLiteral("accepted")).toBool()) {
        qCDebug(DESKTOP) << "Kadunce kept Spread's centre";
        return false;
    }
    const QString output = root.value(QStringLiteral("output")).toString();
    QScreen *screen = nullptr;
    const auto screens = QGuiApplication::screens();
    for (QScreen *candidate : screens) {
        if (candidate->name() == output) {
            screen = candidate;
        }
    }
    const QRect card = rectFrom(root.value(QStringLiteral("card")));
    if (!screen || !card.isValid()) {
        // Granted a place it cannot draw in, the card hands the centre back.
        m_active = true;
        end();
        return false;
    }
    // The surface covers Kadunce's display, so its rectangles are taken as
    // they come, less where that display starts.
    if (window->screen() != screen) {
        window->setScreen(screen);
    }
    m_cardRect = card.translated(-screen->geometry().topLeft());
    m_active = true;
    setMask();
    qCDebug(DESKTOP) << "holding Spread's centre at" << m_cardRect;
    Q_EMIT changed();
    return true;
}

void SpreadGuest::setMask()
{
    if (!m_window) {
        return;
    }
    if (!m_active) {
        m_window->setMask(QRegion());
    } else {
        // Outside the card, presses are Spread's.
        m_window->setMask(QRegion(m_cardRect.adjusted(-InputSafety, -InputSafety, InputSafety, InputSafety)));
    }
}

void SpreadGuest::leave()
{
    m_active = false;
    setMask();
    Q_EMIT changed();
}

void SpreadGuest::end()
{
    if (!m_active) {
        return;
    }
    QDBusConnection::sessionBus().asyncCall(kadunce(QStringLiteral("endLauncherGuest")));
    leave();
}

void SpreadGuest::dismissGuest()
{
    if (!m_active) {
        return;
    }
    qCDebug(DESKTOP) << "Kadunce gave Spread's centre to another guest";
    // Kadunce has already ended this guest; nothing is asked back.
    leave();
    Q_EMIT dismissed();
}

} // namespace Gooseberry
