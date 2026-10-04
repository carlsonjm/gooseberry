// SPDX-License-Identifier: GPL-2.0-or-later
#include "Note.h"

#include <QRegularExpression>
#include <QStringList>

namespace Gooseberry {

namespace {

struct Colour {
    const char *name;
    const char *hex;
};

// The mock-up's five note colours, in the order the quick-note card offers them.
constexpr Colour Colours[] = {
    {"butter", "#F2D98A"},
    {"rhyolite", "#E8B4A8"},
    {"lake", "#9ED6CB"},
    {"lichen", "#C9D89A"},
    {"stone", "#D9D4CC"},
};

const QStringList KnownKeys = {
    QStringLiteral("gooseberry"), QStringLiteral("created"), QStringLiteral("changed"),
    QStringLiteral("colour"), QStringLiteral("belongs"), QStringLiteral("window"),
    QStringLiteral("app"), QStringLiteral("project"), QStringLiteral("workspace"),
    QStringLiteral("tucked"), QStringLiteral("remind"), QStringLiteral("reminded"), QStringLiteral("done"),
    QStringLiteral("ink"), QStringLiteral("read"), QStringLiteral("read-also"),
};

const QString OnOpen = QStringLiteral("opens");

QString quoted(const QString &value)
{
    QString flat = value;
    flat.replace(QLatin1Char('\r'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' '));
    flat.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + flat + QLatin1Char('"');
}

QString unquoted(const QString &raw)
{
    const QString value = raw.trimmed();
    if (value.size() >= 2 && value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))) {
        return value.mid(1, value.size() - 2).replace(QLatin1String("''"), QLatin1String("'"));
    }
    if (value.size() < 2 || !value.startsWith(QLatin1Char('"')) || !value.endsWith(QLatin1Char('"'))) {
        return value;
    }
    QString out;
    out.reserve(value.size());
    for (qsizetype i = 1; i < value.size() - 1; ++i) {
        const QChar c = value.at(i);
        if (c == QLatin1Char('\\') && i + 1 < value.size() - 1) {
            const QChar next = value.at(++i);
            out += next == QLatin1Char('n') ? QChar(QLatin1Char(' ')) : next;
        } else {
            out += c;
        }
    }
    return out;
}

QString timeText(const QDateTime &time)
{
    return time.toOffsetFromUtc(time.offsetFromUtc()).toString(Qt::ISODate);
}

} // namespace

QString belongsName(Belongs belongs)
{
    switch (belongs) {
    case Belongs::Window:
        return QStringLiteral("window");
    case Belongs::Project:
        return QStringLiteral("project");
    case Belongs::Workspace:
        return QStringLiteral("workspace");
    case Belongs::Loose:
        break;
    }
    return QStringLiteral("loose");
}

Belongs belongsFromName(const QString &name)
{
    if (name == QLatin1String("window")) {
        return Belongs::Window;
    }
    if (name == QLatin1String("project")) {
        return Belongs::Project;
    }
    if (name == QLatin1String("workspace")) {
        return Belongs::Workspace;
    }
    return Belongs::Loose;
}

QString Note::title() const
{
    static const QRegularExpression markers(QStringLiteral(R"(^\s*(#+\s*|[-*+]\s+(\[[ xX]\]\s*)?|\d+[.)]\s+|>\s*)*)"));
    const auto lines = QStringView(text).split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        QString plain = line.toString();
        plain.remove(markers);
        plain = plain.trimmed();
        if (!plain.isEmpty()) {
            return plain;
        }
    }
    // Handwriting only: its first words as they were read.
    return read.simplified();
}

QString Note::placeLabel() const
{
    switch (belongs) {
    case Belongs::Window:
        return window.isEmpty() ? QStringLiteral("Loose") : window;
    case Belongs::Project:
        return project.isEmpty() ? QStringLiteral("Loose") : project;
    case Belongs::Workspace:
        return workspace.isEmpty() ? QStringLiteral("Workspace") : workspace;
    case Belongs::Loose:
        break;
    }
    return QStringLiteral("Loose");
}

QString Note::placeKey() const
{
    switch (belongs) {
    case Belongs::Window:
        if (!window.isEmpty()) {
            return QStringLiteral("window:") + window;
        }
        break;
    case Belongs::Project:
        if (!project.isEmpty()) {
            return QStringLiteral("project:") + project;
        }
        break;
    case Belongs::Workspace:
        return QStringLiteral("workspace:") + workspace;
    case Belongs::Loose:
        break;
    }
    return QStringLiteral("loose");
}

QByteArray Note::serialize() const
{
    QString out;
    out += QLatin1String("---\n");
    out += QStringLiteral("gooseberry: %1\n").arg(format);
    out += QStringLiteral("created: %1\n").arg(timeText(created));
    out += QStringLiteral("changed: %1\n").arg(timeText(changed));
    out += QStringLiteral("colour: %1\n").arg(colour);
    out += QStringLiteral("belongs: %1\n").arg(belongsName(belongs));
    if (!window.isEmpty()) {
        out += QStringLiteral("window: %1\n").arg(quoted(window));
    }
    if (!app.isEmpty()) {
        out += QStringLiteral("app: %1\n").arg(quoted(app));
    }
    if (!project.isEmpty()) {
        out += QStringLiteral("project: %1\n").arg(quoted(project));
    }
    if (!workspace.isEmpty()) {
        out += QStringLiteral("workspace: %1\n").arg(quoted(workspace));
    }
    out += QStringLiteral("tucked: %1\n").arg(tucked ? QStringLiteral("true") : QStringLiteral("false"));
    if (remind.isValid()) {
        out += QStringLiteral("remind: %1\n").arg(timeText(remind));
    } else if (remindOnOpen) {
        out += QStringLiteral("remind: %1\n").arg(OnOpen);
    }
    if (reminded.isValid()) {
        out += QStringLiteral("reminded: %1\n").arg(timeText(reminded));
    }
    if (done.isValid()) {
        out += QStringLiteral("done: %1\n").arg(timeText(done));
    }
    if (!ink.isEmpty()) {
        out += QStringLiteral("ink: %1\n").arg(quoted(ink));
    }
    if (!read.isEmpty()) {
        out += QStringLiteral("read: %1\n").arg(quoted(read));
    }
    if (!readAlso.isEmpty()) {
        out += QStringLiteral("read-also: %1\n").arg(quoted(readAlso));
    }
    for (const auto &[key, value] : extra) {
        out += key + QLatin1String(": ") + value + QLatin1Char('\n');
    }
    out += QLatin1String("---\n");
    out += text;
    return out.toUtf8();
}

Note Note::parse(const QByteArray &bytes, const QString &id, const QDateTime &fallbackTime)
{
    Note note;
    note.id = id;
    note.created = fallbackTime;
    note.changed = fallbackTime;

    QString content = QString::fromUtf8(bytes);
    content.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (content.startsWith(QChar(0xFEFF))) {
        content.remove(0, 1);
    }

    if (!content.startsWith(QLatin1String("---\n"))) {
        note.text = content;
        return note;
    }
    const qsizetype end = content.indexOf(QLatin1String("\n---"), 3);
    const bool closes = end >= 0
        && (end + 4 == content.size() || content.at(end + 4) == QLatin1Char('\n'));
    if (!closes) {
        // An opening rule with no closing one is text, not a header.
        note.text = content;
        return note;
    }

    const QString header = content.mid(4, end - 4 + 1);
    note.text = content.mid(qMin(content.size(), end + 5));

    for (const auto &rawLine : QStringView(header).split(QLatin1Char('\n'))) {
        const QString line = rawLine.toString();
        if (line.trimmed().isEmpty() || line.trimmed().startsWith(QLatin1Char('#'))) {
            continue;
        }
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        const QString raw = line.mid(colon + 1).trimmed();
        const QString value = unquoted(raw);
        if (!KnownKeys.contains(key)) {
            note.extra.append({key, raw});
        } else if (key == QLatin1String("gooseberry")) {
            bool ok = false;
            const int format = value.toInt(&ok);
            note.format = ok ? format : NoteFormat;
        } else if (key == QLatin1String("created")) {
            const auto time = QDateTime::fromString(value, Qt::ISODate);
            if (time.isValid()) {
                note.created = time;
            }
        } else if (key == QLatin1String("changed")) {
            const auto time = QDateTime::fromString(value, Qt::ISODate);
            if (time.isValid()) {
                note.changed = time;
            }
        } else if (key == QLatin1String("colour")) {
            note.colour = value;
        } else if (key == QLatin1String("belongs")) {
            note.belongs = belongsFromName(value);
        } else if (key == QLatin1String("window")) {
            note.window = value;
        } else if (key == QLatin1String("app")) {
            note.app = value;
        } else if (key == QLatin1String("project")) {
            note.project = value;
        } else if (key == QLatin1String("workspace")) {
            note.workspace = value;
        } else if (key == QLatin1String("tucked")) {
            note.tucked = value == QLatin1String("true");
        } else if (key == QLatin1String("remind")) {
            if (value == OnOpen) {
                note.remindOnOpen = true;
            } else {
                const auto time = QDateTime::fromString(value, Qt::ISODate);
                if (time.isValid()) {
                    note.remind = time;
                } else {
                    // A reminder this version cannot read is kept as written.
                    note.extra.append({key, raw});
                }
            }
        } else if (key == QLatin1String("reminded")) {
            note.reminded = QDateTime::fromString(value, Qt::ISODate);
        } else if (key == QLatin1String("done")) {
            note.done = QDateTime::fromString(value, Qt::ISODate);
        } else if (key == QLatin1String("ink")) {
            note.ink = value;
        } else if (key == QLatin1String("read")) {
            note.read = value;
        } else if (key == QLatin1String("read-also")) {
            note.readAlso = value;
        }
    }
    return note;
}

QString documentName(const QString &windowTitle, const QString &appName)
{
    QString title = windowTitle.simplified();
    // KDE and many other programs end a title with " — Application"; the
    // long dash is trimmed whatever the application is called. A plain
    // hyphen is trimmed only before the application's own name, since
    // documents use hyphens too.
    static const QStringList dashes = {QStringLiteral(" — "), QStringLiteral(" – ")};
    bool trimmed = false;
    for (const auto &dash : dashes) {
        const qsizetype at = title.lastIndexOf(dash);
        if (at > 0) {
            title = title.left(at).trimmed();
            trimmed = true;
            break;
        }
    }
    const qsizetype hyphen = title.lastIndexOf(QLatin1String(" - "));
    if (!trimmed && hyphen > 0 && !appName.isEmpty()
        && title.mid(hyphen + 3).trimmed().contains(appName, Qt::CaseInsensitive)) {
        title = title.left(hyphen).trimmed();
    }
    static const QRegularExpression modified(QStringLiteral(R"(^\*\s*|\s*(\*|\[modified\]|\(modified\)|●)$)"));
    title.remove(modified);
    title = title.trimmed();
    return title.isEmpty() ? appName : title;
}

QStringList colourNames()
{
    QStringList names;
    for (const auto &colour : Colours) {
        names.append(QString::fromLatin1(colour.name));
    }
    return names;
}

QString colourHex(const QString &name)
{
    for (const auto &colour : Colours) {
        if (name == QLatin1String(colour.name)) {
            return QString::fromLatin1(colour.hex);
        }
    }
    return QString::fromLatin1(Colours[0].hex);
}

} // namespace Gooseberry
