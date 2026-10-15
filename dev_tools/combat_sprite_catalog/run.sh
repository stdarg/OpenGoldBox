#!/usr/bin/env bash
set -euo pipefail

if (( $# > 1 )) || [[ ${1:-} == --help ]]; then
    printf 'Usage: %s [POOLRAD_DIRECTORY]\n' "$0"
    exit $(( $# > 1 ? 2 : 0 ))
fi
tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
game_dir="${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}"
godot_bin="${GODOT_BIN:-}"
if [[ -z "$godot_bin" ]]; then
    godot_bin="$(command -v godot || true)"
    [[ -n "$godot_bin" ]] || godot_bin=/Applications/Godot_mono.app/Contents/MacOS/Godot
fi
if [[ ! -d "$game_dir" || ! -x "$godot_bin" ]]; then
    printf 'Game directory or Godot executable unavailable. Set OPENGOLD_GAME_DIR and GODOT_BIN.\n' >&2
    exit 1
fi
build_dir="$repo_dir/build/combat-sprite-catalog"
cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build "$build_dir" --target opengold_combat_sprite_catalog -j6
if [[ ! -s "$tool_dir/godot/.godot/extension_list.cfg" ]]; then
    if "$godot_bin" --headless --editor --path "$tool_dir/godot" --import --quit; then
        :
    else
        result=$?
        if [[ $result -ne 134 && $result -ne 139 ]] || [[ ! -s "$tool_dir/godot/.godot/extension_list.cfg" ]]; then
            printf 'Godot import failed (exit %s).\n' "$result" >&2
            exit "$result"
        fi
    fi
fi
export OPENGOLD_GAME_DIR="$game_dir"
exec "$godot_bin" --path "$tool_dir/godot"
