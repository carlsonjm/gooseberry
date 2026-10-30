// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QStringList>

#include <functional>

class QTextDocument;

namespace Gooseberry {

class SpellingHighlighter;

// A word in a line worth checking: where it starts and how long it is.
struct WordSpan {
    int start = 0;
    int length = 0;
    bool operator==(const WordSpan &other) const = default;
};

// The words in a line that spelling looks at. A word with a digit, a capital
// past its first letter or an underscore is a name or a code, and a word in
// a link, an address, a path or a #folder is left as it is written.
QList<WordSpan> wordsToCheck(const QString &line);

// What it costs, in tenths of an edit, for fingers meaning one word to type
// the other: a stray extra key, two keys swapped or a missed space cost
// least, then a key's neighbour or a missed key, and another letter most.
int slipCost(const QString &typed, const QString &meant);

// What a word the dictionary does not know was most likely meant to be, from
// the dictionary's guesses and from the words one slip of the fingers away
// (a key too many, two swapped, a neighbour's key, a missed space): the
// nearest, then only others as near, three at most.
QStringList closest(const QString &word, const QStringList &guesses,
                    const std::function<bool(const QString &)> &knows, int most = 3);

// Spelling on a page of text, as the desktop's own spell checker sees it: a
// red line under each word it does not know, and for the word the cursor
// stands in, what it might have been. The word still being typed is left
// alone until the cursor leaves it. The language, and the words added to the
// dictionary, are the desktop's, shared with every KDE program.
class Spelling : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // The page's words, as a TextEdit or TextArea gives them (textDocument).
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    // Off, the page shows no red lines and nothing is offered.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    // Whether the desktop has a dictionary for its language.
    Q_PROPERTY(bool available READ available CONSTANT)
    // The word the cursor stands in, when it is one the dictionary does not
    // know; otherwise empty.
    Q_PROPERTY(QString word READ word NOTIFY wordChanged)
    // What that word might have been, the likeliest first, three at most.
    Q_PROPERTY(QStringList suggestions READ suggestions NOTIFY wordChanged)

public:
    explicit Spelling(QObject *parent = nullptr);
    ~Spelling() override;

    QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    int cursorPosition() const { return m_cursor; }
    void setCursorPosition(int position);
    bool active() const { return m_active; }
    void setActive(bool active);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    bool available() const;
    QString word() const { return m_word; }
    QStringList suggestions() const { return m_suggestions; }

    // Puts that in place of the word the cursor stands in, and gives where
    // the new word ends, for the cursor; -1 when there is no such word.
    Q_INVOKABLE int replace(const QString &with);
    // Adds the word the cursor stands in to the desktop's dictionary, so no
    // page marks it again.
    Q_INVOKABLE void learn();

    // Whether the dictionary knows the word.
    static bool knows(const QString &word);
    // The words of a line the dictionary does not know.
    QList<WordSpan> unknownIn(const QString &line, int lineStart) const;

Q_SIGNALS:
    void documentChanged();
    void cursorPositionChanged();
    void activeChanged();
    void colorChanged();
    void wordChanged();

private:
    QTextDocument *textDocument() const;
    void edited(int position, int removed, int added);
    void rehighlight();
    void findWord();

    QPointer<QQuickTextDocument> m_document;
    QPointer<SpellingHighlighter> m_highlighter;
    QMetaObject::Connection m_edits;
    int m_cursor = 0;
    // Where the word still being typed ends, or -1.
    int m_typing = -1;
    // The page's length as the cursor last moved.
    int m_characters = 0;
    bool m_active = true;
    QColor m_color = QColor(0xFF, 0xB5, 0xA8);
    QString m_word;
    int m_wordStart = -1;
    QStringList m_suggestions;
};

} // namespace Gooseberry
