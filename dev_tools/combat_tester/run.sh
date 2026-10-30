#!/usr/bin/env bash
set -euo pipefail

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
build_dir="$repo_dir/build/combat-tester"
game_dir="${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}"
godot_bin="${GODOT_BIN:-/Applications/Godot_mono.app/Contents/MacOS/Godot}"
if [[ ! -d "$game_dir" || ! -x "$godot_bin" ]]; then
    printf 'Set OPENGOLD_GAME_DIR to the Pool of Radiance files and GODOT_BIN to Godot 4.\n' >&2
    exit 1
fi

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake -S "$tool_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi
cmake --build "$build_dir" --target opengold_combat_tester -j6

project_dir="$build_dir/project"
mkdir -p "$project_dir"
rsync -a --delete --exclude='.godot' --exclude='bin' \
    "$repo_dir/src/OpenGoldBox/godot/" "$project_dir/"
mkdir -p "$project_dir/bin" "$project_dir/data/art" "$project_dir/data/rules" "$project_dir/combat_tester"
mkdir -p "$project_dir/bin/portraits"
cp "$tool_dir/godot/combat_tester.gdextension" "$project_dir/bin/zz_combat_tester.gdextension"
cp "$tool_dir/godot/"*.gd "$tool_dir/godot/setup.tscn" "$project_dir/combat_tester/"
cp "$repo_dir/data/art/combat-body-looks.tsv" "$repo_dir/data/art/combat-weapon-options.tsv" "$project_dir/data/art/"
rsync -a --delete "$repo_dir/data/rules/srd-5.2.1/" "$project_dir/data/rules/srd-5.2.1/"
rsync -a --delete "$repo_dir/data/monsters/" "$project_dir/data/monsters/"
rsync -a "$repo_dir/data/art/portraits/" "$project_dir/bin/portraits/"
if [[ ! -s "$project_dir/.godot/extension_list.cfg" ]]; then
    "$godot_bin" --headless --editor --path "$project_dir" --import --quit
fi

export OPENGOLD_GAME_DIR="$game_dir"
export OPENGOLD_COMBAT_TEST_CONFIG="$tool_dir/config/parties.json"
exec "$godot_bin" --path "$project_dir" res://combat_tester/setup.tscn
