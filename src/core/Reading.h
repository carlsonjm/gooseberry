// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Ink.h"

#include <QObject>
#include <QStringList>
#include <QThread>

#include <memory>
#include <optional>

namespace Gooseberry {

class NoteStore;

// What reads a line of handwriting. It runs on a thread of its own, one line
// at a time; nothing it is given leaves the computer.
class InkReader
{
public:
    struct Read {
        QString text;
        // The runner-up words, so a word first read wrong can still be found.
        QStringList also;
    };

    virtual ~InkReader() = default;
    // Reads one ruled line of strokes; nothing when it cannot.
    virtual std::optional<Read> read(const QList<InkStroke> &line) = 0;
    // Lets go of what reading holds in memory, after a while unused.
    virtual void rest() { }
};

// Reads the handwriting of the notes it is asked to, line by line, out of
// sight, and keeps what it read in each note. Only lines that changed since
// they were read, or were never read, are read; a line fixed by hand never is.
// A reading of a line that changed while it was read is let go, and the line
// is read again.
class Reading : public QObject
{
    Q_OBJECT

public:
    // Without a reader, nothing is read and ink is kept as it is.
    Reading(NoteStore *store, std::unique_ptr<InkReader> reader, QObject *parent = nullptr);
    ~Reading() override;

    bool available() const { return m_reader != nullptr; }
    bool busy() const { return m_running || !m_queue.isEmpty(); }

    // Reads the note's unread lines.
    void request(const QString &id);
    // Every note with unread lines, as after a start.
    void requestUnread();

    // How long the reader may rest unused before it lets go of its memory.
    void setRestAfter(int ms) { m_restAfterMs = ms; }

Q_SIGNALS:
    // A line of this note was read and kept.
    void lineRead(const QString &id);
    // Nothing left to read.
    void idle();

private:
    void next();
    void finished(const QString &id, int row, const QString &digest, const std::optional<InkReader::Read> &read);

    NoteStore *m_store;
    std::unique_ptr<InkReader> m_reader;
    QThread m_thread;
    QObject *m_onThread = nullptr;
    QStringList m_queue;
    bool m_running = false;
    int m_restAfterMs = 60 * 1000;
    quint64 m_restGeneration = 0;
};

} // namespace Gooseberry
