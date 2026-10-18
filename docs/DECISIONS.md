# Decisions

Each settled decision: what was decided, why, and what was turned down.
Decisions still open are kept privately.

## Gooseberry is its own component, under its own name

Gooseberry is a separate program with its own repository and name, and works on
any KDE Plasma desktop. A product that includes it may give it another name
there. (The maintainer, 3 October.) Where Shuffle is installed, the window
titles, the application's displayed name and its sentences say Notes; elsewhere
they say Gooseberry. One build decides at start, by whether Shuffle's Bottom
Surface is installed. Ids, the desktop file, the bus names and the notes folder
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

## A window's notes are a dot in its title bar

A window with stuck notes shows a small dot in its title bar, left of
minimize, in the top note's colour, with a count when there are several. A tap
brings the notes up over the window, each where Robin last placed it on that
window, kept in proportion when the window is resized; a tap on the dot puts
them away, and a tap on the work folds them (§ Notes fold into a dot on their
window). In Spread, the card shows its window's notes, as the next ruling
says. Neither moves or resizes the window, and nothing needs closing.
(The maintainer, 7 October.) This replaces the 3 October ruling that a card's
notes wait behind it in its Stack.

The dot needs a title bar the desktop draws, such as Shuffle's. An application
that draws its own top bar, as web browsers and many web-based applications
do, has no dot; its notes are on its card in Spread and on the board.

Rejected: a tab on the window's edge, at the top-right or bottom-right, which
covers part of what an application draws there, such as a chat list or account
settings; notes always open on the window, which cover part of the work and
need moving by hand; and notes only behind the card in its Stack, which hides
that a window has notes at all.

## In Spread, a note stands where it was placed

In Spread, each note placed on its window stands on that window's card in the
same place, as big as it is there, both scaled with the card, so the card and
the window agree. Notes never placed sit as a small stack at the card's
bottom-right corner, the same size on every card. Each moves with its card
as Spread opens and closes, as one picture with the window. A tap on a note or
on the stack opens the card, as the next ruling says (`DESKTOP.md` § Stuck
notes on the bus). (The maintainer, 15 October.) This replaces, for placed
notes, the 7 October ruling that every card's notes sit in its corner stack.

Rejected: every note in the corner stack, which moves a note Robin put
somewhere on purpose and makes Spread disagree with the window.

## A stuck note is typed on its window, never in a popup

A note stuck to a window is written where it stands over that window. A tap on
it there puts the cursor in it and brings up the on-screen keys, and what is
typed is kept as the quick note keeps it, from the first letter. In Spread, a
tap on a note opens its card, as a tap anywhere on the card does, with the
note out on the window ready to type; a tap on the stack opens the card with
the window's notes out (`DESKTOP.md` § Stuck notes on the bus). A tap
elsewhere on the window ends the typing. (The maintainer, 15 October.) This
replaces the rulings that a tap on a note over a window opens it on the
quick-note card, and that a tap in Spread fans the card's notes out over it.

Rejected: opening the note on the quick-note card, and fanning the notes out
over the card in Spread. Each puts a large popup between Robin and a note
that belongs to the window, when all Robin wants is to type on it.

## Notes fold into a dot on their window

Once Robin goes back to the window, tapping or clicking into the application
rather than a note, or goes to another window, its notes fold into one small
dot where the top note stands, in its colour, with the count when there are
several, drawn as the title bar draws its dot and a 44 px target. A tap on the
dot opens them where they stood, the top note ready to type with the
on-screen keys up. Typing ends kept before they fold. Windows with a title-bar
dot fold the same way; that dot still puts them away. In Spread a card whose
notes are folded shows the dot in their place. (The maintainer, 15 October.)

Where the desktop's panel shows the window's notes as an island, as Shuffle's
does, folded notes leave no dot over the window: the island brings them back,
and Shuffle's title bar and top bar draw no dot. Without such a panel the dot
stays where it is. (The maintainer, 17 October.)

It holds because an application that draws its own top bar, as web browsers
do, has no title-bar dot, so its notes could otherwise come back only from
Spread, and notes left open cover the work. To see the tap that goes to the
window, open notes hold the keys and give them up to it, which is engineering's
choice: the desktop tells Gooseberry nothing else about a tap that misses its
surface.

Rejected: notes that stay open until put away, which leaves Spread the only way
back to them where there is no title-bar dot; and a full-size surface that
sees every tap, which would take taps from the window.

## Notes are plain files; Search makes them look at home

Notes are kept as a folder of plain Markdown files. Tettegouche's Search is the
bridge that shows them as notes rather than files. (The maintainer, 3 October.)

Rejected: a database of Gooseberry's own, which no other app, file index or
assistant can read.

## A card's notes reopen with it

When a card's window closes, its notes stay with what was open in it, and come
back stuck to it when that document opens again: a sticky note on the monitor,
made virtual. Gooseberry remembers the application and the document's name,
read from the window's title; a note whose document cannot be found again
stays in its folder, and on the board under the name it was stuck to. (The
maintainer, 3 October.)

Rejected: unsticking a closed card's notes, which never brings them back.

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

## The core stays apart from the desktop

Notes, the board, the planner and reminders are one core, apart from what any
desktop adds. It was ruled when a mobile companion was a likely later step
(the maintainer, 3 October); the phone companion, sync across computers and
the pen have since been dropped, and nothing more is built for a phone, but
the separation still serves.

- **One core, desktop parts outside it.** Notes, the board, the planner and
  reminders live in a core that knows nothing of Kadunce, Tettegouche, Split
  Rock or any panel. What those add sits outside it and is found at run time.
- **Screens in Kirigami,** KDE's touch-ready toolkit, which draws them in the
  desktop's own style.
- **The note format is a contract.** The folder layout and the header at the
  top of each note are written down and versioned, since any other program
  reading the same folder, a folder sync tool's copy included, depends on them.

Rejected: building for the desktop first and separating later, which would
have tied the core to one desktop.

## The mock-up is the approved visual direction

The six screens in `mockup/` are the approved layout and behaviour: what each
place holds, its sizes and touch targets, the note colours, and how capture,
the board and the planner work. An agent builds to them without asking again.
Where the mock-up and a later ruling disagree, the ruling wins: the rail beside
the card is superseded by the dot in the title bar (§ A window's notes are a dot
in its title bar). (The maintainer, 4 October.)

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
into it. The quick note shows both answers, already filled in: the folder,
and the window in front, with a list of the open windows a tap away.
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
All notes opens the board in Gooseberry's own window and the card goes.
Gooseberry stays an application of its own, not part of the search. (The
maintainer, 4 October; All notes, 17 October.)

This supersedes the bottom sheet in `mockup/Capture.dc.html`; what the card
holds, the size of its controls and the note colours still follow the mock-up.

The card's layout follows § The quick note listens. A note opened from Search's
Notes has a bar on top, ‹ Notes, which puts the card away and brings Search's
Notes back; `Esc` does the same. The search's own note page has that way back,
and the card took the note's place without it. (The maintainer, 17 October.)

Rejected: growing the card into the board, as Apps and Files grow the search.
The grown card stood over the Context Panel with no title bar, and the board
there was not the window Gooseberry's own entry opens. (The maintainer,
17 October.)

Rejected: a sheet from the bottom of the screen, where the on-screen keys come
up over it; and the card growing into a larger page for one note, which leaves
nothing to browse where Apps and Files have their lists.

## The quick note listens

The quick note is its words and little else, shaped as the search: the words
fill the card inside 12 px margins on the card's own ground, and the note's
colour is a dot beside their first line: a tap lays the five colours out beside
it. Words that ask for something offer it under the note, each a tap: a day
named offers a reminder at nine that morning, or six for tonight; lines begun
with "- " offer a checklist; a #word offers to keep the note in that folder,
made if it is new. A taken offer goes, and shows kept on the line below. Past
a hairline is one 44 px line: where the note is kept and what it is stuck to,
as quiet words that open their choices, a tick for kept as written, then
Checklist, Remind, More and All notes as glyphs, with "Words that ask for
something offer it" while nothing is offered and there is room. Remind says
when once a reminder is set. More holds what a kept note rarely needs: Copy
all, Pin, Tuck away and Remove. One note is pinned at a time, and the
desktop's panel keeps it in sight beside a dot that starts a quick note. There
is no Done: going back to the work, or `Esc`, puts the card away, and `Esc`
first closes whatever is open on the note. Sizes and spacing come from the
suite's visual language. (The maintainer, 17 October: "Listens", after first
choosing "Just paper" of three directions.) This replaces the paper page in
the note's colour, and before it the card's layout of 4 October: the header
with the application's tile and name, the five colours and the All notes
pill, the Checklist and Remind pills, the Folder and Stuck to chips, and Saved
as you go, Tuck away, Remove and Done.

It holds because the words are what a quick note is for, and what they ask
for can be offered from them rather than sought among controls.

Rejected: the note on its own over the work with the folders as paper stacks
to drop it on, and the paper page with nothing offered.

## The board is a search

The board opens as a search of every note: Find or start a note along its top,
and the places in one row under it, Today, the folders with Inbox first and
New folder last, a + once there is a folder, which makes one at a tap with its
name ready to type over,
the windows with notes stuck to them, and Tucked away, quieter,
last, each a place to drop a carried note. Enter with words no note has starts
a note with them. The notes of the place chosen fill the rest one a row, as a
search lists what it finds: a swatch in the note's colour, its first line, and
its folder and window under it, each still held to be carried, with the
planner on Today. On a wide board a row shows more, the note's next words
under its first line and its place and when it changed at its end, so the
width tells rather than stands empty. New note floats in the board's corner
in the highlight colour, a large button in reach of a finger while the list
scrolls; under a pointer it lights, with a soft ring of its colour. Sizes and spacing come from the suite's visual language (Kadunce's
`docs/ITASCA-VISUAL-LANGUAGE.md`). (The maintainer, 17 October, choosing wide
rows and a floating New note.) A desktop's search that hosts the notes draws the same
board in its Notes mode, so Gooseberry's own window and the search read as one
(`DESKTOP.md` § The quick note in Search). (The maintainer, 17 October,
choosing the board as a search of three directions.) This replaces the places
running down the board's side.

It holds because the side list took a quarter of the board for what is chosen
once, the search is how a note is most often found, and coloured cards of
different heights read as a different program from the search that opens
them. This replaces the board's notes as cards in columns.

Rejected: one list from today's notes down to the folders, with the note open
beside it, and a desk of folder stacks and windows with their notes.

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
desktop's search does. Over an Active card it stands on its own. All notes, in
either, gives the centre back and opens the board's own window, which Kadunce
shows as any other card. Without Kadunce, or with one that does not offer this, the card
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
  stands where its time was, and its words are struck through. The card has
  no Done of its own (§ The quick note listens).
- **A reminder offers Done and In 10 minutes,** and tapping it opens the note.
  In 10 minutes is a new reminder, shown once like any other.

Checklist and Remind are glyphs on the line under the note (§ The note is the
page); what Remind offers opens between the page and that line.

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

## Another version of a note is kept beside it

When another program or a sync tool changes a note's words while Gooseberry is
writing its own, Gooseberry writes what was typed here and keeps the other version
as a new note in the same folder (`FORMAT.md` § How notes are kept). Any other
change made elsewhere is taken onto the card, and only the typing waiting there
is written over it. This is engineering's choice: the folder is shared with
other devices and programs, and either version may hold words someone wants.

Rejected: what is typed here winning outright, which lost the other device's
edit without a trace; and asking which version to keep, a question in the
middle of writing.

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

## The app icon keeps an icon theme's margin

The icon's tile sits inside the margin icon themes leave around application
artwork, 42 of 48 in Colloid, so it is the size of the apps beside it in a dock,
a launcher or Search. (The maintainer, 7 October, for pixel-matched demo videos.)

Rejected: the tile drawn to the icon's edges, chosen on 6 October, which reads
a size larger than every themed app next to it.
