// SPDX-License-Identifier: GPL-2.0-or-later
#include "QuickNoteService.h"

#include "Board.h"
#include "Capture.h"
#include "NoteStore.h"
#include "ReminderWords.h"
#include "Shell.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusError>

namespace Gooseberry {

namespace {

const QStringList Kinds = {QStringLiteral("window"), QStringLiteral("project"), QStringLiteral("workspace"),
                           QStringLiteral("loose")};

// Marks a call from the bus, so the changes it makes are not told back as
// changes from elsewhere.
struct Calling {
    explicit Calling(int &depth)
        : m_depth(depth)
    {
        ++m_depth;
    }
    ~Calling() { --m_depth; }
    int &m_depth;
};

} // namespace

QuickNoteService::QuickNoteService(Shell *shell, NoteStore *store, QObject *parent)
    : QObject(parent)
    , m_shell(shell)
    , m_store(store)
    , m_capture(new Capture(store, this))
{
    m_changed.setSingleShot(true);
    m_changed.setInterval(0);
    connect(&m_changed, &QTimer::timeout, this, [this] {
        if (m_open) {
            Q_EMIT Changed(state(m_open));
        }
    });
    for (auto signal : {&Capture::textChanged, &Capture::colourChanged, &Capture::belongingChanged,
                        &Capture::noteChanged, &Capture::reminderChanged}) {
        connect(m_capture, signal, this, &QuickNoteService::changedElsewhere);
    }
    connect(m_shell, &Shell::boardShown, this, [this] {
        for (const QString &token : std::as_const(m_boardTokens)) {
            Q_EMIT BoardShown(token);
        }
        m_boardTokens.clear();
    });
}

bool QuickNoteService::publish()
{
    return QDBusConnection::sessionBus().registerObject(QString::fromLatin1(Path), this,
                                                        QDBusConnection::ExportScriptableContents);
}

uint QuickNoteService::ProtocolVersion() const
{
    return Version;
}

QVariantMap QuickNoteService::Start()
{
    Calling calling(m_calling);
    if (!m_open) {
        m_capture->startNew(m_shell->currentContext());
        m_open = true;
    }
    return state(m_open);
}

QVariantMap QuickNoteService::State() const
{
    return state(m_open);
}

QVariantMap QuickNoteService::SetText(const QString &text)
{
    Calling calling(m_calling);
    if (!m_open) {
        sendErrorReply(QDBusError::Failed, QStringLiteral("No quick note is open; call Start first."));
        return {};
    }
    m_capture->setText(text);
    // The search sends its text at each pause, so it is written at once.
    m_capture->flush();
    return state(m_open);
}

QVariantMap QuickNoteService::SetColour(const QString &colour)
{
    Calling calling(m_calling);
    if (!m_open) {
        sendErrorReply(QDBusError::Failed, QStringLiteral("No quick note is open; call Start first."));
        return {};
    }
    if (!colourNames().contains(colour)) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Unknown colour: %1").arg(colour));
        return {};
    }
    m_capture->setColour(colour);
    return state(m_open);
}

QVariantMap QuickNoteService::SetBelongs(const QString &kind, const QString &project)
{
    Calling calling(m_calling);
    if (!m_open) {
        sendErrorReply(QDBusError::Failed, QStringLiteral("No quick note is open; call Start first."));
        return {};
    }
    if (!Kinds.contains(kind)) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Unknown place: %1").arg(kind));
        return {};
    }
    m_capture->setBelongs(kind, project);
    return state(m_open);
}

QVariantMap QuickNoteService::SetReminder(const QString &when)
{
    Calling calling(m_calling);
    if (!m_open) {
        sendErrorReply(QDBusError::Failed, QStringLiteral("No quick note is open; call Start first."));
        return {};
    }
    if (when.isEmpty()) {
        m_capture->clearReminder();
    } else if (when == QLatin1String("opens")) {
        if (m_capture->window().isEmpty()) {
            sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("This note was not written on a window."));
            return {};
        }
        m_capture->setRemindOnOpen();
    } else {
        const QDateTime time = QDateTime::fromString(when, Qt::ISODate);
        if (!time.isValid()) {
            sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Not a time: %1").arg(when));
            return {};
        }
        m_capture->setRemindAt(time);
    }
    return state(m_open);
}

QVariantMap QuickNoteService::SetChecklist(bool on)
{
    Calling calling(m_calling);
    if (!m_open) {
        sendErrorReply(QDBusError::Failed, QStringLiteral("No quick note is open; call Start first."));
        return {};
    }
    if (on != m_capture->checklist()) {
        on ? m_capture->makeChecklist() : m_capture->makePlain();
    }
    return state(m_open);
}

QVariantMap QuickNoteService::SetLineChecked(uint line, bool checked)
{
    Calling calling(m_calling);
    if (!m_open) {
        sendErrorReply(QDBusError::Failed, QStringLiteral("No quick note is open; call Start first."));
        return {};
    }
    const QVariantList lines = m_capture->lines();
    if (line >= uint(lines.size()) || !lines.at(int(line)).toMap().value(QStringLiteral("item")).toBool()) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Line %1 is not an item of the list.").arg(line));
        return {};
    }
    m_capture->setLineChecked(int(line), checked);
    return state(m_open);
}

QVariantMap QuickNoteService::Done()
{
    Calling calling(m_calling);
    if (m_open) {
        m_capture->finish();
        m_open = false;
    }
    return state(m_open);
}

QVariantMap QuickNoteService::TuckAway()
{
    Calling calling(m_calling);
    if (m_open && m_capture->kept()) {
        m_capture->tuckAway();
        m_open = false;
    }
    return state(m_open);
}

QVariantMap QuickNoteService::Remove()
{
    Calling calling(m_calling);
    if (m_open) {
        // To the desktop's trash, never deleted.
        m_capture->remove();
        m_open = false;
    }
    return state(m_open);
}

QVariantMap QuickNoteService::UndoRemove()
{
    Calling calling(m_calling);
    QVariantMap result = state(m_open);
    result.insert(QStringLiteral("restored"), m_capture->undoRemove());
    return result;
}

bool QuickNoteService::OpenBoard(const QString &noteId, const QString &requestToken)
{
    if (!requestToken.isEmpty()) {
        m_boardTokens.append(requestToken);
    }
    m_shell->showBoardOn(noteId);
    return true;
}

QVariantMap QuickNoteService::state(bool open) const
{
    const QString remind = m_capture->remindOnOpen() ? QStringLiteral("opens")
        : m_capture->remindAt().isValid() ? m_capture->remindAt().toOffsetFromUtc(m_capture->remindAt().offsetFromUtc()).toString(Qt::ISODate)
                                          : QString();
    // The same choices, in the same order, as Remind on Gooseberry's card,
    // with each time as ISO 8601 for SetReminder.
    QVariantList remindChoices;
    const QVariantList offered = ReminderWords::choices(!m_capture->window().isEmpty());
    for (const QVariant &choice : offered) {
        QVariantMap map = choice.toMap();
        const QDateTime time = map.value(QStringLiteral("time")).toDateTime();
        map.insert(QStringLiteral("time"), time.isValid() ? time.toOffsetFromUtc(time.offsetFromUtc()).toString(Qt::ISODate) : QString());
        remindChoices.append(map);
    }
    QStringList hexes;
    const QStringList names = colourNames();
    for (const QString &name : names) {
        hexes.append(colourHex(name));
    }
    return {
        {QStringLiteral("open"), open},
        {QStringLiteral("id"), m_capture->noteId()},
        {QStringLiteral("kept"), m_capture->kept()},
        {QStringLiteral("text"), m_capture->text()},
        {QStringLiteral("colour"), m_capture->colour()},
        {QStringLiteral("colours"), names},
        {QStringLiteral("colourHexes"), hexes},
        {QStringLiteral("belongs"), m_capture->belongs()},
        {QStringLiteral("project"), m_capture->project()},
        {QStringLiteral("window"), m_capture->window()},
        {QStringLiteral("workspace"), m_capture->workspace()},
        {QStringLiteral("choices"), choices()},
        {QStringLiteral("readOnly"), m_capture->readOnly()},
        {QStringLiteral("problem"), m_capture->problem()},
        {QStringLiteral("remind"), remind},
        {QStringLiteral("remindLabel"), ReminderWords::label(m_capture->remindAt(), m_capture->remindOnOpen())},
        {QStringLiteral("remindChoices"), remindChoices},
        {QStringLiteral("checklist"), m_capture->checklist()},
        {QStringLiteral("lines"), m_capture->lines()},
    };
}

QVariantList QuickNoteService::choices() const
{
    // The same choices, in the same order, as Belongs to on Gooseberry's card.
    QVariantList list;
    auto add = [&list, this](const QString &kind, const QString &label, const QString &project = {}) {
        const bool chosen = m_capture->belongs() == kind && (kind != QLatin1String("project") || m_capture->project() == project);
        list.append(QVariantMap{{QStringLiteral("kind"), kind},
                                {QStringLiteral("label"), label},
                                {QStringLiteral("project"), project},
                                {QStringLiteral("chosen"), chosen}});
    };
    if (!m_capture->window().isEmpty()) {
        add(QStringLiteral("window"), i18n("This window · %1", m_capture->window()));
    }
    const QStringList projects = m_shell->places()->projects();
    const QString shown = m_capture->belongs() == QLatin1String("project") ? m_capture->project() : projects.value(0);
    if (!shown.isEmpty()) {
        add(QStringLiteral("project"), i18n("Project · %1", shown), shown);
    }
    add(QStringLiteral("workspace"),
        m_capture->workspace().isEmpty() ? i18n("Workspace") : i18n("Workspace · %1", m_capture->workspace()));
    add(QStringLiteral("loose"), i18n("Loose"));
    for (const QString &project : projects) {
        if (project != shown) {
            add(QStringLiteral("project"), project, project);
        }
    }
    return list;
}

void QuickNoteService::changedElsewhere()
{
    if (m_calling > 0 || !m_open) {
        return;
    }
    m_changed.start();
}

} // namespace Gooseberry
