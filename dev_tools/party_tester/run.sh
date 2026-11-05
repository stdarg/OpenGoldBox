#!/usr/bin/env bash
# Builds the Party tester's extension from the game's own code, stages the
# game's Godot project with the tester's screen in build/party-tester/project
# and opens it. Extra arguments go to Godot (check.sh runs the headless check).
set -euo pipefail

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
build_dir="$repo_dir/build/party-tester"
game_dir="${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}"
godot_bin="${GODOT_BIN:-/Applications/Godot_mono.app/Contents/MacOS/Godot}"
if [[ ! -d "$game_dir" || ! -x "$godot_bin" ]]; then
    printf 'Set OPENGOLD_GAME_DIR to the Pool of Radiance files and GODOT_BIN to Godot 4.\n' >&2
    exit 1
fi

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake -S "$tool_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi
cmake --build "$build_dir" --target opengold_party_tester -j6

project_dir="$build_dir/project"
mkdir -p "$project_dir"
rsync -a --delete --exclude='.godot' --exclude='bin' --exclude='party_tester' \
    "$repo_dir/src/OpenGoldBox/godot/" "$project_dir/"
mkdir -p "$project_dir/bin/portraits" "$project_dir/data/art" "$project_dir/party_tester"
cp "$tool_dir/godot/party_tester.gdextension" "$project_dir/bin/zz_party_tester.gdextension"
cp "$tool_dir/godot/"*.gd "$tool_dir/godot/setup.tscn" "$tool_dir/godot/icon.png" \
    "$project_dir/party_tester/"
sed -e 's|^config/name=.*|config/name="OpenGold Party Tester"|' \
    -e 's|^config/icon=.*|config/icon="res://party_tester/icon.png"|' \
    "$project_dir/project.godot" > "$project_dir/project.godot.icon-tmp"
mv "$project_dir/project.godot.icon-tmp" "$project_dir/project.godot"
# The game's data, as its own build stages it, so a fresh checkout works too.
cp "$repo_dir/data/art/combat-body-looks.tsv" "$repo_dir/data/art/combat-weapon-options.tsv" \
    "$project_dir/data/art/"
for folder in art/missiles art/walls companions encounters monsters rules shops; do
    mkdir -p "$project_dir/data/$folder"
    rsync -a --delete "$repo_dir/data/$folder/" "$project_dir/data/$folder/"
done
rsync -a "$repo_dir/data/art/portraits/" "$project_dir/bin/portraits/"
if [[ ! -s "$project_dir/.godot/extension_list.cfg" ]]; then
    "$godot_bin" --headless --editor --path "$project_dir" --import --quit
fi

export OPENGOLD_GAME_DIR="$game_dir"
if [[ $# -gt 1 ]]; then
    exec "$godot_bin" --path "$project_dir" "${@:2}"
fi
exec "$godot_bin" --path "$project_dir" res://party_tester/setup.tscn
