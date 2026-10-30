#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The notes tool an install puts in place, and uninstall.sh taking it away.

Installs the build into a folder of the test's own, as a package does, and
checks that the notes tool an assistant broker reads is where the broker looks,
under the data folder, and is the file in data/. Then checks that uninstall.sh's
fixed list, which removes a developer install, names it.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

build_dir, source_dir = sys.argv[1], sys.argv[2]
failures = []

with tempfile.TemporaryDirectory(prefix="gooseberry-install-") as stage:
    environment = dict(os.environ, DESTDIR=stage)
    # The build's own prefix: /usr after install.sh, /usr/local otherwise.
    subprocess.run(["cmake", "--install", build_dir],
                   check=True, env=environment, stdout=subprocess.DEVNULL)
    tools = [os.path.join(root, name) for root, _, names in os.walk(stage) for name in names
             if os.path.join(root, name).endswith("/share/good-input/capabilities/notes.json")]
    tool = tools[0] if len(tools) == 1 else ""
    if not tool:
        failures.append("notes.json is not in the data folder's good-input/capabilities")
    else:
        with open(tool, encoding="utf-8") as given, \
                open(os.path.join(source_dir, "data/notes.json"), encoding="utf-8") as kept:
            if given.read() != kept.read():
                failures.append("the installed notes.json is not data/notes.json")
        with open(tool, encoding="utf-8") as given:
            description = json.load(given)
        if description.get("app") != "notes":
            failures.append("notes.json does not name the notes app")
        names = [each.get("name") for each in description.get("tools", [])]
        if names != ["notes.search", "notes.read", "notes.create"]:
            failures.append(f"notes.json offers {names}")

with open(os.path.join(source_dir, "uninstall.sh"), encoding="utf-8") as script:
    text = script.read()
listed = re.search(r"^installed=\(\n(.*?)^\)", text, re.S | re.M)
patterns = listed.group(1).split() if listed else []
patterns += re.findall(r"^installed\+=\((\S+)\)", text, re.M)
if "/usr/share/good-input/capabilities/notes.json" not in patterns:
    failures.append("uninstall.sh leaves notes.json behind")

for failure in failures:
    print(failure, file=sys.stderr)
sys.exit(1 if failures else 0)
