#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${project_root}/build"

cmake -S "${project_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
GOOSEBERRY_BUILD_DIR="${build_dir}" "${project_root}/verify.sh"
# Shuffle's install key, where it is set up, installs with no password: the
# build is laid out here with this account's own rights, and a root-owned helper
# takes the files as a stream and puts only Gooseberry's own in place.
install_key=/usr/local/libexec/shuffle/install-step
if [[ -x "${install_key}" ]] \
        && sudo -n -l "${install_key}" gooseberry install >/dev/null 2>&1; then
    stage="$(mktemp -d "${TMPDIR:-/tmp}/gooseberry-stage.XXXXXX")"
    trap 'rm -rf -- "${stage}"' EXIT
    # An earlier install with sudo left this list owned by root.
    rm -f -- "${build_dir}/install_manifest.txt"
    DESTDIR="${stage}" cmake --install "${build_dir}" >/dev/null
    tar -C "${stage}" -cf - . | sudo -n "${install_key}" gooseberry install
    # The list then names where each file landed, as an install with sudo does.
    sed -i "s|^${stage}||" "${build_dir}/install_manifest.txt"
else
    sudo cmake --install "${build_dir}"
fi
# Plasma's list of applications gains Gooseberry, which is also what lets the
# desktop tell Gooseberry which window is in front.
kbuildsycoca6 >/dev/null 2>&1 || true

echo "Gooseberry installed. Find it in the launcher; it starts by itself from the next login."
