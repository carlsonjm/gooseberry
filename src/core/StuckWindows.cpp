// SPDX-License-Identifier: GPL-2.0-or-later
#include "StuckWindows.h"

#include <QRegularExpression>
#include <QUuid>

#include <algorithm>

namespace Gooseberry::StuckWindows {

namespace {

bool sameId(const QString &a, const QString &b)
{
    if (a == b) {
        return true;
    }
    // A UUID may come with or without braces, in either case.
    const QUuid left(a);
    return !left.isNull() && left == QUuid(b);
}

} // namespace

QString bareCaption(const QString &caption)
{
    static const QRegularExpression suffix(QStringLiteral("\\s*<\\d+>$"));
    QString bare = caption.trimmed();
    bare.remove(suffix);
    return bare;
}

bool sameApp(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty()) {
        return false;
    }
    if (a.compare(b, Qt::CaseInsensitive) == 0) {
        return true;
    }
    const auto endsAfterDot = [](const QString &longer, const QString &shorter) {
        return longer.size() > shorter.size() && longer.endsWith(shorter, Qt::CaseInsensitive)
            && longer.at(longer.size() - shorter.size() - 1) == QLatin1Char('.');
    };
    return endsAfterDot(a, b) || endsAfterDot(b, a);
}

QList<Note> notesOn(const OpenWindow &window, const QList<Note> &notes)
{
    QList<Note> on;
    if (window.window.isEmpty()) {
        return on;
    }
    for (const Note &note : notes) {
        if (note.isStuck() && !note.tucked && note.window == window.window && note.app == window.app) {
            on.append(note);
        }
    }
    std::sort(on.begin(), on.end(), [](const Note &a, const Note &b) {
        return a.changed != b.changed ? a.changed > b.changed : a.id > b.id;
    });
    return on;
}

QVariantList entries(const QList<OpenWindow> &windows, const QList<Note> &notes, const QSet<QString> &shownKeys)
{
    QVariantList list;
    for (const OpenWindow &window : windows) {
        const QList<Note> on = notesOn(window, notes);
        if (on.isEmpty()) {
            continue;
        }
        QVariantList noteList;
        for (const Note &note : on) {
            noteList.append(QVariantMap{
                {QStringLiteral("id"), note.id},
                {QStringLiteral("title"), note.title()},
                {QStringLiteral("text"), note.text},
                {QStringLiteral("colour"), note.colour},
                {QStringLiteral("colourHex"), colourHex(note.colour)},
            });
        }
        list.append(QVariantMap{
            {QStringLiteral("windowIds"), window.ids},
            {QStringLiteral("caption"), window.caption},
            {QStringLiteral("app"), window.app},
            {QStringLiteral("window"), window.window},
            {QStringLiteral("count"), uint(on.size())},
            {QStringLiteral("colour"), on.constFirst().colour},
            {QStringLiteral("colourHex"), colourHex(on.constFirst().colour)},
            {QStringLiteral("notes"), noteList},
            {QStringLiteral("shown"), shownKeys.contains(window.key())},
        });
    }
    return list;
}

qsizetype find(const QList<OpenWindow> &windows, const QString &windowId, const QString &caption, const QString &app)
{
    if (!windowId.isEmpty()) {
        for (qsizetype i = 0; i < windows.size(); ++i) {
            for (const QString &id : windows.at(i).ids) {
                if (sameId(id, windowId)) {
                    return i;
                }
            }
        }
    }
    const QString bare = bareCaption(caption);
    if (bare.isEmpty()) {
        return -1;
    }
    qsizetype byCaption = -1;
    int matches = 0;
    for (qsizetype i = 0; i < windows.size(); ++i) {
        if (bareCaption(windows.at(i).caption) != bare) {
            continue;
        }
        if (sameApp(windows.at(i).app, app)) {
            return i;
        }
        byCaption = i;
        ++matches;
    }
    // A title bar's class can differ from the window list's application id;
    // a caption only one window has is that window.
    return matches == 1 ? byCaption : -1;
}

} // namespace Gooseberry::StuckWindows
