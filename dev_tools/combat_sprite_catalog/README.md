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

- Export each ready and action image as a **128 x 128 pixel transparent PNG** in
  a **pixel art style**. This is the final game asset size; larger working images
  may be used while creating the art.
- Keep the character's scale and foot position consistent across ready/action
  poses and every weapon set for that monster. Draw the equipped weapon and
  shield for the selected set in both poses.
- Review the exported images at normal combat zoom as well as in the catalog.
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
and that the character pane has no scrolling selection panel.

Run `--metadata-check` in place of `--catalog-check` to verify JSON persistence,
draft retention, paired one-handed setups, separate ranged and two-handed setups,
replacement preview, and original fallback.
