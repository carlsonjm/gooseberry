# Note format

How Gooseberry keeps notes on disk: where the folder is, what is in it, and the
header at the top of each note. This is a contract. Another device reading the
same folder, the file index, Search and any other program depend on it, so it
is versioned, and a change that breaks it raises the version.

**Folder format:** 1. **Note format:** 1. Both from Milestone 0.

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
| `.gooseberry` | The folder format, as the line `format: 1` |
| `<id>.md` | One note: the header, then the note's text |
| `<id>.svg` | The note's ink, when it has any, as a drawing; from Milestone 2 |

- **The id** is the moment the note was started, in local time, and four
  letters to tell apart notes started in the same second:
  `2026-10-04-144112-k3fm`. The letters are from
  `abcdefghjkmnpqrstuvwxyz23456789`. A note's file is never renamed; its title
  is its first words, read from the text.
- **Anything else is left alone:** other files, subfolders and the files a sync
  tool keeps.
- **A Markdown file with no header** put in the folder by another program is a
  Loose note. Gooseberry adds a header the first time it changes the note.

## A note

```markdown
---
gooseberry: 1
created: 2026-10-04T14:41:12-05:00
changed: 2026-10-04T14:43:05-05:00
colour: butter
belongs: window
window: "SpreadGesture.qml"
app: "org.kde.kate"
workspace: "Desk"
tucked: false
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
| `gooseberry` | yes | The note format: `1` |
| `created` | yes | When the note was started |
| `changed` | yes | When its text, colour, place or tucked state last changed |
| `colour` | yes | `butter`, `rhyolite`, `lake`, `lichen` or `stone` |
| `belongs` | yes | `window`, `project`, `workspace` or `loose` |
| `window` | no | The document or window the note was written on |
| `app` | no | That window's application, by its desktop file name |
| `project` | no | The project's name |
| `workspace` | no | The workspace the note was written on |
| `tucked` | yes | `true` when tucked away, otherwise `false` |

- **Belongs** says which place on the board the note sits in. `window`,
  `project` and `workspace` name theirs in the key of the same name; a note
  that belongs to a window or project with no name there is Loose. `window`,
  `app` and `workspace` record where the note was written and are kept when it
  moves to another place, so a note can later find its way back.
- **Colour** is the person's own sorting and means nothing to Gooseberry. A
  colour this version does not know is shown as butter and kept as written.
- **The text** follows the closing `---` exactly as it was written: UTF-8, with
  line feeds. The first words are the title; there is no title key.

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
