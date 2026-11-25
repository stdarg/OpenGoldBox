# Architecture review — 2026-11-25

A review of the C++ under `src/`, `tests/` and `tools/` for DRY and SOLID
antipatterns and for departures from Scott Meyers' *Effective Modern C++*,
using the vocabulary of [CPP_ANTIPATTERNS.md](CPP_ANTIPATTERNS.md) (each entry
names its C++ Core Guidelines rule). Leads came from clang-tidy (LLVM 23,
`modernize-*`, `performance-*`, `cppcoreguidelines-*`, `misc-*`, `bugprone-*`,
`readability-*`), from a duplicate-block scan over the tree, and from reading
the large files. Every lead was checked in the source before anything changed;
the clang-tidy run had failed to parse 47 translation units, and most of its
"non-const global variable" and "could have internal linkage" reports there were
function declarations.

Five reviewers worked one library each, in the order of the layering in
[TECH.md](TECH.md): Formats with the tools and test support, the rules
interface with the original rules, the SRD rules module, Core, and the Godot
game layer. The fixes are 35 commits of 153 files. Behavior is unchanged:
same outputs, same saves, same text, and no dice roll or random draw moved,
which the fixed-dice whole-campaign route tests replay.

## Warnings as errors

Already in place before this review: `cmake/project_warnings.cmake` applies
`-Wall -Wextra` (Clang adds `-Wshorten-64-to-32`, MSVC `/W4 /permissive-`)
with `COMPILE_WARNING_AS_ERROR` to every target of the game, the demos and the
dev tools, and `build.sh` configures with `-Werror=dev -Werror=deprecated`.
The review found that main at `0cc397a53` did not build on Apple clang under
that policy (`-Wunused-lambda-capture` on a `constexpr` captured in
`native_rank_target`); the one-line fix is the first commit of this review.

## Fixed

### DRY

- **Record decoders** (`src/OpenGold.Formats/src/*_records.cpp`,
  `pool_radiance.cpp`; C.1, F.1): eight per-game decoders each carried their
  own signed-byte reader, class-level reader, attack-and-damage block, the
  interleaved permanent/current ability read, the saving-throw block and the
  13-byte fight tail. They share `record_fields.h` now; each decoder keeps only
  its offsets and quirks. A fingerprint of every creature, item and effect
  record of all twelve installed games is byte-identical before and after.
  Dark Queen's two decoders share one spell-block reader; Death Knights' item
  list reuses the Late-games widening; two music decoders use the shared
  little-endian reader; the EGA picture and combat-icon decoders share their
  header check and pixel reader (art checksums identical).
- **Krynn rules** (`champions_rules.cpp`, `death_knights_rules.cpp`,
  `dark_queen_rules.cpp`): three copies of the race index, the cleric's deity
  gifts and deity thac0 favor, two of the class-progression lookup, the
  known-spell adder and druid spell learning are one each.
- **Core hosts** (`phlan_session.cpp`, `vga_host.cpp`): `show_hall` and
  `show_temple` were literal copies of `show_hall_menu`; the five-line "wait
  for Continue" block (four copies) and the six-line "enter shopping" block
  (three) are `await_continue` and `enter_shop`; the predicate
  `vga_engine(game) || game == dark_queen` (eight spellings) is
  `vga_engine(game)`, which already includes Dark Queen; three `snprintf`
  buffers spelling an Unlimited Adventures library name are
  `ua::loose_library_name`.
- **SRD scope guards** (`srd5.cpp`): six hand-written one-shot guard structs
  are two classes in `scope_guards.h`, `ResetAtExit<T>` and `SetWhileAlive`.
- **Game layer**: eleven identical Escape-key handlers are
  `presentation::escape_pressed` (`godot_keys.h`); seven copies of the modal
  window setup are `attach_modal_dialog`; ten getter/saver pairs in
  `application_settings.cpp` that differed only in key and default are
  `game_switch`/`save_game_switch`; the seven per-game Options rows of
  `game_menu_view.cpp` (handlers, connects, selects, visibility list) are one
  table; a 27-branch id-to-label chain (`training_control.h`) and a 45-way
  ternary chain (`combat_weapon_controls.h`) are tables.

### SOLID

- **Single responsibility by file.** `srd5.cpp` 36,125 → 30,804 lines: spell
  traits, the class spell lists, equipment rules, the combat model (definition,
  actor, resources, spell costs, conditions, geometry) and the character
  recipe are their own translation units with lean headers.
  `combat_session.cpp` 16,328 → 16,025: line geometry, outlook dice and the
  checkpoint text codec left. `rules_module.cpp` 4,554 → 4,277: the
  level-history entry codec is `history_entries.cpp`. `combat_demo.cpp`
  3,640 → 1,210: the 2,400-line computer player is `combat_player.cpp`, the
  square arithmetic both use `combat_geometry.cpp`. `campaign_party.cpp`
  4,821 → 4,330: original item-record conversion is `item_conversion.cpp`.
  `rolf_tour_view.cpp` 4,801 → 4,454: the ending film is
  `rolf_tour_ending.cpp`, and three `.h` files of member definitions included
  at the bottom of a `.cpp` (SF.8) are translation units.
- **Boolean flags selecting behavior**: `original_temple_offerings(bool,
  bool)` takes the game; the catalog decides which games add Heal and
  Restoration.
- **Rules knowledge in the view**: shop item colors come from the theme, not
  from `Color` constants in `rolf_tour_view.cpp`.
- **Node ownership (R.11)**: raw `memnew` + `add_child` with an exception
  window between them (ship combat, ending film, startup) use
  `make_node`/`attach_child`.

### Effective Modern C++

- **Item 17 / C.21, special members**: `DiceTally`, `CombatDemo` (holds the
  campaign edit lock), `RollPurpose`, `TemporaryFile`, `Descriptor`,
  `CreatureData` (pimpl), `Staging`, `Lent`, `ScopedFlag`, `SignalsBlocked`,
  `GodotSoundOutput`, `RestoreLocale`, `CheckSave`, `SavedGameChoice` declare
  all five or delete what they must not have.
- **Items 24–25, forwarding references**: `Run &&`, `Take &&`, `Most &&`,
  `Strike &&`, `Visit &&` parameters that were never forwarded are `const &`;
  `Dice &&` is `Dice &` because `ScriptedDice` is read after the call; the
  tests' `rejects(F &&)` forwards.
- **Items 5–6, `auto`**: deduced pointers are spelled `const auto *`.
- **C.49, members initialized in place**: `LineWalk`, `ShipCombat`, the SRD
  `Session`.
- **`[[nodiscard]]`** on the readers and pure queries whose result is the only
  reason to call them (default-party reader, overland dice, save files, 59
  SRD session queries, 23 original character-rules overrides).
- **ES.50, `const_cast`**: gone from `Fighter::find_effect`/`Session::find`
  (one `first_matching` template deduces constness), `cure_search`, and
  `LevelUpDialog::campaign` (a `const` overload).
- **ES.27, C arrays** in logic are `std::array` or `std::string_view`;
  `older_save_message` is a `constexpr std::string_view`.
- Declaration/definition parameter-name mismatches aligned; file-local helpers
  given internal linkage where they really were file-local; two dev tools
  (`opengold_creatures`, `opengold_census`) report every `std::exception`
  instead of aborting.

## Not fixed: proposals

Each needs the user's decision; none changes behavior by itself, but each is
larger than a surgical commit or crosses a session boundary.

1. **`RulesModule` is a fat interface** (`rules.h`, 1,993 lines, 101
   virtuals, 26 Original-only hooks, 10 SRD-only, 6 that the original module
   implements by throwing). ISP. Split by concern (combat, equipment, checks,
   advancement, rest, camp magic) with `RulesModule` kept as the aggregate so
   Core and both implementers compile unchanged; then narrow Core's parameters
   file by file. About a day.
2. **`Session` in `combat_session.cpp`** (16,025 lines, one class). SRP. Cut
   in order of risk: the class declaration into a private header; Buck Rogers
   ground combat; the area-spell `Forecast`; the checkpoint codec; per-game
   specials; the monster AI. Each a translation-unit split proven by the
   fixed-dice routes; blocked today by the Linux session's unpushed AI work in
   this file.
3. **`Session` in `srd5.cpp`** (24,000 lines, 403 methods). SRP. Same shape:
   `session.h` plus `session_items/spells/commands/attacks/turns/monsters/
   checkpoint.cpp`; `mastery_resolution_impl.h` (a 495-line textual include,
   SF.10) becomes `session_mastery.cpp`. About a day.
4. **`RolfTourSession`** (12,000 lines across five files, 151 data members,
   `campaign_save.cpp` a friend of its privates). SRP/ISP. Hoist each service
   (halls, temples, shops, treasure, vault, camp, Buck Rogers ship) into its
   own object. High risk (save format, three sessions' unpushed work); weeks.
   Cheap first step: split `phlan_session.cpp` by service.
5. **Per-game dispatch chains.** OCP. 105 `game() == ...` tests in
   `phlan_session.cpp`, 50 in `campaign_session.cpp`, 59 in
   `original_rules.cpp` (19 functions that are chains over the per-game
   rule files), three `switch (game)` in `ecl.cpp`. A `constexpr` game-traits
   table in `gold_box_game.h` for Core, a per-game rule-set struct for the
   original rules, one script-engine table for ECL. 2–3 days; the route tests
   catch a mis-transcribed row.
6. **Core depends on the concrete original rules module.** DIP.
   `opengold_core` links `opengold_rules_original` publicly and 20 Core files
   use `original::` (93 uses in `buck_rogers_host.cpp` alone). Cheapest part:
   `original::krynn_game` is a `GoldBoxGame` predicate like `vga_engine` and
   belongs in Formats. The rest needs a documented "original game systems"
   layer or an adapter module.
7. **`english(const Message &)` is written four times** (combat_session,
   srd5, phlan_session, ua_services). One `rules::english` in `message.h`,
   in one coordinated commit (ADL makes a gradual move ambiguous).
8. **Game layer**: the `Translate`/`Text`/`Render` template parameters of
   nine dialog headers have one instantiation each (drop them, move bodies to
   `.cpp`); bonus-action labels are rules knowledge in the view
   (`CombatantView::bonus_actions` should carry them); mutable globals in
   `application_settings.cpp` want a `Settings` object; ~900 lines of `check_*`
   acceptance hooks sit in `rolf_tour_view.cpp`; hard-coded
   `set_custom_minimum_size` rows want `.tscn` templates.
9. **Smaller**: three ways to find `START.EXE`; two 6-bit-to-8-bit colour
   formulas (`level * 255 / 63` vs `level << 2 | level >> 4`) that draw
   different pixels, so a user decision; 64 test files define their own
   `check()` beside `test_support.h`; `spell_table.h` (3,791 lines of
   `inline constexpr`) re-parsed by eight includers; mutable globals in two
   SRD-era tools; `spell_access.cpp`'s two spell-choice appliers share a
   loop; `ua_formats.cpp` holds a dozen record kinds.

## Verification

Each reviewer ran the tests covering each change, then the whole native
suite on its branch (`ctest --preset default`, 230 tests): 229 of 230 on
every branch, the one failure `opengold_dark_queen_earned_ending`, which fails
identically on the untouched base on macOS (built from `git archive`; all 349
party-after-beat lines identical with and without the review's commits). That
route is the Linux session's unpushed work and was reported to it. The
integrated tree ran the whole native and whole game suites once more before
the push; see the commit log.
