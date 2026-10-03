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
