#!/bin/sh
# Builds OpenGoldBox on macOS and Linux. With no options it builds the game
# and re-exports it, mac-package/OpenGoldBox.app on macOS and linux-package/
# plus the .tar.gz players download on Linux, and runs no tests.
#
#   --test      after building, run the native test suite
#   --test-all  after building, run every test, including the Godot checks
#   --test=NAME[,NAME...]
#               after building, run only the named tests, such as
#               --test=town_services,localization_check; a name may leave out
#               its opengold_ or opengold_godot_ prefix
#   --package   macOS: also write the disk image players download
#   <preset>    build another CMake preset: default (native core and tests,
#               build/), debug (the same, unoptimized), macos-universal, linux
#               or linux-release (the package for players, which
#               tools/linux_release_build.sh builds in its container)
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
TESTS=none
PACKAGE=no
NAMES=
for arg in "$@"; do
    case "$arg" in
        --test) TESTS=native ;;
        --test-all) TESTS=all ;;
        --test=*)
            TESTS=named
            NAMES="${arg#--test=}"
            ;;
        --package) PACKAGE=yes ;;
        -*)
            echo "Unknown option: $arg (use --test, --test-all, --test=NAMES, --package or a preset name)" >&2
            exit 2
            ;;
        *) PRESET="$arg" ;;
    esac
done
if [ -z "$PRESET" ]; then
    if [ "$(uname -s)" = Darwin ]; then
        PRESET=macos-universal
    else
        PRESET=linux
    fi
fi

"$CMAKE" --preset "$PRESET"
"$CMAKE" --build --preset "$PRESET" --parallel "$JOBS"
if [ "$PRESET" = macos-universal ]; then
    echo "Game exported: mac-package/OpenGoldBox.app"
    if [ "$PACKAGE" = yes ]; then
        "$CMAKE" --build --preset "$PRESET" --target opengoldbox_macos_dmg
    fi
fi
if [ "$PRESET" = linux ] || [ "$PRESET" = linux-release ]; then
    echo "Game exported: linux-package/; package: build/$PRESET/OpenGoldBox-*-linux-x86_64.tar.gz"
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
    named)
        # A misspelt name stops the run rather than leaving its test out.
        for name in $(echo "$NAMES" | tr ',' ' '); do
            if ! ctest --preset "$PRESET" --show-only \
                    --tests-regex "^(opengold_(godot_)?)?$name\$" | grep -q 'Total Tests: [1-9]'; then
                echo "No test is named $name" >&2
                exit 2
            fi
        done
        ctest --preset "$PRESET" -j "$JOBS" --output-on-failure \
            --tests-regex "^(opengold_(godot_)?)?($(echo "$NAMES" | tr ',' '|'))\$"
        ;;
esac
