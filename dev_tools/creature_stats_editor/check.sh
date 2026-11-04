#!/usr/bin/env bash
# Builds the editor and runs its headless check (godot/test_editor.gd).
set -euo pipefail
tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$tool_dir/../../build/creature-stats-editor/project"
mkdir -p "$project_dir"
cp "$tool_dir/godot/test_editor.gd" "$project_dir/"
exec "$tool_dir/run.sh" "${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}" --headless --script res://test_editor.gd
