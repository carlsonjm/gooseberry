// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Trash.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>

// A home of the test's own. Made before the application object, it points
// every place a person's notes, settings, trash or session could be found at a
// temporary folder, and refuses to run if that did not take.
class TestHome
{
public:
    TestHome()
    {
        if (!m_dir.isValid()) {
            std::fputs("No temporary folder for the test's home.\n", stderr);
            std::exit(1);
        }
        const QByteArray root = QFile::encodeName(m_dir.path());
        qputenv("HOME", root);
        qputenv("XDG_DATA_HOME", QByteArray(root + "/share"));
        qputenv("XDG_CONFIG_HOME", QByteArray(root + "/config"));
        qputenv("XDG_CACHE_HOME", QByteArray(root + "/cache"));
        qputenv("XDG_STATE_HOME", QByteArray(root + "/state"));
        qputenv("GOOSEBERRY_FOLDER", QByteArray(root + "/Documents/Gooseberry"));
        // No session: nothing here may reach the person's desktop.
        qunsetenv("DBUS_SESSION_BUS_ADDRESS");
        qunsetenv("WAYLAND_DISPLAY");
        qunsetenv("DISPLAY");
        QStandardPaths::setTestModeEnabled(true);
        if (QDir::homePath() != m_dir.path()) {
            std::fputs("The test's home did not take.\n", stderr);
            std::exit(1);
        }
    }

    QString path() const { return m_dir.path(); }
    QString notesFolder() const { return qEnvironmentVariable("GOOSEBERRY_FOLDER"); }
    // Where the desktop's trash is for files in the test's home.
    QString trash() const { return Gooseberry::Trash::homeTrash(); }
    bool holds(const QString &path) const
    {
        return QDir::cleanPath(path).startsWith(m_dir.path() + QLatin1Char('/'));
    }

private:
    QTemporaryDir m_dir{QDir::tempPath() + QStringLiteral("/gooseberry-test-XXXXXX")};
};
