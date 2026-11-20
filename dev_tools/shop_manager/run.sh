#!/usr/bin/env bash
set -euo pipefail

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
build_dir="$repo_dir/build/tile-art-viewer"
project_dir="$tool_dir/godot"

if (( $# > 1 )); then
    printf 'Usage: %s [FOLDER_HOLDING_POOL_OF_RADIANCE]\n' "$0" >&2
    exit 2
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
    printf 'Godot executable not found: %s\n' "$godot_bin" >&2
    exit 1
fi

cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build "$build_dir" --target opengold_tile_art_viewer -j6
mkdir -p "$project_dir/bin"
for ext in dylib so dll; do
    src="$repo_dir/dev_tools/tile_art_viewer/godot/bin/opengold_tile_art_viewer.$ext"
    if [[ -f "$src" ]]; then
        cp "$src" "$project_dir/bin/"
    fi
done
if [[ "$OSTYPE" == darwin* ]] && command -v codesign >/dev/null 2>&1; then
    codesign --force --sign - "$project_dir/bin/opengold_tile_art_viewer.dylib" >/dev/null
fi
if [[ ! -s "$project_dir/.godot/extension_list.cfg" ]]; then
    "$godot_bin" --headless --editor --path "$project_dir" --import --quit || {
        [[ -s "$project_dir/.godot/extension_list.cfg" ]] || exit 1
    }
fi

# A folder named here is looked in first, as OPENGOLD_GAME_DIR names one; the
# tool finds Pool of Radiance's files there or as the game does (docs/TECH.md,
# "Finding the games' folders").
if [[ -n ${1:-} ]]; then
    export OPENGOLD_GAME_DIR="$1"
fi
exec "$godot_bin" --path "$project_dir"
