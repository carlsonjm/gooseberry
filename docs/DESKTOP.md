# With cards and Search

What Gooseberry adds where Kadunce's card workspace or Tettegouche's Search is
present, what it reads from them, and what it would need from them. Without
either, Gooseberry is complete on its own; each is found at run time and never
required.

## What Gooseberry offers any desktop

Milestone 0 needs nothing from Kadunce, Tettegouche or Shuffle. On any Plasma
desktop it reads two things from the desktop itself: the window in front, which
a note belongs to by default, and the current workspace's name. From Milestone
1 it also notices a window opening on a document, for "next time this opens",
and shows reminders through the desktop's standard notifications
(`org.freedesktop.Notifications` on the session bus), so whatever shows
notifications on that desktop shows them. Both come from
Plasma's own window list, which Plasma opens to Gooseberry because its desktop
file asks for it. Where the desktop says nothing, a note starts Loose.

A dock, a panel or Search starts Gooseberry by its desktop file,
`io.github.carlsonjm.Gooseberry`:

| To | Start |
| --- | --- |
| Open the board, as an ordinary window | The desktop file, `gooseberry`, its Board action, All notes, or `gooseberry --board` |
| Capture a note, on the quick-note card | Its Capture action, New note, or `gooseberry --capture` |

The quick-note card is not a window: a desktop that waits for an opened
application's window does not wait for it. A search that hosts the quick note
itself uses the interface in § Tettegouche instead.

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

### The quick note in Spread

Kadunce lets an application hold Spread's centre as a companion guest (its
`docs/TETTEGOUCHE-CONTEXT.md` § Companion guests, protocol 1), and the
quick-note card does. Each time the card opens, Gooseberry reads Kadunce's
workspace snapshot:

| Kadunce shows | The card |
| --- | --- |
| Spread (`cardStage.presentation` is `cardLine`) | Asks for the centre if `companionGuestProtocolVersion()` is exactly `1`, and if given it is drawn at the reply's `card` rectangle; a press outside the card is Spread's |
| An Active card (`active`) | Asks for nothing and stands on its own, as without Kadunce |
| Anything else, or no Kadunce, or an older one | Stands on its own, as without Kadunce |

The card asks with `beginCompanionGuest(its unique bus name, "/CompanionGuest",
"io.github.carlsonjm.Gooseberry.CompanionGuest")`, and answers there:

| Method | Gooseberry |
| --- | --- |
| `dismissGuest()` | Another guest took the centre: the card closes, the note kept. Nothing is asked back |
| `completeGuestLaunch(s requestToken)` | The board's window has taken the card's place: the card fades out over 190 ms where it stood |

A card that closes on its own, by Done, a tap outside or Esc, calls
`endLauncherGuest()` once.

**All notes in Spread.** The card calls `setLauncherGuestExpanded(true)` from
the same bus name; the neighbours fade and the card grows to the reply's
`active` rectangle. It then calls
`prepareLauncherGuestLaunch(["io.github.carlsonjm.Gooseberry.desktop"], token)`,
opens the board, and fades on `completeGuestLaunch(token)`. Where Kadunce
refuses to grow the card, the guest ends and the card shows the board itself,
as without Kadunce. Where it will not wait for the board, the guest ends, the
board opens, and the card fades once the board has drawn. Where the board has
not arrived after 10 seconds, the card calls `cancelLauncherGuestLaunch()` and
`endLauncherGuest()` and stays, showing the board itself.

**All notes over an Active card.** No guest: the card grows, the board opens,
and the grown card stays up until the snapshot reports the board's window,
application `io.github.carlsonjm.Gooseberry`, as `cardStage.selectedCardId`,
which is Kadunce putting it in the Active card's place. Then it fades. Gooseberry
reads the snapshot again on each `workspaceContextChanged`, and stops waiting
after 10 seconds, leaving the grown card showing the board itself.

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

### The quick note in Search

Search hosts the quick note as one of its modes (`DECISIONS.md`). It draws the
note pad in its own window and keeps nothing: Gooseberry keeps the note, and
offers it on the session bus. Search loads none of Gooseberry's code.

- **Service:** `io.github.carlsonjm.gooseberry`, owned by the one running
  Gooseberry. A call before Gooseberry has started starts it, from its bus
  service file, and is answered once it is ready.
- **Object:** `/QuickNote`. **Interface:**
  `io.github.carlsonjm.Gooseberry.QuickNote`.
- **Version:** `ProtocolVersion()` returns `1`. A caller uses the interface only
  when the number is one it knows. A method or a key added later, which a
  version-1 caller can ignore, keeps the number; any other change raises it.

The quick note in Search is a session of its own, apart from the note on
Gooseberry's card. Every method that changes the note writes it to the folder
before it returns, and every method returns the note as Gooseberry keeps it, a
dictionary of strings to variants:

| Key | Type | What it is |
| --- | --- | --- |
| `open` | `b` | A note is open in this session |
| `id` | `s` | The note's id; empty until it has text and is kept |
| `kept` | `b` | The note is in the folder |
| `text` | `s` | The note's text |
| `colour` | `s` | Its colour's name |
| `colours`, `colourHexes` | `as`, `as` | The five colours in order, and their colours as `#RRGGBB` |
| `folder` | `s` | The folder it is kept in, by name; empty for Inbox. From Milestone 2 |
| `folderLabel` | `s` | That folder in the words Folder shows on the card. From Milestone 2 |
| `folders` | `av` | What Folder offers, in the card's order: the workspace's folder, Inbox, then the rest; each a dictionary: `name` (empty for Inbox), `label`, `chosen` (`b`) and `workspace` (`b`, the workspace's own). From Milestone 2 |
| `stuck` | `b` | The note is stuck to `window`. From Milestone 2 |
| `windows` | `av` | What Stuck to offers, in the card's order: the note's own window when it is no longer open, then the open windows, the one in front first; each a dictionary: `window`, `app`, `label` (the words to show) and `chosen` (`b`). From Milestone 2 |
| `window`, `app`, `workspace` | `s` | The window the note was written on or stuck to, its application, and the workspace it was written on. `app` from Milestone 2 |
| `belongs`, `project`, `choices` | `s`, `s`, `av` | Belongs to, for a caller of Milestone 1: `belongs` is `window` when stuck, else `project` in a folder and `loose` in Inbox; `project` is the folder; `choices` offers the window, then Inbox as `loose` and each folder as a `project`, each a dictionary: `kind`, `label`, `project`, `chosen` (`b`) |
| `readOnly` | `b` | The note or folder is kept by a newer Gooseberry and is not changed |
| `problem` | `s` | Why the last change could not be kept; empty when it was |
| `remind` | `s` | The reminder: an ISO 8601 time with its offset, `opens` for the next time the note's window opens, or empty for none. From Milestone 1 |
| `remindLabel` | `s` | The reminder in the words Remind shows on the card, such as "Tomorrow 9:00 AM"; empty for none. From Milestone 1 |
| `remindChoices` | `av` | What Remind offers, in the card's order, each a dictionary: `kind` (`later`, `evening`, `tomorrow`, `opens` or `pick`), `label` (the words to show) and `time` (ISO 8601, for the first three; empty otherwise). `opens` only for a note written on a window; `pick` is for the search's own way to choose a day and time. From Milestone 1 |
| `checklist` | `b` | The note is a checklist. From Milestone 1 |
| `lines` | `av` | Every line of the text in order, each a dictionary: `text` (without the box), `item` (`b`, it has a box) and `checked` (`b`). From Milestone 1 |

| Method | What it does |
| --- | --- |
| `ProtocolVersion() → u` | The version, `1` |
| `Start() → a{sv}` | Starts a note, stuck to the window in front and kept in the workspace's folder, or resumes the one the last `Start` began and nothing has finished |
| `State() → a{sv}` | The note as it is |
| `SetText(s text) → a{sv}` | Sets the text. Search sends it at each pause, as Gooseberry's card writes; the first text keeps the note |
| `SetColour(s name) → a{sv}` | One of the five colours; another name is refused |
| `SetBelongs(s kind, s project) → a{sv}` | Belongs to, for a caller of Milestone 1: `window` sticks the note to its window; `project` keeps it in that folder, made when new, `workspace` in the workspace's folder and `loose` in Inbox, each unsticking it |
| `SetFolder(s name) → a{sv}` | Keeps the note in that folder, made when new; empty is Inbox. A name that cannot be a folder's is refused. From Milestone 2 |
| `SetStuck(s window, s app) → a{sv}` | Sticks the note to that window, by its document's name and application, as `windows` gives them; an empty window unsticks it. The folder stays. From Milestone 2 |
| `SetReminder(s when) → a{sv}` | Sets the reminder: an ISO 8601 time, `opens`, or empty to take it away. A new reminder replaces the old one and is shown once more. `opens` on a note written on no window, and anything else that is not a time, are refused. From Milestone 1 |
| `SetChecklist(b on) → a{sv}` | Turns the text into a checklist, as Checklist on the card does, or back into plain text. From Milestone 1 |
| `SetLineChecked(u line, b checked) → a{sv}` | Ticks or unticks the item on that line, counted from 0 as in `lines`; a line that is not an item is refused. From Milestone 1 |
| `Done() → a{sv}` | Finishes the note, as Done on the card; a note left empty goes to the trash. The next `Start` begins a new one |
| `TuckAway() → a{sv}` | Tucks the note away and finishes it |
| `Remove() → a{sv}` | Moves the note to the desktop's trash and finishes it; nothing is deleted |
| `UndoRemove() → a{sv}` | Brings back the note `Remove` sent to the trash; the reply's `restored` says whether it did |
| `OpenBoard(s noteId, s requestToken) → b` | Opens the board as an ordinary window, on the place that note sits in, or on Today for an empty id |

| Signal | When |
| --- | --- |
| `Changed(a{sv})` | The open note changed other than by a call here: on the board, on Gooseberry's card, or by another program writing its file |
| `BoardShown(s requestToken)` | The board's window, asked for by `OpenBoard` with that token, has drawn its first frame |

**Added in Milestone 1, within version 1.** The keys `remind`,
`remindLabel`, `remindChoices`, `checklist` and `lines`, and the methods
`SetReminder`, `SetChecklist` and `SetLineChecked`. A version-1 caller that
does not know them ignores the keys and never calls the methods, and keeps
working as before. A search that offers them shows Remind and Checklist as the
card does: Remind offers `remindChoices` and sends the chosen `time`, or
`opens`; Checklist calls `SetChecklist`, each box calls `SetLineChecked`, and
words typed in an item go with `SetText` as any typing does, the text keeping
its `- [ ] ` and `- [x] ` markers. The reminder itself is Gooseberry's to show,
wherever the note was written. `Changed` also tells a reminder or a tick made
elsewhere.

**Added in Milestone 2, within version 1.** The keys `folder`,
`folderLabel`, `folders`, `stuck`, `windows` and `app`, and the methods
`SetFolder` and `SetStuck`. A caller of Milestone 1 keeps working: its Belongs
to row now offers the window, Inbox and the folders, and each choice does what
the row says. A search that offers them shows the card's two chips in place of
Belongs to: Folder offers `folders` and a field for a new one, sending
`SetFolder`; Stuck to offers `windows` and "Don't stick to a window", sending
`SetStuck`.

For All notes, Search calls `OpenBoard` with a token of its own and waits for
`BoardShown` with that token: from then the board's window, whose application
is `io.github.carlsonjm.Gooseberry`, is on screen, and Search can hand the
moment to Kadunce so the window takes the card's place.

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
| Kadunce | A stack of a card's notes on its corner in Spread, from Gooseberry's notes | Stuck notes in Spread (Milestone 3) |
| Shuffle | A notes dot in Shuffle's title bar for windows with notes, answering a tap; the current title bar cannot show one, so it needs its own drawing code | The dot (Milestone 3) |
| Kadunce | Accepting a note carried to the top edge: opening Spread and telling Gooseberry which card it was dropped on | Sticking by drag (Milestone 3) |
| Kadunce | Its request interface, to stack a companion with a card | Stacking without a hand gesture |
| Tettegouche | A notes source for Search, with a Notes tab | Notes in Search |
| Tettegouche | The quick note as one of Search's modes, over § The quick note in Search, behind a checkbox and only when Gooseberry is installed | Writing a note from Search |
| Split Rock | Use Gooseberry's notes tool where present | The notes tool |
