// SPDX-License-Identifier: GPL-2.0-or-later
#include "Capture.h"

#include "Checklist.h"
#include "NoteStore.h"

#include <QCoreApplication>

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
    connect(m_store, &NoteStore::foldersChanged, this, &Capture::foldersChanged);
    connect(this, &Capture::belongingChanged, this, &Capture::foldersChanged);
    connect(this, &Capture::contextChanged, this, &Capture::foldersChanged);

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

QVariantList Capture::folderChoices() const
{
    QVariantList list;
    const QString own = m_store->workspaceFolder(m_note.workspace);
    auto add = [&list, &own, this](const QString &name) {
        list.append(QVariantMap{{QStringLiteral("name"), name},
                                {QStringLiteral("label"), name.isEmpty() ? inboxLabel() : name},
                                {QStringLiteral("chosen"), name == m_note.folder},
                                {QStringLiteral("workspace"), !own.isEmpty() && name == own}});
    };
    if (!own.isEmpty()) {
        add(own);
    }
    add({});
    const QStringList folders = m_store->folders();
    for (const QString &folder : folders) {
        if (folder != own) {
            add(folder);
        }
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
    if (kept() && !readOnly() && m_note.text.trimmed().isEmpty()) {
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
    note.stuck = !context.window.isEmpty();
    note.folder = m_store->workspaceFolder(context.workspace);
    reset(note, false);
}

void Capture::startNewIn(const QString &placeKey)
{
    Note note;
    note.workspace = m_context.workspace;
    note.folder = m_store->workspaceFolder(m_context.workspace);
    const qsizetype colon = placeKey.indexOf(QLatin1Char(':'));
    const QString kind = placeKey.left(colon);
    const QString name = colon < 0 ? QString() : placeKey.mid(colon + 1);
    if (kind == QLatin1String("folder") && m_store->hasFolder(name)) {
        note.folder = name;
    } else if (placeKey == QLatin1String("inbox")) {
        note.folder.clear();
    } else if (kind == QLatin1String("window") && !name.isEmpty()) {
        // Stuck to the same window as the notes already on it.
        note.stuck = true;
        note.window = name;
        for (const Note &other : m_store->notes()) {
            if (other.isStuck() && other.window == name && !other.app.isEmpty()) {
                note.app = other.app;
                break;
            }
        }
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
        if (m_note.text.trimmed().isEmpty()) {
            // Nothing written yet: a colour or a place chosen first waits for
            // the first letter.
            return;
        }
        const QString id = m_store->create(m_note);
        if (id.isEmpty()) {
            setProblem(m_store->lastError());
            return;
        }
        m_note = *m_store->note(id);
        setProblem({});
        Q_EMIT noteChanged();
        return;
    }
    if (!m_store->save(m_note)) {
        setProblem(m_store->lastError());
        return;
    }
    m_note.changed = m_store->note(m_note.id)->changed;
    setProblem({});
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
    // Later letters wait for the writing to pause, so a note is written once
    // per pause rather than once per letter, and never waits longer than the
    // longest wait while typing goes on.
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

void Capture::setFolder(const QString &folder)
{
    if (readOnly() || folder == m_note.folder || (!folder.isEmpty() && !m_store->hasFolder(folder))) {
        return;
    }
    if (kept()) {
        // Typing still waiting is written where the note is now, then the
        // note moves.
        flush();
        if (!m_store->moveNote(m_note.id, folder)) {
            setProblem(m_store->lastError());
            return;
        }
        m_note.folder = folder;
        m_note.changed = m_store->note(m_note.id)->changed;
        setProblem({});
    } else {
        m_note.folder = folder;
    }
    Q_EMIT belongingChanged();
}

QString Capture::makeFolder(const QString &name)
{
    if (readOnly()) {
        return {};
    }
    const QString simple = name.simplified();
    if (!m_store->hasFolder(simple) && !m_store->makeFolder(simple)) {
        return m_store->lastError();
    }
    setFolder(simple);
    return {};
}

void Capture::setStuck(const QString &window, const QString &app)
{
    if (readOnly()) {
        return;
    }
    if (window.isEmpty()) {
        if (!m_note.stuck) {
            return;
        }
        // Unstuck, it still remembers the window, for Next time this opens.
        m_note.stuck = false;
    } else {
        if (m_note.isStuck() && window == m_note.window && app == m_note.app) {
            return;
        }
        const bool moved = window != m_note.window || app != m_note.app;
        m_note.stuck = true;
        m_note.window = window;
        m_note.app = app;
        if (moved) {
            Q_EMIT contextChanged();
        }
    }
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
            if (onDisk->folder != m_note.folder) {
                m_note.folder = onDisk->folder;
                Q_EMIT belongingChanged();
            }
            if (onDisk->remind != m_note.remind || onDisk->remindOnOpen != m_note.remindOnOpen) {
                m_note.remind = onDisk->remind;
                m_note.remindOnOpen = onDisk->remindOnOpen;
                Q_EMIT reminderChanged();
            }
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
    const bool belonging = onDisk->folder != m_note.folder || onDisk->stuck != m_note.stuck;
    const bool context = onDisk->window != m_note.window || onDisk->app != m_note.app;
    const bool reminder = onDisk->remind != m_note.remind || onDisk->remindOnOpen != m_note.remindOnOpen;
    m_note = *onDisk;
    if (belonging) {
        Q_EMIT belongingChanged();
    }
    if (context) {
        Q_EMIT contextChanged();
    }
    if (reminder) {
        Q_EMIT reminderChanged();
    }
}

} // namespace Gooseberry
