# Decisions

Each settled ruling: the rule, why it holds, and what was rejected. Decisions
still open are in `ROADMAP.md` § Open decisions.

## Gooseberry is its own component, under its own name

Gooseberry is a separate program with its own repository and name, and works on
any KDE Plasma desktop. A product that includes it may give it another name
there. (The maintainer, 3 October.)

Rejected: building notes inside a product's own repository. Notes would only
exist inside that product, and anyone without it would be left with the
problem Gooseberry exists to solve.

## Cards and Search are found, never required

Gooseberry works with Kadunce's cards and Tettegouche's Search when they are
present, and is complete without them. It finds each at run time. (The
maintainer, 3 October.)

Rejected: requiring either. A notes app that needs a particular window manager
is not one most people can use.

## Nothing comes before the note

A note is kept from its first letter. No title, folder, notebook or save step
comes first. (The maintainer, 3 October.)

Rejected: a title or notebook chosen first, as most notes apps ask. That
question is where the thought is lost.

## Window arrangement is requested, never done directly

To show notes beside or with a window, Gooseberry asks the window manager, which
decides and stays the only owner of window positions.

Rejected: Gooseberry moving or resizing windows itself, which would give windows
two owners.

## Written in C++ and Qt, like the rest of the suite

The program uses C++, Qt and KDE's own libraries, as Kadunce, Tettegouche and
Split Rock do. This is engineering's choice.

Rejected: a web toolkit, which is heavy on a tablet's battery and draws nothing
like the rest of the desktop; and a second language and toolchain for a suite
maintained by one person and agents.
