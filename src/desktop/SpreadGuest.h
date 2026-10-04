// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDBusContext>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>

class QWindow;

namespace Gooseberry {

// The quick-note card's place in Kadunce's card workspace, where Kadunce runs
// and offers it (its docs/TETTEGOUCHE-CONTEXT.md § Companion guests). While
// Spread is shown, the card holds Spread's centre as a companion guest, drawn
// where Kadunce says; over an Active card it stands on its own, and Gooseberry
// watches for the board taking the Active card's place. Without Kadunce, or
// with one that does not offer companion guests, nothing here does anything.
class SpreadGuest : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.carlsonjm.Gooseberry.CompanionGuest")
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool expanded READ expanded NOTIFY changed)
    // Drawn where Kadunce said: while a guest, and after handing its place to
    // the board, until the card has faded.
    Q_PROPERTY(bool placed READ placed NOTIFY changed)
    Q_PROPERTY(QRect rect READ rect NOTIFY changed)

public:
    static constexpr int Protocol = 1;
    static constexpr const char *Path = "/CompanionGuest";

    explicit SpreadGuest(QObject *parent = nullptr);

    // Answers Kadunce on the session bus.
    bool publish();

    // Holding Spread's centre.
    bool active() const { return m_active; }
    bool expanded() const { return m_expanded; }
    bool placed() const { return m_active || m_held; }
    // Where the card is drawn, in the window's own coordinates.
    QRect rect() const { return m_expanded || m_held ? m_activeRect : m_cardRect; }
    // Kadunce shows an Active card, so a window opened now takes its place.
    bool overActiveCard() const { return m_overActiveCard; }

    // Called as the card opens: reads what Kadunce shows and, in Spread, asks
    // for its centre, placing the window on Kadunce's display. True when the
    // card is now a guest.
    bool begin(QWindow *window);
    // The card closed on its own: the centre goes back to Spread. With
    // keepPlace the card stays drawn where it was, to fade there.
    void end(bool keepPlace = false);
    // All notes in Spread: asks for the Active card's room. expandAnswered
    // follows, false when Kadunce refused and the guest has ended.
    void expand();
    void collapse();
    // The board is about to open: Kadunce waits for its window to take the
    // card's place, and says so with completeGuestLaunch.
    bool prepareLaunch(const QStringList &applicationIds, const QString &token);
    void cancelLaunch();
    // Over an Active card: tells boardSelected once Kadunce reports a window
    // of this application as the selected card, or watchGaveUp after timeoutMs.
    void watchForSelected(const QString &applicationId, int timeoutMs);
    void stopWatching();

public Q_SLOTS:
    // From Kadunce: another guest took the centre.
    Q_SCRIPTABLE void dismissGuest();
    // From Kadunce: the window launched for requestToken has arrived.
    Q_SCRIPTABLE void completeGuestLaunch(const QString &requestToken);

Q_SIGNALS:
    void changed();
    void dismissed();
    void expandAnswered(bool expanded);
    void launchCompleted(const QString &token);
    void boardSelected();
    void watchGaveUp();

private Q_SLOTS:
    void contextChanged();

private:
    void setMask();
    void leave(bool keepPlace = false);

    QPointer<QWindow> m_window;
    bool m_active = false;
    bool m_expanded = false;
    bool m_held = false;
    bool m_overActiveCard = false;
    QRect m_cardRect;
    QRect m_activeRect;
    QString m_launchToken;
    QString m_watchedApplication;
    QTimer m_watchTimeout;
    quint64 m_generation = 0;
};

} // namespace Gooseberry
