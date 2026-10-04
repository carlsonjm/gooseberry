// SPDX-License-Identifier: GPL-2.0-or-later
#include "Checklist.h"

#include <QRegularExpression>
#include <QStringList>

namespace Gooseberry::Checklist {

namespace {

// The marker, kept apart from the words: indent, bullet, box, and its space.
const QRegularExpression &itemPattern()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^(\s*[-*+]\s+\[([ xX])\])(\s?)(.*)$)"));
    return pattern;
}

QStringList split(const QString &text)
{
    return text.split(QLatin1Char('\n'));
}

} // namespace

QList<ChecklistLine> lines(const QString &text)
{
    QList<ChecklistLine> out;
    const QStringList raw = split(text);
    for (const QString &line : raw) {
        const auto match = itemPattern().match(line);
        if (match.hasMatch()) {
            out.append({match.captured(4), true, match.captured(2) != QLatin1String(" ")});
        } else {
            out.append({line, false, false});
        }
    }
    return out;
}

bool contains(const QString &text)
{
    const QStringList raw = split(text);
    for (const QString &line : raw) {
        if (itemPattern().match(line).hasMatch()) {
            return true;
        }
    }
    return false;
}

QString from(const QString &text)
{
    if (contains(text)) {
        return text;
    }
    QStringList raw = split(text);
    while (!raw.isEmpty() && raw.last().trimmed().isEmpty()) {
        raw.removeLast();
    }
    if (raw.isEmpty()) {
        return QStringLiteral("- [ ] ");
    }
    int withWords = 0;
    for (const QString &line : std::as_const(raw)) {
        withWords += line.trimmed().isEmpty() ? 0 : 1;
    }
    bool headingKept = withWords < 2;
    for (QString &line : raw) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        if (!headingKept) {
            headingKept = true;
            continue;
        }
        line = QStringLiteral("- [ ] ") + line.trimmed();
    }
    return raw.join(QLatin1Char('\n'));
}

QString toPlain(const QString &text)
{
    QStringList raw = split(text);
    for (QString &line : raw) {
        const auto match = itemPattern().match(line);
        if (match.hasMatch()) {
            line = match.captured(4);
        }
    }
    return raw.join(QLatin1Char('\n'));
}

QString withLineText(const QString &text, int line, const QString &lineText)
{
    QStringList raw = split(text);
    if (line < 0 || line >= raw.size()) {
        return text;
    }
    QString flat = lineText;
    flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
    const auto match = itemPattern().match(raw.at(line));
    raw[line] = match.hasMatch() ? match.captured(1) + QLatin1Char(' ') + flat : flat;
    return raw.join(QLatin1Char('\n'));
}

QString withChecked(const QString &text, int line, bool checked)
{
    QStringList raw = split(text);
    if (line < 0 || line >= raw.size()) {
        return text;
    }
    const auto match = itemPattern().match(raw.at(line));
    if (!match.hasMatch()) {
        return text;
    }
    QString marker = match.captured(1);
    marker[marker.size() - 2] = checked ? QLatin1Char('x') : QLatin1Char(' ');
    raw[line] = marker + match.captured(3) + match.captured(4);
    return raw.join(QLatin1Char('\n'));
}

QString withItemAfter(const QString &text, int line)
{
    QStringList raw = split(text);
    QString bullet = QStringLiteral("- [ ] ");
    if (line >= 0 && line < raw.size()) {
        const auto match = itemPattern().match(raw.at(line));
        if (match.hasMatch()) {
            QString marker = match.captured(1);
            marker[marker.size() - 2] = QLatin1Char(' ');
            bullet = marker + QLatin1Char(' ');
        }
    }
    raw.insert(qBound(0, line + 1, int(raw.size())), bullet);
    return raw.join(QLatin1Char('\n'));
}

QString withoutLine(const QString &text, int line)
{
    QStringList raw = split(text);
    if (line < 0 || line >= raw.size() || raw.size() == 1) {
        return text;
    }
    raw.removeAt(line);
    return raw.join(QLatin1Char('\n'));
}

QString heading(const QString &text)
{
    for (const ChecklistLine &line : lines(text)) {
        if (!line.item && !line.text.trimmed().isEmpty()) {
            return line.text.trimmed();
        }
        if (line.item) {
            break;
        }
    }
    return {};
}

QString summary(const QString &text)
{
    QStringList items;
    for (const ChecklistLine &line : lines(text)) {
        if (line.item && !line.text.trimmed().isEmpty()) {
            items.append(line.text.trimmed());
        }
    }
    return items.join(QStringLiteral(" · "));
}

} // namespace Gooseberry::Checklist
