# With cards and Search

What Gooseberry adds where Kadunce's card workspace or Tettegouche's Search is
present, what it reads from them, and what it would need from them. Without
either, Gooseberry is complete on its own; each is found at run time and never
required.

## What Gooseberry offers any desktop

Milestone 0 needs nothing from Kadunce, Tettegouche or Shuffle. On any Plasma
desktop it reads two things from the desktop itself: the window in front, which
a note belongs to by default, and the current workspace's name. Both come from
Plasma's own window list, which Plasma opens to Gooseberry because its desktop
file asks for it. Where the desktop says nothing, a note starts Loose.

A dock, a panel or Search starts Gooseberry by its desktop file,
`io.github.carlsonjm.Gooseberry`:

| To | Start | Shuffle's dock |
| --- | --- | --- |
| Capture a note | The desktop file, or `gooseberry` | A tap |
| Open the board | Its Open the board action, or `gooseberry --board` | A hold |

Every start reaches the one running Gooseberry, which comes up with the
session.

## Kadunce: notes that belong to a card

### What Kadunce offers today

Kadunce publishes a versioned, read-only workspace snapshot on the session bus
(its `docs/TETTEGOUCHE-CONTEXT.md`, version 1). From it Gooseberry can read:

- the window in front, its application and title, and whether it is a card;
- every application window on the current desktop, with its card membership;
- the Active card and, when it is in a Stack, the Stack's other cards in order.

That is enough for capture to default to "This card", and for a Gooseberry
window that Robin has placed in a Stack to know which card's notes to show.

A Stack and a Bento pair are made by hand in Kadunce. Robin can already hold a
Gooseberry window and let it go on a card to stack them, or drag it to an edge
to sit beside a card. Gooseberry cannot make either happen itself.

### What Kadunce does not offer

- **Room beside a card.** Kadunce makes room on the Active card for the keys
  and for nothing else. Gooseberry stacks notes behind the card instead, and
  asks for room only if a rail beside the card is ever revisited.
- **Notes drawn on cards in Spread.** A count of notes on a card's corner, and
  carrying a note from one card to another in Spread, are Kadunce's to draw and
  answer.
- **Arranging on request.** Kadunce's outside interface is read-only by design.
  Stacking a note with its card for Robin would need Kadunce's request
  interface, which is not scheduled.
- **Identity that outlives a window.** A card's identity lasts as long as its
  window. A note that should come back with a document needs an anchor
  Gooseberry keeps itself, such as the application and the document's name or
  path.

## Tettegouche: notes in Search

### What Tettegouche offers today

Search draws only from a closed list of local sources: applications, KDE's file
index and recent documents, settings, a calculator and unit conversion (its
`docs/SEARCH-CONTRACT.md`). If notes are kept as plain files, KDE's file index
already finds them by name, and by content where indexing allows, as files.
Notes are kept that way, so Search finds them before it offers anything more;
Tettegouche is the bridge that makes them look like notes.

### What Tettegouche does not offer

- A Notes tab, with each note's colour, what it belongs to and when it was
  written, opening the note rather than a file.
- Matching handwritten notes by the text read from their ink.

## Split Rock

Where Split Rock is installed and set up, Gooseberry is one of its tools: the
assistant can find notes, read one, and add one when Robin asks, and says when
it reads one. From a note, "next step" sends only that note, and offers a
checklist, a reminder or a better home; nothing changes until Robin chooses.
What Robin keeps from the assistant becomes notes here too (`DECISIONS.md`). Gooseberry
needs no assistant, and offers none of its own.

## Needs

Each is for that component to take up in the open, under its own contracts.
None is assumed.

| Component | Need | Holds up |
| --- | --- | --- |
| Kadunce | A way to show a companion's count on a card in Spread | Notes in Spread |
| Kadunce | Its request interface, to stack a companion with a card | Stacking without a hand gesture |
| Tettegouche | A notes source for Search, with a Notes tab | Notes in Search |
| Tettegouche | A Notes door on Search's first screen, behind a checkbox, shown only when Gooseberry is installed | Opening Gooseberry from Search |
| Split Rock | Use Gooseberry's notes tool where present | The notes tool |
| Shuffle Keyboard | Say how tall the keys are, or leave room above them, for a surface of the desktop's own such as the capture sheet | Writing on the sheet with the keys up, if the tablet shows the keys covering it |
