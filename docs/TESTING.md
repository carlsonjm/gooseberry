# Testing

What the automated checks prove, and what only a person on the tablet can.

## Automated

`./verify.sh` runs them all: the public text check, the build, then every test.
Each test makes a home of its own in a temporary folder, with its own notes
folder, settings and trash, and no connection to the desktop session
(`tests/TestHome.h`). No test reads or changes a person's notes, settings or
session.

| Test | What it proves |
| --- | --- |
| `tst_note` | The header is written and read exactly as `FORMAT.md` says; keys from a newer version survive an edit; a file with no header is a Loose note; a newer note is not changed; the first words are the title; a window's title gives the document's name |
| `tst_store` | A note is on disk from its first letter, typing after it once the writing pauses and on the way through long typing, and at once on Done or any other change; nothing is kept for an empty sheet; Belongs to and colour are one change each; removing sends the note and its ink to the trash with the record the trash needs, and undo brings them back; tuck away and bring back; the board's places, counts, order and search; changes made by other programs are read; a missing folder is made again; a folder or note from a newer version is left alone |
| `tst_keep` | A second copy of the test types a note and is ended as a crash ends a program, after one letter, mid-sentence and after a pause, and as a logout ends it mid-sentence; after a pause or a logout the note is all there, a crash mid-sentence keeps it up to the last pause, nothing half-written is left beside it, and it is back on the board after a restart |
| `tst_screens` | The capture sheet and the board, drawn off screen and used by tap and keyboard: the cursor is in the note when the sheet opens; typing keeps the note; colour and Belongs to are one tap; a new project is named once; Done; Tuck away and Remove show only for a kept note; the board opens on Today, opens a note, tucks one away and brings it back, and starts a new note in the place shown; every target is at least 44 pixels |

With `GOOSEBERRY_SCREENSHOTS` set to a folder, `tst_screens` saves a picture of
each screen there, to compare with the mock-up by eye.

## By hand: Milestone 0

Milestone 0's proof is a week of daily use on the tablet without a lost note,
with capture fast enough that nothing is written elsewhere instead
(`ROADMAP.md`). These checks come first, once, after installing; then the week
of use.

1. Over a document, hold Gooseberry and tap New note. The sheet rises with the
   cursor in the note and the on-screen keys up; "This window" names the
   document. Tap Gooseberry itself: the board opens as an ordinary window.
2. Type one letter and nothing else, then tap the work behind the sheet. Open
   the board: the note is there under Today.
3. Write a note, choose a colour and a project, and finish it. Each choice is
   one tap; the note is on the board in that colour, under the project.
4. Write a note, pause a moment, and while the sheet is still up, end
   Gooseberry the hard way (`pkill -9 gooseberry` in a terminal). Open
   Gooseberry again; the note is on the board, whole.
5. Write a note and log out without closing the sheet. Log in: the note is on
   the board, and New note brings the sheet up at once.
6. Tuck a note away from the board; it waits only under Tucked away. Tap it
   there; it goes back where it was.
7. Remove a note. It is in the desktop's trash, and the trash can put it back.
8. Open the notes folder, `~/Documents/Gooseberry`, in a file manager: one
   Markdown file per note, readable in any editor.
