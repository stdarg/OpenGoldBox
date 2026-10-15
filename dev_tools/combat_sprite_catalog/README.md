# Combat Sprite Catalog

Launch the standalone Godot tool with `./dev_tools/combat_sprite_catalog/run.sh`.
Pass the Pool of Radiance directory as its only argument, or set
`OPENGOLD_GAME_DIR`. Set `GODOT_BIN` if Godot is outside `PATH` and the standard
macOS app location. The launcher builds its native extension and imports the
Godot project on first run. Original game art is loaded locally and never copied
into the repository.

The **Monsters and NPCs** tab lists all records from the eight `MONnCHA.DAX`
archives, including records with no art match, plus unidentified `CPIC` ready/action
pairs. It alternates ready and action
`CPIC` images every second. A same-ID, same-bank pair is preferred, followed by
a same-ID pair from another bank and a same-name alias. These are explicitly
marked as candidates because the source data does not prove every runtime art
binding. Records with usable character-component fields show a candidate assembled
icon; unresolved records remain visible with a clear message. Weapons and
gear come from the corresponding `MONnITM.DAX` record through the game's native
`CreatureCatalog`; readied items are marked. The **Hide unused monsters / NPCs**
filter hides records with no reference in decoded ECL evidence. Dynamic script
paths may be unresolved, so “unused” means no reference found by that analysis.
Type any part of a monster or NPC name into **Filter by name** above the list
to narrow it as you type. Matching ignores letter case and works together with
**Hide unused monsters / NPCs**. Clear the field to show all names again.

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
