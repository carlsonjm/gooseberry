# Repository context and startup

For every task, read only this startup set, in order:

1. `AGENTS.md`
2. `docs/CONCEPT.md`
3. `docs/ROADMAP.md`
4. The suite record, kept privately in the Shuffle repository and read from its
   checkout beside this one: `../shuffle/docs/suite/CURRENT_STATE.md`,
   `../shuffle/docs/suite/ROADMAP-CC.md` and `../shuffle/docs/suite/SWARM.md`.
   When it is not checked out, say that the suite plan was unavailable rather
   than inventing an order.

Then open only the documents the work touches; `docs/README.md` routes each
subject to its document. Before proposing an approach, check
`docs/DECISIONS.md` for one that was already rejected, and `docs/ROADMAP.md`
§ Open decisions for one still waiting on the maintainer.

Claude Code loads `CLAUDE.md` automatically. It routes into this same startup set
and adds no separate protocol.

## Holds

These hold in every Shuffle repository.

- The maintainer approves product behavior and visual direction before
  implementation begins. Engineering may present evidence, constraints and
  alternatives; an unapproved proposal does not become a candidate.
- One implementation owner per repository. A second worker is read-only review or
  a disjoint file set.
- Reproduce a defect and measure the property controlling it before changing it.
- Components never depend on private product features. Gooseberry works on any
  KDE Plasma desktop; what Kadunce, Tettegouche or Shuffle add switches on only
  when they are present, and is found at run time.

These hold here, because this program keeps what a person wrote.

- A note is saved as it is written. No step comes between writing and keeping.
- Nothing deletes a note outright. A removed note goes to the desktop's trash.
- Window positions belong to the window manager. Gooseberry may ask Kadunce for
  room or a place; it never moves or resizes a window itself.
- Nothing about the cards contract is assumed before the maintainer settles
  § Open decisions in `docs/ROADMAP.md`, and nothing is asked of Kadunce or
  Tettegouche that their own documents do not offer. A need goes in
  `docs/DESKTOP.md` § Needs, for that component to take up in the open.
- Tests never read or change a real person's notes, settings or session.

## Writing here

- `docs/ROADMAP.md` is the plan for this repository; suite order lives in the
  suite record.
- A ruling goes in `docs/DECISIONS.md` as its rule, why it holds, and what was
  rejected.
- Public text stays fit to publish: no attribution by initial, and no paths into
  a real home folder. `tests/verify-public.py` checks it.
- Code comments explain code behavior and reasoning only.

## Verification

Run `./verify.sh` for every change.
