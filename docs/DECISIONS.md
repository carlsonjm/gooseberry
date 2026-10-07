# Decisions

Each settled ruling: the rule, why it holds, and what was rejected. Decisions
still open are in `ROADMAP.md` § Open decisions.

## Gooseberry is its own component, under its own name

Gooseberry is a separate program with its own repository and name, and works on
any KDE Plasma desktop. A product that includes it may give it another name
there. (The maintainer, 3 October.) Where Shuffle is installed, the window
titles, the application's displayed name and its sentences say Notes; elsewhere
they say Gooseberry. One build decides at start, by whether Shuffle's bottom
surface is installed. Ids, the desktop file, the bus names and the notes folder
keep Gooseberry's name either way.

Rejected: building notes inside a product's own repository. Notes would only
exist inside that product, and anyone without it would be left with the
problem Gooseberry exists to solve.

## Cards and Search are found, never required

Gooseberry works with Kadunce's cards and Tettegouche's Search when they are
present, and is complete without them. It finds each at run time. (The
maintainer, 3 October.)

Rejected: requiring either. A notes app that needs a particular window manager
is not one most people can use.

## Nothing comes before the note

A note is kept from its first letter. No title, folder, notebook or save step
comes first. (The maintainer, 3 October.)

Rejected: a title or notebook chosen first, as most notes apps ask. That
question is where the thought is lost.

## A note can be stuck to a window, as a folded corner

A note stuck to a window shows as a folded paper tab peeking out from behind
the window's bottom-right edge, in the gap around the card, with a count when
there are several. It stays clear of the window's buttons, title and scroll
bar. A tap brings the notes up over the window, each where Robin last placed
it on that window, kept in proportion when the window is resized; a tap on the
work folds them back. In Spread, each card's notes sit as a small stack at its bottom-right
corner, the same size on every card, and a tap fans them out over
that card. The corner never moves or resizes the window and never needs
closing. (The maintainer, 7 October.) This replaces the 3 October ruling that a
card's notes wait behind it in its Stack.

Rejected: the top-right corner, where the tab covers the window's buttons;
notes always open on the window, which cover part of the work and
need moving by hand; and notes only behind the card in its Stack, which hides
that a window has notes at all.

## Notes are plain files; Search makes them look at home

Notes are kept as a folder of plain Markdown files, ink beside each as a
drawing. Tettegouche's Search is the bridge that shows them as notes rather
than files. (The maintainer, 3 October.)

Rejected: a database of Gooseberry's own, which no other app, file index or
assistant can read.

## A card's notes reopen with it

When a card's window closes, its notes stay with what was open in it, and come
back stuck to it when that document opens again: a sticky note on the monitor,
made virtual. Gooseberry remembers the application and the document's name or
path; a note whose document cannot be found again waits in Loose, marked with
where it came from. (The maintainer, 3 October.)

Rejected: turning a closed card's notes Loose, which never brings them back and
fills Loose up.

## Gooseberry's folder is its own

Gooseberry keeps its notes in its own folder, apart from Split Rock's notebook
about the computer. Search shows both. (The maintainer, 3 October.)

Rejected: one folder shared with Split Rock, which would tie the two
components to one layout and its rules.

## Notes are a tool for the assistant

Where Split Rock is installed, Gooseberry offers it a notes tool: find notes,
read one, and add one when the person asks. The assistant says when it reads a
note, as Split Rock does with its own memory, and next step from a note sends
only that note. Gooseberry works the same without it. (The maintainer, 3
October.)

Rejected: Split Rock reading Gooseberry's folder directly, which would make the
folder's layout a contract and hide from the person what was read.

## What the assistant keeps becomes notes, where Gooseberry is installed

Where Gooseberry is installed, what the person keeps from Split Rock, and the
memory items it needs to call on later, become Gooseberry notes: on the board,
found by Search, under a place of their own. Split Rock keeps its own memory
until it has an installed Gooseberry to use, and always keeps the record of the
computer, such as hardware and maintenance. (The maintainer, 3 October.)

Rejected: kept items staying only in Split Rock's notebook, which splits the
person's notes in two and keeps them off the board and the planner.

## The core is built for a mobile companion

A mobile companion is a likely later step, so Gooseberry is built for it from
the start. (The maintainer, 3 October.)

- **One core, desktop parts outside it.** Notes, the board, the planner and
  reminders live in a core that knows nothing of Kadunce, Tettegouche, Split
  Rock or any panel. What those add sits outside it and is found at run time.
- **Screens in Kirigami.** The same screens then run on Plasma Mobile as they
  are, and are the starting point for a port to Android.
- **The note format is a contract.** The folder layout and the header at the
  top of each note are written down and versioned before Milestone 0 ends, since
  a second device reading the same folder depends on them.

Rejected: building for the desktop first and separating later, which turns a
companion into a second app.

## The mock-up is the approved visual direction

The six screens in `mockup/` are the approved layout and behaviour: what each
place holds, its sizes and touch targets, the note colours, and how capture,
the board and the planner work. An agent builds to them without asking again.
Where the mock-up and a later ruling disagree, the ruling wins: the rail beside
the card is superseded by the folded corner (§ A note can be stuck to a
window). (The maintainer, 4 October.)

## Milestone 0 is built in one pass

The concept, these decisions and the mock-up are the approval for Milestone 0.
It is built whole, then handed over for testing against its proof, rather than
brought back screen by screen. A question the documents do not answer still
comes to the maintainer before it is guessed. (The maintainer, 4 October.)

Rejected: options before each screen, which the mock-up already settles.

## A note is kept in a folder, and may be stuck to a window

Every note is kept in exactly one folder; Inbox is where it lands unless
another is chosen. Separately, a note may be stuck to one window, and comes
back on it when that document opens again. Moving a note to another folder
keeps it stuck. A folder can be made on the board before any workspace or
window exists, lasts until it is removed, even empty, and removing it sends its
notes to Inbox. A workspace can be given a folder, and notes written there go
into it. The quick note shows both answers as chips, already filled in: the
folder, and the window in front, with a list of the open windows a tap away.
(The maintainer, 7 October.) This replaces "A project is named once" of
4 October.

Folders are one level deep and are real folders on disk, so a file manager and
any folder sync see the same projects. Both are engineering's choice.

On the board, a held note lifts and is dropped on a folder, a day or the trash.
Where Kadunce offers it, a lifted note carried to the top edge opens Spread and
is stuck to the card it is dropped on.

Rejected: one Belongs to choice among window, project, workspace and Loose,
which made a note pick between its project and its window; a project that ends
with its last note, which left no way to start one ahead of time; and folders
inside folders, which lengthen every choice.

## A quick note is a card the size of the search

The quick note opens on a card the size of the desktop's search, centred over
the work, with the cursor in the note. With the on-screen keys up it rises
only as far as it must to stay above them, then shortens, as the search does.
All notes grows the card into the board, as Apps and Files grow the search; a
note opened or started there brings the card back to its size. Gooseberry stays
an application of its own, not part of the search. (The maintainer, 4 October.)

This supersedes the bottom sheet in `mockup/Capture.dc.html`; what the card
holds, the size of its controls and the note colours still follow the mock-up.

The card's layout, approved on the tablet (the maintainer, 4 October): the
search's 8 px corners with a 1 px outline and a 22 px inner margin; a 44 px
header with a 32 px tile in the note's yellow carrying the notes glyph and the
application's name at the left, the five colours centred, and All notes at the
right as a 30 px pill 14 px from the edge; the note pad 14 px under the header,
across the width, with 14 px corners in the note's colour; then Belongs to, and
Saved as you go, Tuck away, Remove and Done at the bottom. All notes grows the
card to the whole work area less a 10 px gutter in 220 ms, easing out, with no
Back button at that size: a note opened or started on the board, or `Esc`,
brings the card back.

Rejected: a sheet from the bottom of the screen, where the on-screen keys come
up over it; and the card growing into a larger page for one note, which leaves
nothing to browse where Apps and Files have their lists.

## The quick-note card is a surface of the desktop's own

The card is drawn on a layer of the desktop's own, across the room the panels
leave: it takes the keyboard with the cursor in the note, and a tap on the work
around it puts it away. No window is moved. The layer is the one below the
on-screen keys, so they stay above the card and take their own touches. On a
desktop without such layers it is an ordinary window kept above the others.
This is engineering's choice.

Rejected: an ordinary window, which the window manager places where it
chooses; a panel widget, which exists only where a panel holds it; and the
layer above the keys, where a surface opened after them covers them and takes
every touch meant for them, as the desktop's search found.

## Opening Gooseberry opens the board, as an ordinary window

Gooseberry's entry, in a launcher, a search or pinned to a dock, opens the board
as an ordinary window: it resizes, and a card workspace can place it beside
another, as any application's window. The quick-note card is New note in the
entry's menu, and where a desktop's search hosts the quick note (below), that
is where a quick note is written. (The maintainer, 4 October, after trying the
card from the dock, superseding the entry opening the card, of the same day.)

Rejected: the entry opening the quick-note card, which cannot be resized or
placed beside another window, so Gooseberry had no window of its own to work in.

## A desktop's search can host the quick note

Shuffle's Search hosts the quick note as one of its modes: it draws the note
pad in its own window, in its own card, and Gooseberry keeps the note, through
a small, versioned interface on the session bus (`DESKTOP.md` § Tettegouche).
The search loads none of Gooseberry's code and holds no notes; every change it
sends is written before the call returns, as Gooseberry writes its own. Outside
the search, Gooseberry's own card has the same shape. (The maintainer, 4
October, approved on the tablet.)

Rejected: the search opening Gooseberry's card over itself, which is two
surfaces for one note, and the search reading or writing the notes folder
itself, which would make the folder's layout its contract and bypass
Gooseberry's own keeping.

## In Kadunce, the card takes Spread's centre

Where Kadunce shows Spread, the quick-note card holds Spread's centre, as the
desktop's search does, and All notes grows it into the Active card's room and
hands that room to the board's window, which fades the card out. Over an Active
card it stands on its own, and the board takes the Active card's place before
the card fades. Without Kadunce, or with one that does not offer this, the card
is as it always was (`DESKTOP.md` § The quick note in Spread). (The maintainer,
4 October.)

## The planner's controls

What the mock-up shows as Checklist and Remind on the note, and what it leaves
undrawn, was proposed and approved before it was built (the maintainer, 4
October):

- **Remind offers quick times.** Later today, This evening, Tomorrow morning,
  Next time this opens, and Pick a time for any day and hour: one tap for the
  common case. Once set, Remind says when, with a cross beside it to take the
  reminder away.
- **A checklist is ticked in place.** Each line becomes an item with a box;
  Enter starts the next. A ticked item is struck through where it stands, as
  on a paper list. Checklist again turns it back into text.
- **A planned note is marked done by a tick on its planner row.** Done then
  stands where its time was, and its words are struck through. The card's own
  Done keeps meaning "put the card away".
- **A reminder offers Done and In 10 minutes,** and tapping it opens the note.
  In 10 minutes is a new reminder, shown once like any other.

Checklist and Remind sit in a row under the note, above Belongs to; the note
pad gives up the room for them.

Rejected: a day and time picker every time, which costs two or three taps even
for tomorrow morning; ticked items sinking to the bottom, which makes items
jump under the finger; Mark done on the card, beside the Done that puts it
away; and a reminder with nothing to tap but the note.

## A reminder is recorded before it is shown

When a reminder comes due, Gooseberry writes the time it was shown into the
note, and only then shows it; a reminder that could not be recorded is not
shown. Where nothing on the desktop shows notifications, a due reminder waits,
unrecorded, until something does. A reminder due while the computer slept or
Gooseberry was not running is shown when it wakes or starts. This is
engineering's choice: the proof is every reminder on time and none shown
twice, and the note is the one record a restart, a second program or a second
device reading the folder all see.

Rejected: showing first and recording after, where a crash between the two
shows it again; and a list of shown reminders in Gooseberry's own settings,
which another device reading the same folder cannot see.

## Reminders use the desktop's standard notifications directly

Gooseberry asks for each reminder through the freedesktop.org notification
interface on the session bus, under its own entry, for the desktop's own time
on screen. This is engineering's choice: whatever shows notifications on any
desktop shows it, Plasma keeps it in its history and its notification
settings, it adds no library, and a test can stand in for the desktop.

Rejected: KDE's notification library, which adds a library and an events file
for nothing the person sees here; and a reminder that stays until closed,
which would be something on the work that has to be closed.

## A checklist is Markdown task lines

A checklist is kept in the note's text as `- [ ]` and `- [x]` lines, the way
other Markdown programs write a task list, so any editor, the file index and
Search read it as it is. This is engineering's choice.

Rejected: a header key holding the items, which other programs would not show
as a list.

## Gooseberry stays ready

Gooseberry starts with the session, showing nothing, and keeps running between
notes; closing the board or the card only puts it away. Every start, from its
entry or its menu, reaches the one running Gooseberry. This is engineering's choice: speed to the
cursor is the product, and a program starting from cold takes a moment the
thought may not last.

Rejected: starting Gooseberry afresh on each tap.

## Typing is written at each pause

The first letter makes the note on disk at once; later typing is written when
the writing pauses for half a second, at least every three seconds while it
goes on, and at once on any other change, Done, a logout or quitting
(`FORMAT.md`). This is engineering's choice: writing the whole note on every
letter cost the disk tens of kilobytes a letter for a note of a few hundred
bytes, and a crash now loses at most the last moment of typing, never the note.

Rejected: writing on every letter, for the disk and battery it costs; and
writing only on Done, which a crash would cost the whole note.

## Removing a note uses the desktop's trash directly

Gooseberry moves a removed note into the person's trash itself, as the
freedesktop.org trash specification lays it out, and falls back to Qt's own
trash only for a note on another disk. This is engineering's choice: Qt alone
decides which trash to use by working out which disk a folder is on, which some
systems answer wrongly, and a removal that fails is a note that cannot be
removed.

## Window arrangement is requested, never done directly

To show notes beside or with a window, Gooseberry asks the window manager, which
decides and stays the only owner of window positions.

Rejected: Gooseberry moving or resizing windows itself, which would give windows
two owners.

## Written in C++ and Qt, like the rest of the suite

The program uses C++, Qt and KDE's own libraries, with Kirigami for its
screens, as Kadunce, Tettegouche and Split Rock do. This is engineering's choice.

Rejected: a web toolkit, which is heavy on a tablet's battery and draws nothing
like the rest of the desktop; and a second language and toolchain for a suite
maintained by one person and agents.
