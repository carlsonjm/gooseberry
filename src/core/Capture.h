// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Note.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace Gooseberry {

class NoteStore;

// What was in front when capture was asked for. Empty fields are unknown.
struct CaptureContext {
    QString window; // The document name of the window in front.
    QString app;
    QString workspace;
};

// The note on the quick-note card. It is kept from its first letter: the first
// change that gives it text writes it to the folder at once. Typing after that
// is written when the writing pauses, and at least every few seconds while it
// goes on; every other change, finishing the note and the program ending write
// it at once.
class Capture : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString noteId READ noteId NOTIFY noteChanged)
    Q_PROPERTY(bool kept READ kept NOTIFY noteChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY noteChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY noteChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString colour READ colour WRITE setColour NOTIFY colourChanged)
    // The folder the note is kept in, empty for Inbox, and its name to show.
    Q_PROPERTY(QString folder READ folder NOTIFY belongingChanged)
    Q_PROPERTY(QString folderLabel READ folderLabel NOTIFY belongingChanged)
    // Stuck to the window named by window and app.
    Q_PROPERTY(bool stuck READ stuck NOTIFY belongingChanged)
    Q_PROPERTY(QString window READ window NOTIFY contextChanged)
    Q_PROPERTY(QString app READ app NOTIFY contextChanged)
    Q_PROPERTY(QString workspace READ workspace NOTIFY contextChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(QVariantList colours READ colours CONSTANT)
    // The reminder: a time, or the next time the note's window opens.
    Q_PROPERTY(QDateTime remindAt READ remindAt NOTIFY reminderChanged)
    Q_PROPERTY(bool remindOnOpen READ remindOnOpen NOTIFY reminderChanged)
    Q_PROPERTY(bool hasReminder READ hasReminder NOTIFY reminderChanged)
    // A checklist, and every line of the text: each a map of text, item and
    // checked.
    Q_PROPERTY(bool checklist READ checklist NOTIFY textChanged)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY textChanged)

public:
    explicit Capture(NoteStore *store, QObject *parent = nullptr);
    ~Capture() override;

    // How long the writing rests before typing is written, and the longest
    // typing waits while it goes on.
    static constexpr int PauseMs = 500;
    static constexpr int LongestWaitMs = 3000;
    // Shorter waits, so a test need not sit through the real ones.
    void setWaits(int pauseMs, int longestWaitMs);

    QString noteId() const { return m_note.id; }
    bool kept() const { return !m_note.id.isEmpty(); }
    // True when an existing note was opened, rather than a new one started.
    bool editing() const { return m_editing; }
    bool readOnly() const;
    QString text() const { return m_note.text; }
    QString colour() const { return m_note.colour; }
    QString folder() const { return m_note.folder; }
    QString folderLabel() const { return m_note.placeLabel(); }
    bool stuck() const { return m_note.isStuck(); }
    // The window the note was written on or stuck to, and the workspace it
    // was written on.
    QString window() const { return m_note.window; }
    QString app() const { return m_note.app; }
    QString workspace() const { return m_note.workspace; }
    QString problem() const { return m_problem; }
    // The note colours in the order offered: each a map of name and hex.
    QVariantList colours() const;
    Q_INVOKABLE QString hexFor(const QString &colour) const { return colourHex(colour); }

    QDateTime remindAt() const { return m_note.remind; }
    bool remindOnOpen() const { return m_note.remindOnOpen; }
    bool hasReminder() const { return m_note.hasReminder(); }
    bool checklist() const;
    QVariantList lines() const;

    void setText(const QString &text);
    void setColour(const QString &colour);

    // A reminder replaces any the note had, and is shown once more.
    Q_INVOKABLE void setRemindAt(const QDateTime &time);
    Q_INVOKABLE void setRemindOnOpen();
    Q_INVOKABLE void clearReminder();

    // Checklist on the card: the text becomes a list and back. A tick, a new
    // item and a removed one are kept at once; words typed in an item wait
    // for the pause as any typing does.
    Q_INVOKABLE void makeChecklist();
    Q_INVOKABLE void makePlain();
    Q_INVOKABLE void setLineText(int line, const QString &text);
    Q_INVOKABLE void setLineChecked(int line, bool checked);
    Q_INVOKABLE void addItemAfter(int line);
    Q_INVOKABLE void removeLine(int line);

    // A new, empty note, stuck to the window in front when there is one and
    // kept in the workspace's folder, or Inbox. Nothing is kept until it has
    // text.
    void startNew(const CaptureContext &context);
    // A new note in a place on the board, as "New note" there: kept in its
    // folder, or stuck to its window.
    Q_INVOKABLE void startNewIn(const QString &placeKey);
    // An existing note, to read and change.
    Q_INVOKABLE bool open(const QString &id);

    // Keeps the note in a folder, by name; empty is Inbox.
    Q_INVOKABLE void setFolder(const QString &folder);
    // Makes a folder and keeps the note in it. Returns why it could not, in
    // words for the person, or an empty string.
    Q_INVOKABLE QString makeFolder(const QString &name);
    // Sticks the note to a window, by its document's name and application;
    // an empty window unsticks it. The folder stays as it is.
    Q_INVOKABLE void setStuck(const QString &window, const QString &app = {});

    // The card goes away. A note left with no text goes to the trash.
    Q_INVOKABLE void finish();
    Q_INVOKABLE void tuckAway();
    Q_INVOKABLE void remove();
    // Brings back the note remove() sent to the trash.
    Q_INVOKABLE bool undoRemove();
    // Writes typing still waiting for a pause. Nothing happens when there is
    // none.
    Q_INVOKABLE void flush();
    bool waiting() const { return m_waiting; }

Q_SIGNALS:
    void noteChanged();
    void textChanged();
    void colourChanged();
    void belongingChanged();
    void contextChanged();
    void reminderChanged();
    void problemChanged();
    void finished();
    void removed();

private:
    void keep();
    void setTextNow(const QString &text);
    void setReminder(const QDateTime &time, bool onOpen);
    void stopWaiting();
    void leaveEmpty();
    void setProblem(const QString &problem);
    void reset(const Note &note, bool editing);
    void storeChanged(const QString &id);

    NoteStore *m_store;
    Note m_note;
    CaptureContext m_context;
    bool m_editing = false;
    QString m_problem;
    QString m_removedId;
    QString m_removedPath;
    // Typing not yet written, and since when.
    bool m_waiting = false;
    QTimer m_pause;
    QElapsedTimer m_waitingSince;
    int m_longestWaitMs = LongestWaitMs;
};

} // namespace Gooseberry
