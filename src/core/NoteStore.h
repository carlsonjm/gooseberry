// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Note.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>

#include <optional>

namespace Gooseberry {

// The folder layout version this program reads and writes (docs/FORMAT.md).
inline constexpr int FolderFormat = 1;

// Gooseberry's folder of notes. Every change is written to disk before the
// call that makes it returns; nothing is held back for later.
class NoteStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder CONSTANT)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)

public:
    explicit NoteStore(const QString &folder, QObject *parent = nullptr);

    // The folder used when none is given: GOOSEBERRY_FOLDER, or Gooseberry
    // in the person's Documents folder.
    static QString defaultFolder();

    // Creates the folder and its format marker when missing, then reads it.
    bool open();
    QString folder() const { return m_folder; }
    QString lastError() const { return m_lastError; }
    // True when the folder was laid out by a newer version: notes are shown,
    // and nothing in the folder is changed.
    bool readOnly() const { return m_readOnly; }

    QList<Note> notes() const { return m_notes.values(); }
    std::optional<Note> note(const QString &id) const;
    QString pathFor(const QString &id) const;
    QString inkPathFor(const QString &id) const;

    // Keeps a new note and returns its id, or an empty string with lastError.
    QString create(Note note);
    enum class Touch {
        Changed, // The person changed the note: its changed time is set here.
        Kept, // Gooseberry recorded something about it, as a reminder shown.
    };
    // Writes a changed note.
    bool save(Note note, Touch touch = Touch::Changed);
    // Moves the note, and its ink when it has any, to the desktop's trash.
    // Returns the note's place in the trash, so it can be put back.
    std::optional<QString> trash(const QString &id);
    // Puts back a note trash() moved, from where it went in the trash.
    bool restore(const QString &id, const QString &pathInTrash);

    // Reads the folder again for changes made by other programs.
    void rescan();

Q_SIGNALS:
    void noteAdded(const QString &id);
    void noteChanged(const QString &id);
    void noteRemoved(const QString &id);
    void readOnlyChanged();

private:
    struct Seen {
        qint64 size = -1;
        qint64 inode = 0;
        qint64 seconds = 0;
        qint64 nanoseconds = 0;
        bool operator==(const Seen &) const = default;
    };

    bool write(const Note &note);
    static Seen seen(const QString &path);
    QString newId(const QDateTime &time) const;
    bool readMarker();

    QString m_folder;
    QString m_lastError;
    bool m_readOnly = false;
    QHash<QString, Note> m_notes;
    QHash<QString, Seen> m_seen;
    QFileSystemWatcher m_watcher;
    QTimer m_rescan;
};

} // namespace Gooseberry
