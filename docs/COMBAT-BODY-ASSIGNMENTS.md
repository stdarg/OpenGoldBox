# Combat artwork assignments

Run from PowerShell at the repository root:

```powershell
.\demos\review-combat-bodies.cmd
```

The reviewer displays one original CBODY.DAX body in four previews: short/tall,
ready/action. Previous/Next cycles through the 35 assignable bodies, including the dagger
variants (33 and 34). Check every complete
combination that the artwork represents, such as Mace and Mace & Shield.
Each checkbox change saves immediately. Filtering only changes the visible
checklist; the summary always lists all assignments for the current body.
An empty assignment is visibly Unreviewed. Original game files are decoded
locally and are not distributed.

## Review tabs

- **Review & Assign**: the body previews, navigation and assignment checklist.
- **Unassigned**: combinations with no assigned body.
- **Multiple Assignments**: combinations assigned to more than one body, with
  a review button for each matching body.
- **Manage Combinations**: delete invalid combinations, such as Two-Handed Sword
  & Shield. Deleting removes that combination from all lists and every body;
  it does not delete the plain Two-Handed Sword combination.

Each list has its own filter. Counts and reports update after every edit.
Deletion saves immediately, together with the updated body assignments.

## Shared catalog

`data/art/combat-body-looks.tsv` has 35 body rows: body ID, a tab, then comma-separated
combination IDs (or `unreviewed`). For example:

```text
1	type_41,type_42,type_43,type_44,type_45
```

The options in `data/art/combat-weapon-options.tsv` describe ordinary shop
weapons and Unarmed, each with an optional `_shield` combination. Silver weapons
share ordinary associations. Both the game and reviewer accept the old single
ID format and normalize legacy `silver_N` IDs to `type_N`, preserving shield
suffixes and deduplicating associations. The next checkbox edit writes the
whole catalog in the new format. Saves use a temporary file and replacement;
a failed save keeps the previous assignments.

The original bow body 1 has been visually inspected in both poses and sizes
and shared between the bow options. Other existing assignments are retained;
unclassified bodies remain unreviewed. These are editable art classifications,
not changes to equipment rules.

Body 32 is derived at runtime from original body 21. The gray wand projection is
removed in the short and tall ready/action poses, leaving the shield and hand.
The original archive and body 21 remain intact. Body 32 is assigned to Unarmed
& Shield; body 21 remains available for Wand & Shield. No derived art is stored
in the repository.

The reviewer and game offer Body 33 (Dagger, from Body 7) and Body 34 (Dagger &
Shield, from Body 24). These shorten the blade to approximately half length in
memory for all four size/pose variants, preserving the original sword bodies.
They use the normal assignment controls and shared catalog: Body 33 is assigned
to Dagger and Body 34 to Dagger & Shield. Older 33-body catalogs still load with
the new bodies unassigned. No generated image assets are stored.

Version 3 also stores `deleted` rows, for example `deleted<TAB>type_38_shield`.
These persist removal of individual combinations without changing the weapon
options or equipment rules. The reviewer and native loader both exclude these
combinations. Older catalogs without deleted rows still load unchanged.

## Game use

Player characters and recruited NPCs use the same native equipment resolver in
the party screen, training combat, campaign encounters and combat showcase.
Only equipped inventory items contribute. Encounter creatures keep their own art.

The catalog chooses an equipment pose for the exact weapon/shield combination.
A matching saved body is preferred, otherwise the lowest matching ID wins.
`ResolvedCombatAppearance` retains the complete saved appearance. Its `icon`
method combines the saved anatomy with the selected wielding arms and equipment
through `CharacterArt::equipped_icon`; it never replaces the saved torso or legs
with a robed or trousered donor. The head, colors, size and pose scale remain
stable. Silver names do not select separate artwork.

Original CBODY records contain complete poses, not independent layers. The
runtime separates semantic color regions plus coordinate masks for gray weapon
outlines, which share a palette entry with boots and body outlines. Visible
saved garment pixels take priority; anatomy hidden by the original arm or
weapon is restored from homologous decoded bodies of the same clothing family.
This is done in memory; no original or derived artwork is distributed. Arms
and hands may change pose and naturally cover parts of the torso.

Missing/deleted combinations are reported and rendered unarmed on the saved
body, rather than displaying an unrelated weapon baked into that body. The
reviewer's assignments and deletion rows remain authoritative and unchanged.

The party screen's Ready and Action previews refresh immediately after Equip
or Unequip. Combat uses the same compositor on entry, and equipment remains
locked during combat; a character there also shows its carried bow as it shoots and
its weapon without a shield it took off. All 47 reviewer weapon types now have equipment/rules
conversions; the demo uses a temporary real campaign and the same equip/unequip
operations. See [party equipment](PARTY.md) for the SRD equivalents and current
combat limitations.

Rebuild the game after editing to refresh its packaged catalogs. The reviewer
always edits the shared source catalog, not a packaged copy.

If the game cannot load the two catalogs (`combat-body-looks.tsv` and
`combat-weapon-options.tsv`), each party member shows the original combat icon
it was saved with, and the game's log names both files and says why.

## Validation

Native party tests cover every enabled catalog combination, equipment changes
for PCs and NPCs, fallback, saved-body preference and campaign save/load. With
`OPENGOLD_GAME_DIR` set, they also compare both showcase poses and verify that
original encounter-creature art remains unchanged.

After building, run this original-data integration check from PowerShell:

```powershell
.\win-package\opengoldbox.exe -- --equipment-art-check --capture
```

It exercises the existing Equip/Unequip controls for a tall PC and short recruited
NPC, compares both preview and combat texture pixels, checks campaign loading,
and checks training and campaign encounters. Success prints
`Equipment artwork checks passed`. Captures are written to `user://checks` as
`equipment-art-*.png`; omit `--capture` when running with `--headless`.
