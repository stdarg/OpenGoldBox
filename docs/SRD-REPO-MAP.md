# SRD repository map

Navigation reference, verified 2026-09-24. Use symbols rather than stale line
numbers. Update affected entries when ownership moves; this is not a second
architecture document or completion ledger. See [TECH](TECH.md) for boundaries.

| Concern | Implementation entry points | Focused evidence / target |
| --- | --- | --- |
| Character creation, scores, class choices | `src/OpenGold.Rules.Srd5/src/character_rules.cpp`; Core `character_creator.cpp`, `character.cpp`, `character_pool.cpp` | `opengold_character_tests`, `opengold_training_tests` |
| Feature/feat provenance | `src/OpenGold.Rules.Srd5/src/feature_grants.cpp` | `opengold_feature_grant_tests` |
| Skills, tools, languages, starting styles | `src/OpenGold.Rules.Srd5/src/training.cpp` | `opengold_training_tests`; [training](TRAINING.md) |
| Cantrips, books, preparation, source validation | `src/OpenGold.Rules.Srd5/src/spell_access.cpp` | `opengold_spell_access_tests`; per-spell tests |
| Combat, legal commands, profiles, progression, module migration | `src/OpenGold.Rules.Srd5/src/srd5.cpp`: `character_definition`, `legal_commands`, `character_profile`, `validate_saved_grants`, `Session::restore` | `opengold_rules_tests`; affected feature tests |
| Status lifecycles and codecs | `src/OpenGold.Rules.Srd5/src/status_effects.cpp` | `opengold_status_effect_tests` |
| Movement/occupancy | `src/OpenGold.Rules.Srd5/src/combat_grid.cpp` | Locate grid/movement targets in root CMakeLists; [transit](UNCONSCIOUS-TRANSIT.md) |
| Components and weapon/armor rules | `src/OpenGold.Rules.Srd5/src/spell_components.h`; adjacent catalog headers | `opengold_spell_component_tests`, `opengold_weapon_catalog_tests`, `opengold_armor_catalog_tests` |
| Physical thrown inventory | SRD `Session::throw_weapon`, `ground_one`, combat 19; Core `CampaignParty::apply_physical_items` | `opengold_thrown_weapon_tests`, `opengold_godot_thrown`; [packet](THROWN-WEAPONS.md) |
| Campaign handoff, resources and saves | `src/OpenGold.Core/src/campaign_party.cpp`, `campaign_rest.cpp`, `campaign_save.cpp`, `combat_demo.cpp` | `opengold_campaign_rest_tests`, `opengold_rest_resource_tests`, `opengold_save_tests` |
| Creator and training controls | `src/OpenGoldBox/character_creation_view.cpp`, `training_control.h`, `cantrip_control.h` | Asset-backed `tests/*cantrip_view_tests.gd`; [training](TRAINING.md) |
| Combat UI and decisions | `src/OpenGoldBox/combat_view.cpp` | `tests/*view_tests.gd`; registered targets in `src/OpenGoldBox/CMakeLists.txt` |
| Advancement and sheets | `src/OpenGoldBox/level_up_view.cpp`, `character_sheet_view.cpp` | Feature-specific player-path checks and advancement checks |
| Demo equivalents | `demos/src/OpenGold.Godot/` | Build `opengold_godot` in `build/sprite-demo`; inspect reuse before duplicating edits |
| Localization | `src/OpenGoldBox/godot/locale/dynamic_sources.json`, `en.po`, `es.po`; `tools/localization.py` | `python3 tools/localization.py --check` |
| Test registration and historical saves | Root `CMakeLists.txt`, `src/OpenGoldBox/CMakeLists.txt`, `tests/fixtures/README.md` | Prior-writer provenance and exact continuation; never regenerate old fixtures with a new writer |

Core paths in the table are relative to `src/OpenGold.Core/src/` where shortened.
Rules own SRD semantics; Core owns campaign orchestration; Godot presents queries
and commands. C++20/RAII throughout; no new stack or general rules framework.

## Local build environment

- Windows and macOS are both supported; main build `build` (the `default`
  preset), demo build `build/sprite-demo`.
- Godot `/Applications/Godot_mono.app/Contents/MacOS/Godot`.
- Main project `src/OpenGoldBox/godot`; original assets `/Users/edmond/POOLRAD`.
- Prepare `opengoldbox_test_project` after final native/scene/localization edits.
  Only then use `--fixture-exclude-setup godot_project` for affected Godot tests.
- Godot tests run serially because they share user-data checkpoint paths.
- Asset-backed creator checks need `OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD`;
  they are not all registered in the asset-free CTest suite. Use
  `tests/run_godot_test.cmake`, which detects script failures as well as exit codes.
- Creator training appears in `Description`; modifier details use `ModifiersModal`.
  Back/Next refreshes creator locale-dependent text.

Use [workflow](SRD-WORKFLOW.md) for commands/check selection and
[coverage](SRD-COVERAGE.md) for delivery evidence. Discover target names with
`ctest --test-dir build -N` or targeted CMake searches; do not assume a
previous build contains newly added targets.
