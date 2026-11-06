# New close-up animations

The game plays these in the 3D view in place of the original's animated
close-ups (`PIC1.DAX` to `PIC8.DAX`): the monster that has come up to the party,
or a picture a script shows. Each was drawn only from the written description in
the Combat Sprite Catalog's **Close-ups** tab, never from the original picture.

Each folder is one original record, named after its archive and record:
`pic2_dax_4` is `PIC2.DAX` record 4. It holds `frame_0.png`, `frame_1.png` and
so on, one 352 × 422 PNG for each frame the original holds on screen, in order:
the 3D view's 5:6 shape, so the game shows it without stretching.
The new frames keep the original's timing. A close-up with no folder, or one
whose folder is incomplete, shows the original.

Do not edit this folder by hand. The catalog's **Export to game** writes it from
`dev_tools/combat_sprite_catalog/closeups/art`; see
[the catalog's guide](../../../dev_tools/combat_sprite_catalog/README.md#close-ups).
