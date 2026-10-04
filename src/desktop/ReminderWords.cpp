// SPDX-License-Identifier: GPL-2.0-or-later
#include "ReminderWords.h"

#include "Reminders.h"

#include <KLocalizedString>

#include <QLocale>

namespace Gooseberry::ReminderWords {

namespace {

QString timeText(const QDateTime &time)
{
    // The desktop's short time, to the minute.
    QString format = QLocale().timeFormat(QLocale::ShortFormat);
    format.remove(QStringLiteral(":ss")).remove(QStringLiteral(".ss"));
    return QLocale().toString(time.toLocalTime().time(), format);
}

} // namespace

QString label(const QDateTime &remind, bool onOpen, const QDateTime &now)
{
    if (onOpen) {
        return i18n("Next time this opens");
    }
    if (!remind.isValid()) {
        return {};
    }
    const QDateTime local = remind.toLocalTime();
    const qint64 days = now.toLocalTime().date().daysTo(local.date());
    const QString time = timeText(local);
    if (days == 0) {
        return i18nc("a reminder's day and time", "Today %1", time);
    }
    if (days == 1) {
        return i18nc("a reminder's day and time", "Tomorrow %1", time);
    }
    if (days == -1) {
        return i18nc("a reminder's day and time", "Yesterday %1", time);
    }
    if (days > 1 && days < 7) {
        return i18nc("a reminder's weekday and time", "%1 %2", QLocale().dayName(local.date().dayOfWeek()), time);
    }
    return i18nc("a reminder's date and time", "%1 %2", QLocale().toString(local.date(), QStringLiteral("d MMMM")), time);
}

QVariantList choices(bool hasWindow, const QDateTime &now)
{
    QVariantList list;
    for (const QuickTime &quick : quickTimes(now)) {
        QString words;
        if (quick.kind == QLatin1String("later")) {
            words = i18n("Later today · %1", timeText(quick.time));
        } else if (quick.kind == QLatin1String("evening")) {
            words = i18n("This evening · %1", timeText(quick.time));
        } else {
            words = i18n("Tomorrow · %1", timeText(quick.time));
        }
        list.append(QVariantMap{{QStringLiteral("kind"), quick.kind},
                                {QStringLiteral("label"), words},
                                {QStringLiteral("time"), quick.time}});
    }
    if (hasWindow) {
        list.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("opens")}, {QStringLiteral("label"), i18n("Next time this opens")}});
    }
    list.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("pick")}, {QStringLiteral("label"), i18n("Pick a time…")}});
    return list;
}

} // namespace Gooseberry::ReminderWords
