#!/usr/bin/env bash
# Builds the Party tester and runs its level sweep (godot/level_sweep.gd): every
# class and pair of classes from level 1 to 15 through the game's level-up
# dialog, with fights at levels 1, 5, 10 and 15. See README.md for its settings.
# Godot exits 0 when a script fails to load, so the pass line decides.
set -euo pipefail
tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
log="$(mktemp)"
trap 'rm -f "$log"' EXIT
"$tool_dir/run.sh" "${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}" --headless \
    --script res://party_tester/level_sweep.gd 2>&1 | tee "$log"
grep -q '^Level sweep passed$' "$log"
