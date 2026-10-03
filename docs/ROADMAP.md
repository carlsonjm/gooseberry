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

Waits on § Open decisions 1 and 3.

- Capture defaults to the card in front.
- Notes come back with their card's document.
- Notes on a card shown the way decision 1 chooses.

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
- Notes on several computers, through whatever folder sync the person already
  uses, if notes are plain files.

## Not planned

- An account, a cloud service or a sync service of Gooseberry's own.
- Long documents and rich formatting. Gooseberry is for notes; a word processor
  is for documents.
- Shared or collaborative notes.

## Open decisions

The maintainer's to make. Each has two options, a recommendation and what
neither covers.

### 1. On the card: pinned beside it, or stacked behind it

Holds up Milestone 3 and any request to Kadunce.

- **Stacked behind the card.** The card's notes are a Gooseberry window in the
  card's Stack. They take no room and there is nothing to put away: stepping
  back to the card is putting them away. Side by side, when wanted, is Bento,
  which already exists. Costs: no glance at a note without a step or a Bento
  pair; a Stack is made by hand, so Robin drops the notes on the card once;
  one notes window for each card that has notes.
- **Pinned beside the card** (the first mock-up). A slim rail of stickies, with
  the card made narrower to fit. Notes are in view while working. Costs: needs
  Kadunce to offer room beside the card, which it does not yet; and a rule that
  puts the rail away by itself, such as folding when the card's own content is
  touched, so it never needs a tap to close.
- **Other.** A floating sticky over the card is not offered: it covers the
  work, and every one needs moving or closing, which is the extra tap this
  product exists to avoid.

**Recommendation:** stacked behind the card first. It needs nothing new from
Kadunce, takes no room, and puts itself away. Build the rail only if daily use
shows that glancing at a note while working matters, with folding on touch
designed in from the start.

Whichever is chosen, two rules hold: a card's notes show only as a count until
asked for, and nothing opens a note on its own except Robin writing one.

### 2. How notes are kept

Holds up Milestone 0.

- **Plain files.** A folder of Markdown notes, with ink saved beside each as a
  drawing. KDE's file index finds them, other apps open them, folder sync can
  carry them, and Split Rock can read them. Costs: the planner reads every note
  with a time from a folder rather than asking a database.
- **A database.** One file Gooseberry owns. The planner and board are simpler
  to make fast. Costs: no other app can read the notes, KDE's file index cannot
  find their words, and Split Rock cannot read them as files.
- **Other.** Whether Gooseberry's folder and Split Rock's notebook are one
  folder or two.

**Recommendation:** plain files. They match how Split Rock keeps its notebook,
and they let Search find notes before Tettegouche offers a Notes tab.

### 3. When a card's window closes

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
