# Gooseberry

Notes, stickies and a planner for a touchscreen Linux desktop.

Write a thought down in one tap. Keep it beside the work it belongs to, put it
on the planner when it has a time, or tuck it away without losing it. Nothing
asks for a title, a folder or a save before an idea is kept.

**Status:** Milestone 0, capture and keep, and Milestone 1, the planner, are
built and waiting on their proof: the quick-note card, the board, tucking away
and removing; reminders at a time or the next time a document opens, the
planner and checklists. The pen comes next. The concept is in [docs/CONCEPT.md](docs/CONCEPT.md) and
the plan in [docs/ROADMAP.md](docs/ROADMAP.md).

## What it does

- **Captures.** One tap opens a page with the cursor already in it. Type, or
  write with a pen. It is saved as you go.
- **Keeps notes where they belong.** A note can belong to what you are working
  on, to a project, to the whole workspace, or to nothing yet.
- **Plans.** A note with a time goes on the planner and reminds you; everything
  else is an idea, and waits without a date.
- **Finds.** Typed and handwritten notes both turn up when you search.

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
running, a note can belong to the card in front. Where Tettegouche's Search is
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
