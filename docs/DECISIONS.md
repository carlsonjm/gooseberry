# Decisions

Each settled ruling: the rule, why it holds, and what was rejected. Decisions
still open are in `ROADMAP.md` § Open decisions.

## Gooseberry is its own component, under its own name

Gooseberry is a separate program with its own repository and name, and works on
any KDE Plasma desktop. A product that includes it may give it another name
there. (The maintainer, 3 October.)

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

## On a card, notes stack behind it

A card's notes are a Gooseberry window in that card's Stack, behind it. Going
back to the card puts them away; side by side is a Bento pair. Like a note
tucked behind the page rather than one stuck to the monitor's edge, it takes no
room and never needs closing. (The maintainer, 3 October.)

Rejected, for now: a rail of stickies beside the card, which needs Kadunce to
offer room and a rule to fold it away, and is revisited only if daily use shows
that seeing a note while working matters; and stickies floating over the card,
which cover the work and each need moving or closing.

## Notes are plain files; Search makes them look at home

Notes are kept as a folder of plain Markdown files, ink beside each as a
drawing. Tettegouche's Search is the bridge that shows them as notes rather
than files. (The maintainer, 3 October.)

Rejected: a database of Gooseberry's own, which no other app, file index or
assistant can read.

## A card's notes reopen with it

When a card's window closes, its notes stay with what was open in it, and come
back in its Stack when that document opens again: a sticky note on the monitor,
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
the card is superseded by notes stacked behind it. (The maintainer, 4 October.)

## Milestone 0 is built in one pass

The concept, these decisions and the mock-up are the approval for Milestone 0.
It is built whole, then handed over for testing against its proof, rather than
brought back screen by screen. A question the documents do not answer still
comes to the maintainer before it is guessed. (The maintainer, 4 October.)

Rejected: options before each screen, which the mock-up already settles.

## A project is named once

Under Belongs to, a project's name is typed the first time it is used; after
that it is one tap, and the project used last is offered first. A project lasts
as long as a note belongs to it. (The maintainer, 4 October.)

Rejected: working a project out from where the document lives. Plain Plasma
does not say where most windows' documents are, so the choice would rarely
appear before notes on cards.

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

## Opening Gooseberry opens the quick-note card

Gooseberry's entry, in a launcher, a search or pinned to a dock, opens the
quick-note card. The board is All notes on the card and All notes in the
entry's menu, where New note stays too. (The maintainer, 4 October, superseding
the board opening from the entry, of 3 October.)

Where a desktop's search hosts the quick note (below), the search no longer
opens Gooseberry and waits for a window to arrive; it draws the note itself.

Rejected: the entry opening the board, which put a window between the person
and the note they opened Gooseberry to write.

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
