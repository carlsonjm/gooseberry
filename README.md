# Gooseberry

Notes, stickies and a planner for a touchscreen Linux desktop.

Write a thought down in one tap. Keep it beside the work it belongs to, put it
on the planner when it has a time, or tuck it away without losing it. Nothing
asks for a title, a folder or a save before an idea is kept.

**Status:** capture, the board, the planner, folders, notes stuck to windows
and notes in Search are built. The concept is in
[docs/CONCEPT.md](docs/CONCEPT.md) and the plan in
[docs/ROADMAP.md](docs/ROADMAP.md).

## What it does

- **Captures.** One tap opens a page with the cursor already in it. It is
  saved as you type.
- **Keeps notes in folders.** Each note is kept in a folder, Inbox unless you
  choose, and can be stuck to the window you are working in. Start a folder
  before any work exists; give one to a workspace and its notes go there.
- **Plans.** A note with a time goes on the planner and reminds you; everything
  else is an idea, and waits without a date.
- **Finds.** A note turns up when you search the board, or Search where
  Tettegouche is installed.

## What it never does

- Ask for a title or a folder before a note is kept.
- Lose a note. A note you remove goes to the desktop's trash first.
- Take up room you did not ask it to.

## Installing

Gooseberry needs KDE Plasma 6 with Qt 6.9 and KDE Frameworks 6.17 or newer, and
their development files to build: CMake, Extra CMake Modules, Kirigami,
KCoreAddons, KDBusAddons, KI18n, KWindowSystem, Layer Shell Qt and Plasma's
task manager library.

```sh
./install.sh
```

It builds Gooseberry, runs every check, and installs it for every account on
the computer, asking once for a password. Gooseberry is then in the launcher,
and starts by itself, out of sight, from the next login. `./uninstall.sh`
removes it again; notes stay.

Notes are kept in `Gooseberry` in your Documents folder, one Markdown file
each ([docs/FORMAT.md](docs/FORMAT.md)).

## With cards and Search

Gooseberry works on any KDE Plasma desktop. Where the Kadunce card workspace is
running, the card in front is a window a note can be stuck to. Where Tettegouche's Search is
installed, notes are found there too. What each adds, and what it needs, is in
[docs/DESKTOP.md](docs/DESKTOP.md).

## Development

Read [AGENTS.md](AGENTS.md) before working in this repository. The documentation
index is [docs/README.md](docs/README.md). Run `./verify.sh` before proposing a
change.

## License

Gooseberry is licensed under GPL-2.0-or-later. See [LICENSE](LICENSE).

The project names and marks are not covered by that licence. See
[TRADEMARKS.md](TRADEMARKS.md).
