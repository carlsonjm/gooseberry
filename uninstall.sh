#!/usr/bin/env bash
set -euo pipefail

# Only the program and its desktop entries go; the notes folder is never
# touched. What goes is this fixed list of Gooseberry's own files, the same
# ones install.sh puts in place, so removing needs no build folder and reads
# no list another program could have written.
installed=(
    /usr/bin/gooseberry
    /usr/share/applications/io.github.carlsonjm.Gooseberry.desktop
    /usr/share/dbus-1/services/io.github.carlsonjm.gooseberry.service
    /etc/xdg/autostart/io.github.carlsonjm.Gooseberry.desktop
    /usr/share/icons/hicolor/scalable/apps/io.github.carlsonjm.Gooseberry.svg
    /usr/share/good-input/capabilities/notes.json
)
shopt -s nullglob
installed+=(/usr/share/locale/*/LC_MESSAGES/gooseberry.mo)
shopt -u nullglob

present=()
for path in "${installed[@]}"; do
    if [[ -f $path || -L $path ]]; then
        present+=("$path")
    fi
done

install_key=/usr/local/libexec/shuffle/install-step
if [[ -x "${install_key}" ]] \
        && sudo -n -l "${install_key}" gooseberry remove >/dev/null 2>&1; then
    sudo -n "${install_key}" gooseberry remove
    # An install key set up before Notes had a tool file leaves it behind.
    capabilities=/usr/share/good-input/capabilities/notes.json
    if [[ -f $capabilities || -L $capabilities ]]; then
        sudo rm -f -- "${capabilities}"
    fi
elif ((${#present[@]} == 0)); then
    echo "Nothing to remove: Gooseberry is not installed." >&2
    exit 1
else
    sudo rm -f -- "${present[@]}"
fi
# The tools folder is shared with other apps, so it goes only when empty.
for folder in /usr/share/good-input/capabilities /usr/share/good-input; do
    if [[ -d $folder ]] && [[ -z "$(ls -A -- "${folder}")" ]]; then
        sudo rmdir -- "${folder}"
    fi
done
kbuildsycoca6 >/dev/null 2>&1 || true

echo "Gooseberry removed. Notes stay in their folder."
