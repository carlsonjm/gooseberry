# Concept

What Gooseberry is for, who it is for, and the rules it keeps. The plan and the
decisions still open are in `ROADMAP.md`; settled rulings and what they rejected
are in `DECISIONS.md`. What the card workspace and Search add is in
`DESKTOP.md`.

## The person

Robin works on a Linux tablet, mostly by touch, sometimes with a pen and
sometimes with a keyboard. Ideas, reminders and half-plans arrive in the middle
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

1. **Capture.** One tap opens a sheet from the bottom of the screen with the
   cursor already in it. Type, or switch to Pen. It saves as Robin writes. By
   default the note belongs to whatever Robin is working on; one tap changes
   that. Going back to the work closes the sheet.
2. **Notes on the work.** A note that belongs to a window comes back with that
   window. How it is shown there, beside the window or behind it, is open
   (`ROADMAP.md` § Open decisions).
3. **The board.** Every note in one place, gathered without Robin filing
   anything: Loose, Today, Tucked away, each window or document that has notes,
   each project, and the workspace.
4. **The planner.** A note with a time goes on the planner, day by day, and
   reminds Robin when it comes due. A note without one is an idea and needs no
   date.

## A note

- **What it holds:** typed text, a checklist, or handwriting. A handwritten
  note is read as text so it can be found; the ink is what Robin sees.
- **A colour,** chosen with one tap, for Robin's own sorting. Colour carries no
  meaning Gooseberry acts on.
- **Where it belongs:** a window or document, a project, the workspace, or
  Loose. Belonging is a label, never a folder Robin has to choose first.
- **A reminder,** optional: a time, or "next time this opens".
- **No title.** The first words are the title.

## Rules

- **Nothing before the note.** No title, folder, notebook or save comes between
  the tap and the writing.
- **Never lost.** A note is kept as it is written, through a crash or a
  restart. Removing one sends it to the desktop's trash.
- **No room taken unasked.** A note never takes space on screen that Robin did
  not ask for, and putting it away never costs an extra tap. Tucking a note away
  keeps it, findable, on the board.
- **The work stays in front.** Gooseberry never covers what Robin is working on
  with something that has to be closed.
- **Big and touchable.** 44 px touch targets, room to write with a finger or a
  pen, and every action reachable by touch alone.
- **Plain words.** Belongs to, Tuck away, Remind. No jargon in the interface.

## What Gooseberry relies on

| Need | Approach | Notes |
| --- | --- | --- |
| Keeping notes | Open: plain files or a database (`ROADMAP.md` § Open decisions) | Either way, saved as written |
| Reminders | The desktop's standard notifications | Whatever shows notifications on that desktop shows them |
| Handwriting | Qt's pen and touch input; recognition on the computer | Nothing leaves the computer to be read |
| Knowing what Robin is working on | Kadunce's read-only workspace snapshot, where Kadunce runs | Without Kadunce, the window in front as the desktop reports it |
| Being found | Tettegouche's Search, where installed | `DESKTOP.md` § Needs |
| Turning a note into a next step | Split Rock, where installed and set up | Optional; Gooseberry needs no assistant |

## Risks

- **Belonging outlives windows badly.** A window's identity lasts only while it
  is open. A note tied to it has to find its way back when the document opens
  again, or it becomes clutter (`ROADMAP.md` § Open decisions).
- **Room is not Gooseberry's to take.** On the card workspace, Kadunce owns
  every window's size and place. Anything shown beside a card needs Kadunce to
  offer room, which it does not yet (`DESKTOP.md`).
- **Handwriting recognition on the computer** may be too weak or too heavy for a
  tablet. Ink stays the note either way; only finding it depends on the reading.
- **Habit.** A notes app that loses one thought, or asks one question too many,
  is abandoned. Speed to the cursor is the product, not polish.
