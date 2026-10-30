// SPDX-License-Identifier: GPL-2.0-or-later
#include "Spelling.h"

#include <Sonnet/Speller>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBoundaryFinder>
#include <QTextCursor>
#include <QTextDocument>

namespace Gooseberry {

namespace {

// One speller for the program, in the desktop's language. Never freed: the
// spell checker's own plugins may be gone by the time a static would be.
Sonnet::Speller &speller()
{
    static auto *shared = new Sonnet::Speller();
    return *shared;
}

// Every page with spelling on, so a word added to the dictionary leaves all
// of them at once.
QList<Spelling *> &pages()
{
    static QList<Spelling *> all;
    return all;
}

bool isApostrophe(QChar c)
{
    return c == QLatin1Char('\'') || c == QChar(0x2019);
}

// The run of text between spaces the word stands in.
QStringView tokenAround(const QString &line, int start, int end)
{
    while (start > 0 && !line.at(start - 1).isSpace()) {
        --start;
    }
    while (end < line.size() && !line.at(end).isSpace()) {
        ++end;
    }
    return QStringView(line).mid(start, end - start);
}

bool isLeftAlone(QStringView token)
{
    return token.startsWith(QLatin1Char('#')) || token.startsWith(QLatin1Char('@'))
        || token.contains(QLatin1String("://")) || token.startsWith(QLatin1String("www."), Qt::CaseInsensitive)
        || token.contains(QLatin1Char('@')) || token.contains(QLatin1Char('/')) || token.contains(QLatin1Char('\\'));
}

bool isNameOrCode(QStringView word)
{
    for (qsizetype i = 0; i < word.size(); ++i) {
        const QChar c = word.at(i);
        if (c.isDigit() || c == QLatin1Char('_') || c == QLatin1Char('.') || (i > 0 && c.isUpper())) {
            return true;
        }
    }
    return false;
}

// What each slip costs, in tenths of an edit. Touch typing slips most by a
// stray extra key, two keys swapped or a missed space, then by a key's
// neighbour or a missed key; another letter altogether is a different word.
constexpr int strayKey = 5;
constexpr int swappedKeys = 4;
constexpr int neighbourKey = 8;
constexpr int missedKey = 9;
constexpr int otherKey = 10;
constexpr int missedSpace = 3;
// A guess further than this is offered only when it is the only one.
constexpr int farOff = 12;

// Where a letter sits on a QWERTY keyboard: its row, and across, each row
// set half a key further right than the one above.
bool keyAt(QChar c, int &row, qreal &across)
{
    static const char *const rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    const char letter = c.toLower().toLatin1();
    for (int r = 0; r < 3; ++r) {
        if (const char *at = letter ? std::strchr(rows[r], letter) : nullptr) {
            row = r;
            across = (at - rows[r]) + 0.5 * r;
            return true;
        }
    }
    return false;
}

bool neighbours(QChar a, QChar b)
{
    int rowA = 0;
    int rowB = 0;
    qreal acrossA = 0;
    qreal acrossB = 0;
    return a.toLower() != b.toLower() && keyAt(a, rowA, acrossA) && keyAt(b, rowB, acrossB)
        && std::abs(rowA - rowB) <= 1 && std::abs(acrossA - acrossB) <= 1.0;
}

bool isGap(QChar c)
{
    return c == QLatin1Char(' ') || c == QLatin1Char('-');
}

} // namespace

int slipCost(const QString &typed, const QString &meant)
{
    const QString a = typed.toLower();
    const QString b = meant.toLower();
    const auto missed = [&b](int j) { return isGap(b[j - 1]) ? missedSpace : missedKey; };
    QList<QList<int>> rows(a.size() + 1, QList<int>(b.size() + 1, 0));
    for (int i = 1; i <= a.size(); ++i) {
        rows[i][0] = rows[i - 1][0] + strayKey;
    }
    for (int j = 1; j <= b.size(); ++j) {
        rows[0][j] = rows[0][j - 1] + missed(j);
    }
    for (int i = 1; i <= a.size(); ++i) {
        for (int j = 1; j <= b.size(); ++j) {
            const int key = a[i - 1] == b[j - 1] ? 0 : neighbours(a[i - 1], b[j - 1]) ? neighbourKey : otherKey;
            rows[i][j] = std::min({rows[i - 1][j] + strayKey, rows[i][j - 1] + missed(j), rows[i - 1][j - 1] + key});
            if (i > 1 && j > 1 && a[i - 1] != a[i - 2] && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                rows[i][j] = std::min(rows[i][j], rows[i - 2][j - 2] + swappedKeys);
            }
        }
    }
    return rows.constLast().constLast();
}

QStringList closest(const QString &word, const QStringList &guesses, const std::function<bool(const QString &)> &knows, int most)
{
    // What one slip of the fingers would have turned a known word into: a
    // key too many, two swapped, a neighbour's key, or a missed space.
    QStringList slips;
    for (qsizetype i = 0; i < word.size(); ++i) {
        slips.append(QString(word).remove(i, 1));
    }
    for (qsizetype i = 0; i + 1 < word.size(); ++i) {
        QString swapped = word;
        std::swap(swapped[i], swapped[i + 1]);
        slips.append(swapped);
    }
    static const QString letters = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    for (qsizetype i = 0; i < word.size(); ++i) {
        for (const QChar letter : letters) {
            if (neighbours(word[i], letter)) {
                slips.append(QString(word).replace(i, 1, word[i].isUpper() ? letter.toUpper() : letter));
            }
        }
    }
    for (qsizetype i = 1; i < word.size(); ++i) {
        slips.append(word.left(i) + QLatin1Char(' ') + word.mid(i));
    }

    // A word of one letter is only "a" or "I". A guess in parts is offered
    // only when each part is a word, and a part of two letters only when it
    // is a common one: the dictionary also knows "ms" and "ed".
    const auto isWord = [&knows](const QString &part) {
        const bool letterWord = part.compare(QLatin1String("a"), Qt::CaseInsensitive) == 0 || part == QLatin1String("I");
        return (part.size() > 1 || letterWord) && knows(part);
    };
    static const QStringList twoLetterWords = QStringLiteral("am an as at be by do go he if in is it me my no of on or so to up us we")
                                                  .split(QLatin1Char(' '));
    const auto isPart = [&isWord](const QString &part) {
        return isWord(part) && (part.size() != 2 || twoLetterWords.contains(part.toLower()));
    };
    QStringList candidates;
    const QStringList all = guesses + slips;
    for (const QString &candidate : all) {
        if (candidate == word || candidates.contains(candidate)) {
            continue;
        }
        const bool parts = candidate.contains(QLatin1Char(' ')) || candidate.contains(QLatin1Char('-'));
        if (parts) {
            const auto pieces = candidate.split(QRegularExpression(QStringLiteral("[ -]")));
            if (!std::all_of(pieces.begin(), pieces.end(), isPart)) {
                continue;
            }
        } else if (!guesses.contains(candidate) && !isWord(candidate)) {
            continue;
        }
        candidates.append(candidate);
    }
    if (candidates.isEmpty()) {
        return {};
    }

    // The nearest first, the dictionary's own order between equals; then
    // only those as near as the nearest.
    QList<std::pair<int, qsizetype>> ranked;
    for (qsizetype i = 0; i < candidates.size(); ++i) {
        ranked.append({slipCost(word, candidates.at(i)), i});
    }
    std::sort(ranked.begin(), ranked.end());
    const int nearest = ranked.first().first;
    QStringList kept;
    for (const auto &[cost, at] : std::as_const(ranked)) {
        if (kept.size() >= most || (!kept.isEmpty() && (cost > nearest || cost > farOff))) {
            break;
        }
        kept.append(candidates.at(at));
    }
    return kept;
}

QList<WordSpan> wordsToCheck(const QString &line)
{
    QList<WordSpan> found;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Word, line);
    int from = 0;
    for (int to = finder.toNextBoundary(); to >= 0; to = finder.toNextBoundary()) {
        int start = from;
        int end = to;
        from = to;
        // Quotes around a word are not part of it.
        while (start < end && isApostrophe(line.at(start))) {
            ++start;
        }
        while (end > start && isApostrophe(line.at(end - 1))) {
            --end;
        }
        const QStringView word = QStringView(line).mid(start, end - start);
        if (word.size() < 2 || !word.at(0).isLetter()) {
            continue;
        }
        if (isNameOrCode(word) || isLeftAlone(tokenAround(line, start, end))) {
            continue;
        }
        found.append({start, int(word.size())});
    }
    return found;
}

// The red line under each word the dictionary does not know.
class SpellingHighlighter : public QSyntaxHighlighter
{
public:
    SpellingHighlighter(Spelling *spelling, QTextDocument *document)
        : QSyntaxHighlighter(document)
        , m_spelling(spelling)
    {
    }

protected:
    void highlightBlock(const QString &text) override
    {
        if (!m_spelling->active()) {
            return;
        }
        QTextCharFormat unknown;
        unknown.setUnderlineColor(m_spelling->color());
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
        // The wavy line, which Qt Quick draws from 6.12.
        unknown.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
#else
        unknown.setUnderlineStyle(QTextCharFormat::SingleUnderline);
#endif
        const auto words = m_spelling->unknownIn(text, currentBlock().position());
        for (const WordSpan &word : words) {
            setFormat(word.start, word.length, unknown);
        }
    }

private:
    Spelling *m_spelling;
};

Spelling::Spelling(QObject *parent)
    : QObject(parent)
{
    pages().append(this);
}

Spelling::~Spelling()
{
    pages().removeAll(this);
    delete m_highlighter;
}

QTextDocument *Spelling::textDocument() const
{
    return m_document ? m_document->textDocument() : nullptr;
}

void Spelling::setDocument(QQuickTextDocument *document)
{
    if (m_document == document) {
        return;
    }
    QObject::disconnect(m_edits);
    delete m_highlighter;
    m_document = document;
    m_typing = -1;
    m_characters = 0;
    if (QTextDocument *text = textDocument()) {
        m_characters = text->characterCount();
        m_edits = connect(text, &QTextDocument::contentsChanged, this, &Spelling::findWord);
        m_highlighter = new SpellingHighlighter(this, text);
    }
    Q_EMIT documentChanged();
    findWord();
}

void Spelling::setCursorPosition(int position)
{
    if (m_cursor == position) {
        return;
    }
    // The cursor carried on by just what was added, after a letter, is a
    // word being typed; anywhere else, the word it was in is finished.
    int typing = -1;
    if (QTextDocument *text = textDocument()) {
        const int added = text->characterCount() - m_characters;
        m_characters = text->characterCount();
        const QChar before = position > 0 ? text->characterAt(position - 1) : QChar();
        if (added > 0 && position - m_cursor == added && (before.isLetter() || isApostrophe(before))) {
            typing = position;
        }
    }
    const int was = m_typing;
    m_typing = typing;
    m_cursor = position;
    if (was != typing && m_highlighter) {
        QTextDocument *text = textDocument();
        if (was > 0) {
            m_highlighter->rehighlightBlock(text->findBlock(was - 1));
        }
        if (typing > 0) {
            m_highlighter->rehighlightBlock(text->findBlock(typing - 1));
        }
    }
    Q_EMIT cursorPositionChanged();
    findWord();
}

void Spelling::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    Q_EMIT activeChanged();
    rehighlight();
    findWord();
}

void Spelling::setColor(const QColor &color)
{
    if (m_color == color) {
        return;
    }
    m_color = color;
    Q_EMIT colorChanged();
    rehighlight();
}

bool Spelling::available() const
{
    return speller().isValid();
}

bool Spelling::knows(const QString &word)
{
    return !speller().isValid() || speller().isCorrect(word);
}

QList<WordSpan> Spelling::unknownIn(const QString &line, int lineStart) const
{
    QList<WordSpan> unknown;
    if (!speller().isValid()) {
        return unknown;
    }
    const auto words = wordsToCheck(line);
    for (const WordSpan &word : words) {
        if (lineStart + word.start + word.length == m_typing) {
            continue;
        }
        if (!knows(line.mid(word.start, word.length))) {
            unknown.append(word);
        }
    }
    return unknown;
}

void Spelling::rehighlight()
{
    if (m_highlighter) {
        m_highlighter->rehighlight();
    }
}

void Spelling::findWord()
{
    QString word;
    int wordStart = -1;
    QTextDocument *text = textDocument();
    if (m_active && text && m_cursor != m_typing && speller().isValid()) {
        const QTextBlock block = text->findBlock(m_cursor);
        const int at = m_cursor - block.position();
        // The cursor at the very end of the page is where a note opens, not
        // a word chosen, so nothing is offered there.
        const bool atEnd = m_cursor >= text->characterCount() - 1;
        const auto unknown = unknownIn(block.text(), block.position());
        for (const WordSpan &span : unknown) {
            const int end = span.start + span.length;
            if (at >= span.start && (at < end || (at == end && !atEnd))) {
                word = block.text().mid(span.start, span.length);
                wordStart = block.position() + span.start;
                break;
            }
        }
    }
    if (word == m_word && wordStart == m_wordStart) {
        return;
    }
    m_word = word;
    m_wordStart = wordStart;
    m_suggestions = word.isEmpty() ? QStringList() : closest(word, speller().suggest(word), [](const QString &part) { return speller().isCorrect(part); });
    Q_EMIT wordChanged();
}

int Spelling::replace(const QString &with)
{
    QTextDocument *text = textDocument();
    if (!text || m_wordStart < 0 || with.isEmpty()) {
        return -1;
    }
    const int start = m_wordStart;
    QTextCursor cursor(text);
    cursor.setPosition(start);
    cursor.setPosition(start + int(m_word.size()), QTextCursor::KeepAnchor);
    cursor.insertText(with);
    m_characters = text->characterCount();
    findWord();
    return start + int(with.size());
}

void Spelling::learn()
{
    if (m_word.isEmpty()) {
        return;
    }
    speller().addToPersonal(m_word);
    const auto all = pages();
    for (Spelling *page : all) {
        page->rehighlight();
        page->findWord();
    }
}

} // namespace Gooseberry
