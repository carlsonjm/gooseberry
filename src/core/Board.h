// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QDate>
#include <QTimer>
#include <QVariantMap>

namespace Gooseberry {

class NoteStore;
struct Note;

// The board's places: Today; Inbox, each folder and New folder; each window
// with notes stuck to it; and Tucked away. Folders are the person's, kept
// even when empty; the windows are gathered from the notes.
class Places : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        LabelRole,
        SectionRole,
        CountRole,
    };

    explicit Places(NoteStore *store, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The folders by name, in alphabetical order; Inbox is not among them.
    QStringList folders() const;
    Q_INVOKABLE QString labelFor(const QString &key) const;
    Q_INVOKABLE int countFor(const QString &key) const;

    // Folders on the board. Each returns why it could not be done, in words
    // for the person, or an empty string.
    Q_INVOKABLE QString makeFolder(const QString &name);
    Q_INVOKABLE QString renameFolder(const QString &from, const QString &to);
    // Removes a folder, its notes going to Inbox. Gives back what undo()
    // needs: the folder's name, the ids moved and its workspaces; empty when
    // nothing was removed, with problem saying why.
    Q_INVOKABLE QVariantMap removeFolder(const QString &name);
    Q_INVOKABLE bool undoRemoveFolder(const QVariantMap &removed);
    // Moves a note to a place's folder: "inbox" or "folder:<name>".
    Q_INVOKABLE bool moveNote(const QString &id, const QString &placeKey);
    // The folder notes written on a workspace go into, or empty for Inbox.
    Q_INVOKABLE QString workspaceFolder(const QString &workspace) const;
    Q_INVOKABLE bool setWorkspaceFolder(const QString &workspace, const QString &folder);
    // Why the last change asked of the board could not be made.
    Q_INVOKABLE QString problem() const { return m_problem; }

    // True when the note sits in the place: the board's one rule for it.
    static bool contains(const QString &key, const Note &note, const QDate &today);

Q_SIGNALS:
    void foldersChanged();

private:
    struct Place {
        QString key;
        QString label;
        QString section;
        int count = 0;
    };

    void rebuild();
    void scheduleMidnight();

    NoteStore *m_store;
    QList<Place> m_places;
    QString m_problem;
    QTimer m_rebuild;
    QTimer m_midnight;
};

// The notes in one place, the most recently changed first, narrowed by what
// is typed in the board's search.
class PlaceNotes : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString place READ place WRITE setPlace NOTIFY placeChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TextRole,
        TitleRole,
        ColourRole,
        ColourHexRole,
        PlaceKeyRole,
        PlaceLabelRole,
        StuckToRole,
        TuckedRole,
        ChangedRole,
        ReadOnlyRole,
        ChecklistRole,
        ChecklistHeadingRole,
        ChecklistItemsRole,
    };

    explicit PlaceNotes(NoteStore *store, QObject *parent = nullptr);

    QString place() const { return m_place; }
    void setPlace(const QString &place);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    int count() const { return int(m_ids.size()); }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool tuckAway(const QString &id);
    Q_INVOKABLE bool bringBack(const QString &id);
    // Moves a note to the desktop's trash, giving back where it went, for
    // restore(); empty when it could not.
    Q_INVOKABLE QString remove(const QString &id);
    Q_INVOKABLE bool restore(const QString &id, const QString &pathInTrash);
    // Puts a note on the planner on a day: at the time of day it already
    // has, else nine in the morning, or the next hour when that has gone.
    Q_INVOKABLE bool planOn(const QString &id, const QDate &day);
    // The note's reminder time, for undoing planOn(); invalid for none.
    Q_INVOKABLE QDateTime remindOf(const QString &id) const;
    // Sets the reminder time back, or takes it away when invalid.
    Q_INVOKABLE bool setRemind(const QString &id, const QDateTime &remind);

Q_SIGNALS:
    void placeChanged();
    void searchChanged();
    void countChanged();

private:
    QStringList wanted() const;
    void refresh(const QString &changedId = {});

    NoteStore *m_store;
    QString m_place = QStringLiteral("today");
    QString m_search;
    QStringList m_ids;
    QTimer m_midnight;
};

} // namespace Gooseberry
