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

1. **Capture.** One tap opens a card in the middle of the screen, the size of
   the desktop's search, with the cursor already in it; with the on-screen
   keys up it keeps above them. Type, or switch to Pen. It saves as Robin
   writes. By default the note belongs to whatever Robin is working on; one tap
   changes that. All notes grows the card into the board. Going back to the
   work puts the card away.
2. **Notes on the work.** A window's notes wait behind it in its Stack, shown
   only as a count until Robin steps to them. Stepping back to the work puts
   them away.
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
| Keeping notes | A folder of plain Markdown files, ink beside each as a drawing | Other apps, the file index and folder sync can all read them |
| Reminders | The desktop's standard notifications | Whatever shows notifications on that desktop shows them |
| Handwriting | Qt's pen and touch input; recognition on the computer | Nothing leaves the computer to be read |
| Knowing what Robin is working on | Kadunce's read-only workspace snapshot, where Kadunce runs | Without Kadunce, the window in front as the desktop reports it |
| Being found | Tettegouche's Search, where installed | `DESKTOP.md` § Needs |
| The assistant | Split Rock, where installed and set up, using Gooseberry as a tool | Optional; Gooseberry needs no assistant |

## Risks

- **Finding the document again.** A window's identity lasts only while it is
  open, so a card's notes come back by the document's name or path. A renamed
  or moved document leaves its notes in Loose (`DECISIONS.md`).
- **A Stack is made by hand.** Kadunce owns every window's place, so Robin
  drops a card's notes on the card once to stack them (`DESKTOP.md`).
- **Handwriting recognition on the computer** may be too weak or too heavy for a
  tablet. Ink stays the note either way; only finding it depends on the reading.
- **Habit.** A notes app that loses one thought, or asks one question too many,
  is abandoned. Speed to the cursor is the product, not polish.
