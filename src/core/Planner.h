// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace Gooseberry {

class NoteStore;

// The planner: notes with a time, day by day. A day strip from two days back
// to three ahead, the chosen day's notes in time order, then the days ahead
// that have any. A note marked done stays on its day, as Done.
class Planner : public QObject
{
    Q_OBJECT
    // Days reach the screen as their noon, in local time. A plain date would
    // reach it as midnight in UTC, which west of Greenwich is still the day
    // before, so every day would be named for the one before it.
    Q_PROPERTY(QDateTime day READ dayShown WRITE showDay NOTIFY changed)
    Q_PROPERTY(QDateTime today READ todayShown NOTIFY changed)
    // The day strip: each a map of date, today, chosen and planned (it has
    // notes and is not the chosen day).
    Q_PROPERTY(QVariantList strip READ strip NOTIFY changed)
    // The chosen day, then each day ahead with notes: each a map of date,
    // today, and rows. A row is a map of id, time, done, title, place, next
    // (the next one still to come today) and colour.
    Q_PROPERTY(QVariantList days READ days NOTIFY changed)

public:
    static constexpr int DaysBack = 2;
    static constexpr int DaysAhead = 3;
    // How far past the chosen day the planner looks for days ahead.
    static constexpr int LooksAhead = 7;

    explicit Planner(NoteStore *store, QObject *parent = nullptr);

    QDate day() const { return m_day; }
    void setDay(const QDate &day);
    QDate today() const { return m_today; }
    QDateTime dayShown() const { return shown(m_day); }
    void showDay(const QDateTime &day) { setDay(day.toLocalTime().date()); }
    QDateTime todayShown() const { return shown(m_today); }
    // A day as the screen is given it: its noon, in local time.
    static QDateTime shown(const QDate &day) { return QDateTime(day, QTime(12, 0)); }
    QVariantList strip() const { return m_strip; }
    QVariantList days() const { return m_days; }

    // Back to today, as each time the board opens.
    Q_INVOKABLE void showToday();
    Q_INVOKABLE bool setDone(const QString &id, bool done);

Q_SIGNALS:
    void changed();

private:
    void rebuild();

    NoteStore *m_store;
    QDate m_today;
    QDate m_day;
    QVariantList m_strip;
    QVariantList m_days;
    QTimer m_settle;
    QTimer m_minute;
};

// Marks a note done, or not; false when nothing changed.
bool setNoteDone(NoteStore *store, const QString &id, bool done);

} // namespace Gooseberry
