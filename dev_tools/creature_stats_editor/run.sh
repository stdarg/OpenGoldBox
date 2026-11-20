#!/usr/bin/env bash
# Builds the Creature Stats Editor's extension from the game's own code, stages
# its Godot project in build/creature-stats-editor/project and opens it.
set -euo pipefail

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
build_dir="$repo_dir/build/creature-stats-editor"
godot_bin="${GODOT_BIN:-/Applications/Godot_mono.app/Contents/MacOS/Godot}"
if [[ ! -x "$godot_bin" ]]; then
    printf 'Set GODOT_BIN to Godot 4.\n' >&2
    exit 1
fi

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake -S "$tool_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi
cmake --build "$build_dir" --target opengold_creature_stats_editor -j6

project_dir="$build_dir/project"
mkdir -p "$project_dir/bin"
cp "$tool_dir/godot/project.godot" "$tool_dir/godot/editor.tscn" "$tool_dir/godot/editor.gd" \
    "$project_dir/"
cp "$tool_dir/godot/creature_stats_editor.gdextension" "$project_dir/bin/"
if [[ ! -s "$project_dir/.godot/extension_list.cfg" ]]; then
    # Godot can crash as the import exits; the import is done once it lists
    # the extension.
    "$godot_bin" --headless --editor --path "$project_dir" --import --quit ||
        [[ -s "$project_dir/.godot/extension_list.cfg" ]]
fi

export OPENGOLD_REPOSITORY="$repo_dir"
# A folder named here is looked in first, as OPENGOLD_GAME_DIR names one; the
# tool finds Pool of Radiance's files there or as the game does (docs/TECH.md,
# "Finding the games' folders").
if [[ -n ${1:-} ]]; then
    export OPENGOLD_GAME_DIR="$1"
fi
export OPENGOLD_EDITOR_BUILD="$build_dir"
export OPENGOLD_CMAKE="$(command -v cmake)"
exec "$godot_bin" --path "$project_dir" "${@:2}"
