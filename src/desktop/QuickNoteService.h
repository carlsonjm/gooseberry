// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDBusContext>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

namespace Gooseberry {

class Capture;
class NoteStore;
class Shell;

// The quick note, offered on the session bus to a desktop search that draws
// the note pad in its own window (docs/DESKTOP.md § Tettegouche). The search
// holds no notes: every change it sends is written before the call returns,
// and what it gets back is always the note as Gooseberry keeps it. The
// session is the search's own, apart from the note on Gooseberry's card.
class QuickNoteService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.carlsonjm.Gooseberry.QuickNote")

public:
    static constexpr uint Version = 1;
    static constexpr const char *Path = "/QuickNote";

    QuickNoteService(Shell *shell, NoteStore *store, QObject *parent = nullptr);

    // Registers the interface on the session bus.
    bool publish();

public Q_SLOTS:
    Q_SCRIPTABLE uint ProtocolVersion() const;
    // Starts a note, or resumes the one the last Start began that has not
    // been finished.
    Q_SCRIPTABLE QVariantMap Start();
    Q_SCRIPTABLE QVariantMap State() const;
    Q_SCRIPTABLE QVariantMap SetText(const QString &text);
    Q_SCRIPTABLE QVariantMap SetColour(const QString &colour);
    // Kept for callers of Milestone 1; SetFolder and SetStuck replace it.
    Q_SCRIPTABLE QVariantMap SetBelongs(const QString &kind, const QString &project);
    // Added in version 1, from Milestone 1; a caller that does not know them
    // ignores them. A reminder: an ISO 8601 time, "opens" for the next time
    // the note's window opens, or empty for none.
    Q_SCRIPTABLE QVariantMap SetReminder(const QString &when);
    Q_SCRIPTABLE QVariantMap SetChecklist(bool on);
    Q_SCRIPTABLE QVariantMap SetLineChecked(uint line, bool checked);
    // Added in version 1, from Milestone 2. The folder, by name, empty for
    // Inbox; a new name makes the folder. The window to stick to, by its
    // document's name and application; an empty window unsticks.
    Q_SCRIPTABLE QVariantMap SetFolder(const QString &name);
    Q_SCRIPTABLE QVariantMap SetStuck(const QString &window, const QString &app);
    Q_SCRIPTABLE QVariantMap Done();
    Q_SCRIPTABLE QVariantMap TuckAway();
    Q_SCRIPTABLE QVariantMap Remove();
    Q_SCRIPTABLE QVariantMap UndoRemove();
    // Opens the board on the note's place, as an ordinary window. BoardShown
    // follows with the same token once the window has drawn its first frame.
    Q_SCRIPTABLE bool OpenBoard(const QString &noteId, const QString &requestToken);

Q_SIGNALS:
    // The note changed by something other than a call here: the board, the
    // card or another program writing its file.
    Q_SCRIPTABLE void Changed(const QVariantMap &state);
    Q_SCRIPTABLE void BoardShown(const QString &requestToken);

private:
    QVariantMap state(bool open) const;
    QVariantList choices() const;
    QVariantList windowChoices() const;
    void changedElsewhere();

    Shell *m_shell;
    NoteStore *m_store;
    Capture *m_capture;
    bool m_open = false;
    int m_calling = 0;
    // Tokens of OpenBoard calls waiting for the board's first frame.
    QStringList m_boardTokens;
    // One change from elsewhere can touch text, colour and place at once;
    // it is told once.
    QTimer m_changed;
};

} // namespace Gooseberry
