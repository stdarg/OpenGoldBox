#!/bin/sh
# Builds OpenGoldBox on macOS and Linux. With no options it builds the game
# and re-exports mac-package/OpenGoldBox.app on macOS (the native core on
# Linux, where the game is not packaged), and runs no tests.
#
#   --test      after building, run the native test suite
#   --test-all  after building, run every test, including the Godot checks
#   --werror    make every compiler warning fail the build
#   <preset>    build another CMake preset: default (native core and tests,
#               build/), debug (the same, unoptimized) or macos-universal
#
# A non-zero exit means the build or a requested test failed. Tests never run
# after a failed build, which is how a stale binary can otherwise pass.
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

PRESET=
WARNINGS_AS_ERRORS=OFF
TESTS=none
for arg in "$@"; do
    case "$arg" in
        --werror) WARNINGS_AS_ERRORS=ON ;;
        --test) TESTS=native ;;
        --test-all) TESTS=all ;;
        -*)
            echo "Unknown option: $arg (use --test, --test-all, --werror or a preset name)" >&2
            exit 2
            ;;
        *) PRESET="$arg" ;;
    esac
done
if [ -z "$PRESET" ]; then
    if [ "$(uname -s)" = Darwin ]; then
        PRESET=macos-universal
    else
        PRESET=default
    fi
fi

# Passed on every run, so a tree configured with --werror goes back to
# warnings when the flag is dropped.
"$CMAKE" --preset "$PRESET" -DCMAKE_COMPILE_WARNING_AS_ERROR="$WARNINGS_AS_ERRORS"
"$CMAKE" --build --preset "$PRESET" --parallel "$JOBS"
if [ "$PRESET" = macos-universal ]; then
    echo "Game exported: mac-package/OpenGoldBox.app"
fi

case "$TESTS" in
    native)
        # The Godot runtime checks need a Godot install and share a user-data
        # path, so --test leaves them out. See docs/TECH.md 14.1.
        ctest --preset "$PRESET" -j "$JOBS" --exclude-regex '^opengold_godot_'
        ;;
    all)
        ctest --preset "$PRESET" -j "$JOBS" --output-on-failure
        ;;
esac
