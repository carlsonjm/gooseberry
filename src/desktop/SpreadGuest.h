// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDBusContext>
#include <QObject>
#include <QPointer>
#include <QRect>

class QWindow;

namespace Gooseberry {

// The quick-note card's place in Kadunce's card workspace, where Kadunce runs
// and offers it (its docs/TETTEGOUCHE-CONTEXT.md § Companion guests). While
// Spread is shown, the card holds Spread's centre as a companion guest, drawn
// where Kadunce says; over an Active card it stands on its own. Without
// Kadunce, or with one that does not offer companion guests, nothing here
// does anything.
class SpreadGuest : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.carlsonjm.Gooseberry.CompanionGuest")
    Q_PROPERTY(bool active READ active NOTIFY changed)
    // Drawn where Kadunce said, while a guest.
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
    bool placed() const { return m_active; }
    // Where the card is drawn, in the window's own coordinates.
    QRect rect() const { return m_cardRect; }

    // Called as the card opens: reads what Kadunce shows and, in Spread, asks
    // for its centre, placing the window on Kadunce's display. True when the
    // card is now a guest.
    bool begin(QWindow *window);
    // The card closed on its own: the centre goes back to Spread.
    void end();

public Q_SLOTS:
    // From Kadunce: another guest took the centre.
    Q_SCRIPTABLE void dismissGuest();

Q_SIGNALS:
    void changed();
    void dismissed();

private:
    void setMask();
    void leave();

    QPointer<QWindow> m_window;
    bool m_active = false;
    QRect m_cardRect;
};

} // namespace Gooseberry
