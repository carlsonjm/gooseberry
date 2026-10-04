#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${project_root}/build"

cmake -S "${project_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
GOOSEBERRY_BUILD_DIR="${build_dir}" "${project_root}/verify.sh"
sudo cmake --install "${build_dir}"
# Plasma's list of applications gains Gooseberry, which is also what lets the
# desktop tell Gooseberry which window is in front.
kbuildsycoca6 >/dev/null 2>&1 || true

echo "Gooseberry installed. Find it in the launcher; it starts by itself from the next login."
