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

class Capture;
class NoteStore;
class WindowContext;

// Notes stuck to open windows, told to the desktop on the session bus
// (docs/DESKTOP.md § Stuck notes on the bus): a title bar draws its dot from
// it, and Spread draws them on its cards. A tap on the dot brings the
// window's notes up over it, each where it was last placed, on a surface of
// Gooseberry's own that follows the window; nothing moves or resizes the
// window itself. A note is typed on where it stands, never in a popup: a tap
// on it there, or on it in Spread, makes it the one written on, saved as the
// quick note is. Notes brought up stay up until Robin taps the dot again.
// While they are open the surface holds the keys, so a tap or a key that goes
// to the window, or to any other, is seen: the notes then fold into a small
// dot where the top one stands, and a tap on that dot opens them again with
// the top note ready to type. They step aside while their window is not in
// front, or while Spread covers the windows, and come back with it.
class StuckNotes : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.carlsonjm.Gooseberry.StuckNotes")
    // The notes over the window now, each a map of id, title, text,
    // colourHex, x and y (fractions of the window, negative when never
    // placed); empty when none are shown.
    Q_PROPERTY(QVariantList shownNotes READ shownNotes NOTIFY shownChanged)
    // The note being typed on where it stands, empty when none, and its
    // words as they are kept. While it is typed on, shownNotes keeps its
    // words and its place in the list as they were when typing began, so the
    // surface keeps the note it is typing on.
    Q_PROPERTY(QString writingId READ writingId NOTIFY writingChanged)
    Q_PROPERTY(QString writingText READ writingText NOTIFY writingTextChanged)
    // The notes over the window now are folded into a dot.
    Q_PROPERTY(bool folded READ folded NOTIFY foldedChanged)

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
    QString writingId() const { return m_writingId; }
    QString writingText() const;
    bool folded() const { return m_folded; }

    // Typing on a note shown over its window, where it stands: its words are
    // kept as the quick note keeps them, from the first letter, through
    // Capture. It ends with stopWriting, when the surface loses the keys,
    // when the notes step aside for Spread or the card, or are put away.
    Q_INVOKABLE void write(const QString &noteId);
    Q_INVOKABLE void setWritingText(const QString &text);
    Q_INVOKABLE void stopWriting();
    // The surface lost the keys, to the window under it or to any other: any
    // typing ends, kept, and the notes fold into a dot, unless they are only
    // stepping aside or take the keys back first.
    Q_INVOKABLE void keysLeft();
    // The dot was tapped: the notes open, the top one ready to type.
    Q_INVOKABLE void unfold();

    // A note let go on the surface, at these fractions of the window.
    Q_INVOKABLE void place(const QString &noteId, qreal x, qreal y);
    // A note resized on the surface, to this size in the desktop's pixels.
    Q_INVOKABLE void resize(const QString &noteId, qreal width, qreal height);
    // Where the surface takes presses, in its own coordinates: the notes.
    // Everywhere else reaches the window.
    Q_INVOKABLE void setPressable(const QVariantList &rects);
    // While words on a note are selected, or it is typed on, the surface
    // takes the keys, so Ctrl+C copies them. It also holds them while the
    // notes are open, so it knows when Robin goes back to the window.
    Q_INVOKABLE void setTakesKeys(bool takes);
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
    // A new note on the quick-note card, stuck to that window.
    Q_SCRIPTABLE bool NewOn(const QString &windowId, const QString &caption, const QString &app);
    Q_SCRIPTABLE bool StickTo(const QString &noteId, const QString &windowId, const QString &caption, const QString &app);
    // Brings that window's notes up, as Show does, with this note ready to
    // type on where it stands, once the notes are drawn over the window.
    Q_SCRIPTABLE bool Write(const QString &noteId, const QString &windowId, const QString &caption, const QString &app);

Q_SIGNALS:
    Q_SCRIPTABLE void WindowsChanged(const QVariantList &windows);
    void shownChanged();
    void writingChanged();
    void writingTextChanged();
    void foldedChanged();
    // The title bar's + was tapped: a new note stuck to this window.
    void newRequested(const QString &window, const QString &app);

private:
    void refresh();
    void setShown(const OpenWindow &window, bool shown);
    // Whether the note is stuck to the window with that key.
    bool noteOnWindow(const QString &noteId, const QString &key) const;
    // Typing on a note begins over the window with that key, or ends; each
    // says whether anything changed, and neither tells the surface.
    bool beginWriting(const QString &noteId, const QString &key);
    bool endWriting();
    void follow();
    void prepareSurface();
    // Asks the desktop for the keys while the notes are open or a note on
    // them wants them, once the surface has drawn; otherwise gives them back.
    void applyKeys();
    void foldIfLeft();
    // The notes are drawn over the window with that key from now; open notes
    // left over another window fold.
    void drawOver(const QString &key);
    const OpenWindow *drawnWindow() const;
    QVariantList currentShownNotes() const;

    NoteStore *m_store;
    WindowContext *m_context;
    std::function<QQuickWindow *()> m_makeSurface;
    QPointer<QQuickWindow> m_surface;
    QList<OpenWindow> m_windows;
    QVariantList m_entries;
    // The windows whose notes are up, until Robin puts them away, and those
    // of them whose notes are folded into a dot.
    QSet<QString> m_shownKeys;
    QSet<QString> m_foldedKeys;
    bool m_folded = false;
    // The one window they are drawn over now: the one in front, or the last
    // one in front while the desktop says none is.
    QString m_drawnKey;
    // The window the desktop said was in front when notes were last brought
    // up by a call: until the desktop names another, the notes stay over
    // the window they were brought up on, however late it is to say so.
    QString m_activeWhenShown;
    bool m_paused = false;
    bool m_cardOpen = false;
    QVariantList m_shownNotes;
    // The note typed on, the window it is typed on over, and what keeps it.
    QString m_writingId;
    QString m_writingKey;
    QVariantMap m_writingShown;
    Capture *m_writer = nullptr;
    // Notes and windows change in bursts; they are told once.
    QTimer m_refresh;
    // The keys leave the surface for a moment as it steps aside or another
    // window's notes come up; the notes fold only once they have stayed away.
    QTimer m_foldLater;
    // A note on the surface wants the keys; the keys are the surface's now;
    // the surface was shown and has not drawn yet.
    bool m_notesWantKeys = false;
    bool m_keysTaken = false;
    bool m_waitingToDraw = false;
};

} // namespace Gooseberry
