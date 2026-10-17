# Concept

What Gooseberry is for, who it is for, and the rules it keeps. What it does not
do yet, and will not do, is in `LIMITATIONS.md`. Settled decisions, and what
they turned down, are in `DECISIONS.md`. What the card workspace and Search add
is in `DESKTOP.md`.

## The person

Robin works on a Linux tablet, mostly by touch and sometimes with a
keyboard. Ideas, reminders and half-plans arrive in the middle
of other work: a bug noticed while reading code, a question for someone, a
grocery item, a wallpaper idea.

Robin has tried the notes apps Linux offers. Each wants a notebook chosen, a
title typed or a file saved before the thought is kept, and none knows what
Robin was doing when the thought arrived. By the time the app is ready, the
idea has gone.

Every decision here is tested against that moment first: Robin, mid-task, with
a thought that will not last ten seconds.

## What it is

Four places, one set of notes.

1. **Capture.** One tap opens a card in the middle of the screen, the size of
   the desktop's search, with the cursor already in it; with the on-screen
   keys up it keeps above them. It saves as Robin writes. By default the note
   is stuck to whatever Robin is working on, and kept in the workspace's
   folder; one tap changes either. All notes grows the card into the board.
   Going back to the work puts the card away.
2. **Notes on the work.** A window with notes shows a dot in its title bar,
   with a count when there are several. A tap brings them up where Robin left
   them; another tap puts them away. Going back to the window folds them into
   a small dot where they stand, and a tap on it opens them, ready to type.
3. **The board.** Every note in one place: Today, Inbox and each folder, the
   windows that have notes, and Tucked away. A held note drops on a folder.
4. **The planner.** A note with a time goes on the planner, day by day, and
   reminds Robin when it comes due. A note without one is an idea and needs no
   date.

## A note

- **What it holds:** typed text, or a checklist.
- **A colour,** chosen with one tap, for Robin's own sorting. Colour carries no
  meaning Gooseberry acts on.
- **Where it is kept:** one folder, Inbox unless Robin or the workspace says
  otherwise. Gooseberry fills it in, so Robin has nothing to answer first.
- **What it is stuck to:** one window or document, or nothing.
- **A reminder,** optional: a time, or "next time this opens".
- **No title.** The first words are the title.

## Rules

- **Nothing before the note.** No title, folder, notebook or save comes between
  the tap and the writing.
- **Never lost.** A note is kept as it is written, through a crash or a
  restart. Removing one sends it to the desktop's trash.
- **No room taken unasked.** A note takes screen space only when Robin asks for
  it, and putting it away costs no extra tap. A tucked-away note stays on the
  board, where Robin can find it.
- **The work stays in front.** Gooseberry does not cover Robin's work with
  anything that has to be closed.
- **Big and touchable.** 44 px touch targets, room to type, and every action
  reachable by touch alone.
- **Plain words.** Folder, Stuck to, Tuck away, Remind. No jargon in the
  interface.

## What Gooseberry relies on

| Need | Approach | Notes |
| --- | --- | --- |
| Keeping notes | A folder of plain Markdown files | Other apps, the file index and folder sync can all read them |
| Reminders | The desktop's standard notifications | Whatever shows notifications on that desktop shows them |
| Knowing what Robin is working on | Plasma's own window list: the window in front and the workspace's name | Works the same with or without Kadunce. Kadunce's snapshot only reports what Kadunce is showing, which places the quick note (`DESKTOP.md`) |
| Being found | Tettegouche's Search, where installed | `DESKTOP.md` § Needs |
| The assistant | Split Rock, where installed and set up, using Gooseberry as a tool | Optional |

## Risks

- **Finding the document again.** A window's identity lasts only while it is
  open, so a card's notes come back by the document's name. A renamed
  document leaves its notes waiting under the old name, on the board and in
  their folder (`DECISIONS.md`).
- **The dot needs a title bar the desktop draws.** An application that draws
  its own top bar has no dot there; its notes are on the board and, with
  Kadunce, in Spread, and once brought up they fold to their dot on the window
  (`DESKTOP.md`).
- **Habit.** A notes app that loses one thought, or asks one question too many,
  is abandoned. How fast the cursor appears matters more than polish.
