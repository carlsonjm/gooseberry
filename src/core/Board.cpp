// SPDX-License-Identifier: GPL-2.0-or-later
#include "Board.h"

#include "NoteStore.h"

#include <QDateTime>

#include <algorithm>

namespace Gooseberry {

namespace {

const QString Loose = QStringLiteral("loose");
const QString Today = QStringLiteral("today");
const QString Tucked = QStringLiteral("tucked");

int msecsToMidnight()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime midnight(now.date().addDays(1), QTime(0, 0));
    return int(qBound(qint64(1000), now.msecsTo(midnight) + 1000, qint64(24 * 3600 * 1000)));
}

bool newerFirst(const Note &a, const Note &b)
{
    if (a.changed != b.changed) {
        return a.changed > b.changed;
    }
    return a.id > b.id;
}

} // namespace

bool Places::contains(const QString &key, const Note &note, const QDate &today)
{
    if (key == Tucked) {
        return note.tucked;
    }
    if (note.tucked) {
        // A note tucked away waits only there until it is brought back.
        return false;
    }
    if (key == Today) {
        return note.changed.toLocalTime().date() == today || note.created.toLocalTime().date() == today;
    }
    return note.placeKey() == key;
}

Places::Places(NoteStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    // A burst of saves while typing is answered once, after it settles.
    m_rebuild.setSingleShot(true);
    m_rebuild.setInterval(0);
    connect(&m_rebuild, &QTimer::timeout, this, &Places::rebuild);
    for (auto signal : {&NoteStore::noteAdded, &NoteStore::noteChanged, &NoteStore::noteRemoved}) {
        connect(m_store, signal, &m_rebuild, qOverload<>(&QTimer::start));
    }
    m_midnight.setSingleShot(true);
    connect(&m_midnight, &QTimer::timeout, this, [this] {
        rebuild();
        scheduleMidnight();
    });
    scheduleMidnight();
    rebuild();
}

void Places::scheduleMidnight()
{
    m_midnight.start(msecsToMidnight());
}

void Places::rebuild()
{
    const QDate today = QDate::currentDate();
    QList<Note> notes = m_store->notes();
    std::sort(notes.begin(), notes.end(), newerFirst);

    QList<Place> places = {
        {Loose, QStringLiteral("Loose"), QString(), 0},
        {Today, QStringLiteral("Today"), QString(), 0},
        {Tucked, QStringLiteral("Tucked away"), QString(), 0},
    };
    QList<Place> windows;
    QList<Place> projects;
    QList<Place> workspaces;
    QStringList projectNames;

    auto bump = [](QList<Place> &list, const QString &key, const QString &label, const QString &section) {
        for (auto &place : list) {
            if (place.key == key) {
                ++place.count;
                return;
            }
        }
        list.append({key, label, section, 1});
    };

    for (const Note &note : std::as_const(notes)) {
        if (!note.project.isEmpty() && !projectNames.contains(note.project)) {
            projectNames.append(note.project);
        }
        for (int i = 0; i < 3; ++i) {
            if (contains(places[i].key, note, today)) {
                ++places[i].count;
            }
        }
        if (note.tucked) {
            continue;
        }
        switch (note.belongs) {
        case Belongs::Window:
            if (!note.window.isEmpty()) {
                bump(windows, note.placeKey(), note.window, QStringLiteral("windows"));
            }
            break;
        case Belongs::Project:
            if (!note.project.isEmpty()) {
                bump(projects, note.placeKey(), note.project, QStringLiteral("projects"));
            }
            break;
        case Belongs::Workspace:
            bump(workspaces, note.placeKey(), note.placeLabel(), QStringLiteral("workspaces"));
            break;
        case Belongs::Loose:
            break;
        }
    }

    auto byName = [](const Place &a, const Place &b) {
        return QString::localeAwareCompare(a.label, b.label) < 0;
    };
    std::sort(projects.begin(), projects.end(), byName);
    std::sort(workspaces.begin(), workspaces.end(), byName);
    places += windows;
    places += projects;
    places += workspaces;

    bool sameRows = places.size() == m_places.size();
    for (qsizetype i = 0; sameRows && i < places.size(); ++i) {
        sameRows = places[i].key == m_places[i].key && places[i].label == m_places[i].label;
    }
    if (sameRows) {
        // Only counts moved: the list stays still under a finger.
        for (qsizetype i = 0; i < places.size(); ++i) {
            if (places[i].count != m_places[i].count) {
                m_places[i].count = places[i].count;
                Q_EMIT dataChanged(index(int(i)), index(int(i)), {CountRole});
            }
        }
    } else {
        beginResetModel();
        m_places = places;
        endResetModel();
    }
    if (projectNames != m_projects) {
        m_projects = projectNames;
        Q_EMIT projectsChanged();
    }
}

int Places::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_places.size());
}

QVariant Places::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_places.size()) {
        return {};
    }
    const Place &place = m_places.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return place.label;
    case KeyRole:
        return place.key;
    case SectionRole:
        return place.section;
    case CountRole:
        return place.count;
    }
    return {};
}

QHash<int, QByteArray> Places::roleNames() const
{
    return {
        {KeyRole, "key"},
        {LabelRole, "label"},
        {SectionRole, "section"},
        {CountRole, "count"},
    };
}

QString Places::labelFor(const QString &key) const
{
    for (const auto &place : m_places) {
        if (place.key == key) {
            return place.label;
        }
    }
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    return colon < 0 ? key : key.mid(colon + 1);
}

int Places::countFor(const QString &key) const
{
    for (const auto &place : m_places) {
        if (place.key == key) {
            return place.count;
        }
    }
    return 0;
}

PlaceNotes::PlaceNotes(NoteStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    connect(m_store, &NoteStore::noteAdded, this, [this](const QString &id) { refresh(id); });
    connect(m_store, &NoteStore::noteChanged, this, [this](const QString &id) { refresh(id); });
    connect(m_store, &NoteStore::noteRemoved, this, [this] { refresh(); });
    m_midnight.setSingleShot(true);
    connect(&m_midnight, &QTimer::timeout, this, [this] {
        refresh();
        m_midnight.start(msecsToMidnight());
    });
    m_midnight.start(msecsToMidnight());
    refresh();
}

void PlaceNotes::setPlace(const QString &place)
{
    if (place == m_place) {
        return;
    }
    m_place = place;
    Q_EMIT placeChanged();
    beginResetModel();
    m_ids = wanted();
    endResetModel();
    Q_EMIT countChanged();
}

void PlaceNotes::setSearch(const QString &search)
{
    if (search == m_search) {
        return;
    }
    m_search = search;
    Q_EMIT searchChanged();
    refresh();
}

QStringList PlaceNotes::wanted() const
{
    const QDate today = QDate::currentDate();
    const QString needle = m_search.simplified();
    QList<Note> notes;
    for (const Note &note : m_store->notes()) {
        // A search looks through every note, tucked away ones included.
        if (needle.isEmpty() && !Places::contains(m_place, note, today)) {
            continue;
        }
        if (!needle.isEmpty() && !note.text.contains(needle, Qt::CaseInsensitive)
            && !note.placeLabel().contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        notes.append(note);
    }
    std::sort(notes.begin(), notes.end(), newerFirst);
    QStringList ids;
    ids.reserve(notes.size());
    for (const Note &note : std::as_const(notes)) {
        ids.append(note.id);
    }
    return ids;
}

void PlaceNotes::refresh(const QString &changedId)
{
    const QStringList next = wanted();
    const int before = int(m_ids.size());

    // Changes are applied row by row rather than as a reset, so the board
    // stays where it was scrolled while a note on it is being written.
    for (int row = int(m_ids.size()) - 1; row >= 0; --row) {
        if (!next.contains(m_ids.at(row))) {
            beginRemoveRows({}, row, row);
            m_ids.removeAt(row);
            endRemoveRows();
        }
    }
    for (int row = 0; row < next.size(); ++row) {
        if (row < m_ids.size() && m_ids.at(row) == next.at(row)) {
            continue;
        }
        const int from = int(m_ids.indexOf(next.at(row)));
        if (from > row) {
            beginMoveRows({}, from, from, {}, row);
            m_ids.move(from, row);
            endMoveRows();
        } else {
            beginInsertRows({}, row, row);
            m_ids.insert(row, next.at(row));
            endInsertRows();
        }
    }
    if (!changedId.isEmpty()) {
        const int row = int(m_ids.indexOf(changedId));
        if (row >= 0) {
            Q_EMIT dataChanged(index(row), index(row));
        }
    }
    if (before != m_ids.size()) {
        Q_EMIT countChanged();
    }
}

int PlaceNotes::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_ids.size());
}

QVariant PlaceNotes::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_ids.size()) {
        return {};
    }
    const auto note = m_store->note(m_ids.at(index.row()));
    if (!note) {
        return {};
    }
    switch (role) {
    case IdRole:
        return note->id;
    case Qt::DisplayRole:
    case TextRole:
        return note->text;
    case TitleRole:
        return note->title();
    case ColourRole:
        return note->colour;
    case ColourHexRole:
        return colourHex(note->colour);
    case PlaceKeyRole:
        return note->placeKey();
    case PlaceLabelRole:
        return note->placeLabel();
    case BelongsRole:
        return belongsName(note->belongs);
    case TuckedRole:
        return note->tucked;
    case ChangedRole:
        return note->changed;
    case ReadOnlyRole:
        return note->newerFormat() || m_store->readOnly();
    }
    return {};
}

QHash<int, QByteArray> PlaceNotes::roleNames() const
{
    return {
        {IdRole, "noteId"},
        {TextRole, "text"},
        {TitleRole, "title"},
        {ColourRole, "colour"},
        {ColourHexRole, "colourHex"},
        {PlaceKeyRole, "placeKey"},
        {PlaceLabelRole, "placeLabel"},
        {BelongsRole, "belongs"},
        {TuckedRole, "tucked"},
        {ChangedRole, "changed"},
        {ReadOnlyRole, "readOnly"},
    };
}

bool PlaceNotes::tuckAway(const QString &id)
{
    auto note = m_store->note(id);
    if (!note || note->tucked) {
        return false;
    }
    note->tucked = true;
    return m_store->save(*note);
}

bool PlaceNotes::bringBack(const QString &id)
{
    auto note = m_store->note(id);
    if (!note || !note->tucked) {
        return false;
    }
    note->tucked = false;
    return m_store->save(*note);
}

} // namespace Gooseberry
