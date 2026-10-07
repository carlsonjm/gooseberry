// SPDX-License-Identifier: GPL-2.0-or-later
#include "Planner.h"

#include "Checklist.h"
#include "NoteStore.h"

#include <QDateTime>

#include <algorithm>

namespace Gooseberry {

namespace {

QDateTime nowToTheSecond()
{
    QDateTime now = QDateTime::currentDateTime();
    now.setTime(QTime(now.time().hour(), now.time().minute(), now.time().second()));
    return now;
}

// The words a planner row shows: a checklist by its heading, or its items.
QString rowTitle(const Note &note)
{
    if (Checklist::contains(note.text)) {
        const QString heading = Checklist::heading(note.text);
        return heading.isEmpty() ? Checklist::summary(note.text) : heading;
    }
    return note.title();
}

} // namespace

bool setNoteDone(NoteStore *store, const QString &id, bool done)
{
    auto note = store->note(id);
    if (!note || note->newerFormat() || store->readOnly() || note->done.isValid() == done) {
        return false;
    }
    note->done = done ? nowToTheSecond() : QDateTime();
    return store->save(*note);
}

Planner::Planner(NoteStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_today(QDate::currentDate())
    , m_day(m_today)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(0);
    connect(&m_settle, &QTimer::timeout, this, &Planner::rebuild);
    for (auto signal : {&NoteStore::noteAdded, &NoteStore::noteChanged, &NoteStore::noteRemoved}) {
        connect(m_store, signal, &m_settle, qOverload<>(&QTimer::start));
    }
    // Which note is next, and which day is today, move with the clock.
    m_minute.setInterval(60 * 1000);
    connect(&m_minute, &QTimer::timeout, this, [this] {
        const QDate today = QDate::currentDate();
        if (today != m_today) {
            // Past midnight, a planner left on the old today follows it.
            if (m_day == m_today) {
                m_day = today;
            }
            m_today = today;
        }
        rebuild();
    });
    m_minute.start();
    rebuild();
}

void Planner::setDay(const QDate &day)
{
    if (!day.isValid() || day == m_day) {
        return;
    }
    m_day = day;
    rebuild();
}

void Planner::showToday()
{
    m_today = QDate::currentDate();
    m_day = m_today;
    rebuild();
}

bool Planner::setDone(const QString &id, bool done)
{
    return setNoteDone(m_store, id, done);
}

void Planner::rebuild()
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<Note> timed;
    for (const Note &note : m_store->notes()) {
        if (note.remind.isValid()) {
            timed.append(note);
        }
    }
    std::sort(timed.begin(), timed.end(), [](const Note &a, const Note &b) {
        return a.remind != b.remind ? a.remind < b.remind : a.id < b.id;
    });

    // The next note still to come today is the one the planner points at.
    QString next;
    for (const Note &note : std::as_const(timed)) {
        if (note.plannedDay() == m_today && !note.done.isValid() && note.remind >= now) {
            next = note.id;
            break;
        }
    }

    auto rowsFor = [&](const QDate &date) {
        QVariantList rows;
        for (const Note &note : std::as_const(timed)) {
            if (note.plannedDay() != date) {
                continue;
            }
            rows.append(QVariantMap{
                {QStringLiteral("id"), note.id},
                {QStringLiteral("time"), note.remind.toLocalTime()},
                {QStringLiteral("done"), note.done.isValid()},
                {QStringLiteral("title"), rowTitle(note)},
                {QStringLiteral("place"), note.whereLabel()},
                {QStringLiteral("next"), note.id == next},
                {QStringLiteral("colour"), colourHex(note.colour)},
                {QStringLiteral("readOnly"), note.newerFormat() || m_store->readOnly()},
            });
        }
        return rows;
    };

    QVariantList strip;
    for (int offset = -DaysBack; offset <= DaysAhead; ++offset) {
        const QDate date = m_today.addDays(offset);
        bool planned = false;
        for (const Note &note : std::as_const(timed)) {
            planned = planned || note.plannedDay() == date;
        }
        strip.append(QVariantMap{
            {QStringLiteral("date"), shown(date)},
            {QStringLiteral("today"), date == m_today},
            {QStringLiteral("chosen"), date == m_day},
            {QStringLiteral("planned"), planned && date != m_day},
        });
    }

    QVariantList days;
    days.append(QVariantMap{
        {QStringLiteral("date"), shown(m_day)},
        {QStringLiteral("today"), m_day == m_today},
        {QStringLiteral("rows"), rowsFor(m_day)},
    });
    for (int offset = 1; offset <= LooksAhead; ++offset) {
        const QDate date = m_day.addDays(offset);
        const QVariantList rows = rowsFor(date);
        if (!rows.isEmpty()) {
            days.append(QVariantMap{
                {QStringLiteral("date"), shown(date)},
                {QStringLiteral("today"), date == m_today},
                {QStringLiteral("rows"), rows},
            });
        }
    }

    if (strip != m_strip || days != m_days) {
        m_strip = strip;
        m_days = days;
        Q_EMIT changed();
    }
}

} // namespace Gooseberry
