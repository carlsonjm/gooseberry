// SPDX-License-Identifier: GPL-2.0-or-later
#include "Trash.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace Gooseberry::Trash {

namespace {

QString infoPathFor(const QString &pathInTrash)
{
    const QFileInfo inTrash(pathInTrash);
    return QDir::cleanPath(inTrash.absolutePath() + QStringLiteral("/../info/") + inTrash.fileName()
                           + QStringLiteral(".trashinfo"));
}

} // namespace

QString homeTrash()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
}

QString move(const QString &path)
{
    const QFileInfo source(path);
    if (!source.exists()) {
        return {};
    }
    const QString trash = homeTrash();
    QDir files(trash + QStringLiteral("/files"));
    QDir info(trash + QStringLiteral("/info"));
    if (!QDir().mkpath(files.path()) || !QDir().mkpath(info.path())) {
        return {};
    }
    QFile::setPermissions(trash, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    // The record is made first, and made only if no other file has the name,
    // so two removals never claim the same place in the trash.
    const QString base = source.completeBaseName();
    const QString suffix = source.suffix().isEmpty() ? QString() : QLatin1Char('.') + source.suffix();
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const QString name = attempt == 0 ? source.fileName() : QStringLiteral("%1 %2%3").arg(base).arg(attempt + 1).arg(suffix);
        QFile record(info.filePath(name + QStringLiteral(".trashinfo")));
        if (!record.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            continue;
        }
        if (files.exists(name)) {
            record.remove();
            continue;
        }
        const QByteArray encoded = QUrl::toPercentEncoding(source.absoluteFilePath(), "/");
        record.write("[Trash Info]\nPath=" + encoded + "\nDeletionDate="
                     + QDateTime::currentDateTime().toString(Qt::ISODate).left(19).toUtf8() + "\n");
        record.close();
        const QString target = files.filePath(name);
        if (QFile::rename(source.absoluteFilePath(), target)) {
            return target;
        }
        record.remove();
        // On another disk than the home trash: the trash kept on that disk.
        QString elsewhere;
        if (QFile::moveToTrash(source.absoluteFilePath(), &elsewhere)) {
            return elsewhere;
        }
        return {};
    }
    return {};
}

bool restore(const QString &pathInTrash, const QString &path)
{
    if (QFileInfo::exists(path) || !QFile::rename(pathInTrash, path)) {
        return false;
    }
    QFile::remove(infoPathFor(pathInTrash));
    return true;
}

} // namespace Gooseberry::Trash
