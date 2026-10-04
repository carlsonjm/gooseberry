// SPDX-License-Identifier: GPL-2.0-or-later
#include "Capture.h"

#include "Checklist.h"
#include "NoteStore.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace Gooseberry {

Capture::Capture(NoteStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    connect(m_store, &NoteStore::noteChanged, this, &Capture::storeChanged);
    connect(m_store, &NoteStore::noteRemoved, this, [this](const QString &id) {
        if (id != m_note.id) {
            return;
        }
        // Removed by another program while open here. The text stays on the
        // card, and the next change keeps it again as a new note.
        m_note.id.clear();
        Q_EMIT noteChanged();
    });
    connect(m_store, &NoteStore::readOnlyChanged, this, &Capture::noteChanged);

    m_pause.setSingleShot(true);
    m_pause.setInterval(PauseMs);
    connect(&m_pause, &QTimer::timeout, this, &Capture::flush);
    // However the program is asked to end, typing waiting for a pause is
    // written first.
    if (auto *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &Capture::flush);
    }
}

Capture::~Capture()
{
    flush();
}

void Capture::setWaits(int pauseMs, int longestWaitMs)
{
    m_pause.setInterval(pauseMs);
    m_longestWaitMs = longestWaitMs;
}

QVariantList Capture::colours() const
{
    QVariantList list;
    for (const QString &name : colourNames()) {
        list.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("hex"), colourHex(name)}});
    }
    return list;
}

bool Capture::readOnly() const
{
    return m_store->readOnly() || m_note.newerFormat();
}

void Capture::leaveEmpty()
{
    // A note left with no text is not kept: it goes to the trash, however the
    // card was left.
    if (kept() && !readOnly() && m_note.text.trimmed().isEmpty() && m_ink.isEmpty()) {
        stopWaiting();
        m_store->trash(m_note.id);
        m_note.id.clear();
        Q_EMIT noteChanged();
    }
}

void Capture::reset(const Note &note, bool editing)
{
    flush();
    leaveEmpty();
    m_note = note;
    m_editing = editing;
    m_ink = note.ink.isEmpty() || note.id.isEmpty() ? Ink() : m_store->ink(note.id);
    m_inkDirty = false;
    m_stroking = false;
    m_erasing.clear();
    m_erased.clear();
    ++m_inkVersion;
    Q_EMIT inkChanged();
    setProblem({});
    Q_EMIT noteChanged();
    Q_EMIT textChanged();
    Q_EMIT colourChanged();
    Q_EMIT belongingChanged();
    Q_EMIT contextChanged();
    Q_EMIT reminderChanged();
}

void Capture::startNew(const CaptureContext &context)
{
    m_context = context;
    Note note;
    note.window = context.window;
    note.app = context.app;
    note.workspace = context.workspace;
    note.belongs = context.window.isEmpty() ? Belongs::Loose : Belongs::Window;
    reset(note, false);
}

void Capture::startNewIn(const QString &placeKey)
{
    Note note;
    note.workspace = m_context.workspace;
    const qsizetype colon = placeKey.indexOf(QLatin1Char(':'));
    const QString kind = placeKey.left(colon);
    const QString name = colon < 0 ? QString() : placeKey.mid(colon + 1);
    if (kind == QLatin1String("window") && !name.isEmpty()) {
        note.belongs = Belongs::Window;
        note.window = name;
    } else if (kind == QLatin1String("project") && !name.isEmpty()) {
        note.belongs = Belongs::Project;
        note.project = name;
    } else if (kind == QLatin1String("workspace")) {
        note.belongs = Belongs::Workspace;
        note.workspace = name;
    }
    reset(note, false);
}

bool Capture::open(const QString &id)
{
    const auto found = m_store->note(id);
    if (!found) {
        return false;
    }
    reset(*found, true);
    return true;
}

void Capture::stopWaiting()
{
    m_pause.stop();
    m_waiting = false;
    m_waitingSince.invalidate();
}

void Capture::flush()
{
    if (m_waiting) {
        keep();
    }
}

void Capture::keep()
{
    // Whatever is written now includes any typing that was waiting.
    stopWaiting();
    if (readOnly()) {
        return;
    }
    if (m_note.id.isEmpty()) {
        if (m_note.text.trimmed().isEmpty() && m_ink.isEmpty()) {
            // Nothing written yet: a colour or a place chosen first waits for
            // the first letter or stroke.
            return;
        }
        const QString id = m_store->create(m_note);
        if (id.isEmpty()) {
            setProblem(m_store->lastError());
            return;
        }
        m_note = *m_store->note(id);
        Q_EMIT noteChanged();
        if (m_ink.isEmpty()) {
            setProblem({});
            return;
        }
    }
    const bool saved = m_inkDirty ? m_store->saveInk(m_note, m_ink) : m_store->save(m_note);
    if (!saved) {
        setProblem(m_store->lastError());
        return;
    }
    m_inkDirty = false;
    const auto kept = m_store->note(m_note.id);
    m_note.changed = kept->changed;
    m_note.ink = kept->ink;
    m_note.read = kept->read;
    m_note.readAlso = kept->readAlso;
    setProblem({});
}

void Capture::waitToKeep()
{
    // Later changes wait for the writing to pause, so a note is written once
    // per pause rather than once per letter or stroke, and never waits longer
    // than the longest wait while writing goes on.
    if (!m_waiting) {
        m_waiting = true;
        m_waitingSince.start();
    }
    if (m_waitingSince.elapsed() >= m_longestWaitMs) {
        keep();
        return;
    }
    m_pause.start();
}

void Capture::inkTouched()
{
    m_inkDirty = true;
    ++m_inkVersion;
    Q_EMIT inkChanged();
}

namespace {

InkPoint inkPoint(qreal x, qreal y, qreal pressure)
{
    return {std::clamp(x, 0.0, Ink::PageWidth), std::max(0.0, y), std::clamp(pressure, 0.0, 1.0)};
}

} // namespace

void Capture::beginStroke(qreal x, qreal y, qreal pressure, const QString &ink)
{
    if (readOnly()) {
        return;
    }
    // While the pen is down nothing is written, so no half stroke is kept.
    m_pause.stop();
    InkStroke stroke;
    stroke.colour = inkNames().contains(ink) ? ink : inkNames().first();
    stroke.points.append(inkPoint(x, y, pressure));
    m_ink.strokes.append(stroke);
    m_stroking = true;
    inkTouched();
}

void Capture::extendStroke(qreal x, qreal y, qreal pressure)
{
    if (!m_stroking || m_ink.strokes.isEmpty()) {
        return;
    }
    auto &points = m_ink.strokes.last().points;
    const InkPoint point = inkPoint(x, y, pressure);
    // Points closer than the drawing keeps them add nothing to the line.
    if (std::hypot(point.x - points.last().x, point.y - points.last().y) < 0.8) {
        return;
    }
    points.append(point);
    inkTouched();
}

void Capture::endStroke()
{
    if (!m_stroking) {
        return;
    }
    m_stroking = false;
    m_erased.clear();
    inkTouched();
    if (!kept()) {
        // The first stroke makes the note, at once.
        keep();
        return;
    }
    waitToKeep();
}

void Capture::eraseAt(qreal x, qreal y)
{
    if (readOnly()) {
        return;
    }
    const QList<InkStroke> taken = m_ink.eraseAt({x, y}, 6);
    if (taken.isEmpty()) {
        return;
    }
    m_erasing += taken;
    m_ink.dropEmptyReadings();
    inkTouched();
}

int Capture::endErase()
{
    const int count = int(m_erasing.size());
    if (count > 0) {
        m_erased = std::exchange(m_erasing, {});
        inkTouched();
        keep();
    }
    return count;
}

void Capture::undoErase()
{
    if (m_erased.isEmpty()) {
        return;
    }
    m_ink.restore(std::exchange(m_erased, {}));
    inkTouched();
    keep();
}

void Capture::fixReading(const QString &text)
{
    if (readOnly() || m_ink.isEmpty()) {
        return;
    }
    m_ink.fix(text);
    inkTouched();
    keep();
}

void Capture::setText(const QString &text)
{
    if (text == m_note.text || readOnly()) {
        return;
    }
    m_note.text = text;
    Q_EMIT textChanged();
    if (!kept()) {
        // The first letter makes the note, at once.
        keep();
        return;
    }
    waitToKeep();
}

bool Capture::checklist() const
{
    return Checklist::contains(m_note.text);
}

QVariantList Capture::lines() const
{
    QVariantList list;
    for (const ChecklistLine &line : Checklist::lines(m_note.text)) {
        list.append(QVariantMap{{QStringLiteral("text"), line.text},
                                {QStringLiteral("item"), line.item},
                                {QStringLiteral("checked"), line.checked}});
    }
    return list;
}

void Capture::setTextNow(const QString &text)
{
    setText(text);
    flush();
}

void Capture::makeChecklist()
{
    setTextNow(Checklist::from(m_note.text));
}

void Capture::makePlain()
{
    setTextNow(Checklist::toPlain(m_note.text));
}

void Capture::setLineText(int line, const QString &text)
{
    setText(Checklist::withLineText(m_note.text, line, text));
}

void Capture::setLineChecked(int line, bool checked)
{
    setTextNow(Checklist::withChecked(m_note.text, line, checked));
}

void Capture::addItemAfter(int line)
{
    setTextNow(Checklist::withItemAfter(m_note.text, line));
}

void Capture::removeLine(int line)
{
    setTextNow(Checklist::withoutLine(m_note.text, line));
}

void Capture::setReminder(const QDateTime &time, bool onOpen)
{
    if (readOnly() || (onOpen && m_note.window.isEmpty())) {
        return;
    }
    QDateTime when = time;
    if (when.isValid()) {
        when.setTime(QTime(when.time().hour(), when.time().minute(), when.time().second()));
    }
    m_note.remind = when;
    m_note.remindOnOpen = onOpen;
    m_note.reminded = {};
    m_note.done = {};
    Q_EMIT reminderChanged();
    keep();
}

void Capture::setRemindAt(const QDateTime &time)
{
    if (time.isValid()) {
        setReminder(time, false);
    }
}

void Capture::setRemindOnOpen()
{
    setReminder({}, true);
}

void Capture::clearReminder()
{
    if (m_note.hasReminder()) {
        setReminder({}, false);
    }
}

void Capture::setColour(const QString &colour)
{
    if (colour == m_note.colour || readOnly()) {
        return;
    }
    m_note.colour = colour;
    Q_EMIT colourChanged();
    keep();
}

void Capture::setBelongs(const QString &kind, const QString &project)
{
    if (readOnly()) {
        return;
    }
    const Belongs belongs = belongsFromName(kind);
    if (belongs == Belongs::Project) {
        const QString name = project.simplified();
        if (name.isEmpty()) {
            return;
        }
        m_note.project = name;
    } else if (belongs == Belongs::Window && m_note.window.isEmpty()) {
        return;
    }
    m_note.belongs = belongs;
    Q_EMIT belongingChanged();
    keep();
}

void Capture::finish()
{
    flush();
    leaveEmpty();
    Q_EMIT finished();
}

void Capture::tuckAway()
{
    if (!kept() || readOnly()) {
        return;
    }
    m_note.tucked = true;
    keep();
    Q_EMIT finished();
}

void Capture::remove()
{
    if (!kept() || readOnly()) {
        Q_EMIT finished();
        return;
    }
    stopWaiting();
    const QString id = m_note.id;
    const auto inTrash = m_store->trash(id);
    if (!inTrash) {
        setProblem(m_store->lastError());
        return;
    }
    m_removedId = id;
    m_removedPath = *inTrash;
    m_note.id.clear();
    Q_EMIT noteChanged();
    Q_EMIT removed();
    Q_EMIT finished();
}

bool Capture::undoRemove()
{
    if (m_removedId.isEmpty()) {
        return false;
    }
    const bool back = m_store->restore(m_removedId, m_removedPath);
    m_removedId.clear();
    m_removedPath.clear();
    return back;
}

void Capture::setProblem(const QString &problem)
{
    if (problem != m_problem) {
        m_problem = problem;
        Q_EMIT problemChanged();
    }
}

void Capture::takeReadings(const Note &onDisk)
{
    if (onDisk.ink.isEmpty() || (onDisk.read == m_note.read && onDisk.readAlso == m_note.readAlso)) {
        return;
    }
    // The handwriting was read while strokes here waited to be written: the
    // readings of lines that have not changed since are kept with them.
    const Ink read = m_store->ink(onDisk.id);
    for (const InkReading &reading : read.readings) {
        if (reading.fixed) {
            continue;
        }
        m_ink.setReading(reading.row, reading.digest, reading.text, reading.also);
    }
    m_note.read = onDisk.read;
    m_note.readAlso = onDisk.readAlso;
    m_inkDirty = true;
    ++m_inkVersion;
    Q_EMIT inkChanged();
}

void Capture::storeChanged(const QString &id)
{
    if (id != m_note.id) {
        return;
    }
    if (m_waiting) {
        // Another program changed the note while typing here waited for a
        // pause: what is being typed wins, and is written now. What
        // Gooseberry recorded about it meanwhile, a reminder shown or the
        // note marked done, is kept.
        if (const auto onDisk = m_store->note(id)) {
            m_note.reminded = onDisk->reminded;
            m_note.done = onDisk->done;
            if (onDisk->remind != m_note.remind || onDisk->remindOnOpen != m_note.remindOnOpen) {
                m_note.remind = onDisk->remind;
                m_note.remindOnOpen = onDisk->remindOnOpen;
                Q_EMIT reminderChanged();
            }
            takeReadings(*onDisk);
        }
        keep();
        return;
    }
    const auto onDisk = m_store->note(id);
    if (!onDisk) {
        return;
    }
    // Saves made here come back as the same note; only a change made by
    // another program is taken onto the card.
    if (onDisk->text != m_note.text) {
        m_note.text = onDisk->text;
        Q_EMIT textChanged();
    }
    if (onDisk->colour != m_note.colour) {
        m_note.colour = onDisk->colour;
        Q_EMIT colourChanged();
    }
    if (onDisk->belongs != m_note.belongs || onDisk->project != m_note.project) {
        m_note.belongs = onDisk->belongs;
        m_note.project = onDisk->project;
        Q_EMIT belongingChanged();
    }
    const bool reminder = onDisk->remind != m_note.remind || onDisk->remindOnOpen != m_note.remindOnOpen;
    if (!m_stroking && (onDisk->ink != m_note.ink || onDisk->read != m_note.read || onDisk->readAlso != m_note.readAlso)) {
        // The handwriting was read, or changed by another program.
        m_ink = onDisk->ink.isEmpty() ? Ink() : m_store->ink(id);
        ++m_inkVersion;
        Q_EMIT inkChanged();
    }
    m_note = *onDisk;
    if (reminder) {
        Q_EMIT reminderChanged();
    }
}

} // namespace Gooseberry
