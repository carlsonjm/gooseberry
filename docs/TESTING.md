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
| `tst_store` | A note is on disk from its first letter, typing after it once the writing pauses and on the way through long typing, and at once on Done or any other change; nothing is kept for an empty note; Belongs to and colour are one change each; removing sends the note and its ink to the trash with the record the trash needs, and undo brings them back; tuck away and bring back; the board's places, counts, order and search; changes made by other programs are read; a missing folder is made again; a folder or note from a newer version is left alone |
| `tst_keep` | A second copy of the test types a note and is ended as a crash ends a program, after one letter, mid-sentence and after a pause, and as a logout ends it mid-sentence; after a pause or a logout the note is all there, a crash mid-sentence keeps it up to the last pause, nothing half-written is left beside it, and it is back on the board after a restart |
| `tst_planner` | Reminders, done and checklists kept in the note, read back as written, and a reminder this version cannot read kept as it was; a due reminder is recorded in its note before it is told, then never shown again, through a look at the clock or a restart; one not yet due waits and comes on time, within a second; with nothing to show it, it waits unrecorded; "next time this opens" only for that document in that application, once; In 10 minutes is a new reminder; recording a reminder shown leaves the note's changed time alone; a reminder shown while typing waits for a pause is not lost when the typing is written; on the card, a reminder is kept at once, a new one shown again, and "next time this opens" only for a note written on a window; checklists made from text, ticked, added to, taken back, with their heading; Today holds the notes planned for today and those written today, the planned ones on the planner rather than among the ideas; the planner's day strip, rows in time order with Done and the next one to come, the days ahead, another day chosen; and what Remind offers through the day |
| `tst_ink` | Handwriting kept: the drawing is plain SVG, each stroke a filled shape in its ink, as tall as the writing, its points, pressure and readings read back exactly; pressing harder draws a wider line and a touch is a dot; the page grows a line below the writing; the eraser takes whole strokes and puts them back; each line is read once, a changed line again, a fix is never read over and lines after it are read; on the card the first stroke keeps a new note at once and later strokes wait for a pause, ink alone is a note, the eraser and Undo, a page erased bare and finished goes to the trash, the drawing goes to the trash with its note and comes back with it, a fix is kept at once, a reading made while strokes wait is not lost; the board's search finds handwriting by its reading and by a runner-up word |
| `tst_reading` | Reading on the computer: a line drawn black on white for reading, whatever its ink; the real reader, run by ONNX Runtime on a stand-in model of the real one's shape (`tests/reading/make-fake-reader.py`), reads a line and gives its runner-up words, rests and loads again, and with no model reads nothing; notes are read line by line out of sight, kept without counting as a change, and not read again; a fix is never read over; a line changed while being read is read again; after a start every unread note is read, a reader that fails leaves the note for another time, and the reader rests when there is nothing to read |
| `tst_reading_bus` | The real Gooseberry, with the stand-in reader, reads handwriting it has not read a little after it starts, and keeps the reading in the note without changing its changed time |
| `tst_reminders_bus` | Reminders as the desktop shows them: the real Gooseberry on a bus of the test's own, with the test standing in for the desktop's notifications. With nothing to show notifications a due reminder waits unrecorded, then is shown once something can; the notification carries the note's first words, when and where, Open, Done and In 10 minutes, Gooseberry's own entry, and the desktop's own time on screen; one due while Gooseberry runs is shown on time, once; Done marks the note done and In 10 minutes sets a new reminder ten minutes on; started again, Gooseberry shows nothing it has shown |
| `tst_quicknote_bus` | The quick note's session-bus interface, used by a client that loads none of Gooseberry's code against the real Gooseberry, on a bus and in a home of the test's own: the version; Start gives an empty note and its choices and keeps nothing; text, colour and Belongs to are on disk when each call returns, and nonsense is refused; Start resumes the open note; a change made to the file elsewhere comes back as Changed; Done, Tuck away, and Remove to the trash with Undo; OpenBoard opens the board and BoardShown follows with the caller's token; from Milestone 1, what Remind offers, a reminder set, refused and taken away, and a checklist made, ticked and turned back, each on disk when the call returns; from Milestone 2, `ink` and `read` are offered |
| `tst_spread` | The quick-note card in Kadunce's card workspace, the real card and board against a stand-in Kadunce on a bus of the test's own: without Kadunce, with an older one or a newer protocol, refused the centre, or outside Spread and an Active card, the card stands alone and asks nothing; in Spread it asks only after the version is exactly 1, with its own bus name, object and interface, is drawn at the given place and takes only the presses there; All notes grows it to the Active card's room from the same bus name, prepares the board's launch by its desktop id, opens the board, ignores another request's word, and fades where it stood on its own; put away it gives the centre back once; dismissed it closes, keeping the note, and asks nothing back; growing refused, the launch refused, and the board never arriving each end the guest as `DESKTOP.md` says; over an Active card there is no guest, and the grown card fades only once the board is reported as the selected card, or stays after the wait |
| `tst_screens` | The quick-note card and the board, drawn off screen and used by tap and keyboard: the card is the size of the desktop's search, centred, and keeps above the on-screen keys, rising only as far as it must and then shortening; its header follows the approved layout, with the application's tile and name, the colours centred and a 30 px All notes pill 14 px from the edge, and the note pad 14 px below; All notes grows it to the whole work area into the board, with no Back button, and a note opened there or `Esc` brings the note back; a tap around it puts it away; the cursor is in the note when the card opens; typing keeps the note; colour and Belongs to are one tap; a new project is named once; Done; Tuck away and Remove show only for a kept note; the board opens on Today, opens a note, tucks one away and brings it back, and starts a new note in the place shown; every target is at least 44 pixels; from Milestone 1, Remind offers the quick times, next time this opens and Pick a time, a choice is kept and the pill says when, the cross takes it away, and Pick a time steps day, hour and quarter hour, never before now; Checklist turns lines into items under their heading, ticks in place, Enter starts an item, Backspace in an empty one takes it, the line written scrolls into sight, and Checklist again gives the text back; the board's Today as the mock-up draws it, with the day strip and its dots, the planner's rows in order with the next one marked and a done one struck through, the days ahead, the ideas with no date beside it and a checklist as heading and items, a tick marking done and undone, a row opening its note, another day chosen, and the strip on its own line in a narrow window; from Milestone 2, Pen opens the ruled page under the words, a pen's stroke is as wide as it presses and in the ink chosen, a finger writes until the pen hovers near and then writes nothing, the pen's eraser end and Eraser in the palette take whole strokes and Undo puts them back, "Read as" under the ink can be fixed and the fix is kept, a pen touching the ink out of Pen switches to Pen, every palette target is 44 pixels, and the board shows handwriting as a picture marked Handwritten, found by search with "Read as" and the word marked |

With `GOOSEBERRY_SCREENSHOTS` set to a folder, `tst_screens` saves a picture of
each screen there, to compare with the mock-up by eye.

## By hand: Milestone 0

Milestone 0's proof is a week of daily use on the tablet without a lost note,
with capture fast enough that nothing is written elsewhere instead
(`ROADMAP.md`). These checks come first, once, after installing; then the week
of use.

1. Over a document, tap Gooseberry. The card opens in the middle of the
   screen with the cursor in the note and the on-screen keys up, the whole card
   above the keys and every control reachable; "This window" names the
   document. Hold Gooseberry and tap All notes: the board opens as an ordinary
   window.
2. Type one letter and nothing else, then tap the work around the card. Open
   the board: the note is there under Today.
3. Write a note, choose a colour and a project, and finish it. Each choice is
   one tap; the note is on the board in that colour, under the project.
4. Write a note, pause a moment, and while the card is still up, end
   Gooseberry the hard way (`pkill -9 gooseberry` in a terminal). Open
   Gooseberry again; the note is on the board, whole.
5. Write a note and log out without closing the card. Log in: the note is on
   the board, and New note brings the card up at once.
6. Tuck a note away from the board; it waits only under Tucked away. Tap it
   there; it goes back where it was.
7. Remove a note. It is in the desktop's trash, and the trash can put it back.
8. On the card, tap All notes: it grows to the whole screen, less a narrow
   margin, into the board, as Apps grows the search. Tap a note there: the card
   comes back to that note.
9. Open the notes folder, `~/Documents/Gooseberry`, in a file manager: one
   Markdown file per note, readable in any editor.

## By hand: in Kadunce

Only the tablet shows whether Kadunce and the card move together. With Kadunce
installed and set up:

1. Open Spread and tap Gooseberry. The card sits in Spread's centre, the cards
   on either side moved in to make room, as with the desktop's search; the
   keys come up and the card stays above them. A tap on a card beside it is
   Spread's.
2. Write a word, then tap All notes. The cards beside it fade, the card grows
   to the Active card's size, the board appears in its place, and the card
   fades out over it. The board is the Active card.
3. Open Spread, tap Gooseberry, and put the card away. Spread is as it was.
4. With the card in Spread's centre, open the desktop's search. The card
   closes, its note kept, and the search takes the centre.
5. Over an Active card, tap Gooseberry, then All notes. The card grows, the
   board takes the Active card's place, and only then does the card fade.
6. Without Kadunce running, steps 1 and 2 behave as in § By hand:
   Milestone 0.

## By hand: Milestone 1

Milestone 1's proof is a week of the maintainer's days planned in Gooseberry,
every reminder on time and none shown twice (`ROADMAP.md`). These checks come
first, once, after installing; then the week of use.

1. Write a note and tap Remind. The times offered fit the hour of the day; tap
   one. Remind says when, and the note is on the board's Today planner, or on
   its day in the strip.
2. Set a reminder two minutes ahead with Pick a time, put the card away and go
   back to work. The desktop's notification comes on the minute, with the
   note's first words, when and where it belongs. It goes away by itself and
   waits in the desktop's history.
3. Tap a reminder: the note opens on the card. Tap Done on another: the note
   shows Done on the planner, struck through. Tap In 10 minutes on a third: it
   comes back once, ten minutes later.
4. Set a reminder a few minutes ahead, then log out before it comes, and log
   in after: it is shown once, at login. Restart Gooseberry: nothing is shown
   again.
5. Set a reminder a few minutes ahead and let the tablet sleep past it. On
   waking, it is shown within half a minute, once.
6. Over a document, write a note and tap Remind, then Next time this opens.
   Close the document and open it again: the reminder comes then, once.
7. Make a checklist from a few lines, tick an item with a finger, add one with
   Enter on the on-screen keys, and take one away with Backspace. On the board
   the list reads as its heading and items, the ticked one struck through.
8. On the board's Today, tick a planned note done and untick it; tap another
   day in the strip, and a row to open its note.
9. Open a note with a reminder in a text editor: `remind`, `reminded` and
   `done` read plainly, and a checklist is `- [ ]` and `- [x]` lines.

## By hand: Milestone 2

Milestone 2's proof is handwritten notes from daily use found by a word in
them (`ROADMAP.md`). Only the tablet shows how the pen feels and how well the
real reader reads; the tests above prove everything round them with a stand-in
reader. These checks come first, once, after installing; then daily use.

1. Install with a network: `./install.sh` says it fetched the handwriting
   reader. Without one it says handwriting will be kept but not read.
2. New note, then Pen. Write two lines with the pen: the line follows the pen
   without lag, and thickens as you press. Choose blue, then red, and write.
3. Rest your hand on the page while writing: it leaves no marks. Put the pen
   down, and write a word with a finger: it writes.
4. Turn the pen over, if it has an eraser end, and rub out a word: whole
   strokes go. Tap Undo: they come back. Do the same with Eraser in the
   palette.
5. Type a line above the ink with Type, then put the card away. Open the
   board: the note shows the words, the ink under them, and Handwritten.
6. Search the board for a word you wrote by hand, a few seconds after putting
   the card away: the note is found, with "Read as" and the word marked. Note
   how long the reading took, and how many words it read right.
7. Open the note: "Read as" is under the ink. Tap it, fix a misread word,
   press Enter. Search for the fixed word: found. Write another line, put the
   card away, and open it again: the fix stands, and the new line is read.
8. Open the notes folder in a file manager: the drawing beside the note shows
   the handwriting, and the note's file says what it was read as.
9. Remove a note with ink, then Undo: the ink comes back with it.
