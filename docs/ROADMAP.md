# Roadmap

What is planned for Gooseberry, in order, and what is not. Each milestone ends
with what it has to prove before the next one starts. Decisions still open are
at the end, each named with what it holds up.

**Where it sits in the suite:** listed in the suite record, not yet scheduled.

## Milestone 0: capture and keep

Gooseberry on its own, on any Plasma desktop.

- The capture sheet: one tap, cursor ready, saved as written, Belongs to and
  colour in one tap each, Done by going back to the work.
- The board: Loose, Today, Tucked away, by window or document, by project, by
  workspace.
- Tuck away and bring back. Removing sends a note to the trash.
- Notes kept through a crash, a logout and a restart.

**Proves:** a week of the maintainer's daily use on the tablet without a lost
note, and with capture fast enough that nothing is written elsewhere instead.

## Milestone 1: the planner

- Reminders: a time, or next time a document opens.
- The planner: today's timed notes, the days ahead, and what is done.
- Checklists.
- Reminders through the desktop's standard notifications.

**Proves:** a week of the maintainer's days planned in Gooseberry, every
reminder on time, and none shown twice.

## Milestone 2: pen

- Handwritten notes, with the pen palette and an eraser.
- Handwriting read on the computer, so ink is found by search.

**Proves:** handwritten notes from daily use are found by a word in them.

## Milestone 3: with cards

- Capture defaults to the card in front.
- Notes come back with their card's document.
- A card's notes kept in its Stack, shown only as a count until asked for.

**Proves:** a week of daily use on Shuffle where notes on cards never need
putting away by hand.

## Milestone 4: with Search

Waits on Tettegouche taking up its need in `DESKTOP.md`.

- Notes found in Search, typed and handwritten, each saying where it belongs.

## Later

- The notes tool for Split Rock: find, read and add, and next step from a
  note.
- Notes on cards in Spread, and carrying a note between cards, once Kadunce
  offers it.
- A rail of stickies beside the card, only if daily use asks for it.
- Notes on several computers, through whatever folder sync the person already
  uses.

## Not planned

- An account, a cloud service or a sync service of Gooseberry's own.
- Long documents and rich formatting. Gooseberry is for notes; a word processor
  is for documents.
- Shared or collaborative notes.

## Open decisions

The maintainer's to make. Each has two options, a recommendation and what
neither covers. Settled ones move to `DECISIONS.md`.

### Where the assistant's memory lives

Holds up the notes tool's add, and Split Rock's own `docs/MEMORY.md`, which is
Split Rock's to change. The maintainer leans toward Gooseberry.

- **Kept items in Gooseberry.** What the person keeps from the assistant, and
  memory items it needs to call on later, become notes: on the board, found by
  Search, with a place of their own under Belongs to. Split Rock keeps the
  record of the computer and its page about the person. Costs: Split Rock must
  still keep its own notes folder for desktops without Gooseberry, so it has
  two places to write and one to choose at run time.
- **Kept items stay in Split Rock's notebook,** as its `docs/MEMORY.md` has
  them now. Costs: the person's notes are in two places, and what the assistant
  keeps never shows on the board or the planner.
- **Other.** Whether the page about the person moves too.

**Recommendation:** kept items in Gooseberry when it is installed, Split Rock's
own folder when not. It puts what the person keeps where the person already
looks.
