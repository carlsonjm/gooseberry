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

Waits on § Open decisions, When a card's window closes.

- Capture defaults to the card in front.
- Notes come back with their card's document.
- A card's notes kept in its Stack, shown only as a count until asked for.

**Proves:** a week of daily use on Shuffle where notes on cards never need
putting away by hand.

## Milestone 4: with Search

Waits on Tettegouche taking up its need in `DESKTOP.md`.

- Notes found in Search, typed and handwritten, each saying where it belongs.

## Later

- Next step with Split Rock: a checklist, a reminder or a better home, offered
  from a note.
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

### When a card's window closes

Holds up Milestone 3.

- **The note follows the document.** Gooseberry remembers the application and
  the document's name or path, and the note comes back when that document
  opens again. Costs: a renamed or moved document loses its notes to Loose,
  where they wait with where they came from.
- **The note turns Loose.** When the window closes, its notes go to the board
  under Loose, marked with where they were. Costs: notes never come back on
  their own, and Loose fills up.
- **Other.** A note about an application rather than one document, such as a
  terminal.

**Recommendation:** the note follows the document, falling back to Loose with
where it came from.

### Gooseberry's folder and Split Rock's notebook

Holds up Milestone 0's folder, and next step with Split Rock.

- **Two folders.** Gooseberry keeps its notes; Split Rock keeps its notebook
  about the computer. Split Rock reads a note only when opened from it.
  Costs: two places on disk, though Search shows both.
- **One folder.** Split Rock's notebook gains a notes part Gooseberry owns, and
  the assistant can see every note. Costs: the two components share a layout
  and its rules, and the assistant sees more than the note in hand.
- **Other.** Where an answer kept from Split Rock lands.

**Recommendation:** two folders.
