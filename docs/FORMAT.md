# Note format

How Gooseberry keeps notes on disk: where the folder is, what is in it, its
folders, and the header at the top of each note. This is a contract. Another device reading the
same folder, the file index, Search and any other program depend on it, so it
is versioned, and a change that breaks it raises the version.

**Folder format:** 2. **Note format:** 2. Both from Milestone 2, when notes
went into folders; the first of each is read and brought up to date
(§ Versions).

## Where the folder is

Gooseberry keeps its notes in a folder named `Gooseberry` in the person's
Documents folder: the desktop's own Documents folder where one is set, which is
`~/Documents/Gooseberry` on most systems. Gooseberry makes the folder the
first time it keeps a note, and makes it again if it is gone.

This is engineering's choice, for three reasons:

- It is a visible folder, so the desktop's file index finds notes by name and
  by what they say. The index leaves hidden folders out.
- A person, a file manager or a folder sync tool can find it without being
  told where to look.
- It is Gooseberry's own, apart from Split Rock's notebook about the computer
  (`DECISIONS.md`).

Rejected: a hidden folder under `~/.local/share`, which the file index and
Search would skip; and asking where to keep notes at first start, which puts a
question before the first note.

The environment variable `GOOSEBERRY_FOLDER` points Gooseberry at another
folder. It is for tests and trials.

## What is in the folder

| Name | What it is |
| --- | --- |
| `.gooseberry` | The folder format, as the line `format: 2` |
| `.workspaces` | Which folder each workspace's new notes go into, when one has been given a folder |
| `<id>.md` | One note in Inbox: the header, then the note's text |
| `<id>.svg` | Ink beside its note, as a drawing. Gooseberry writes none, since the pen is not planned, but one found there moves and goes to the trash with its note |
| `<folder>/` | One folder, by its name, holding its notes and their ink as above |

- **The id** is the moment the note was started, in local time, and four
  letters to tell apart notes started in the same second:
  `2026-10-04-144112-k3fm`. The letters are from
  `abcdefghjkmnpqrstuvwxyz23456789`. A note's file is never renamed; its title
  is its first words, read from the text.
- **Anything else is left alone:** other files, hidden folders such as a sync
  tool's, and anything deeper than one folder down.
- **A Markdown file with no header** put in the folder, or in one of its
  folders, by another program is a note kept there, stuck to nothing.
  Gooseberry adds a header the first time it changes the note.

## Folders

Every note is kept in exactly one folder. **Inbox** is the notes folder itself;
every other folder is a visible folder inside it, one level deep, named as the
person named it. A file manager or a sync tool sees the same folders the board
does, and a folder made, renamed or removed there is seen within moments.

- **The folder is where the file is.** No header key says it, so moving a note
  is moving its file, in one step, with its ink beside it. The note's `changed`
  time is then set, since its place changed.
- **A folder lasts until it is removed,** empty or not. Removing it moves its
  notes and their ink to Inbox first; whatever else is left in it, such as a
  sync tool's own files, goes to the desktop's trash with the folder. Nothing
  is deleted outright.
- **A name** is any text without a slash, not starting with a dot, and not
  Inbox, in any case. Spaces at its ends and runs of spaces inside are taken
  away.
- **The same note in two folders,** as a copy made by hand, is read once: from
  Inbox first, then the folders in alphabetical order.
- **`.workspaces`** has a line for each workspace given a folder, the
  workspace's name and the folder's in double quotes as a header writes text:
  `"Desk": "Shuffle launch"`. A workspace with no line, or whose folder is
  gone, puts new notes in Inbox. Renaming a folder renames it here;
  removing it takes its lines away.

## A note

```markdown
---
gooseberry: 2
created: 2026-10-04T14:41:12-05:00
changed: 2026-10-04T14:43:05-05:00
colour: butter
stuck: true
window: "SpreadGesture.qml"
app: "org.kde.kate"
workspace: "Desk"
place: 0.712 0.084
tucked: false
remind: 2026-10-05T09:00:00-05:00
---
Flick threshold feels short on the Z13. Measure the real velocity before
touching 1400.
```

The header sits between two lines of `---` at the very top, the way other
Markdown programs expect front matter. Each line is one `key: value`. Text
values are in double quotes, with `\"` for a quote and `\\` for a backslash.
Times are ISO 8601 with their offset from UTC, to the second.

| Key | Required | Value |
| --- | --- | --- |
| `gooseberry` | yes | The note format: `2` |
| `created` | yes | When the note was started |
| `changed` | yes | When its text, colour, folder, window or tucked state last changed |
| `colour` | yes | `butter`, `rhyolite`, `lake`, `lichen` or `stone` |
| `stuck` | no | `true` when the note is stuck to its `window`; left out otherwise |
| `window` | no | The document or window the note was written on, or was stuck to last |
| `app` | no | That window's application, by its desktop file name |
| `workspace` | no | The workspace the note was written on |
| `place` | no | Where the note sits on its window when the window's notes are shown: its top-left corner as two fractions, of the window's width and of its height, each from 0 to 1, to three places. Left out when the note has never been placed there. From Milestone 3 |
| `tucked` | yes | `true` when tucked away, otherwise `false` |
| `remind` | no | The reminder: a time, which also puts the note on the planner on that day, or `opens` for the next time a window opens on the note's `window` in its `app` |
| `reminded` | no | When the reminder was shown |
| `done` | no | When the note was marked done on the planner |

- **Stuck** is apart from the folder: a stuck note is still kept in its
  folder, moving it to another folder leaves it stuck, and unsticking it leaves
  it where it is. `window` and `app` stay when it is unstuck, so "next time
  this opens" still has a window to wait for. A note stuck to a window with no
  name is not stuck.
- **Place** keeps a note where it was let go on its window, in proportion
  when the window is resized. Setting it does not change `changed`, and
  sticking the note to another window takes it away. A `place` this version
  cannot read is kept as written, and the note is placed as one never placed.
- **Colour** is the person's own sorting and means nothing to Gooseberry. A
  colour this version does not know is shown as butter and kept as written.
- **The text** follows the closing `---` exactly as it was written: UTF-8, with
  line feeds. The first words are the title; there is no title key.
- **A reminder is shown once.** It is due when `remind` has come and the note
  has no `reminded` and no `done`. Gooseberry writes `reminded` into the note
  before it shows the reminder, so a restart, or another program reading the
  folder, never shows it again. Setting a new reminder removes `reminded` and
  `done`. Writing `reminded` does not change `changed`, which stays the
  person's own last change. A `remind` this version cannot read is kept as
  written and does nothing.
- **A checklist** is Markdown task lines in the text, `- [ ] oats` for an item
  and `- [x] oats` for one ticked, as other Markdown programs write them. Any
  other line is plain text; the first, above the items, is the list's heading.

## How notes are kept

- **From the first letter.** A note is written to the folder the moment it
  first has text. Typing after that is written when the writing pauses for half
  a second, and at least every three seconds while it goes on; any other
  change, Done, a logout and quitting write it at once. A crash loses at most
  the typing since the last pause, never the note. A quick note closed with nothing
  on it leaves nothing behind.
- **Whole or not at all.** A change is written to a new file beside the note
  and then put in its place in one step, so a crash leaves the note as it was
  or as it is, never half written.
- **Removed to the trash.** Removing a note moves it, and its ink, to the
  desktop's trash as the freedesktop.org trash specification lays it out: the
  file under `Trash/files` in the person's data folder, and a `.trashinfo`
  record of where it came from, so the desktop's trash can put it back. A note
  emptied of text and closed goes the same way. Nothing is deleted outright.
- **Changes from elsewhere.** A note changed, added or removed by another
  program or a sync tool is read again within moments.

## Versions

- A key added later, which a reader can ignore, does not change the version.
  A reader keeps the keys it does not know, in order, when it changes a note.
- A change to what a key means, to what is required, or to the folder layout
  raises the version.
- A reader that finds a version newer than it knows shows those notes and
  changes nothing: in that note, for a note's version, or anywhere in the
  folder, for the folder's.

| Version | From | What changed |
| --- | --- | --- |
| 1 | Milestone 0 | The first format |
| 1 | Milestone 1 | `remind`, `reminded` and `done` added, and checklists written down; a reader of the first format ignores them, so the version stays |
| 2 | Milestone 2 | Folders, one level down, with Inbox the notes folder itself, and `.workspaces`. In a note, `stuck` replaces the required `belongs`, and `project` goes, since the folder is the project |
| 2 | Milestone 3 | `place` added; a reader of Milestone 2 ignores it, so the version stays |

**Bringing the first version up to date.** A folder whose `.gooseberry` says
`format: 1` is brought up to date when Gooseberry opens it. Each note that
belonged to a project moves, with its ink, into a folder of the project's name
(a slash in it becomes a hyphen; a name that cannot be a folder's leaves the
note in Inbox), keeping its `changed` time. A note that belonged to a window is
read as stuck to it; one that belonged to the workspace or was Loose is in
Inbox. Then `.gooseberry` says `format: 2`. Each note's header is written in
the second format the next time the note changes; until then a note of the
first format is read as above wherever it is found. A first-version Gooseberry
then finds a newer folder and changes nothing in it, as § Versions says.
