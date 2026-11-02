# Claude handoff: combat sprite work outside `dev_tools/`

Read `docs/CODEX_COMBAT_SPRITE_WORK.md` for the complete task order, monster IDs,
approval gates, export procedure, and checks. This file assigns only the work
outside `dev_tools/` to Claude. Codex owns catalog JSON, replacement art, and
art review inside `dev_tools/`. Coordinate one monster at a time; do not export
or change tests until the user has approved the exact final ready/action PNGs
for every required set on that monster.

## Per-monster delivery

After Codex has finished review and the user has approved the exact images:

1. Confirm the approved catalog state is present, including the appropriate
   regeneration flag state and all required pairs. Use the export script in
   `docs/CODEX_COMBAT_SPRITE_WORK.md` to write `data/monsters`. Do not hand-edit
   exported monster data, and do not use the catalog's `--export-to-game` flag.
2. Check `git status --short data/monsters`. Only the current monster's folder
   may change. Stop and report any other changed monster folder.
3. For `MON5CHA.DAX:55`, remove that ID from `art_pending` in
   `tests/monster_bundle_tests.cpp`. For `MON6CHA.DAX:101`, remove that ID.
   Do not change this set for `MON6CHA.DAX:48`, `MON5CHA.DAX:56`, or any of the
   shield-off pairs. Never add an ID merely to make the test pass.
4. Run the targeted checks and full suite specified in the source handoff on
   the integrated catalog, art, export, and test changes. Keep the current
   monster's work in one commit and push it before starting the next monster,
   as directed there. Report the commit, test results, and any limitation.

The current `art_pending` line, checked 2026-11-02, is:

```cpp
const std::set<std::string> art_pending{"MON5CHA.DAX:55", "MON6CHA.DAX:101"};
```

Recheck it when taking over; other sessions may have changed it. Do not edit
other C++, rules, `combat.rules`, scenes, or other monsters for this handoff.

## Game behavior check

For the first shield-off pair delivered in task 5, fight that monster in the
game, let it remove its shield for a bow or crossbow attack, then have it strike
in melee. Report which sprite appeared. This check belongs with Claude because
it exercises the game outside `dev_tools/`. Any game-code fix discovered by the
check is a separate task requiring user authorization.

## Coordination boundary

Codex can prepare and review art entirely in `dev_tools/`. Claude handles
`data/monsters`, `tests/monster_bundle_tests.cpp`, game verification, and the
integrated commit, checks, and push for each approved monster. Keep the source
handoff's task order: Hill Giant, 5th Lvl Fighter, Dwarven Fighter, Fire Giant,
then the 18 shield bearers. If export or tests fail for a reason outside this
boundary, report it rather than changing unrelated code.
