// SPDX-License-Identifier: GPL-2.0-or-later
#include "Board.h"

#include "Checklist.h"
#include "NoteStore.h"

#include <QDateTime>
#include <QVariantMap>

#include <algorithm>

namespace Gooseberry {

namespace {

const QString Inbox = QStringLiteral("inbox");
const QString Today = QStringLiteral("today");
const QString Tucked = QStringLiteral("tucked");
const QString NewFolder = QStringLiteral("newfolder");
const QString FolderPrefix = QStringLiteral("folder:");
const QString WindowPrefix = QStringLiteral("window:");

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
        // A note with a time is on the day it is planned for; any other is
        // today's when it was written or changed today.
        if (note.remind.isValid()) {
            return note.plannedDay() == today;
        }
        return note.changed.toLocalTime().date() == today || note.created.toLocalTime().date() == today;
    }
    if (key.startsWith(WindowPrefix)) {
        return note.stuckKey() == key;
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
    connect(m_store, &NoteStore::foldersChanged, &m_rebuild, qOverload<>(&QTimer::start));
    connect(m_store, &NoteStore::foldersChanged, this, &Places::foldersChanged);
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

    QList<Place> places = {{Today, QStringLiteral("Today"), QString(), 0}, {Inbox, inboxLabel(), QStringLiteral("folders"), 0}};
    // Every folder has a row, empty ones too: a folder lasts until it is
    // removed.
    const QStringList folders = m_store->folders();
    for (const QString &folder : folders) {
        places.append({FolderPrefix + folder, folder, QStringLiteral("folders"), 0});
    }
    places.append({NewFolder, QStringLiteral("New folder"), QStringLiteral("folders"), 0});
    QList<Place> windows;
    Place tucked{Tucked, QStringLiteral("Tucked away"), QStringLiteral("end"), 0};

    auto find = [&places](const QString &key) -> Place * {
        for (auto &place : places) {
            if (place.key == key) {
                return &place;
            }
        }
        return nullptr;
    };

    for (const Note &note : std::as_const(notes)) {
        if (contains(Today, note, today)) {
            ++places[0].count;
        }
        if (note.tucked) {
            ++tucked.count;
            continue;
        }
        if (Place *place = find(note.placeKey())) {
            ++place->count;
        }
        // Only windows with notes stuck to them, the most recent first, so
        // the list stays short.
        const QString stuck = note.stuckKey();
        if (!stuck.isEmpty()) {
            auto found = std::find_if(windows.begin(), windows.end(), [&stuck](const Place &p) {
                return p.key == stuck;
            });
            if (found == windows.end()) {
                windows.append({stuck, note.window, QStringLiteral("windows"), 1});
            } else {
                ++found->count;
            }
        }
    }
    places += windows;
    places.append(tucked);

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
}

QStringList Places::folders() const
{
    return m_store->folders();
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

QString Places::makeFolder(const QString &name)
{
    return m_store->makeFolder(name) ? QString() : m_store->lastError();
}

QString Places::renameFolder(const QString &from, const QString &to)
{
    return m_store->renameFolder(from, to) ? QString() : m_store->lastError();
}

QVariantMap Places::removeFolder(const QString &name)
{
    QStringList workspaces;
    // The workspaces given this folder, so undo gives it back to them.
    for (const QString &workspace : m_store->workspacesOf(name)) {
        workspaces.append(workspace);
    }
    const auto moved = m_store->removeFolder(name);
    if (!moved) {
        m_problem = m_store->lastError();
        return {};
    }
    m_problem.clear();
    return {{QStringLiteral("name"), name}, {QStringLiteral("ids"), *moved}, {QStringLiteral("workspaces"), workspaces}};
}

bool Places::undoRemoveFolder(const QVariantMap &removed)
{
    const QString name = removed.value(QStringLiteral("name")).toString();
    if (name.isEmpty() || (!m_store->hasFolder(name) && !m_store->makeFolder(name))) {
        return false;
    }
    bool all = true;
    const QStringList ids = removed.value(QStringLiteral("ids")).toStringList();
    for (const QString &id : ids) {
        // A note moved again since, or removed, stays where it is now.
        const auto note = m_store->note(id);
        if (note && note->folder.isEmpty()) {
            all = m_store->moveNote(id, name) && all;
        }
    }
    const QStringList workspaces = removed.value(QStringLiteral("workspaces")).toStringList();
    for (const QString &workspace : workspaces) {
        if (m_store->workspaceFolder(workspace).isEmpty()) {
            m_store->setWorkspaceFolder(workspace, name);
        }
    }
    return all;
}

bool Places::moveNote(const QString &id, const QString &placeKey)
{
    QString folder;
    if (placeKey.startsWith(FolderPrefix)) {
        folder = placeKey.mid(FolderPrefix.size());
    } else if (placeKey != Inbox) {
        return false;
    }
    if (!m_store->moveNote(id, folder)) {
        m_problem = m_store->lastError();
        return false;
    }
    m_problem.clear();
    return true;
}

QString Places::workspaceFolder(const QString &workspace) const
{
    return m_store->workspaceFolder(workspace);
}

bool Places::setWorkspaceFolder(const QString &workspace, const QString &folder)
{
    if (!m_store->setWorkspaceFolder(workspace, folder)) {
        m_problem = m_store->lastError();
        return false;
    }
    m_problem.clear();
    return true;
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
        // On Today, notes with a time are on the planner beside the others.
        if (needle.isEmpty() && m_place == Today && note.remind.isValid()) {
            continue;
        }
        if (!needle.isEmpty() && !note.text.contains(needle, Qt::CaseInsensitive)
            && !note.placeLabel().contains(needle, Qt::CaseInsensitive)
            && !(note.isStuck() && note.window.contains(needle, Qt::CaseInsensitive))) {
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
    case StuckToRole:
        return note->isStuck() ? note->window : QString();
    case TuckedRole:
        return note->tucked;
    case ChangedRole:
        return note->changed;
    case ReadOnlyRole:
        return note->newerFormat() || m_store->readOnly();
    case ChecklistRole:
        return Checklist::contains(note->text);
    case ChecklistHeadingRole:
        return Checklist::heading(note->text);
    case ChecklistItemsRole: {
        QVariantList items;
        for (const ChecklistLine &line : Checklist::lines(note->text)) {
            if (line.item && !line.text.trimmed().isEmpty()) {
                items.append(QVariantMap{{QStringLiteral("text"), line.text.trimmed()}, {QStringLiteral("checked"), line.checked}});
            }
        }
        return items;
    }
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
        {StuckToRole, "stuckTo"},
        {TuckedRole, "tucked"},
        {ChangedRole, "changed"},
        {ReadOnlyRole, "readOnly"},
        {ChecklistRole, "checklist"},
        {ChecklistHeadingRole, "checklistHeading"},
        {ChecklistItemsRole, "checklistItems"},
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

QString PlaceNotes::remove(const QString &id)
{
    return m_store->trash(id).value_or(QString());
}

bool PlaceNotes::restore(const QString &id, const QString &pathInTrash)
{
    return !pathInTrash.isEmpty() && m_store->restore(id, pathInTrash);
}

bool PlaceNotes::planOn(const QString &id, const QDate &day)
{
    auto note = m_store->note(id);
    if (!note || !day.isValid()) {
        return false;
    }
    QTime time = note->remind.isValid() ? note->remind.toLocalTime().time() : QTime(9, 0);
    QDateTime when(day, time);
    const QDateTime now = QDateTime::currentDateTime();
    if (when <= now) {
        // A time already gone today would be due at once: the next hour.
        when = QDateTime(now.date(), QTime(now.time().hour(), 0)).addSecs(3600);
        if (when.date() != day) {
            return false;
        }
    }
    return setRemind(id, when);
}

QDateTime PlaceNotes::remindOf(const QString &id) const
{
    const auto note = m_store->note(id);
    return note ? note->remind : QDateTime();
}

bool PlaceNotes::setRemind(const QString &id, const QDateTime &remind)
{
    auto note = m_store->note(id);
    if (!note) {
        return false;
    }
    QDateTime when = remind;
    if (when.isValid()) {
        when.setTime(QTime(when.time().hour(), when.time().minute(), when.time().second()));
    }
    if (when == note->remind && !note->remindOnOpen) {
        return true;
    }
    // A new reminder is shown once more, as when it is set on the card.
    note->remind = when;
    note->remindOnOpen = false;
    note->reminded = {};
    note->done = {};
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
