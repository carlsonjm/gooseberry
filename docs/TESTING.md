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
| `tst_quicknote_bus` | The quick note's session-bus interface, used by a client that loads none of Gooseberry's code against the real Gooseberry, on a bus and in a home of the test's own: the version; Start gives an empty note and its choices and keeps nothing; text, colour and Belongs to are on disk when each call returns, and nonsense is refused; Start resumes the open note; a change made to the file elsewhere comes back as Changed; Done, Tuck away, and Remove to the trash with Undo; OpenBoard opens the board and BoardShown follows with the caller's token |
| `tst_spread` | The quick-note card in Kadunce's card workspace, the real card and board against a stand-in Kadunce on a bus of the test's own: without Kadunce, with an older one or a newer protocol, refused the centre, or outside Spread and an Active card, the card stands alone and asks nothing; in Spread it asks only after the version is exactly 1, with its own bus name, object and interface, is drawn at the given place and takes only the presses there; All notes grows it to the Active card's room from the same bus name, prepares the board's launch by its desktop id, opens the board, ignores another request's word, and fades where it stood on its own; put away it gives the centre back once; dismissed it closes, keeping the note, and asks nothing back; growing refused, the launch refused, and the board never arriving each end the guest as `DESKTOP.md` says; over an Active card there is no guest, and the grown card fades only once the board is reported as the selected card, or stays after the wait |
| `tst_screens` | The quick-note card and the board, drawn off screen and used by tap and keyboard: the card is the size of the desktop's search, centred, and keeps above the on-screen keys, rising only as far as it must and then shortening; its header follows the approved layout, with the application's tile and name, the colours centred and a 30 px All notes pill 14 px from the edge, and the note pad 14 px below; All notes grows it to the whole work area into the board, with no Back button, and a note opened there or `Esc` brings the note back; a tap around it puts it away; the cursor is in the note when the card opens; typing keeps the note; colour and Belongs to are one tap; a new project is named once; Done; Tuck away and Remove show only for a kept note; the board opens on Today, opens a note, tucks one away and brings it back, and starts a new note in the place shown; every target is at least 44 pixels |

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
