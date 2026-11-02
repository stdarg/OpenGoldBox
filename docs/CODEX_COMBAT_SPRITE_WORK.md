# Codex: combat sprite work

Written 2026-11-02, against commit `7d49827b`. Check the facts below against the
current files before acting. If one no longer holds, **stop and ask the user**.

## Why this work exists

A monster can fire a ranged attack only when its gear allows it, and it shows a
ranged sprite only when it has exported art for that weapon. These lists can
disagree. When they do, the monster fires while still drawn with the wrong
weapon. The game code is fixed. What remains is art and catalog data, and that is
your job.

## What to do, in order

| # | Monster | Work | Status |
|---|---|---|---|
| 1 | Hill Giant, `MON5CHA.DAX:55` | Review existing art, clear flag, export | Ready |
| 2 | 5th Lvl Fighter, `MON6CHA.DAX:101` | Re-review the spear art, clear flag, export | Ready |
| 3 | Dwarven Fighter, `MON6CHA.DAX:48` | Finish the review, clear flag, export | Ready |
| 4 | Fire Giant, `MON5CHA.DAX:56` | Add a boulder and generate new art | Ready |
| 5 | 18 shield-bearers | Melee art without a shield | **BLOCKED: do not start** |

Do one task per commit. Finish and push a task before starting the next.

## Rules you must not break

1. **Read first.** Read these in full before touching anything:
   `dev_tools/combat_sprite_catalog/generation/README.md`,
   `dev_tools/combat_sprite_catalog/generation/rules.txt` and
   `dev_tools/docs/combat-sprite-catalog.md`. They govern every image.
2. **Stay in scope.** Change only the files this document names. Do not change
   C++ (one line of a test in tasks 1, 2 and 4 is the only exception), rules,
   `combat.rules`, scenes, or any other monster.
3. **Never touch art that is not named in your task.** Do not regenerate art
   "while you are there". Do not delete old art folders, such as
   `art/mon5cha_dax_55/default`, `art/mon6cha_dax_101/default` or
   `art/mon6cha_dax_48/item_002__one_hand`.
4. **Never edit `data/monsters` by hand.** It is only ever written by the export.
5. **Never use original game art as a reference.** That includes extracted
   sprites, contact sheets and the `default` art of a record if it came from the
   original.
6. **The user clears the way, not you.** Clearing `flag_for_regeneration` puts the
   art in the game. Before you clear any flag, show the user the final `ready.png`
   and `action.png` for every set on that monster, and wait for their approval.
   Their approval applies only to those exact images. If you change an image, ask
   again.
7. **Tick a review item only if you really inspected it.** A passing file check
   does not prove the art is correct. Do not copy old `review.json` answers, and
   do not set a check to `true` to make `accept` pass.
8. **Do not bypass failures.** No `--no-verify`, no deleting or weakening a test.
   If a check fails and you do not understand why, stop and tell the user.
9. **If an instruction here seems to conflict with the workflow README, stop and
   ask.** Do not choose for yourself.

## Set up, once per session

```bash
cd /Users/edmond/src/OpenGold
git pull --ff-only origin main
git worktree add -b codex/combat-sprite-work ../OpenGold-codex-sprites origin/main
cd ../OpenGold-codex-sprites
tools/session_board.py claim --task "Combat sprite work" \
  --areas "dev_tools/combat_sprite_catalog, data/monsters, tests/monster_bundle_tests.cpp"
```

Before each task, bring the worktree up to date: `git rebase origin/main`.

Set these in every shell you use:

```bash
godot_bin=/Applications/Godot_mono.app/Contents/MacOS/Godot
tool_dir="$PWD/dev_tools/combat_sprite_catalog"
export OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD
```

## How to finish any task

Do these after the user has approved the art.

1. Clear **Flag for Regen** through the catalog's Save
   (`./dev_tools/combat_sprite_catalog/run.sh`). Tasks 1 to 3 change that one
   field: confirm with `git diff` that nothing else in the JSON moved.
2. Export, using the script in **The export script** below. It must print `Exported`.
3. Check that the export changed only your monster:
   ```bash
   git status --short data/monsters
   ```
   If any other monster's folder appears, **stop**. Do not commit it, and tell
   the user.
4. Update the test, if the task says to (see below).
5. Run the targeted checks:
   ```bash
   cmake --preset macos-universal -DOPENGOLD_GAME_DIR="$OPENGOLD_GAME_DIR"
   cmake --build build/macos-universal
   ctest --test-dir build/macos-universal \
     -R "opengold_monster_bundle_tests|opengold_monster_export_check|opengold_godot_weapon_sets" \
     --output-on-failure
   ```
6. Run the full suite, which the project requires before every push to main:
   ```bash
   ctest --test-dir build/macos-universal -j8 --output-on-failure
   ```
   Every test must pass. Report any failure to the user rather than pushing.
7. Commit the catalog JSON, art, `data/monsters` and the test together, then push:
   ```bash
   git push origin HEAD:main
   ```
8. When all the tasks you are doing are pushed:
   `tools/session_board.py release`.

### The export script

This rewrites `data/monsters`. Use it and **not** the catalog's `--export-to-game`
flag, which hung for over three minutes in a fresh worktree when tried on 2026-11-02.
On the unchanged catalog, this script changed no file in `data/monsters`.

```bash
mkdir -p build
cat > build/export_monsters.gd <<'SCRIPT'
extends SceneTree

const MonsterExport = preload("res://scripts/monster_export.gd")

func _init() -> void:
    var error: String = MonsterExport.export_all()
    if not error.is_empty():
        printerr(error)
        quit(1)
        return
    print("Exported")
    quit()
SCRIPT
"$godot_bin" --headless --path "$tool_dir/godot" --script "$PWD/build/export_monsters.gd"
```

### The test you must update

`tests/monster_bundle_tests.cpp` has this line:

```cpp
const std::set<std::string> art_pending{"MON5CHA.DAX:55", "MON6CHA.DAX:101"};
```

It lists monsters that carry a ranged weapon but have no exported art for it. The
test **fails on purpose** when a listed monster gains that art, and also when an
unlisted monster lacks it.

- Task 1: remove `"MON5CHA.DAX:55"`.
- Task 2: remove `"MON6CHA.DAX:101"`.
- Tasks 3 and 4: leave the line alone.

Never add a monster to this list to make the test pass.

## Task 1: Hill Giant, `MON5CHA.DAX:55`

**Files:** `json/mon5cha_dax_55.json`, set `item_000__ranged`.

**Facts:**
- It is flagged because the user asked for a new action pose on 2026-10-24: *the
  giant has just thrown the boulder, leaning right, with the throwing arm
  outstretched.*
- Art was installed on 2026-10-25, and `review.json` matches the current PNGs. The
  flag was never cleared.
- The record has one set only. **Do not add a club.** `MON2CHA.DAX:55` has a club;
  this one does not.

**Do this:**
1. Open the pair in the catalog and alternate it for at least a few cycles.
2. Check it against the user's direction above. The boulder must be gone from the
   hand in `action.png`, and the arm that held it in `ready.png` must be the arm
   now outstretched. Apply the throwing-arm identity gate in the workflow README.
3. Show the user the pair and ask for approval.
4. If they approve, follow **How to finish any task**, and remove this monster from
   `art_pending`.
5. If they reject it, stop and ask what to change. This record's description has no
   `IMAGEGEN PROMPT` block, and `prepare` refuses to run without one, so you must
   save the full prompt in the description first.

## Task 2: 5th Lvl Fighter, `MON6CHA.DAX:101`

**Files:** `json/mon6cha_dax_101.json`, sets `added_5278681468__added_5264957017`
(spear and shield) and `added_5249940073__ranged` (heavy crossbow).

**Facts:**
- This monster currently fires a crossbow with no crossbow sprite. It is the most
  visible bug in this document.
- Its spear `ready.png` was changed on 2026-10-25, after the review. **That set's
  `review.json` does not match the current image**, so its old approval is void.
- The commit that flagged it says the corrected spear grip was staged for catalog
  review. Nobody has recorded that review.
- The spear must read as a melee spear, not a thrown one.
- The crossbow's review matches its PNGs, but inspect it anyway.

**Do this:**
1. Review the spear pair from scratch: grip, spear shape, shield, scale, and planted
   boots at the same pixels in both frames.
2. Compute the current hashes and record them in that set's `review.json` with your
   name, the date and accurate `visual_checks`. Run `check` and `accept` as the
   workflow README describes.
3. Inspect the crossbow pair: loaded bolt in `ready.png`, bolt gone in `action.png`,
   the body at the same scale as the spear set.
4. Show the user both pairs and ask for approval.
5. If they approve, follow **How to finish any task**, and remove this monster from
   `art_pending`.

Two sets must both pass. Do not clear the flag with one done.

## Task 3: Dwarven Fighter, `MON6CHA.DAX:48`

**Files:** `json/mon6cha_dax_48.json`, set `added_61186225371__two_hand`.

**Facts:**
- Its `review.json` matches its PNGs, but
  `pair_reviewed_alternating_in_catalog` is `false`, so the review is unfinished.
- It carries no ranged weapon, so it is not in `art_pending` and the shot bug does
  not affect it. It is flagged only because its art is unfinished.
- It is a Small figure, `height_feet` 4. Check that height in both frames.
- It holds **one two-handed battle axe**. No shield, sword or gauntlets.

**Do this:**
1. Alternate the pair in the catalog. Check height, palette, torso continuity and
   planted boots.
2. Set that check to `true` only if you did this and it passed. Update the reviewer
   and date.
3. Show the user the pair and ask for approval.
4. If they approve, follow **How to finish any task**.

## Task 4: Fire Giant, `MON5CHA.DAX:56`

The user approved a new boulder-throwing sprite. Today this giant throws boulders
while drawn holding only a halberd.

**Files:** `json/mon5cha_dax_56.json`, new set `added_<digits>__ranged`.

**Do not change:** the halberd set `added_164509205__two_hand`, the `default` folder,
or `innate_ranged fire-giant boulder` in `combat.rules`. The halberd stays readied,
so it stays the sprite the giant starts in.

**Step 1: add exactly one item.** In the catalog, use the add-gear control with the
original item type **Boulder** (type 87). Do not choose "Other gear (art only)": that
item would not count as a ranged weapon. After Save, the new entry must look like
this, and the `added_` number will differ:

```json
{
  "id": "added_1234567890",
  "kind": "weapon",
  "name": "Boulder",
  "readied": false,
  "source_hands": -1,
  "source_range": -1,
  "source_type": 87,
  "weapon_role": "ranged"
}
```

**The catalog will not give you this by default.** It classifies type 87 as `gear` with
no role, so you must change the row to a ranged weapon, as the Hill Giants' first
boulder is. If the catalog has no control for that, stop and ask the user. **`readied`
must be `false`.** Do not add the second "gear" boulder the Hill
Giants have.

Saving rebuilds `variants`. The record must end up with exactly two: the existing
halberd set and the new `added_<digits>__ranged`, with `shield_id` empty. If the
halberd entry changed in any way, stop.

**Step 2: prepare.** Save your exact image prompt in the description, under an
`IMAGEGEN PROMPT` heading, and **keep the description's existing text**. Then:

```bash
metadata_path="$tool_dir/json/mon5cha_dax_56.json"
review_dir="$PWD/build/sprite-review/fire-giant-boulder-v1"
"$godot_bin" --headless --path "$tool_dir/godot" \
  --script res://scripts/sprite_art_workflow.gd -- \
  prepare "$metadata_path" added_<digits>__ranged "$review_dir"
```

Use a new `review_dir` for every attempt.

**Step 3: generate.** Requirements for this pair:

- **The same giant.** Use this record's approved `ready.png` from
  `added_164509205__two_hand` as the identity reference, and name its role in the
  prompt: dark basalt skin with subtle ember cracks, ember-red hair and beard, dark
  forged iron armor. Do not use the Hill Giant as a reference.
- **`ready.png`:** both hands or the throwing hand hold one boulder raised, ready to
  throw. **No halberd anywhere**, not even on its back.
- **`action.png`:** just after release. **The boulder is gone**, because the game
  draws the flying boulder itself. The arm that held it is outstretched toward screen
  right at shoulder height or higher, and the giant leans right.
- **Throwing arm:** apply the throwing-arm identity gate. Trace the same shoulder from
  ready to action, and record that trace in `review.json` `notes`.
- **Size:** Tall, about 1.7 squares high, in a footprint one square wide by two tall.
  The body and feet stay in the footprint in both frames. Compare its height against
  the halberd set: it must not look smaller or larger.
- **Planted feet:** the same x and y pixels in both frames. Make `action.png` by
  editing `ready.png`, not by drawing a new giant.
- **Output:** 128 × 128, real transparency, nothing touching the border. No flames
  on the boulder, effects, ground, shadow, text, or a second figure.

**Step 4: review.** Follow sections 3 and 4 of the workflow README: `check`, the visual
review on light gray, magenta and dark backgrounds, `review.json`, then `accept`.
Install the pair, and run `accept` again against the installed folder.

**Step 5:** Show the user the final pair next to the halberd pair, and ask for
approval.

**Step 6:** If they approve, follow **How to finish any task**. The flag is already
clear on this record, so this JSON's diff is the new item, the new variant and the
description. Do not touch `art_pending`: the test already expects this monster to
have art once it carries a boulder, which is why the item and the art must land in
the **same commit**.

**Step 7: tell the user** the Fire Giant is ready to see in play. Valjevo Castle is
one place it appears.

## Task 5: melee art without a shield (BLOCKED)

**Do not start this. Do not generate anything for it.** It needs code work first, and
that work is not yours. Tell the user when you reach this point.

**Why it is blocked:**
- A monster holding a shield now takes it off, for the rest of the fight, before it
  shoots a two-handed weapon. If it then fights in melee, its sprite still shows the
  shield. The fix is melee art without one.
- The catalog cannot save such a set. `rebuild_variants` in
  `godot/scripts/monster_metadata.gd` offers a one-handed weapon alone only when the
  creature has no shield at all, and the next catalog Save would discard a set added
  by hand. **Do not hand-edit it in, and do not change `rebuild_variants`.**
- The workflow README forbids these sets: "Do not generate extra shield-only or
  weapon-without-shield alternatives."
- The game would not show them yet: `attack_variants` always picks the set with the
  shield.
- The user has not yet decided whether a monster's Armor Class changes with the shield.

Wait until the user tells you the catalog, workflow and game have been updated.

**The monsters it will cover, for reference.** Each needs one new pair of the weapon
named, identical to its existing set except that the shield is gone and the free arm
looks natural.

| Catalog file | Monster | Melee weapon | Existing set with shield |
|---|---|---|---|
| `mon1cha_dax_2` | Goblin Guard | Short sword | `item_000__item_001` |
| `mon1cha_dax_3` | Goblin Leader | Short sword | `item_002__item_003` |
| `mon1cha_dax_4` | Orc | Longsword | `added_50856018__added_88447737` |
| `mon1cha_dax_44` | Orc | Short sword | `added_12402564125__added_12435671922` |
| `mon1cha_dax_7` | Hobgoblin Leader | Long sword | `item_000__added_4050213316` |
| `mon2cha_dax_1` | Kobold Leader | Short sword | `item_002__item_003` |
| `mon2cha_dax_4` | Orc | Long sword | `added_12685407688__added_12725666116` |
| `mon3cha_dax_103` | Warrior | Battle axe | `item_003__item_000` |
| `mon3cha_dax_40` | Nomad | Short sword | `item_002__item_003` |
| `mon4cha_dax_4` | Orc | Long sword | `added_13642017509__added_13649870159` |
| `mon6cha_dax_101` | 5th Lvl Fighter | Spear | `added_5278681468__added_5264957017` |
| `mon6cha_dax_102` | Captain | Long sword +4 | `item_001__item_000` |
| `mon6cha_dax_97` | Bandit | Short sword | `item_002__item_003` |
| `mon7cha_dax_1` | Kobold Leader | Short sword | `item_002__item_003` |
| `mon7cha_dax_40` | Nomad | Short sword | `item_002__item_003` |
| `mon7cha_dax_97` | Bandit | Short sword | `item_002__item_003` |
| `mon7cha_dax_98` | Merchant | Short sword | `item_002__item_003` |
| `mon8cha_dax_1` | Kobold Leader | Short sword | `item_002__item_003` |

`cpic7_dax_12` (Nomad combat icon) also holds a shield and a bow, but it is a shared
icon and is not exported to the game, so it is not on this list.

## Mistakes to avoid

| Do not | Because |
|---|---|
| Clear a flag because the file checks pass | They check pixels and hashes. They cannot see a wrong arm or a floating sword. |
| Flip `flag_for_regeneration` in the JSON and export | That ships unreviewed art to players. |
| Regenerate art that only needed a review | Tasks 1 to 3 need review. Generate only if the user rejects the art. |
| Use a monster's number alone | `MON2CHA.DAX:55` and `MON5CHA.DAX:55` are both Hill Giants with different gear. Always use the full ID and set. |
| Commit the export of other monsters | An unexplained change in `data/monsters` means something else is out of date. Ask. |
| Add the boulder without its art, or its art without the boulder | `opengold_monster_bundle_tests` and `opengold_monster_export_check` will fail. |
| Put a boulder in the Fire Giant's `action.png` | The game draws the flying boulder. A second one would appear. |
| Leave a halberd on the Fire Giant's boulder set | The set shows only what the giant holds for that attack. |
| Crop or center each frame separately | The feet slide and the figure jumps when the frames alternate. |
| Mark `review.json` true without looking | The review is the only thing standing between a bad image and the game. |

## When you report back

For each task, tell the user:

- which monster and which sets you changed, by full ID
- the commit hash
- the targeted and full test results, with any failure quoted
- anything you skipped, could not check, or were unsure about

Do not say a task is done unless the art is approved, exported, tested and pushed.
