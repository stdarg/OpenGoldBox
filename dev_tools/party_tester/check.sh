#!/usr/bin/env bash
# Builds the Party tester and runs its headless check (godot/test_party.gd).
set -euo pipefail
tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$tool_dir/run.sh" "${1:-${OPENGOLD_GAME_DIR:-$HOME/POOLRAD}}" --headless \
    --script res://party_tester/test_party.gd
