// SPDX-License-Identifier: GPL-2.0-or-later
#include "Reminders.h"

#include "NoteStore.h"
#include "Planner.h"

#include <algorithm>

namespace Gooseberry {

namespace {

// The longest wait between two looks at the clock. A timer stands still while
// the computer sleeps, so a long one would wake late; this bounds how late.
constexpr qint64 LongestWaitMs = 30 * 1000;

QDateTime nowToTheSecond()
{
    QDateTime now = QDateTime::currentDateTime();
    now.setTime(QTime(now.time().hour(), now.time().minute(), now.time().second()));
    return now;
}

} // namespace

QList<QuickTime> quickTimes(const QDateTime &now)
{
    QList<QuickTime> times;
    const QDate today = now.date();
    const QDateTime evening(today, QTime(19, 0));
    QDateTime later = now.addSecs(2 * 3600);
    later = QDateTime(later.date(), QTime(later.time().hour(), 0));
    if (later < now.addSecs(2 * 3600)) {
        later = later.addSecs(3600);
    }
    if (later.date() == today && later < evening) {
        times.append({QStringLiteral("later"), later});
    }
    if (now.time() < QTime(18, 0)) {
        times.append({QStringLiteral("evening"), evening});
    }
    times.append({QStringLiteral("tomorrow"), QDateTime(today.addDays(1), QTime(9, 0))});
    return times;
}

Reminders::Reminders(NoteStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Reminders::check);
    // A burst of saves while typing is answered once.
    m_settle.setSingleShot(true);
    m_settle.setInterval(0);
    connect(&m_settle, &QTimer::timeout, this, &Reminders::schedule);
    for (auto signal : {&NoteStore::noteAdded, &NoteStore::noteChanged, &NoteStore::noteRemoved}) {
        connect(m_store, signal, &m_settle, qOverload<>(&QTimer::start));
    }
    m_settle.start();
}

void Reminders::setReady(bool ready)
{
    if (ready == m_ready) {
        return;
    }
    m_ready = ready;
    if (m_ready) {
        check();
    }
}

QDateTime Reminders::nextDue() const
{
    QDateTime next;
    for (const Note &note : m_store->notes()) {
        if (note.reminderWaiting() && note.remind.isValid() && (!next.isValid() || note.remind < next)) {
            next = note.remind;
        }
    }
    return next;
}

void Reminders::schedule()
{
    const QDateTime next = nextDue();
    if (!next.isValid()) {
        m_timer.stop();
        return;
    }
    const qint64 wait = QDateTime::currentDateTime().msecsTo(next);
    if (wait <= 0 && !m_checking) {
        // A reminder whose time has already come, as one set in the past or
        // read from another program: shown now.
        check();
        return;
    }
    // Past the look just made, a reminder still waiting could not be shown
    // yet; it is tried again after the longest wait.
    m_timer.start(int(wait <= 0 ? LongestWaitMs : std::min(wait, LongestWaitMs)));
}

void Reminders::check()
{
    m_checking = true;
    if (m_ready && !m_store->readOnly()) {
        const QDateTime now = QDateTime::currentDateTime();
        QList<Note> due;
        for (const Note &note : m_store->notes()) {
            if (note.reminderWaiting() && note.remind.isValid() && note.remind <= now && !note.newerFormat()) {
                due.append(note);
            }
        }
        std::sort(due.begin(), due.end(), [](const Note &a, const Note &b) {
            return a.remind < b.remind;
        });
        for (const Note &note : std::as_const(due)) {
            fire(note.id);
        }
    }
    schedule();
    m_checking = false;
}

void Reminders::documentOpened(const QString &window, const QString &app)
{
    if (!m_ready || m_store->readOnly() || window.isEmpty()) {
        return;
    }
    QStringList ids;
    for (const Note &note : m_store->notes()) {
        if (note.reminderWaiting() && note.remindOnOpen && !note.newerFormat() && note.window == window
            && (note.app.isEmpty() || app.isEmpty() || note.app == app)) {
            ids.append(note.id);
        }
    }
    ids.sort();
    for (const QString &id : std::as_const(ids)) {
        fire(id);
    }
}

void Reminders::fire(const QString &id)
{
    auto note = m_store->note(id);
    if (!note || !note->reminderWaiting()) {
        return;
    }
    // Recorded first: a reminder that could not be recorded is not shown, so
    // it can never be shown twice.
    note->reminded = nowToTheSecond();
    if (!m_store->save(*note, NoteStore::Touch::Kept)) {
        return;
    }
    Q_EMIT due(id);
}

bool Reminders::remindAgainIn(const QString &id, int minutes)
{
    auto note = m_store->note(id);
    if (!note || note->newerFormat()) {
        return false;
    }
    note->remind = nowToTheSecond().addSecs(qint64(minutes) * 60);
    note->remindOnOpen = false;
    note->reminded = {};
    note->done = {};
    return m_store->save(*note);
}

bool Reminders::setDone(const QString &id, bool done)
{
    return setNoteDone(m_store, id, done);
}

} // namespace Gooseberry
