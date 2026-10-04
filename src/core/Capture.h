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

// The note on the capture sheet. It is kept from its first letter: the first
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
    Q_PROPERTY(QString belongs READ belongs NOTIFY belongingChanged)
    Q_PROPERTY(QString project READ project NOTIFY belongingChanged)
    Q_PROPERTY(QString window READ window NOTIFY contextChanged)
    Q_PROPERTY(QString workspace READ workspace NOTIFY contextChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(QVariantList colours READ colours CONSTANT)

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
    QString belongs() const { return belongsName(m_note.belongs); }
    QString project() const { return m_note.project; }
    // Where the note was written: the window and workspace offered under
    // Belongs to.
    QString window() const { return m_note.window; }
    QString workspace() const { return m_note.workspace; }
    QString problem() const { return m_problem; }
    // The note colours in the order offered: each a map of name and hex.
    QVariantList colours() const;
    Q_INVOKABLE QString hexFor(const QString &colour) const { return colourHex(colour); }

    void setText(const QString &text);
    void setColour(const QString &colour);

    // A new, empty note. It belongs to the window in front when there is
    // one, and is Loose otherwise. Nothing is kept until it has text.
    void startNew(const CaptureContext &context);
    // A new note that belongs to a place on the board, as "New note" there.
    Q_INVOKABLE void startNewIn(const QString &placeKey);
    // An existing note, to read and change.
    Q_INVOKABLE bool open(const QString &id);

    // Belongs to: "window", "project", "workspace" or "loose". A project
    // takes its name; the others use what was in front.
    Q_INVOKABLE void setBelongs(const QString &kind, const QString &project = {});

    // The sheet goes down. A note left with no text goes to the trash.
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
    void problemChanged();
    void finished();
    void removed();

private:
    void keep();
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
