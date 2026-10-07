// SPDX-License-Identifier: GPL-2.0-or-later
#include "NoteStore.h"

#include "Product.h"
#include "Trash.h"
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace Gooseberry {

namespace {

const QString MarkerName = QStringLiteral(".gooseberry");
const QString NoteSuffix = QStringLiteral(".md");
const QString InkSuffix = QStringLiteral(".svg");
const QString WorkspacesName = QStringLiteral(".workspaces");

bool sameNote(const Note &a, const Note &b)
{
    return a.text == b.text && a.colour == b.colour && a.folder == b.folder && a.stuck == b.stuck
        && a.window == b.window && a.app == b.app && a.workspace == b.workspace && a.tucked == b.tucked
        && a.format == b.format && a.extra == b.extra && a.created == b.created && a.changed == b.changed
        && a.remind == b.remind && a.remindOnOpen == b.remindOnOpen && a.reminded == b.reminded && a.done == b.done
        && a.formerProject == b.formerProject;
}

// A first-format project's name made fit to be a folder's, or empty when it
// cannot be one.
QString folderNameFor(const QString &project)
{
    QString name = project.simplified();
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    while (name.startsWith(QLatin1Char('.'))) {
        name.remove(0, 1);
    }
    name = name.simplified();
    return NoteStore::folderNameProblem(name).isEmpty() ? name : QString();
}

// Times are kept to the second, as the header writes them, so a note in
// memory and the same note read back from disk are equal.
QDateTime nowToTheSecond()
{
    QDateTime now = QDateTime::currentDateTime();
    now.setTime(QTime(now.time().hour(), now.time().minute(), now.time().second()));
    return now;
}

} // namespace

NoteStore::NoteStore(const QString &folder, QObject *parent)
    : QObject(parent)
    , m_folder(QDir::cleanPath(QDir(folder).absolutePath()))
{
    // Several files can change at once, as when a sync tool brings a batch;
    // one read of the folder answers them all.
    m_rescan.setSingleShot(true);
    m_rescan.setInterval(150);
    connect(&m_rescan, &QTimer::timeout, this, &NoteStore::rescan);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_rescan.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_rescan.start(); });
}

QString NoteStore::defaultFolder()
{
    const QString chosen = qEnvironmentVariable("GOOSEBERRY_FOLDER");
    if (!chosen.isEmpty()) {
        return chosen;
    }
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) {
        documents = QDir::homePath() + QStringLiteral("/Documents");
    }
    return documents + QStringLiteral("/Gooseberry");
}

bool NoteStore::open()
{
    if (!QDir().mkpath(m_folder)) {
        m_lastError = i18n("The notes folder %1 could not be made.", m_folder);
        return false;
    }
    const int format = readMarker();
    if (format < 0) {
        return false;
    }
    if (format == 0 && !writeMarker()) {
        return false;
    }
    m_watcher.addPath(m_folder);
    readWorkspaces();
    rescan();
    if (format == 1) {
        bringUpToDate();
    }
    return true;
}

bool NoteStore::writeMarker()
{
    QSaveFile out(m_folder + QLatin1Char('/') + MarkerName);
    if (!out.open(QIODevice::WriteOnly) || out.write(QStringLiteral("format: %1\n").arg(FolderFormat).toUtf8()) < 0
        || !out.commit()) {
        m_lastError = i18n("The notes folder %1 could not be written to.", m_folder);
        return false;
    }
    return true;
}

void NoteStore::bringUpToDate()
{
    // The first layout kept every note in the folder itself, with the
    // project it belonged to in its header. Each such note moves into a
    // folder named for its project; its header is written in the new format
    // the next time it changes.
    const auto all = m_notes.values();
    for (const Note &note : all) {
        if (!note.folder.isEmpty() || note.formerProject.isEmpty() || note.newerFormat()) {
            continue;
        }
        const QString name = folderNameFor(note.formerProject);
        if (name.isEmpty() || (!hasFolder(name) && !makeFolder(name))) {
            continue;
        }
        m_watcher.removePath(pathFor(note.id));
        if (!moveFiles(note.id, {}, name)) {
            continue;
        }
        // Moved by the update, not by the person: its changed time stays.
        Note moved = note;
        moved.folder = name;
        m_notes.insert(note.id, moved);
        save(moved, Touch::Kept);
    }
    writeMarker();
}

int NoteStore::readMarker()
{
    QFile marker(m_folder + QLatin1Char('/') + MarkerName);
    if (!marker.exists()) {
        return 0;
    }
    if (!marker.open(QIODevice::ReadOnly)) {
        m_lastError = i18n("The notes folder's format file could not be read.");
        return -1;
    }
    int format = FolderFormat;
    for (const auto &line : QString::fromUtf8(marker.readAll()).split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("format:"))) {
            bool ok = false;
            const int found = line.mid(7).trimmed().toInt(&ok);
            if (ok) {
                format = found;
            }
        }
    }
    const bool newer = format > FolderFormat;
    if (newer != m_readOnly) {
        m_readOnly = newer;
        Q_EMIT readOnlyChanged();
    }
    return format;
}

bool NoteStore::refuseWhenReadOnly()
{
    if (m_readOnly) {
        m_lastError = i18n("This folder was laid out by a newer %1, so nothing in it is changed here.", productName());
    }
    return m_readOnly;
}

QString NoteStore::dirFor(const QString &folder) const
{
    return folder.isEmpty() ? m_folder : m_folder + QLatin1Char('/') + folder;
}

QString NoteStore::folderOf(const QString &id) const
{
    const auto found = m_notes.constFind(id);
    return found == m_notes.constEnd() ? QString() : found->folder;
}

QString NoteStore::folderNameProblem(const QString &name)
{
    const QString simple = name.simplified();
    if (simple.isEmpty()) {
        return i18n("A folder needs a name.");
    }
    if (simple.contains(QLatin1Char('/')) || simple.contains(QChar(0))) {
        return i18n("A folder's name cannot have a slash in it.");
    }
    if (simple.startsWith(QLatin1Char('.'))) {
        return i18n("A folder's name cannot start with a dot.");
    }
    if (simple.compare(inboxLabel(), Qt::CaseInsensitive) == 0) {
        return i18nc("%1 is the folder that holds loose notes", "%1 is already there.", inboxLabel());
    }
    if (simple.toUtf8().size() > 200) {
        return i18n("That name is too long for a folder.");
    }
    return {};
}

bool NoteStore::makeFolder(const QString &name)
{
    if (refuseWhenReadOnly()) {
        return false;
    }
    const QString simple = name.simplified();
    const QString problem = folderNameProblem(simple);
    if (!problem.isEmpty()) {
        m_lastError = problem;
        return false;
    }
    if (hasFolder(simple)) {
        m_lastError = i18n("There is already a folder called %1.", simple);
        return false;
    }
    if (!QDir().mkpath(dirFor(simple))) {
        m_lastError = i18n("The folder %1 could not be made.", simple);
        return false;
    }
    rescan();
    return true;
}

bool NoteStore::renameFolder(const QString &from, const QString &to)
{
    if (refuseWhenReadOnly()) {
        return false;
    }
    const QString simple = to.simplified();
    if (!hasFolder(from)) {
        m_lastError = i18n("That folder is no longer there.");
        return false;
    }
    if (simple == from) {
        return true;
    }
    const QString problem = folderNameProblem(simple);
    if (!problem.isEmpty()) {
        m_lastError = problem;
        return false;
    }
    if (hasFolder(simple) || QFileInfo::exists(dirFor(simple))) {
        m_lastError = i18n("There is already a folder called %1.", simple);
        return false;
    }
    if (!QDir().rename(dirFor(from), dirFor(simple))) {
        m_lastError = i18n("The folder %1 could not be renamed.", from);
        return false;
    }
    // The notes are the same files in a folder of a new name: nothing in
    // them changes, so their changed time stays the person's last change.
    for (auto it = m_notes.begin(); it != m_notes.end(); ++it) {
        if (it->folder == from) {
            it->folder = simple;
        }
    }
    bool workspaces = false;
    for (auto it = m_workspaces.begin(); it != m_workspaces.end(); ++it) {
        if (it.value() == from) {
            it.value() = simple;
            workspaces = true;
        }
    }
    if (workspaces) {
        writeWorkspaces();
    }
    m_folders.removeOne(from);
    rescan();
    for (const Note &note : std::as_const(m_notes)) {
        if (note.folder == simple) {
            Q_EMIT noteChanged(note.id);
        }
    }
    return true;
}

std::optional<QStringList> NoteStore::removeFolder(const QString &name)
{
    if (refuseWhenReadOnly()) {
        return std::nullopt;
    }
    if (!hasFolder(name)) {
        m_lastError = i18n("That folder is no longer there.");
        return std::nullopt;
    }
    QStringList moved;
    const auto all = m_notes.values();
    for (const Note &note : all) {
        if (note.folder != name) {
            continue;
        }
        if (!moveNote(note.id, {})) {
            return std::nullopt;
        }
        moved.append(note.id);
    }
    // Whatever else is in it, such as a sync tool's own files, goes to the
    // trash with the folder; nothing is deleted outright.
    const QString path = dirFor(name);
    const bool empty = QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    const bool gone = empty ? QDir().rmdir(path) : !Trash::move(path).isEmpty();
    if (!gone) {
        m_lastError = i18nc("%2 is the folder that holds loose notes", "The folder %1 could not be removed; its notes are in %2.", name, inboxLabel());
        rescan();
        return std::nullopt;
    }
    bool workspaces = false;
    for (auto it = m_workspaces.begin(); it != m_workspaces.end();) {
        if (it.value() == name) {
            it = m_workspaces.erase(it);
            workspaces = true;
        } else {
            ++it;
        }
    }
    if (workspaces) {
        writeWorkspaces();
    }
    rescan();
    return moved;
}

bool NoteStore::moveFiles(const QString &id, const QString &from, const QString &to)
{
    const QString source = dirFor(from) + QLatin1Char('/') + id;
    const QString target = dirFor(to) + QLatin1Char('/') + id;
    if (QFileInfo::exists(target + NoteSuffix) || !QFile::rename(source + NoteSuffix, target + NoteSuffix)) {
        return false;
    }
    // Ink goes with its note.
    if (QFileInfo::exists(source + InkSuffix) && !QFileInfo::exists(target + InkSuffix)) {
        QFile::rename(source + InkSuffix, target + InkSuffix);
    }
    return true;
}

bool NoteStore::moveNote(const QString &id, const QString &folder)
{
    if (refuseWhenReadOnly()) {
        return false;
    }
    auto found = m_notes.find(id);
    if (found == m_notes.end()) {
        m_lastError = i18n("That note is no longer in the folder.");
        return false;
    }
    if (found->folder == folder) {
        return true;
    }
    if (!folder.isEmpty() && !hasFolder(folder)) {
        m_lastError = i18n("The folder %1 is no longer there.", folder);
        return false;
    }
    if (found->newerFormat()) {
        m_lastError = i18n("This note was kept by a newer %1, so it is not changed here.", productName());
        return false;
    }
    const QString before = found->folder;
    // One step on disk: the file is in one folder or the other, never both.
    m_watcher.removePath(pathFor(id));
    if (!moveFiles(id, before, folder)) {
        m_lastError = i18n("The note could not be moved to %1.", folder.isEmpty() ? inboxLabel() : folder);
        return false;
    }
    Note note = *found;
    note.folder = folder;
    m_notes.insert(id, note);
    // Its place changed, so its header says so; a crash before this leaves
    // the note moved with its old time, never lost.
    return save(note);
}

QString NoteStore::workspaceFolder(const QString &workspace) const
{
    const QString folder = m_workspaces.value(workspace);
    return hasFolder(folder) ? folder : QString();
}

bool NoteStore::setWorkspaceFolder(const QString &workspace, const QString &folder)
{
    if (refuseWhenReadOnly()) {
        return false;
    }
    if (workspace.isEmpty() || (!folder.isEmpty() && !hasFolder(folder))) {
        m_lastError = i18n("That folder is no longer there.");
        return false;
    }
    if (folder.isEmpty()) {
        m_workspaces.remove(workspace);
    } else {
        m_workspaces.insert(workspace, folder);
    }
    if (!writeWorkspaces()) {
        return false;
    }
    Q_EMIT foldersChanged();
    return true;
}

void NoteStore::readWorkspaces()
{
    m_workspaces.clear();
    QFile file(m_folder + QLatin1Char('/') + WorkspacesName);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    // Each line is a workspace's name and its folder's, as a note's header
    // writes text: "Desk": "Shuffle launch".
    for (const QString &line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1Char('"'))) {
            continue;
        }
        qsizetype end = 1;
        while (end < trimmed.size() && trimmed.at(end) != QLatin1Char('"')) {
            end += trimmed.at(end) == QLatin1Char('\\') ? 2 : 1;
        }
        const qsizetype colon = trimmed.indexOf(QLatin1Char(':'), end);
        if (colon < 0) {
            continue;
        }
        const QString workspace = headerUnquoted(trimmed.left(end + 1));
        const QString folder = headerUnquoted(trimmed.mid(colon + 1));
        if (!workspace.isEmpty() && !folder.isEmpty()) {
            m_workspaces.insert(workspace, folder);
        }
    }
}

bool NoteStore::writeWorkspaces()
{
    QStringList workspaces = m_workspaces.keys();
    workspaces.sort();
    QString out;
    for (const QString &workspace : std::as_const(workspaces)) {
        out += headerQuoted(workspace) + QLatin1String(": ") + headerQuoted(m_workspaces.value(workspace)) + QLatin1Char('\n');
    }
    QSaveFile file(m_folder + QLatin1Char('/') + WorkspacesName);
    if (!file.open(QIODevice::WriteOnly) || file.write(out.toUtf8()) < 0 || !file.commit()) {
        m_lastError = i18n("The workspace's folder could not be kept.");
        return false;
    }
    return true;
}

std::optional<Note> NoteStore::note(const QString &id) const
{
    const auto found = m_notes.constFind(id);
    if (found == m_notes.constEnd()) {
        return std::nullopt;
    }
    return *found;
}

QString NoteStore::pathFor(const QString &id) const
{
    return dirFor(folderOf(id)) + QLatin1Char('/') + id + NoteSuffix;
}

QString NoteStore::inkPathFor(const QString &id) const
{
    return dirFor(folderOf(id)) + QLatin1Char('/') + id + InkSuffix;
}

QString NoteStore::newId(const QDateTime &time) const
{
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    for (;;) {
        QString suffix;
        for (int i = 0; i < 4; ++i) {
            suffix += QLatin1Char(alphabet[QRandomGenerator::global()->bounded(int(sizeof(alphabet) - 1))]);
        }
        const QString id = time.toString(QStringLiteral("yyyy-MM-dd-HHmmss-")) + suffix;
        if (!m_notes.contains(id) && !QFile::exists(pathFor(id))) {
            return id;
        }
    }
}

NoteStore::Seen NoteStore::seen(const QString &path)
{
#ifdef Q_OS_UNIX
    // The file's identity and its time to the nanosecond: a change made by
    // another program a moment after a save here is still seen.
    struct stat info;
    if (::stat(QFile::encodeName(path).constData(), &info) != 0) {
        return {};
    }
    return {qint64(info.st_size), qint64(info.st_ino), qint64(info.st_mtim.tv_sec), qint64(info.st_mtim.tv_nsec)};
#else
    const QFileInfo info(path);
    if (!info.exists()) {
        return {};
    }
    const qint64 msecs = info.lastModified().toMSecsSinceEpoch();
    return {info.size(), 0, msecs / 1000, (msecs % 1000) * 1000000};
#endif
}

bool NoteStore::write(const Note &note)
{
    if (m_readOnly || note.newerFormat()) {
        m_lastError = i18n("This note was kept by a newer %1, so it is not changed here.", productName());
        return false;
    }
    if (!QFileInfo::exists(m_folder)) {
        // The folder went while Gooseberry was open: it is made again rather
        // than the note being lost.
        if (!QDir().mkpath(m_folder) || readMarker() < 0 || !writeMarker()) {
            m_lastError = i18n("The notes folder %1 could not be made.", m_folder);
            return false;
        }
        m_watcher.addPath(m_folder);
    }
    const QString dir = dirFor(note.folder);
    if (!note.folder.isEmpty() && !QFileInfo::exists(dir)) {
        // Its folder was removed by another program: it is made again.
        if (!QDir().mkpath(dir)) {
            m_lastError = i18n("The folder %1 could not be made.", note.folder);
            return false;
        }
        m_watcher.addPath(dir);
    }
    const QString path = dir + QLatin1Char('/') + note.id + NoteSuffix;
    // A note is replaced whole: the old file stays until the new one is
    // complete on disk, so a crash mid-write never leaves half a note.
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = file.errorString();
        return false;
    }
    const QByteArray bytes = note.serialize();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        m_lastError = file.errorString();
        return false;
    }
    m_seen.insert(note.id, seen(path));
    if (!m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }
    return true;
}

QString NoteStore::create(Note note)
{
    const QDateTime now = nowToTheSecond();
    if (!note.created.isValid()) {
        note.created = now;
    }
    note.changed = now;
    note.format = NoteFormat;
    note.formerProject.clear();
    if (!note.folder.isEmpty() && !hasFolder(note.folder)) {
        note.folder.clear();
    }
    note.id = newId(note.created);
    if (!write(note)) {
        return {};
    }
    m_notes.insert(note.id, note);
    Q_EMIT noteAdded(note.id);
    return note.id;
}

bool NoteStore::save(Note note, Touch touch)
{
    if (!m_notes.contains(note.id)) {
        m_lastError = i18n("That note is no longer in the folder.");
        return false;
    }
    if (touch == Touch::Changed) {
        note.changed = nowToTheSecond();
    }
    // A note stays where it is on disk: only moveNote() changes its folder,
    // so a save from a card that has not caught up with a move never moves
    // it back.
    note.folder = folderOf(note.id);
    if (note.format < NoteFormat && !note.newerFormat()) {
        note.format = NoteFormat;
        note.formerProject.clear();
    }
    if (!write(note)) {
        return false;
    }
    m_notes.insert(note.id, note);
    Q_EMIT noteChanged(note.id);
    return true;
}

std::optional<QString> NoteStore::trash(const QString &id)
{
    if (refuseWhenReadOnly()) {
        return std::nullopt;
    }
    const QString path = pathFor(id);
    const QString folder = folderOf(id);
    QString pathInTrash;
    if (QFile::exists(path)) {
        pathInTrash = Trash::move(path);
        if (pathInTrash.isEmpty()) {
            m_lastError = i18n("The note could not be moved to the trash.");
            return std::nullopt;
        }
    }
    // Ink goes with its note, so the trash holds the whole of it.
    const QString ink = inkPathFor(id);
    if (QFile::exists(ink)) {
        Trash::move(ink);
    }
    m_watcher.removePath(path);
    m_seen.remove(id);
    m_trashedFrom.insert(id, folder);
    if (m_notes.remove(id) > 0) {
        Q_EMIT noteRemoved(id);
    }
    return pathInTrash;
}

bool NoteStore::restore(const QString &id, const QString &pathInTrash)
{
    // Back to the folder it came from, or Inbox when that has gone since.
    const QString from = m_trashedFrom.take(id);
    const QString folder = hasFolder(from) ? from : QString();
    if (!Trash::restore(pathInTrash, dirFor(folder) + QLatin1Char('/') + id + NoteSuffix)) {
        m_lastError = i18n("The note could not be brought back from the trash.");
        return false;
    }
    rescan();
    return m_notes.contains(id);
}

void NoteStore::rescan()
{
    readMarker();
    // Every folder is a visible folder inside the notes folder, one level
    // deep. Hidden ones, such as a sync tool's, are not notes' folders.
    QStringList folders;
    const auto dirs = QDir(m_folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    for (const QFileInfo &info : dirs) {
        if (folderNameProblem(info.fileName()).isEmpty() && info.fileName() == info.fileName().simplified()) {
            folders.append(info.fileName());
        }
    }
    std::sort(folders.begin(), folders.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    const bool foldersMoved = folders != m_folders;
    m_folders = folders;

    QSet<QString> present;
    QStringList watched;
    QStringList watchedDirs{m_folder};
    QStringList places{QString()};
    places += folders;
    for (const QString &folder : std::as_const(places)) {
        const QDir dir(dirFor(folder));
        if (!folder.isEmpty()) {
            watchedDirs.append(dir.absolutePath());
        }
        const auto entries = dir.entryInfoList({QStringLiteral("*") + NoteSuffix}, QDir::Files | QDir::Readable);
        for (const QFileInfo &info : entries) {
            const QString id = info.completeBaseName();
            if (present.contains(id)) {
                // The same note in two folders, as a copy made by hand: the
                // first found, Inbox and then folders in order, is the one.
                continue;
            }
            present.insert(id);
            watched.append(info.absoluteFilePath());
            const Seen now = seen(info.absoluteFilePath());
            const auto known = m_seen.constFind(id);
            const auto existing = m_notes.constFind(id);
            const bool sameFolder = existing != m_notes.constEnd() && existing->folder == folder;
            if (known != m_seen.constEnd() && *known == now && sameFolder) {
                continue;
            }
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            Note read = Note::parse(file.readAll(), id, info.lastModified());
            read.folder = folder;
            m_seen.insert(id, now);
            if (existing == m_notes.constEnd()) {
                m_notes.insert(id, read);
                Q_EMIT noteAdded(id);
            } else if (!sameNote(*existing, read)) {
                m_notes.insert(id, read);
                Q_EMIT noteChanged(id);
            }
        }
    }

    const auto ids = m_notes.keys();
    for (const QString &id : ids) {
        if (!present.contains(id)) {
            m_notes.remove(id);
            m_seen.remove(id);
            Q_EMIT noteRemoved(id);
        }
    }

    watched += watchedDirs;
    const QStringList current = m_watcher.files() + m_watcher.directories();
    for (const QString &path : current) {
        if (!watched.contains(path)) {
            m_watcher.removePath(path);
        }
    }
    for (const QString &path : std::as_const(watched)) {
        if (!current.contains(path)) {
            m_watcher.addPath(path);
        }
    }
    if (foldersMoved) {
        Q_EMIT foldersChanged();
    }
}

} // namespace Gooseberry
