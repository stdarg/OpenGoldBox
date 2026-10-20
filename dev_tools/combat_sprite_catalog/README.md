# Combat Sprite Catalog

Launch the standalone Godot tool with `./dev_tools/combat_sprite_catalog/run.sh`.
Pass the Pool of Radiance directory as its only argument, or set
`OPENGOLD_GAME_DIR`. Set `GODOT_BIN` if Godot is outside `PATH` and the standard
macOS app location. The launcher builds its native extension and imports the
Godot project on first run. Original game art is loaded locally and never copied
into the repository.

The **Monsters and NPCs** tab lists all records from the eight `MONnCHA.DAX`
archives, including records with no art match, plus unmatched `CPIC` ready/action
pairs. It alternates ready and action images every second. Script-linked
`LOAD MONSTER` icons take precedence. Without such evidence, a same-ID,
same-bank pair is tried, followed by a same-ID pair from another bank and a
same-name alias. These weaker matches are labeled as candidates. Records with
usable character-component fields show a candidate assembled
icon; unresolved records remain visible with a clear message. Weapons and
gear come from the corresponding `MONnITM.DAX` record through the game's native
`CreatureCatalog`; readied items are marked. The **Hide unused monsters / NPCs**
filter hides records with no reference in decoded ECL evidence. Dynamic script
paths may be unresolved, so “unused” means no reference found by that analysis.
Type any part of a monster or NPC name into **Filter by name** above the list
to narrow it as you type. Matching ignores letter case and works together with
**Hide unused monsters / NPCs**. Clear the field to show all names again.

The monster pane is also an editor for independent replacement art metadata.
Edit **Name** and press **Rename** to save a catalog-only name. Enter a name and
press **Copy as new monster** to create an independent JSON record and copy all
available replacement PNGs into its own art directory. Copies reappear on later
launches, including when unused records are hidden. These actions do not change
the supplied game files or the game's runtime monster catalog.
Enter a description, edit the decoded equipment list, set **Flag for regeneration**
when art needs another pass, and press **Save monster data**. The first save
creates `dev_tools/combat_sprite_catalog/json/<source-key>.json` (lowercase,
with punctuation replaced by underscores); each monster
or unidentified sprite has its own file, even when names repeat. The JSON records
the monster name, original creature and art source files/records, description,
regeneration flag, equipment, and image variants. Unsaved edits remain available
while switching between records during the current session.

Set **Sprite size (combat squares)** to the intended visual footprint and save
it with the other monster data. The dropdown offers Unknown, 1 × 1, 1 × 2 tall,
2 × 1 wide, 2 × 2, and 3 × 3. JSON stores the choice in `sprite_size` as
`width_squares` and `height_squares`; Unknown stores `null`. The 199 built-in
records are initialized from the dimensions of the game's currently displayed
original ready/action icons. See the [size audit and renderer distinction](../docs/combat-sprite-catalog.md#how-sprite-sizes-were-assigned).
The adjacent **Humanoid height** dropdown records Small, Normal, Tall, or
N/A (non-humanoid) in JSON as `humanoid_size`. It guides consistent apparent
body height across ready/action frames and weapon sets; it does not change
combat-square dimensions or game rules. Art preparation includes the saved
height tier in its prompts and requires reviewers to check it. The monster/NPC
preview always shows the same 3 × 3 dotted combat grid, with the selected
footprint shaded from its lower-left corner. Ready and action use one fixed
scale and planted-base anchor; neither pose is automatically fitted to its own
bounds. A message below the grid reports opaque pixels outside the selected
footprint. See the [fixed preview and height targets](../docs/combat-sprite-catalog.md#fixed-combat-square-preview).

Every built-in monster/NPC template and extra combat-icon row has a saved JSON
record. Descriptions include the original inventory and wealth where a creature
record exists. `wealth` preserves the seven original character fields in order:
`copper`, `silver`, `electrum`, `gold`, `platinum`, `gems`, and `jewelry`.
These are amounts on the creature template, not treasure awarded by an encounter
script. A shared combat icon has no single inventory or wealth; inspect its
linked creature records instead. `description_sources` distinguishes original
record evidence, Cluebook PDF pages, and general appearance references. The
Cluebook and modern creature references inform descriptions only; they do not
add items or change combat rules. Copied monsters retain their source wealth
snapshot in their independent metadata.

The viewer selects original combat icons from decoded ECL `LOAD MONSTER`
instructions: the creature ID and icon ID are separate operands. If a creature
has several script-linked icons, the viewer uses the most referenced candidate,
preferring its archive bank on ties, and lists alternatives in the source label.
Previously unidentified `CPIC` rows show linked creature names, or identify an
icon shared by several creatures. A uniquely linked icon uses that creature's
`MONnITM.DAX` gear. When the original record has no items, the viewer says so;
it does not infer inventory from the picture. These are static script bindings,
so dynamic encounters can remain uncertain. Original item type 87 is **Boulder**,
identified by name component 244 in the supplied `START.EXE` word table.

Unlinked archive copies can also be identified by comparing both complete,
decompressed ready/action records with other `CPIC` pairs. Exact duplicates show
their shared art identity, matching archive records and known users in the list
and source details. The catalog keeps the original archive/record and its own
usage status: a duplicate of used art can still show **Unused** when no direct
script reference was found for that copy. Shared artwork does not imply shared
inventory; select a named creature to edit its gear.

The JSON `source` stores these relationships as `identical_art_keys` and
`shared_art_creature_keys`, separately from direct `linked_creature_keys`.
`direct_script_reference_found` records whether that archive copy has a decoded
script reference. User names and descriptions remain editable. See
[the identified duplicate copies](../../docs/npc-art-identification.md#identical-combat-art-in-other-archives)
for the five formerly unidentified entries.

The equipment editor labels each item **1H weapon**, **2H weapon**, **Ranged**,
**Shield**, or **Gear** from the original item data. Click an item's type button
to correct it. The **Weapon set** dropdown sits directly above the monster
image, with complete setups such as **Mace + Shield** and **Bow**.
Each one-handed weapon pairs with each available shield, and neither is offered
alone when both are present. Ranged and two-handed weapons are separate setups;
a lone one-handed weapon is offered alone when there is no shield. Each setup
swaps the existing monster preview to its own ready/action images, which continue
alternating every second. Changing the dropdown does not change the monster's
gear. A creature without a weapon setup has one
**Default** pair. Put
independently created PNGs at
`dev_tools/combat_sprite_catalog/art/<source-key>/<variant-id>/ready.png` and
`action.png`. The JSON's `variants` entries contain these relative paths.

### Replacement combat sprite art specification

For every bipedal monster generation/regeneration, follow the
[generation and review workflow](generation/README.md). It supplies the mandatory
screen-right front three-quarter facing, raised melee ready guard, late-strike
action, loaded/released projectile poses, original pose guide, prompt preparation
and acceptance checks. The guide's user approval status is tracked separately.

- Export each ready and action image as a **128 x 128 pixel transparent PNG** in
  a **pixel art style**. This is the final game asset size; larger working images
  may be used while creating the art.
- Keep the character's scale and foot position consistent across ready/action
  poses and every weapon set for that monster. Draw the equipped weapon and
  shield for the selected set in both poses.
- Review the exported images at normal combat zoom as well as in the catalog.
  On the catalog's fixed grid, apparent foot-to-head heights should be about
  0.6 square for Small, 0.9 for Normal, and 1.7 for Tall. Keep the entire
  silhouette, including equipment, inside its selected footprint.
  The inspected original tactical icons are 24 x 24 pixels; the game currently
  fits visible sprite bounds to about 90% of a combat cell. Detail must remain
  readable when the 128 x 128 image is displayed at that size.

The generated monster set and its selection criteria are documented in
[`art/README.md`](art/README.md).

**Show new art when available** starts enabled; turn it off to inspect the
original art. A missing or invalid PNG falls
back to that monster's original pose, and the source label names the selected
set and missing pose. Replacing a PNG while the tool is open refreshes the
preview on its next update. Turn off **Show new art when available** to inspect
the original image and binding in the source label. This tool prepares and
previews art; it does not generate replacement images.
After removing a weapon or shield changes the selected set, the last replacement
art remains visible as **Previous new art** until art for the new set is created.
The fallback survives Save and a later launch, and the label warns that the
pictured gear may be outdated.

The **Character sprites** tab uses the game's `CharacterArt` compositor and
combat-body assignment catalog. Its controls fill the pane, following the
game's Combat appearance screen: step through 14 heads and the distinct body
looks with the arrow buttons, choose Short, Regular, or Large, then select a
Color 1 or Color 2 part and click a palette swatch. Weapon and shield choices
sit beside Head, Body, and Size. Ready and Action poses stay visible together.
The swatch control and palette values match character creation. Weapon and
shield combinations are available even when game rules would forbid equipping
them; missing art mappings display the existing unarmed fallback and a status
label. The tab container leaves room for future tabs.

After building, run the focused check with:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless \
  --path dev_tools/combat_sprite_catalog/godot -- --catalog-check
```

It checks the complete displayed catalog, both monster poses, the unused and
name filters, and character ready/action previews across every weapon with shield.
It also checks the in-pane Head and Body buttons, color swatches, Large size,
and that the character pane has no scrolling selection panel. For every built-in
monster and combat-icon row, it also checks that saved descriptions have at
least 80 characters and saved wealth matches the decoded creature record.

Run `--metadata-check` in place of `--catalog-check` to verify JSON persistence,
draft retention, paired one-handed setups, separate ranged and two-handed setups,
replacement preview, and original fallback.
