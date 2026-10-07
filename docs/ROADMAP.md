# Roadmap

What is planned for Gooseberry, in order, and what is not. Each milestone ends
with what it has to prove before the next one starts. Decisions still open are
at the end, each named with what it holds up.

**Where it sits in the suite:** listed in the suite record, not yet scheduled.

## Milestone 0: capture and keep

Gooseberry on its own, on any Plasma desktop. Built in one pass to the mock-up
in `mockup/`, then tested against the proof (`DECISIONS.md`).

**Status:** built; waiting on its proof. The checks by hand are in
`TESTING.md`.

- The quick-note card: one tap, cursor ready, saved as written, Belongs to and
  colour in one tap each, kept above the on-screen keys, grown into the board
  by All notes, Done by going back to the work. Gooseberry's icon opens it.
- The quick note offered to a desktop's search on the session bus, so the
  search can host it as one of its modes.
- The board: Loose, Today, Tucked away, by window or document, by project, by
  workspace.
- Tuck away and bring back. Removing sends a note to the trash.
- Notes kept through a crash, a logout and a restart.
- The note format written down and versioned: the folder layout and each
  note's header.

**Proves:** a week of the maintainer's daily use on the tablet without a lost
note, and with capture fast enough that nothing is written elsewhere instead.

## Milestone 1: the planner

**Status:** built; waiting on its proof. The checks by hand are in
`TESTING.md`. Remind, Checklist, the tick on the planner and the reminder's
own buttons were drawn as proposals and approved by the maintainer
(`DECISIONS.md`).

- Reminders: a time, or next time a document opens.
- The planner: today's timed notes, the days ahead, and what is done.
- Checklists.
- Reminders through the desktop's standard notifications.

**Proves:** a week of the maintainer's days planned in Gooseberry, every
reminder on time, and none shown twice.

## Milestone 2: folders

Order from the maintainer, 7 October: folders, stuck notes, Search, sync and
the phone companion, with pen last (`DECISIONS.md` § A note is kept in a
folder).

- Folders on the board, Inbox first and New folder last; a folder kept until
  removed, its notes sent to Inbox when it is.
- The quick note's two chips: the folder, and Stuck to, listing the open
  windows. Search's Notes mode offers the same.
- A workspace given a folder; notes written there go into it.
- Hold a note on the board and drop it on a folder, a day or the trash.
- Folders as real folders on disk, with existing notes moved in on update.

**Proves:** a project started on the board before its work, and a week of
notes sorted without opening a menu.

## Milestone 3: stuck to windows

Needs Kadunce (`DESKTOP.md` § Needs).

**Status:** being built, 7 October: Gooseberry tells the desktop which windows
have notes (`DESKTOP.md` § Stuck notes on the bus) and shows them over the
window; Shuffle's title bar draws the dot and Kadunce's Spread the stacks.
The checks by hand are in `TESTING.md`.

- A dot in the window's title bar; a tap shows its notes where they were
  placed.
- In Spread, each card's notes as a stack on its corner; a tap fans them.
- A note carried to the top edge opens Spread and is stuck to the card it is
  dropped on.
- Notes come back on their document when it opens again.

**Proves:** a week of daily use on Shuffle where notes on windows never need
putting away by hand.

## Milestone 4: with Search

Waits on Tettegouche taking up its need in `DESKTOP.md`.

- Notes found in Search, each saying its folder and window.

## Milestone 5: sync and the phone

- Notes on several computers, through whatever folder sync the person already
  uses, with two copies of a note edited apart offered to merge rather than
  left side by side.
- A mobile companion: capture, the planner and reminders on a phone, reading
  the same folder. On Plasma Mobile it is the same program; on Android, a port
  of the core and screens, with the phone's own widget, share sheet and alarms.
  A reminder rings once, not on every device.

## Milestone 6: pen

- Handwritten notes, with the pen palette and an eraser.
- Handwriting read on the computer, so ink is found by search.

**Proves:** handwritten notes from daily use are found by a word in them.

## Later

- The notes tool for Split Rock: find, read and add, and next step from a
  note.

## Not planned

- An account, a cloud service or a sync service of Gooseberry's own.
- Long documents and rich formatting. Gooseberry is for notes; a word processor
  is for documents.
- Shared or collaborative notes.

## Open decisions

None open. Settled ones are in `DECISIONS.md`; a new one is added here with two
options, a recommendation and what neither covers.
