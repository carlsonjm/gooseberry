// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QDate>
#include <QTimer>

namespace Gooseberry {

class NoteStore;
struct Note;

// The board's places: Loose, Today and Tucked away, then each window or
// document, each project and each workspace that has notes. Gathered from the
// notes themselves; nothing is filed by hand.
class Places : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList projects READ projects NOTIFY projectsChanged)

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

    // Project names, the most recently used first.
    QStringList projects() const { return m_projects; }
    Q_INVOKABLE QString labelFor(const QString &key) const;
    Q_INVOKABLE int countFor(const QString &key) const;

    // True when the note sits in the place: the board's one rule for it.
    static bool contains(const QString &key, const Note &note, const QDate &today);

Q_SIGNALS:
    void projectsChanged();

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
    QStringList m_projects;
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
        BelongsRole,
        TuckedRole,
        ChangedRole,
        ReadOnlyRole,
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
