# With cards and Search

What Gooseberry adds where Kadunce's card workspace or Tettegouche's Search is
present, what it reads from them, and what it would need from them. Without
either, Gooseberry is complete on its own; each is found at run time and never
required.

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
  and for nothing else. A sticky rail beside a card needs Kadunce to offer that
  room to a companion.
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

### What Tettegouche does not offer

- A Notes tab, with each note's colour, what it belongs to and when it was
  written, opening the note rather than a file.
- Matching handwritten notes by the text read from their ink.

## Split Rock

Where Split Rock is installed and set up, a note can offer "next step": turn it
into a checklist, a reminder or a better home. Split Rock sees only the note it
was opened from, and nothing changes until Robin chooses. Gooseberry needs no
assistant, and offers none of its own.

## Needs

Each is for that component to take up in the open, under its own contracts.
None is assumed, and none is asked for before `ROADMAP.md` § Open decisions are
settled.

| Component | Need | Holds up |
| --- | --- | --- |
| Kadunce | Room beside the Active card for a companion, offered and withdrawn by Kadunce | Notes pinned beside a card, if chosen |
| Kadunce | A way to show a companion's count on a card in Spread | Notes in Spread |
| Kadunce | Its request interface, to stack a companion with a card | Stacking without a hand gesture |
| Tettegouche | A notes source for Search, with a Notes tab | Notes in Search |
