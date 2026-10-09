// SPDX-License-Identifier: GPL-2.0-or-later
#include "SpreadGuest.h"

#include "Log.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QGuiApplication>
#include <QJsonArray>
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

QString withoutDesktop(QString id)
{
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    return id.toLower();
}

} // namespace

SpreadGuest::SpreadGuest(QObject *parent)
    : QObject(parent)
{
    m_watchTimeout.setSingleShot(true);
    connect(&m_watchTimeout, &QTimer::timeout, this, [this] {
        stopWatching();
        qCDebug(DESKTOP) << "the board was not seen in the Active card's place";
        Q_EMIT watchGaveUp();
    });
}

bool SpreadGuest::publish()
{
    return QDBusConnection::sessionBus().registerObject(QString::fromLatin1(Path), this,
                                                        QDBusConnection::ExportScriptableSlots);
}

bool SpreadGuest::begin(QWindow *window)
{
    m_window = window;
    m_overActiveCard = false;
    if (m_active) {
        return true;
    }
    m_held = false;
    // A card that handed its place to the board kept that place's input
    // while it faded; a new card starts with the whole surface again.
    window->setMask(QRegion());
    auto *bus = QDBusConnection::sessionBus().interface();
    if (!bus || !bus->isServiceRegistered(Kadunce)) {
        return false;
    }
    const QJsonObject context = workspaceContext();
    const QJsonObject stage = context.value(QStringLiteral("cardStage")).toObject();
    const QString presentation = stage.value(QStringLiteral("presentation")).toString();
    if (presentation == QLatin1String("active")) {
        m_overActiveCard = true;
        qCDebug(DESKTOP) << "over an Active card: no guest";
        return false;
    }
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
    const QPoint origin = screen->geometry().topLeft();
    m_cardRect = card.translated(-origin);
    const QRect active = rectFrom(root.value(QStringLiteral("active"))).translated(-origin);
    m_activeRect = root.value(QStringLiteral("presentationCapability")).toInt() == 1
            && QRect(QPoint(), screen->geometry().size()).contains(active)
        ? active
        : QRect();
    m_active = true;
    m_expanded = false;
    ++m_generation;
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
    } else if (m_expanded) {
        m_window->setMask(QRegion(m_activeRect));
    } else {
        // Outside the card, presses are Spread's.
        m_window->setMask(QRegion(m_cardRect.adjusted(-InputSafety, -InputSafety, InputSafety, InputSafety)));
    }
}

void SpreadGuest::leave(bool keepPlace)
{
    ++m_generation;
    // A card fading where it stood keeps that place's input until it is gone.
    m_held = keepPlace && m_expanded;
    m_active = false;
    m_expanded = false;
    m_launchToken.clear();
    if (!m_held) {
        setMask();
    }
    Q_EMIT changed();
}

void SpreadGuest::end(bool keepPlace)
{
    if (!m_active) {
        if (m_held && !keepPlace) {
            m_held = false;
            Q_EMIT changed();
        }
        return;
    }
    QDBusConnection::sessionBus().asyncCall(kadunce(QStringLiteral("endLauncherGuest")));
    leave(keepPlace);
}

void SpreadGuest::expand()
{
    if (!m_active || !m_activeRect.isValid()) {
        end();
        Q_EMIT expandAnswered(false);
        return;
    }
    const quint64 generation = ++m_generation;
    QDBusMessage request = kadunce(QStringLiteral("setLauncherGuestExpanded"));
    request.setArguments({true});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 700), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
        const QDBusPendingReply<bool> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation || !m_active) {
            return;
        }
        if (reply.isError() || !reply.value()) {
            end();
            Q_EMIT expandAnswered(false);
            return;
        }
        m_expanded = true;
        setMask();
        Q_EMIT changed();
        Q_EMIT expandAnswered(true);
    });
}

void SpreadGuest::collapse()
{
    if (!m_active || !m_expanded) {
        return;
    }
    cancelLaunch();
    m_expanded = false;
    Q_EMIT changed();
    // The input keeps the larger room until the card has shrunk into the
    // smaller one.
    const quint64 generation = ++m_generation;
    QTimer::singleShot(260, this, [this, generation] {
        if (generation != m_generation || !m_active) {
            return;
        }
        QDBusMessage request = kadunce(QStringLiteral("setLauncherGuestExpanded"));
        request.setArguments({false});
        QDBusConnection::sessionBus().asyncCall(request);
        setMask();
    });
}

bool SpreadGuest::prepareLaunch(const QStringList &applicationIds, const QString &token)
{
    if (!m_active) {
        return false;
    }
    QDBusMessage request = kadunce(QStringLiteral("prepareLauncherGuestLaunch"));
    request.setArguments({applicationIds, token});
    const QDBusReply<bool> reply = QDBusConnection::sessionBus().call(request, QDBus::Block, 500);
    if (!reply.isValid() || !reply.value()) {
        return false;
    }
    m_launchToken = token;
    return true;
}

void SpreadGuest::cancelLaunch()
{
    if (m_launchToken.isEmpty()) {
        return;
    }
    m_launchToken.clear();
    QDBusConnection::sessionBus().asyncCall(kadunce(QStringLiteral("cancelLauncherGuestLaunch")));
}

void SpreadGuest::watchForSelected(const QString &applicationId, int timeoutMs)
{
    stopWatching();
    m_watchedApplication = withoutDesktop(applicationId);
    QDBusConnection::sessionBus().connect(Kadunce, KadunceObject, KadunceInterface,
                                          QStringLiteral("workspaceContextChanged"), this, SLOT(contextChanged()));
    m_watchTimeout.start(timeoutMs);
    contextChanged();
}

void SpreadGuest::stopWatching()
{
    m_watchTimeout.stop();
    if (m_watchedApplication.isEmpty()) {
        return;
    }
    m_watchedApplication.clear();
    QDBusConnection::sessionBus().disconnect(Kadunce, KadunceObject, KadunceInterface,
                                             QStringLiteral("workspaceContextChanged"), this, SLOT(contextChanged()));
}

void SpreadGuest::contextChanged()
{
    if (m_watchedApplication.isEmpty()) {
        return;
    }
    const QJsonObject context = workspaceContext();
    const QString selected = context.value(QStringLiteral("cardStage")).toObject().value(QStringLiteral("selectedCardId")).toString();
    if (selected.isEmpty()) {
        return;
    }
    const QJsonArray applications = context.value(QStringLiteral("applications")).toArray();
    for (const QJsonValue &value : applications) {
        const QJsonObject application = value.toObject();
        if (application.value(QStringLiteral("windowId")).toString() == selected
            && withoutDesktop(application.value(QStringLiteral("appId")).toString()) == m_watchedApplication) {
            stopWatching();
            qCDebug(DESKTOP) << "the board stands in the Active card's place";
            Q_EMIT boardSelected();
            return;
        }
    }
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

void SpreadGuest::completeGuestLaunch(const QString &requestToken)
{
    if (!m_active || requestToken.isEmpty() || requestToken != m_launchToken) {
        return;
    }
    qCDebug(DESKTOP) << "the board has arrived in the card's place";
    // The window takes the place; Kadunce ends the guest with the hand-off.
    leave(true);
    Q_EMIT launchCompleted(requestToken);
}

} // namespace Gooseberry
