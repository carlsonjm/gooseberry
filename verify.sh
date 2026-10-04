#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${GOOSEBERRY_BUILD_DIR:-${project_root}/build}"

python3 "${project_root}/tests/verify-public.py"

cmake -S "${project_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON >/dev/null
cmake --build "${build_dir}" -j"$(nproc)"
ctest --test-dir "${build_dir}" --output-on-failure

echo "Gooseberry verification passed."
