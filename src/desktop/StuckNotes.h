// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "StuckWindows.h"

#include <QDBusContext>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include <functional>

class QQuickWindow;

namespace Gooseberry {

class NoteStore;
class WindowContext;

// Notes stuck to open windows, told to the desktop on the session bus
// (docs/DESKTOP.md § Stuck notes on the bus): a title bar draws its dot from
// it, and Spread its stacks. A tap on the dot brings the window's notes up
// over it, each where it was last placed, on a surface of Gooseberry's own
// that follows the window; nothing moves or resizes the window itself. Notes
// brought up stay up until Robin puts them away; they step aside while their
// window is not in front, or while Spread covers the windows, and come back
// with it.
class StuckNotes : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.carlsonjm.Gooseberry.StuckNotes")
    // The notes over the window now, each a map of id, title, text,
    // colourHex, x and y (fractions of the window, negative when never
    // placed); empty when none are shown.
    Q_PROPERTY(QVariantList shownNotes READ shownNotes NOTIFY shownChanged)

public:
    static constexpr uint Version = 1;
    static constexpr const char *Path = "/StuckNotes";

    // makeSurface gives the surface the notes are shown on, made once.
    StuckNotes(NoteStore *store, WindowContext *context, std::function<QQuickWindow *()> makeSurface,
               QObject *parent = nullptr);
    ~StuckNotes() override;

    bool publish();

    QVariantList shownNotes() const;
    bool shown() const { return !m_shownKeys.isEmpty(); }

    // A note let go on the surface, at these fractions of the window.
    Q_INVOKABLE void place(const QString &noteId, qreal x, qreal y);
    // Where the surface takes presses, in its own coordinates: the notes and
    // the button that puts them away. Everywhere else reaches the window.
    Q_INVOKABLE void setPressable(const QVariantList &rects);
    // Puts away the notes of the window they are drawn over.
    Q_INVOKABLE void putAway();
    // While the quick-note card is open the notes step aside, as in Spread,
    // and come back when it closes.
    void setCardOpen(bool open);

public Q_SLOTS:
    Q_SCRIPTABLE uint ProtocolVersion() const;
    Q_SCRIPTABLE QVariantList Windows() const;
    Q_SCRIPTABLE bool Toggle(const QString &windowId, const QString &caption, const QString &app);
    Q_SCRIPTABLE bool Show(const QString &windowId, const QString &caption, const QString &app);
    // Puts away every window's notes.
    Q_SCRIPTABLE void Hide();
    // While paused, as while Spread covers the windows, the surface steps
    // aside; nothing is put away.
    Q_SCRIPTABLE void Pause(bool paused);
    Q_SCRIPTABLE bool StickTo(const QString &noteId, const QString &windowId, const QString &caption, const QString &app);

Q_SIGNALS:
    Q_SCRIPTABLE void WindowsChanged(const QVariantList &windows);
    void shownChanged();
    // A note on the surface was tapped: it opens on the quick-note card.
    void openRequested(const QString &noteId);

private:
    void refresh();
    void setShown(const OpenWindow &window, bool shown);
    void follow();
    void prepareSurface();
    const OpenWindow *drawnWindow() const;
    QVariantList currentShownNotes() const;

    NoteStore *m_store;
    WindowContext *m_context;
    std::function<QQuickWindow *()> m_makeSurface;
    QPointer<QQuickWindow> m_surface;
    QList<OpenWindow> m_windows;
    QVariantList m_entries;
    // The windows whose notes are up, until Robin puts them away.
    QSet<QString> m_shownKeys;
    // The one window they are drawn over now: the one in front, or the last
    // one in front while the desktop says none is.
    QString m_drawnKey;
    bool m_paused = false;
    bool m_cardOpen = false;
    QVariantList m_shownNotes;
    // Notes and windows change in bursts; they are told once.
    QTimer m_refresh;
};

} // namespace Gooseberry
