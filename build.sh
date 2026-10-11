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

# "./build.sh debug" runs the slower Debug preset; the default is optimized.
# "--werror" makes every compiler warning fail the build.
PRESET=default
WARNINGS_AS_ERRORS=OFF
for arg in "$@"; do
    case "$arg" in
        --werror) WARNINGS_AS_ERRORS=ON ;;
        *) PRESET="$arg" ;;
    esac
done

# Passed on every run, so a tree configured with --werror goes back to
# warnings when the flag is dropped.
"$CMAKE" --preset "$PRESET" -DCMAKE_COMPILE_WARNING_AS_ERROR="$WARNINGS_AS_ERRORS"
"$CMAKE" --build --preset "$PRESET" --parallel "$JOBS"

# Exclude the Godot runtime checks: they need a Godot install and share a
# user-data path, so they are run serially and separately. See docs/TECH.md 14.1.
ctest --preset "$PRESET" -j "$JOBS" --exclude-regex '^opengold_godot_'
