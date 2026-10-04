// SPDX-License-Identifier: GPL-2.0-or-later
#include "Capture.h"

#include "NoteStore.h"

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
        // sheet, and the next change keeps it again as a new note.
        m_note.id.clear();
        Q_EMIT noteChanged();
    });
    connect(m_store, &NoteStore::readOnlyChanged, this, &Capture::noteChanged);
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
    // sheet was left.
    if (kept() && !readOnly() && m_note.text.trimmed().isEmpty()) {
        m_store->trash(m_note.id);
        m_note.id.clear();
        Q_EMIT noteChanged();
    }
}

void Capture::reset(const Note &note, bool editing)
{
    leaveEmpty();
    m_note = note;
    m_editing = editing;
    setProblem({});
    Q_EMIT noteChanged();
    Q_EMIT textChanged();
    Q_EMIT colourChanged();
    Q_EMIT belongingChanged();
    Q_EMIT contextChanged();
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

void Capture::keep()
{
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
    keep();
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
    const auto onDisk = m_store->note(id);
    if (!onDisk) {
        return;
    }
    // Saves made here come back as the same note; only a change made by
    // another program is taken onto the sheet.
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
    m_note = *onDisk;
}

} // namespace Gooseberry
