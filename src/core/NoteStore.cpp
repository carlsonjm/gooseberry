// SPDX-License-Identifier: GPL-2.0-or-later
#include "NoteStore.h"

#include "Trash.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace Gooseberry {

namespace {

const QString MarkerName = QStringLiteral(".gooseberry");
const QString NoteSuffix = QStringLiteral(".md");
const QString InkSuffix = QStringLiteral(".svg");

bool sameNote(const Note &a, const Note &b)
{
    return a.text == b.text && a.colour == b.colour && a.belongs == b.belongs && a.window == b.window
        && a.app == b.app && a.project == b.project && a.workspace == b.workspace && a.tucked == b.tucked
        && a.format == b.format && a.extra == b.extra && a.created == b.created && a.changed == b.changed;
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
        m_lastError = QStringLiteral("The notes folder %1 could not be made.").arg(m_folder);
        return false;
    }
    if (!readMarker()) {
        return false;
    }
    m_watcher.addPath(m_folder);
    rescan();
    return true;
}

bool NoteStore::readMarker()
{
    QFile marker(m_folder + QLatin1Char('/') + MarkerName);
    if (!marker.exists()) {
        QSaveFile out(marker.fileName());
        if (!out.open(QIODevice::WriteOnly) || out.write(QStringLiteral("format: %1\n").arg(FolderFormat).toUtf8()) < 0
            || !out.commit()) {
            m_lastError = QStringLiteral("The notes folder %1 could not be written to.").arg(m_folder);
            return false;
        }
        return true;
    }
    if (!marker.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("The notes folder's format file could not be read.");
        return false;
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
    return m_folder + QLatin1Char('/') + id + NoteSuffix;
}

QString NoteStore::inkPathFor(const QString &id) const
{
    return m_folder + QLatin1Char('/') + id + InkSuffix;
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
        m_lastError = QStringLiteral("This note was kept by a newer Gooseberry, so it is not changed here.");
        return false;
    }
    if (!QFileInfo::exists(m_folder)) {
        // The folder went while Gooseberry was open: it is made again rather
        // than the note being lost.
        if (!QDir().mkpath(m_folder) || !readMarker()) {
            m_lastError = QStringLiteral("The notes folder %1 could not be made.").arg(m_folder);
            return false;
        }
        m_watcher.addPath(m_folder);
    }
    const QString path = pathFor(note.id);
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
    note.id = newId(note.created);
    if (!write(note)) {
        return {};
    }
    m_notes.insert(note.id, note);
    Q_EMIT noteAdded(note.id);
    return note.id;
}

bool NoteStore::save(Note note)
{
    if (!m_notes.contains(note.id)) {
        m_lastError = QStringLiteral("That note is no longer in the folder.");
        return false;
    }
    note.changed = nowToTheSecond();
    if (!write(note)) {
        return false;
    }
    m_notes.insert(note.id, note);
    Q_EMIT noteChanged(note.id);
    return true;
}

std::optional<QString> NoteStore::trash(const QString &id)
{
    if (m_readOnly) {
        m_lastError = QStringLiteral("This folder was laid out by a newer Gooseberry, so nothing in it is changed here.");
        return std::nullopt;
    }
    const QString path = pathFor(id);
    QString pathInTrash;
    if (QFile::exists(path)) {
        pathInTrash = Trash::move(path);
        if (pathInTrash.isEmpty()) {
            m_lastError = QStringLiteral("The note could not be moved to the trash.");
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
    if (m_notes.remove(id) > 0) {
        Q_EMIT noteRemoved(id);
    }
    return pathInTrash;
}

bool NoteStore::restore(const QString &id, const QString &pathInTrash)
{
    if (!Trash::restore(pathInTrash, pathFor(id))) {
        m_lastError = QStringLiteral("The note could not be brought back from the trash.");
        return false;
    }
    rescan();
    return m_notes.contains(id);
}

void NoteStore::rescan()
{
    readMarker();
    const QDir dir(m_folder);
    const auto entries = dir.entryInfoList({QStringLiteral("*") + NoteSuffix}, QDir::Files | QDir::Readable);

    QSet<QString> present;
    QStringList watched;
    for (const QFileInfo &info : entries) {
        const QString id = info.completeBaseName();
        present.insert(id);
        watched.append(info.absoluteFilePath());
        const Seen now = seen(info.absoluteFilePath());
        const auto known = m_seen.constFind(id);
        if (known != m_seen.constEnd() && *known == now) {
            continue;
        }
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        Note read = Note::parse(file.readAll(), id, info.lastModified());
        m_seen.insert(id, now);
        const auto existing = m_notes.constFind(id);
        if (existing == m_notes.constEnd()) {
            m_notes.insert(id, read);
            Q_EMIT noteAdded(id);
        } else if (!sameNote(*existing, read)) {
            m_notes.insert(id, read);
            Q_EMIT noteChanged(id);
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

    const QStringList current = m_watcher.files();
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
}

} // namespace Gooseberry
