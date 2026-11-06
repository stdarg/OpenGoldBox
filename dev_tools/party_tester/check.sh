#!/usr/bin/env bash
# Builds the Party tester and runs its headless check (godot/test_party.gd).
# Godot exits 0 when a script fails to load, so the pass line decides.
set -euo pipefail
tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
log="$(mktemp)"
trap 'rm -f "$log"' EXIT
"$tool_dir/run.sh" "${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}" --headless \
    --script res://party_tester/test_party.gd 2>&1 | tee "$log"
grep -q '^Party tester passed$' "$log"
