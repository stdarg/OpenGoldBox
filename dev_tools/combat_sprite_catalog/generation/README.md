# Monster combat sprite generation and review

This is the required workflow for generating or regenerating **bipedal monster
combat sprites**, including a request that only says "regenerate the flagged
monsters." It uses the existing catalog JSON and Godot; no API key, new runtime,
game code or catalog UI changes are required. Other body plans need their own
pose decision; do not force a quadruped into this guide.
The weapon-set coverage rule below applies to **every monster**, regardless of
body plan.

## Approved requirements

The authoritative prompt text is [rules.txt](rules.txt). The user confirmed on
2026-10-17:

- Face **screen right, front three-quarter view, chest partly visible**, in both
  poses. "Right" is the viewer's right edge of the image.
- Melee ready: weapon **held up, ready to strike**, with the appropriate one- or
  two-handed grip. This supersedes the earlier suggestion of a lowered weapon.
- Melee action: near the **end of a swing or thrust**, not the windup.
- Projectile ready: weapon **loaded** (nocked arrow, loaded bolt, stone in sling).
  Action: **just after release**, with the projectile no longer loaded.
- Preserve identity, selected equipment, camera, scale and ground alignment.
  Export 128 x 128 transparent pixel-art PNGs, readable at normal combat zoom.
- Register both frames to the same planted-foot x/y coordinates. Animate the
  attack in place; never slide the whole figure between frames.
- Inspect the monster's gear and create a ready/action pair for **every weapon
  set**, including both ranged and melee alternatives (confirmed 2026-10-17).

The independently generated [pose guide](../art/pose-guide/README.md) was
**approved by the user on 2026-10-17** for production generation. Acceptance
checks its recorded approval against the exact guide image hash; a changed guide
requires fresh user approval. This workflow does not certify existing replacement
sprites retrospectively.

## Animation registration

Ready and action are consecutive pictures of **one stationary character**. As
described by [Aseprite's frame-copy workflow](https://www.aseprite.org/docs/new-frame/),
start the next frame from the current one. Use an overlay or
[onion skinning](https://www.aseprite.org/docs/onion-skinning/) to compare them at
the same pixel coordinates. [Godot plays the images as successive sprite
frames](https://docs.godotengine.org/en/stable/classes/class_animatedsprite2d.html);
a changing silhouette does not imply that the character's ground position should
change. The fixed-foot rule below is this project's specific requirement.

- Choose the 128 x 128 canvas, common body scale and one planted-foot anchor
  before creating ready. Leave enough space for the action's weapon sweep.
- Use ready as the action edit target. The planted feet must remain at the same
  **x and y pixel positions**. Keep the hips and most of the torso registered;
  animate the weapon and arms, with only a modest upper-body follow-through.
- Do not center each silhouette independently, shift the whole cutout, crop to
  its changing bounds, or fit either pose separately into the canvas. Matching
  only the lowest occupied row does not prevent a horizontal shuffle.
- The approved pose guide shows facing and attack mechanics. Its feet are **not**
  the registration target: its example action figures have different horizontal
  foot positions. The generated ready sprite establishes the anchor for its pair.
- Inspect an exact-coordinate overlay, both 128 x 128 frames side by side, and
  one-second alternation in the catalog. Reject any visible movement of the
  planted base, even if file checks pass. Review other weapon sets against the
  same character scale and ground position.

## 1. Prepare from the current JSON

Save description and equipment edits in the Combat Sprite Catalog first. Inspect
the **entire current equipment list**, including weapons not currently readied,
then compare the JSON's `variants` with the complete weapon sets it should offer.
Do not silently omit a weapon because it is missing from `variants` or classified
as ordinary gear. Resolve uncertain classification or set membership with the
user before generating art.

A weapon set is the complete setup wielded together for one combat art pair:

- **One-handed weapon + shield:** one unit when a shield is available. Do not
  generate extra shield-only or weapon-without-shield alternatives for that unit.
- **One-handed weapon without a shield:** its own set when no shield is available.
- **Ranged weapon:** its own set, separate from melee equipment.
- **Two-handed weapon:** its own set, separate from the ranged or one-handed set.

| Monster's gear | Required sets | Required images |
| --- | --- | --- |
| Bow, mace, shield | Bow; mace + shield | 2 ready/action pairs (4 PNGs) |
| Bow, sword, no shield | Bow; sword | 2 ready/action pairs (4 PNGs) |
| Crossbow, greatsword | Crossbow; greatsword | 2 ready/action pairs (4 PNGs) |
| Bow, mace, shield, greatsword | Bow; mace + shield; greatsword | 3 ready/action pairs (6 PNGs) |

Enumerate all sets as a completion checklist before generation. For multiple
one-handed weapons or shields, follow the catalog's existing set list (each
one-handed weapon paired with each available shield); ask if an unusual loadout
makes a combination unclear. Generate, review and accept **both images for each
set**. A currently selected or readied set does not limit generation coverage.

Resolve conflicts between the description and equipment with the user. For an
unarmed, magical or unusual attack, ask for the pose behavior if unspecified.

From the repository root, in bash on macOS:

```bash
godot_bin="${GODOT_BIN:-/Applications/Godot_mono.app/Contents/MacOS/Godot}"
tool_dir="$PWD/dev_tools/combat_sprite_catalog"
metadata_path="$tool_dir/json/mon1cha_dax_6.json"
review_dir="$PWD/build/sprite-review/hobgoblin-long-sword-v1"

"$godot_bin" --headless --path "$tool_dir/godot" \
  --script res://scripts/sprite_art_workflow.gd -- \
  prepare "$metadata_path" item_000__one_hand "$review_dir"
```

Set `GODOT_BIN` to your Godot 4 executable on other installations. No extension
build, editor import or original-game files are needed for these commands.
Use absolute metadata/staging paths. Choose a **new** staging directory each
time; preparation refuses to overwrite existing work.
Run preparation separately for **every enumerated set**, using its variant ID
and its own staging directory. The command prepares one pair at a time.

The command reuses the catalog's equipment-set builder and writes:

- `ready-prompt.txt` and `action-prompt.txt`: the full shared specification,
  current monster description, selected equipment and pose-specific instructions.
  Other weapon sets are excluded. Reuse the entire prompt on every attempt.
- `review.json`: source/set identifiers, intended install paths, input hashes,
  and an initially incomplete visual checklist. It is separate from the editable
  monster JSON, so the catalog cannot accidentally discard review information.

The commands never write the catalog JSON, clear regeneration flags or install
art. The monster's current JSON stays authoritative. A changed description or
equipment list requires a fresh packet and review before acceptance.

## 2. Generate and inspect the pair

1. Read the specification and approved guide. Use the built-in image generator,
   requesting a genuine transparent background. Do not supply any original game
   sprite, extracted image, or original-art contact sheet as a reference.
2. Generate **ready** with the complete ready prompt. Attach the approved guide
   as a pose reference. For another weapon set, also attach this monster's first
   approved **new** ready sprite as the identity reference. Name each reference's
   role explicitly. A filename in prompt text does not attach an image.
3. Inspect ready for facing, raised melee guard or loaded projectile, grip,
   equipment and identity. Retry until it passes before creating action.
4. Derive **action** by editing this set's approved new ready sprite with the
   complete action prompt and the guide as pose reference. Keep the canvas
   origin and planted-foot pixels fixed. Change the weapon, arms and only the
   modest upper-body motion needed for a late strike or just-released projectile.
   Reject a newly composed figure that merely resembles ready or shifts as a
   whole. Inspect identity, facing and gear as well as attack timing.
5. Before export, compare the full silhouettes and the body landmarks in both
   working images. Set the common scale from whichever pose occupies more space,
   then place both poses on the same canvas with transparent margins. Keep the
   head and torso the same apparent size and keep the planted feet at the same
   x/y pixels. A shared baseline without horizontal registration is not enough.
   If one generated figure is intrinsically smaller, regenerate that frame; a
   shared export transform cannot fix mismatched character proportions. Do not
   recenter, crop, translate or rotate either pose independently. Export
   `ready.png` and `action.png` with nearest-neighbor sampling and preserved
   alpha. Never crop or fit the two poses independently to their visible bounds.
6. Save any added attack-specific prompt text in the prompt files. Inspect the
   final 128 x 128 exports, not only the larger working images. Reject defects
   and regenerate the failing frame. Regenerating ready also requires rechecking
   action against the new reference.

A prompt cannot guarantee a correct first attempt. The required review and
retry loop determine whether a result is accepted.

## 3. Run file checks and review visually

```bash
"$godot_bin" --headless --path "$tool_dir/godot" \
  --script res://scripts/sprite_art_workflow.gd -- check "$review_dir"
```

The check returns a nonzero exit code for missing or invalid PNGs, wrong
dimensions, no real transparency, empty images, occupied border pixels (possible
clipping), or identical ready/action pixels. It prints each file's SHA-256 hash.
**It does not recognize facing, grip, anatomy, correct alpha cutouts or poses.**
Those require visual review; a technical pass is not acceptance.

Review the pair overlaid at native pixel coordinates, side by side at normal
combat size, and alternating every second in the catalog. Confirm the planted
feet stay at the same x/y coordinates and the whole body does not slide or jump.
Compare other weapon sets for identity, scale and foot position.
To preview before installation, use a separate worktree/copy of the catalog,
copy the staged images to that copy's `install_paths`, and run its existing
`./dev_tools/combat_sprite_catalog/run.sh`. Keep the main checkout's accepted art
until the staged pair passes. The catalog already reloads changed PNGs.

In `review.json`, set each `visual_checks` entry to `true` **only after inspecting
it**. Enter `reviewer`, an ISO date/time in `reviewed_at`, and the exact inspected
`ready_sha256` and `action_sha256` from the check. Use `notes` for relevant pose
decisions, references and comparisons. Review every weapon set independently.

## 4. Accept, then install

```bash
"$godot_bin" --headless --path "$tool_dir/godot" \
  --script res://scripts/sprite_art_workflow.gd -- \
  accept "$review_dir" "$metadata_path"
```

Acceptance requires passing file checks, explicit user approval of the exact
pose-guide image, unchanged specification and source JSON, matching source/set
and destination paths, complete visual attestations, and matching PNG hashes.
Replacing an image invalidates its previous review. The command verifies recorded
review; it cannot determine whether a reviewer truthfully inspected the art.

After acceptance, copy the pair together to the two `install_paths` (relative to
`dev_tools/combat_sprite_catalog`), and keep the prompts and `review.json` beside
the installed pair. Run acceptance again against the installed directory and
current JSON. Preserve the previous accepted pair until this succeeds. Clear
**Flag for Regen** through the catalog's Save only after the full weapon-set
checklist is complete: **every set has an accepted ready AND action image**.
An explicitly requested single-set revision leaves other outstanding sets on
the checklist. This later flag-only JSON change naturally invalidates the earlier
input hash; the saved review remains a historical record of the generation input.
Future regeneration always prepares from the then-current JSON.

## Regression checks

```bash
"$godot_bin" --headless --path "$tool_dir/godot" \
  --script res://../generation/test_workflow.gd
```

These synthetic tests require no original game art or native extension. They
exercise prompt preparation, paired equipment selection, file rejection, pending
guide approval, complete acceptance and invalidation after image/input changes.
