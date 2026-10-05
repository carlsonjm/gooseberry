#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
manifest="${project_root}/build/install_manifest.txt"

if [[ ! -f $manifest ]]; then
    echo "Nothing to remove: Gooseberry was not installed from this folder." >&2
    exit 1
fi

# Only the program and its desktop entries go; the notes folder is never touched.
install_key=/usr/local/libexec/shuffle/install-step
if [[ -x "${install_key}" ]] \
        && sudo -n -l "${install_key}" gooseberry remove >/dev/null 2>&1; then
    sudo -n "${install_key}" gooseberry remove
else
    while IFS= read -r installed; do
        if [[ -n $installed && ( -f $installed || -L $installed ) ]]; then
            sudo rm -- "$installed"
        fi
    done < "$manifest"
fi
kbuildsycoca6 >/dev/null 2>&1 || true

echo "Gooseberry removed. Notes stay in their folder."
