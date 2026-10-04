// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDate>
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
    Q_PROPERTY(QDate day READ day WRITE setDay NOTIFY changed)
    Q_PROPERTY(QDate today READ today NOTIFY changed)
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
