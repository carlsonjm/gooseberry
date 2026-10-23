// SPDX-License-Identifier: GPL-2.0-or-later
#include "Welcome.h"

#include "NoteStore.h"
#include "Product.h"

#include <KLocalizedString>

namespace Gooseberry {

Note welcomeNote()
{
    // Each paragraph is one line of the note, a blank line between them. The
    // day named and the #word are real, so the note offers a reminder and a
    // folder; the quick note leaves the full stop after #ideas out of the
    // folder's name.
    const QStringList paragraphs = {
        i18nc("@info welcome note's first line, its title", "Welcome to %1", productName()),
        i18nc("@info welcome note", "Write anything here. It's kept as you type, with nothing to save."),
        i18nc("@info welcome note",
              "A new note sticks to the window you were using, like a sticky on a page. Tap the dot on that "
              "window's title bar to bring it back."),
        i18nc("@info welcome note; keep the # and the word after it as one word, which becomes a folder's name, "
              "and use the word for the next day that the note listens for",
              "Name a day, like tomorrow, and the note offers to remind you. Put # before a word to keep a note in that "
              "folder: #ideas. Lines that start with a dash become a checklist."),
        i18nc("@info welcome note",
              "Tap the grid below to see all your notes. When you're done with this one, tap More, then Remove."),
    };
    Note note;
    note.text = paragraphs.join(QStringLiteral("\n\n"));
    note.colour = QStringLiteral("butter");
    return note;
}

QString welcomeIfFresh(NoteStore &store)
{
    if (!store.fresh() || store.readOnly()) {
        return {};
    }
    return store.create(welcomeNote());
}

} // namespace Gooseberry
