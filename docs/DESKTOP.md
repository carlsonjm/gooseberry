# With cards and Search

What Gooseberry adds where Kadunce's card workspace or Tettegouche's Search is
present, what it reads from them, and what it would need from them. Without
either, Gooseberry is complete on its own; each is found at run time and never
required.

## What Gooseberry offers any desktop

Milestone 0 needs nothing from Kadunce, Tettegouche or Shuffle. On any Plasma
desktop it reads two things from the desktop itself: the window in front, which
a new note is stuck to by default, and the current workspace's name, whose
folder a new note is kept in. From Milestone 1 it also notices a window opening
on a document, for "next time this opens", and shows reminders through the
desktop's standard notifications (`org.freedesktop.Notifications` on the
session bus), so whatever shows notifications on that desktop shows them. Both
come from Plasma's own window list, which Plasma opens to Gooseberry because
its desktop file asks for it. Where the desktop says nothing, a note starts
stuck to nothing, in the workspace's folder or Inbox.

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
(its `docs/TETTEGOUCHE-CONTEXT.md`, version 1): the window in front, every
application window with its card membership, the Active card and its Stack.
Gooseberry reads from it only what Kadunce is showing, Spread or an Active
card, and which card is selected, for where the quick-note card stands
(below). The window a note is stuck to comes from Plasma's window list, with
or without Kadunce.

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
  and for nothing else. Gooseberry shows a card's notes over its window and as
  a stack in Spread instead, and asks for room only if a rail beside the card
  is ever revisited.
- **Stacking on request.** Kadunce takes a request to open an application at
  a point (its `docs/REQUESTS.md`), but none to stack a window with a card.
  Stacking a Gooseberry window with its card for Robin would need one.
- **Identity that outlives a window.** A card's identity lasts as long as its
  window. A note that comes back with a document uses an anchor Gooseberry
  keeps itself: the application and the document's name, read from the
  window's title.

## Tettegouche: notes in Search

### What Tettegouche offers today

Search draws only from a closed list of local sources: applications, KDE's file
index and recent documents, settings, a calculator and unit conversion (its
`docs/SEARCH-CONTRACT.md`). If notes are kept as plain files, KDE's file index
already finds them by name, and by content where indexing allows, as files.
Notes are kept that way, so Search finds them before it offers anything more;
Tettegouche is the bridge that makes them look like notes.

From Milestone 4, Search also asks Gooseberry for the notes with the words
typed (`Find`, below) and lists them among its results, each with its colour
and its folder or window, opening the note rather than a file.

### What Tettegouche does not offer

- A Notes tab of its own, listing every note by when it was written.

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
| `Find(s words, u limit) → av` | The notes with those words in their text, their folder's name or the window they are stuck to, as the board's search finds them, tucked away ones included; the most recently changed first, and no more than `limit` (0 for all). Each a dictionary: `id`, `title` (the first words), `excerpt` (the line with the words, when they are not in the title; else empty), `colour`, `colourHex`, `folder`, `folderLabel`, `stuck` (`b`), `window` and `app` (when stuck), `tucked` (`b`) and `changed` (ISO 8601). It changes nothing and opens nothing. From Milestone 4 |
| `OpenNote(s noteId) → b` | Opens that note on Gooseberry's own card, as a tap on the board does; false for a note that is not there. From Milestone 4 |

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

**Added in Milestone 4, within version 1.** The methods `Find` and
`OpenNote`. A search lists what `Find` gives among its results as notes, each
saying its folder or window, and a tap calls `OpenNote`. Since a search asks
as each letter is typed, it calls `Find` only while Gooseberry is running,
without starting it, and never waits on the reply.

For All notes, Search calls `OpenBoard` with a token of its own and waits for
`BoardShown` with that token: from then the board's window, whose application
is `io.github.carlsonjm.Gooseberry`, is on screen, and Search can hand the
moment to Kadunce so the window takes the card's place.

## Stuck notes on the bus

From Milestone 3, Gooseberry tells the desktop which open windows have notes
stuck to them, and answers a tap on them. A title bar draws the dot from it
(Shuffle), and Spread draws each card's notes (Kadunce). Neither keeps notes:
what they draw is always what this interface last said.

- **Service:** `io.github.carlsonjm.gooseberry`, as for the quick note. A
  caller never starts Gooseberry for this: it watches for the name and, while
  it is absent, draws no dot and no stack.
- **Object:** `/StuckNotes`. **Interface:**
  `io.github.carlsonjm.Gooseberry.StuckNotes`.
- **Version:** `ProtocolVersion()` returns `1`, with the same rule as the quick
  note: a key or method a version-1 caller can ignore keeps the number.
- **Callers inside the compositor** (a window decoration, an effect) call only
  asynchronously, and never wait for a reply while drawing.

One entry for each open window with at least one stuck note that is not
tucked away, a dictionary of strings to variants:

| Key | Type | What it is |
| --- | --- | --- |
| `windowIds` | `as` | The desktop's ids for that window, as Plasma's window list gives them: on Wayland the compositor's own id for the window, a UUID written with braces; on X11 the window id in decimal |
| `caption` | `s` | The window's title as the desktop reports it now |
| `app` | `s` | Its application, by desktop file name without `.desktop` |
| `window` | `s` | The document the notes are stuck to, as a note's `window` header names it |
| `count` | `u` | How many notes are stuck to it |
| `colour`, `colourHex` | `s`, `s` | The top note's colour, by name and as `#RRGGBB` |
| `notes` | `av` | Its notes, the top one first (the one changed last), each a dictionary: `id`, `title` (the first words), `text`, `colour`, `colourHex`, and where it stands on the window as `x`, `y`, `width` and `height` (`d`) |
| `shown` | `b` | Its notes are up: brought up and not yet put away. They stay up while the window is behind another or Spread is open, and are drawn again when it comes back |
| `folded` | `b` | Its notes are up but folded into one dot, 18 px across in the top note's colour within a 44 px target, whose top-right corner is the top note's: Robin went back to the window or to another. Spread draws the dot in their place. False when they are not up |

| Method | What it does |
| --- | --- |
| `ProtocolVersion() → u` | The version, `1` |
| `Windows() → av` | Every entry as above, in no set order; empty when no open window has notes |
| `Toggle(s windowId, s caption, s app) → b` | Shows that window's notes over it, open, each where it was last placed, or puts them away when they are shown, open or folded. The window is found by `windowId` when it is given and open, otherwise by `caption` and `app`. Returns whether they are shown now; false when no window with notes matches |
| `Show(s windowId, s caption, s app) → b` | Brings that window's notes up, found as `Toggle` finds it, open; folded, they open; nothing changes when they are open already. Returns false when no window with notes matches |
| `Hide()` | Puts away every window's notes |
| `Pause(b paused)` | While paused, notes that are up step aside without being put away, and come back when the pause ends; a note being typed on is kept and the typing ends. Spread pauses while it is open, so the notes it draws on its cards are the only notes on screen, and Kadunce while a window is carried, so the notes do not stay behind as it moves. Notes that folded in the moment before a pause, as the press that picked their window up took the keys, come back open |
| `NewOn(s windowId, s caption, s app) → b` | Opens the quick-note card on a new note stuck to that window, found as `Toggle` finds it; it is kept from its first letter, as any quick note. Returns false when no open window matches or notes cannot be written. A title bar calls it from its + button |
| `StickTo(s noteId, s windowId, s caption, s app) → b` | Sticks the note to that window, found as `Toggle` finds it, keeping its folder. Returns false when the note or the window is not found, or the note cannot be changed |
| `Write(s noteId, s windowId, s caption, s app) → b` | Brings that window's notes up open, as `Show` does, with that note ready to type on where it stands: once they are drawn over the window, the cursor is in it and the on-screen keys come up. While paused it waits for the pause to end. Returns false when the window is not found or the note is not stuck to it. Spread calls it when a note or a folded dot on a card is tapped, and opens the card; a caller from before it calls `Show` |

| Signal | When |
| --- | --- |
| `WindowsChanged(av windows)` | Any entry came, went or changed: a note stuck, unstuck, tucked, recoloured or retitled, a window opened, closed or retitled, or notes shown or put away. It carries every entry, as `Windows()` returns them |

**Where a note stands.** `x` and `y` are the note's `place` (`FORMAT.md`):
its top-left corner as shares of the window's width and height, from 0 to 1,
negative when it was never placed. `width` and `height` are its `size`, in the
desktop's pixels, negative when it was never resized there; it is then drawn
at the size every note starts at. They are in pixels, not shares, so resizing
a window sends nothing new. Spread draws a placed note on the card in that
place, scaled with the card, and keeps notes never placed in the corner stack.
A caller that ignores these keys draws every note in the stack.

**Matching a window.** A caller that knows the compositor's id for a window,
as Kadunce does, matches on `windowIds`. A title bar that knows only the
window's title and class matches on `caption`, with any ` <2>`-style suffix the
compositor adds to tell same-titled windows apart taken off both sides, and
uses `app` only to choose between entries with the same caption (compared
without case, and also matching when either ends with the other after a dot).

**Shown over the window.** A tap on the dot brings the notes up on a surface of
Gooseberry's own, above the windows and laid exactly over the one with the
notes, following it as it moves or is resized; the window itself is never moved
or resized. Each note stands where it was last let go there, written as the
note's `place` (`FORMAT.md`); one never placed starts at the top-right, the
newest on top. Each is as big as it was last resized there, written as its
`size`. Notes brought up stay up until Robin puts them away with the dot
again. Nothing else puts them away. The surface takes presses only on the
notes, so a tap anywhere else on the window reaches the window. A note is
typed on where it stands, never in a popup: a tap on it puts the cursor in it,
and its words are kept as the quick note keeps them, from the first letter
(`Capture`). While the notes are open the surface holds the keys, asked for
once it has drawn: the desktop brings a top-layer surface to the front as
soon as it will take them, and hands them back to whatever Robin taps or
types into next. Losing them so, to the window or another, the notes fold
into one dot where the top note stands, as `folded` describes; the dot takes
no keys, and a tap on it opens the notes with the top note ready to type.
Stepping aside, for Spread or the quick-note card, does not fold them, but
open notes left over a window that goes behind another do fold. A tap
anywhere else, `Esc`, Spread opening or another window's notes coming up ends
the typing; what was typed is kept. While a window's
notes are up, its title bar shows a + to the left of the dot, which starts a
new note stuck to it (`NewOn`). While the window is behind another, while
Spread is open and while a note is open on the quick-note card, the notes step
aside, and they come back with it.

## Split Rock

Where Split Rock is installed and set up, Gooseberry is one of its tools: the
assistant can find notes, read one, and add one when Robin asks, and says when
it reads one. From a note, "next step" sends only that note, and offers a
checklist, a reminder or a better home; nothing changes until Robin chooses.
What Robin keeps from the assistant becomes notes here too (`DECISIONS.md`).
This waits while Split Rock's own work is on hold. Gooseberry needs no
assistant, and offers none of its own.

## Needs

Each is for that component to take up in the open, under its own contracts.
None is assumed. Already taken up: Kadunce draws each card's notes in Spread and
carries a note to another card (`StickTo`); Shuffle's title bar draws the dot;
Tettegouche lists notes among its results (`Find`) and hosts the quick note as
its Notes mode.

| Component | Need | Holds up |
| --- | --- | --- |
| Kadunce | Accepting a note carried to the top edge: opening Spread and telling Gooseberry which card it was dropped on | Sticking by drag (Milestone 3) |
| Kadunce | A request to stack a window with a card | Stacking without a hand gesture |
| Tettegouche | A Notes tab in Search, listing every note | Browsing notes from Search |
| Split Rock | Use Gooseberry's notes tool where present; on hold while Split Rock's own work is | The notes tool |
