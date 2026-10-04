// SPDX-License-Identifier: GPL-2.0-or-later
#include "Reading.h"

#include "NoteStore.h"

#include <QTimer>

namespace Gooseberry {

Reading::Reading(NoteStore *store, std::unique_ptr<InkReader> reader, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_reader(std::move(reader))
{
    if (!m_reader) {
        return;
    }
    m_thread.setObjectName(QStringLiteral("Gooseberry reading"));
    m_onThread = new QObject;
    m_onThread->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_onThread, &QObject::deleteLater);
    // Reading is never in the way of writing.
    m_thread.start(QThread::LowPriority);
}

Reading::~Reading()
{
    if (m_reader) {
        m_thread.quit();
        m_thread.wait();
    }
}

void Reading::request(const QString &id)
{
    if (!m_reader || m_queue.contains(id)) {
        return;
    }
    const auto note = m_store->note(id);
    if (!note || note->ink.isEmpty()) {
        return;
    }
    m_queue.append(id);
    next();
}

void Reading::requestUnread()
{
    if (!m_reader) {
        return;
    }
    for (const Note &note : m_store->notes()) {
        if (!note.ink.isEmpty() && !note.newerFormat() && !m_store->ink(note.id).unread().isEmpty()) {
            request(note.id);
        }
    }
}

void Reading::next()
{
    if (m_running) {
        return;
    }
    while (!m_queue.isEmpty()) {
        const QString id = m_queue.first();
        const auto note = m_store->note(id);
        const Ink ink = note && !note->ink.isEmpty() && !note->newerFormat() && !m_store->readOnly() ? m_store->ink(id) : Ink();
        const QList<int> unread = ink.unread();
        if (unread.isEmpty()) {
            m_queue.removeFirst();
            continue;
        }
        const int row = unread.first();
        const QString digest = ink.digest(row);
        const QList<InkStroke> line = ink.strokesIn(row);
        m_running = true;
        ++m_restGeneration;
        InkReader *reader = m_reader.get();
        QMetaObject::invokeMethod(
            m_onThread,
            [this, reader, id, row, digest, line] {
                const std::optional<InkReader::Read> read = reader->read(line);
                QMetaObject::invokeMethod(
                    this, [this, id, row, digest, read] { finished(id, row, digest, read); }, Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
        return;
    }
    // Rested a while unused, the reader lets go of its memory.
    const quint64 generation = m_restGeneration;
    QTimer::singleShot(m_restAfterMs, this, [this, generation] {
        if (generation == m_restGeneration && !busy()) {
            InkReader *reader = m_reader.get();
            QMetaObject::invokeMethod(m_onThread, [reader] { reader->rest(); }, Qt::QueuedConnection);
        }
    });
    Q_EMIT idle();
}

void Reading::finished(const QString &id, int row, const QString &digest, const std::optional<InkReader::Read> &read)
{
    m_running = false;
    if (!read) {
        // The reader could not read it: the note waits for another time,
        // rather than being tried over and over.
        m_queue.removeAll(id);
        next();
        return;
    }
    const auto note = m_store->note(id);
    if (note) {
        // Read from the folder again: the card may have kept more strokes
        // meanwhile, and a line that changed is not given an old reading.
        Ink ink = m_store->ink(id);
        if (ink.setReading(row, digest, read->text, read->also)) {
            if (m_store->saveInk(*note, ink, NoteStore::Touch::Kept)) {
                Q_EMIT lineRead(id);
            } else {
                // Not kept: read again another time, not over and over.
                m_queue.removeAll(id);
            }
        }
    }
    next();
}

} // namespace Gooseberry
