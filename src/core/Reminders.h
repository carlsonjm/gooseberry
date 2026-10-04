// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>

namespace Gooseberry {

class NoteStore;

// The times Remind offers first, as from `now`: "later" (two hours on, to the
// hour, while that is before the evening), "evening" (7 PM, until 6 PM) and
// "tomorrow" (9 AM tomorrow).
struct QuickTime {
    QString kind;
    QDateTime time;
};
QList<QuickTime> quickTimes(const QDateTime &now);

// When each note's reminder is due, and the record that it was shown. A
// reminder is shown once: before `due` is told, the time it was shown is
// written into the note, so neither a restart nor another program reading the
// folder shows it again. How it is shown is the desktop's part.
class Reminders : public QObject
{
    Q_OBJECT

public:
    explicit Reminders(NoteStore *store, QObject *parent = nullptr);

    // Whether a reminder can reach the person now. While it cannot, a due
    // reminder waits rather than being marked shown and lost.
    void setReady(bool ready);
    bool ready() const { return m_ready; }

    // Shows every reminder whose time has come. Called by itself at each due
    // time, and by the desktop after the computer wakes.
    void check();
    // A window showing this document has opened: its "next time this opens"
    // reminders are due.
    void documentOpened(const QString &window, const QString &app);

    // In 10 minutes, from the reminder itself: a new reminder at that time.
    bool remindAgainIn(const QString &id, int minutes);
    // Done on the planner, or from the reminder.
    bool setDone(const QString &id, bool done);

    // The next time a reminder is due; invalid when none is waiting.
    QDateTime nextDue() const;

Q_SIGNALS:
    // The reminder is due and recorded as shown.
    void due(const QString &id);

private:
    void schedule();
    void fire(const QString &id);

    NoteStore *m_store;
    bool m_ready = true;
    bool m_checking = false;
    QTimer m_timer;
    QTimer m_settle;
};

} // namespace Gooseberry
