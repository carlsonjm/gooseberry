// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Note.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>

#include <optional>

namespace Gooseberry {

// The folder layout version this program writes (docs/FORMAT.md). A folder
// of the first layout is brought up to it when it is opened.
inline constexpr int FolderFormat = 2;

// Gooseberry's folder of notes. Every change is written to disk before the
// call that makes it returns; nothing is held back for later.
class NoteStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder CONSTANT)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)

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

    // The folders notes are kept in, by name, in alphabetical order. Inbox,
    // the notes folder itself, is not among them.
    QStringList folders() const { return m_folders; }
    bool hasFolder(const QString &name) const { return m_folders.contains(name); }
    // Why a name cannot be a folder's, in words for the person; empty when
    // it can. The name is taken as simplified() gives it.
    static QString folderNameProblem(const QString &name);
    // Each makes its change on disk before it returns, or returns false with
    // lastError. A folder is made empty and lasts until it is removed.
    bool makeFolder(const QString &name);
    bool renameFolder(const QString &from, const QString &to);
    // Moves the folder's notes, and their ink, to Inbox, then sends what is
    // left of the folder to the desktop's trash. The moved notes' ids are
    // given back, so the removal can be undone.
    std::optional<QStringList> removeFolder(const QString &name);
    // Moves a note, and its ink, to a folder; an empty name is Inbox.
    bool moveNote(const QString &id, const QString &folder);

    // The folder notes written on a workspace go into; empty for Inbox.
    QString workspaceFolder(const QString &workspace) const;
    // The workspaces given a folder.
    QStringList workspacesOf(const QString &folder) const { return m_workspaces.keys(folder); }
    // Gives a workspace a folder, or Inbox again for an empty name.
    bool setWorkspaceFolder(const QString &workspace, const QString &folder);

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
    void foldersChanged();

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
    // Reads the folder's format, and returns it.
    int readMarker();
    bool writeMarker();
    bool refuseWhenReadOnly();
    QString dirFor(const QString &folder) const;
    QString folderOf(const QString &id) const;
    bool moveFiles(const QString &id, const QString &from, const QString &to);
    // Moves the notes of a first-layout folder that belonged to a project
    // into a folder of that name.
    void bringUpToDate();
    void readWorkspaces();
    bool writeWorkspaces();

    QString m_folder;
    QString m_lastError;
    bool m_readOnly = false;
    QHash<QString, Note> m_notes;
    QStringList m_folders;
    // Workspace name to folder name.
    QHash<QString, QString> m_workspaces;
    // The folder each note sent to the trash came from, to put it back.
    QHash<QString, QString> m_trashedFrom;
    QHash<QString, Seen> m_seen;
    QFileSystemWatcher m_watcher;
    QTimer m_rescan;
};

} // namespace Gooseberry
