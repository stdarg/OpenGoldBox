# Monster combat sprite generation and review

This is the required workflow for generating or regenerating **bipedal monster
combat sprites**, including a request that only says "regenerate the flagged
monsters." It uses the existing catalog JSON and Godot; no API key, new runtime,
game code or catalog UI changes are required. Other body plans need their own
pose decision; do not force a quadruped into this guide.
The weapon-set coverage rule below applies to **every monster**, regardless of
body plan.

## Approved requirements

The authoritative prompt text is [rules.txt](rules.txt). The user approved the
pose requirements on 2026-10-17 and clarified the height and footprint limits
on 2026-10-22:

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
- Take `sprite_size` from the decoded original game combat art: 24 source pixels
  per tile, using the larger width and height of ready/action. SRD creature
  space is not sprite width or height. Do not enlarge a 1 × 2 source sprite to
  2 × 2 merely because its creature is Large.
- Each square on the fixed 3 × 3 combat guide represents **5 × 5 feet**. Use
  the saved `humanoid_size` and, for Small figures, `height_feet` to keep body
  height consistent: Small humanoids stand **2–4 feet** (0.4–0.8 square), with
  goblins typically 3–4 feet and kobolds 2–3 feet; Normal figures,
  including orcs, stand a **full 5 feet** (one square), never 3–4 feet; Tall
  figures such as hill giants aim for about 1.7 squares when their footprint
  permits. Measure planted feet to head, excluding weapons and raised arms.
  N/A is for non-humanoid body plans. Do not shrink a Normal body to clear a
  weapon or make a leader appear taller.
- In **ready**, a raised weapon may extend above the record's `sprite_size`
  footprint into the combat square above. In **action**, the moving attack
  weapon may cross the footprint boundary during its strike. The body, feet,
  shield and other gear remain within the footprint in both poses. The ready
  weapon must not cross a side or bottom boundary. Draw the raised weapon over
  any combat sprite in the square above. Both images still keep every visible
  pixel inside their 128 × 128 PNG canvas with transparent margin.
- For a leader, compare every pose and weapon set against the regular creature
  of its kind in the same archive. Make the leader broader and visually more
  impressive, with richer treatment of its saved armor and weapons. Within a
  1 × 1 Normal footprint, both regular and leader bodies stand 5 feet tall;
  do not shrink the regular creature or push the leader's head above its square.
  Where the saved height tier and footprint leave room, a leader may also be
  modestly taller. Do not invent gameplay equipment to signal rank.

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
- If a generated action moves the base, a deterministic pixel composite may
  transfer its attacking arm and weapon onto the ready figure while retaining
  the ready lower body and planted feet at their exact pixels. Use only newly
  generated character art for this composite. Reject a splice with doubled
  gear, a visible seam, displaced head/body height, or an inconsistent limb.
  Apply any final canvas translation or scale equally to both frames.
- **Torso continuity gate:** Prefer an action edited from the complete ready
  figure. Never attach an independently generated upper-body cutout at a straight
  horizontal waist or chest boundary. If compositing is necessary, retain a
  continuous ready torso from hips through neck and change only the attacking
  arm and its shoulder, or redraw the transition by hand across an overlapping
  region. Inspect the final action enlarged around the neck, chest, abdomen,
  waist, hips and garment folds. Trace both outer contours and the central
  clothing/skin details through that region; reject any offset, gap, doubled
  edge, abrupt pattern break or apparent severing, even if the feet register.
  Then inspect at normal combat size and alternate with ready. A technical PNG
  check, matching silhouette or identical lower-body pixels cannot clear a
  failed torso-continuity gate.
- **Throwing-arm identity gate:** Before editing a thrown-weapon action, mark
  the ready frame's weapon hand and trace that arm back to its shoulder. Mark
  the other arm separately. In action, trace the *same shoulder and arm* to the
  empty hand at the end of the throw; the offhand must not become the apparent
  throwing arm. An extended empty hand alone is insufficient. Reject an action
  that extends the offhand while leaving the former weapon arm cocked or at the
  waist. Check this in the final ready/action pair at combat size, using an
  enlarged annotated comparison when the arm path is hard to read.
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
Use the complete archive-qualified Monster ID, weapon-set ID, and frame when
tracking work; never select a monster by its numeric record alone. If an art
request does not name a frame, it covers **both ready and action**. If it names
only a monster, inspect its saved gear and cover every weapon set. An explicit
single-frame request changes only that frame, followed by review of the pair.

Resolve conflicts between the description and equipment with the user. For a
monster with arms making a bare-handed attack, the action leans slightly toward
screen right and punches or reaches with outstretched arms. A humanoid always
punches. Ask which motion to use for a non-humanoid if the description gives no
direction. Keep the planted feet fixed. For an unspecified magical or other
unusual attack, ask for the ready/action behavior.
A saved wand classified as gear remains gear; show it in a spellcaster's
default casting pair when present rather than silently omitting it.

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
   For a thrown weapon, first mark the ready frame's object, gripping hand,
   elbow, and shoulder on a separate planning copy. Draw an arrow from that
   shoulder toward screen right for the intended empty-hand follow-through.
   Use the unmarked ready sprite as the art reference; a marked copy can guide
   arm identity but its marks must never appear in the export. If a two-handed
   grip is shown, name the primary throwing arm and the supporting arm.
5. Before export, compare the full silhouettes and the body landmarks in both
   working images. Set the common scale from whichever pose occupies more space,
   then place both poses on the same canvas with transparent margins. Keep the
   head and torso the same apparent size and keep the planted feet at the same
   x/y pixels. A shared baseline without horizontal registration is not enough.
   If one generated figure is intrinsically smaller, regenerate that frame; a
   shared export transform cannot fix mismatched character proportions. A raised
   ready weapon may cross only the top tile-footprint boundary; the moving
   action weapon may cross the footprint during its strike. Both bodies remain
   inside the footprint. All pixels stay inside the PNG.
   Do not recenter, crop, translate or rotate either pose independently. Export
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

For a localized weapon correction, start with the exact monster ID, weapon set,
and requested ready/action frames. Keep the accepted body, shield, scale, and
planted-foot pixels registered; compare the unchanged lower body pixel for
pixel after compositing. Do not alter another weapon set to solve this one.
Preserve source alpha through every crop and composite: an erased area must
have alpha zero, not opaque black RGB, and a pasted weapon must retain its
original cutout. Disabling alpha or painting a mask black can create opaque
wedges and boxes even when the PNG passes file checks.

Review the pair overlaid at native pixel coordinates, side by side at normal
combat size, and alternating every second in the catalog. Confirm the planted
feet stay at the same x/y coordinates and the whole body does not slide or jump.
For a thrown weapon, explicitly apply the throwing-arm identity gate above;
do not infer success from the projectile being absent or from an empty extended
hand without tracing which shoulder it belongs to.
For any action made with an edit or composite, apply the torso continuity gate
above to the **exported action PNG** on a contrasting background before recording
the visual review. Reopen the saved PNG after the last export or alpha operation;
reviewing only a working image can miss a newly introduced waist seam.
For every thrown-weapon pair, trace the ready frame from the thrown object
through its gripping hand, wrist, elbow, and shoulder. Then trace that **same**
shoulder through the action elbow and empty hand. Its hand must reach past the
torso toward screen right near shoulder height or higher. Reject the action if
this hand drops, if only the opposite hand extends, if the object remains in
the hand, or if the arm connection is unclear. Record the identified arm and
observed release path in `review.json` notes beside the inspected image hashes;
do not attest `action_late_strike_or_after_release` until this passes. The
technical PNG check cannot establish arm identity or direction.
Inspect the **final exported files**, not an earlier draft, over light gray,
magenta, and dark backgrounds. A dark-only preview can hide black boxes, seams,
halos, and stray opaque pixels. For a sword correction, check the visible
guard-to-tip blade length, shape, and hand connection in **both** frames at
combat size; the equipment label alone does not prove it reads as a sword.
Compare other weapon sets for identity, scale and foot position. For a humanoid,
also compare its foot-to-head height with the saved height tier and other art
of that species. Check the stationary dotted grid and any footprint-overflow
warning in both poses; the preview never shrinks a frame to suppress overflow.
A ready warning is acceptable only when the raised weapon alone crosses the top
tile line. An action warning is acceptable only when the moving weapon alone
crosses a tile line. The preview cannot identify which pixels belong to the
weapon, so inspect this visually. A correct 128 x 128 canvas is not proof of
consistent height.
To preview before installation, use a separate worktree/copy of the catalog,
copy the staged images to that copy's `install_paths`, and run its existing
`./dev_tools/combat_sprite_catalog/run.sh`. Keep the main checkout's accepted art
until the staged pair passes. The catalog already reloads changed PNGs.
If the preview still shows old art, confirm the selected monster and weapon set,
the loaded file paths and hashes, and reload or restart the preview before
drawing a conclusion from it.

In `review.json`, set each `visual_checks` entry to `true` **only after inspecting
it**. Enter `reviewer`, an ISO date/time in `reviewed_at`, and the exact inspected
`ready_sha256` and `action_sha256` from the check. Use `notes` for relevant pose
decisions, references and comparisons. Review every weapon set independently.
Changing either image after review invalidates that attestation; recheck the
final bytes and inspect them again before recording approval.

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

## Natural attacks for other body plans

The user approved natural, in-place attacks with a fixed base on 2026-10-18 for
unarmed, magical, and non-biped creatures. For a quadruped or invertebrate,
establish every planted foot or walking-leg contact in `ready.png` and keep those
contacts, the body, camera, and scale registered in `action.png`. Animate only
the attacking jaw, claw, pincer, stinger, or other local part. Reserve enough
transparent space for the larger pose before exporting **both** frames with the
same canvas transform. A leap, step, body slide, or independently recentered
silhouette fails review.

The `bipedal-v1` pose guide and `accept` checklist have human-specific fields
such as a visible chest and raised weapon. They do not certify quadrupeds or
invertebrates. For those body plans, run the same `check` command, inspect the
exact 128 x 128 PNG hashes side by side and alternating in the catalog, and save
`ready-prompt.txt`, `action-prompt.txt`, and a `natural-in-place-v1` `review.json`
beside the pair. Record the planted contacts, localized attack, complete
silhouette, readability, equipment, source identity, reviewer, review time, and
both hashes. Clear the regeneration flag only after every set is reviewed.

## Regression checks

```bash
"$godot_bin" --headless --path "$tool_dir/godot" \
  --script res://../generation/test_workflow.gd
```

These synthetic tests require no original game art or native extension. They
exercise prompt preparation, paired equipment selection, file rejection, pending
guide approval, complete acceptance and invalidation after image/input changes.
