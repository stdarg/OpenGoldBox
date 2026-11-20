#!/usr/bin/env bash
# Replays the campaign's fights: run.sh [--new-pack] [opengold_ai_lab options].
# The first run, or --new-pack, makes the scenario pack from the original files.
set -euo pipefail

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$tool_dir/../.." && pwd)"
build_dir="$repo_dir/build/ai-lab"
pack_dir="$build_dir/pack"

if [[ "${1:-}" == "--new-pack" ]]; then
    rm -rf "$pack_dir"
    shift
fi
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake -S "$repo_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DOPENGOLD_BUILD_TESTS=ON
fi
cmake --build "$build_dir" --target opengold_expedition_tests opengold_ai_lab
if [[ ! -d "$pack_dir" ]]; then
    # The expedition test finds Pool of Radiance's files as the game does.
    OPENGOLD_AI_SCENARIOS="$pack_dir" "$build_dir/opengold_expedition_tests"
    if [[ ! -d "$pack_dir" ]]; then
        printf "Pool of Radiance's files were not found: set OPENGOLD_GAME_DIR to their folder to make a pack.\n" >&2
        exit 1
    fi
fi
"$build_dir/dev_tools/ai_lab/opengold_ai_lab" --pack "$pack_dir" "$@"
