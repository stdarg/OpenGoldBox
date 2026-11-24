#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Build and run the Tile Art Viewer.

Usage: dev_tools/tile_art_viewer/run.sh [FOLDER_HOLDING_POOL_OF_RADIANCE]

Finds Pool of Radiance's files as the game does: in the folder given, else
OPENGOLD_GAME_DIR's, else upward from the checkout. Set GODOT_BIN to use a
particular Godot 4.5+ executable.
EOF
}

if [[ ${1:-} == -h || ${1:-} == --help ]]; then
    usage
    exit 0
fi
if (( $# > 1 )); then
    usage >&2
    exit 2
fi

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
project_dir="$tool_dir/godot"
build_dir="$repo_dir/build/tile-art-viewer"
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    printf 'CMake and Ninja are required.\n' >&2
    exit 1
fi

godot_bin="${GODOT_BIN:-}"
if [[ -z "$godot_bin" ]]; then
    if command -v godot >/dev/null 2>&1; then
        godot_bin="$(command -v godot)"
    else
        godot_bin=/Applications/Godot_mono.app/Contents/MacOS/Godot
    fi
fi
if [[ ! -x "$godot_bin" ]]; then
    printf 'Godot executable not found: %s\nSet GODOT_BIN to your Godot 4.5+ executable.\n' "$godot_bin" >&2
    exit 1
fi

cmake -S "$repo_dir" -B "$build_dir" -G Ninja -Werror=dev -Werror=deprecated \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build "$build_dir" --target opengold_tile_art_viewer -j6

if [[ ! -s "$project_dir/.godot/extension_list.cfg" ]]; then
    if "$godot_bin" --headless --editor --path "$project_dir" --import --quit; then
        :
    else
        import_status=$?
        if [[ $import_status -ne 134 && $import_status -ne 139 ]] || \
           [[ ! -s "$project_dir/.godot/extension_list.cfg" ]]; then
            printf 'Godot import failed (exit %s).\n' "$import_status" >&2
            exit "$import_status"
        fi
        printf 'Godot crashed on import exit after registering the native extension; continuing.\n' >&2
    fi
fi

# A folder named here is looked in first, as OPENGOLD_GAME_DIR names one; the
# tool finds Pool of Radiance's files there or as the game does (docs/TECH.md,
# "Finding the games' folders").
if [[ -n ${1:-} ]]; then
    export OPENGOLD_GAME_DIR="$1"
fi
exec "$godot_bin" --path "$project_dir"
