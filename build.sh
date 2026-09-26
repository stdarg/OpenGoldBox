#!/bin/sh
# macOS and Linux counterpart of build.cmd: configure, build, then run the
# native test suite, stopping at the first failure.
#
# The contract matches build.cmd deliberately -- "run the suite" must be one
# documented command on every platform (docs/TECH.md section 14.1). A non-zero
# exit means the build or a test failed; it never reaches ctest with a stale
# binary, which is how a failed compile can otherwise be reported as a pass.
set -eu

cd "$(dirname "$0")"

CMAKE="${CMAKE:-cmake}"
if ! command -v "$CMAKE" >/dev/null 2>&1; then
    echo "cmake not found. Install it, or set CMAKE to its full path." >&2
    exit 1
fi

JOBS="${JOBS:-$( (command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu) \
              || (command -v nproc  >/dev/null 2>&1 && nproc) \
              || echo 4 )}"

"$CMAKE" --preset default
"$CMAKE" --build --preset default --parallel "$JOBS"

# Exclude the Godot runtime checks: they need a Godot install and share a
# user-data path, so they are run serially and separately. See docs/TECH.md 14.1.
"$CMAKE" -E chdir build ctest --output-on-failure --exclude-regex '^opengold_godot_'
