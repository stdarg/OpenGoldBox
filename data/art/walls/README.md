# New wall art

The game's first-person view shows these sets in place of the original wall
appearances. Each folder is one original appearance, named after its key with
`.` and `:` written as `_`: `WALLDEF1_DAX_1_4` is appearance 4 of
`WALLDEF1.DAX` record 1. A folder holds ten PNGs, each four times the original
piece's size: `far_front`, `far_left`, `far_right`, `middle_front`,
`middle_left`, `middle_right`, `near_front`, `near_left`, `near_right`, and
`far_extension`. Its `manifest.json` records the brief the set was made from.

An appearance without a folder keeps its original art from the player's game
files, enlarged four times. With 3D view art set to Original (Options,
Interface) every wall shows its original art. The game refuses a folder it cannot use, such as
one with a missing piece or a piece of the wrong size.

The [Replacement Art Creator](../../../dev_tools/replacement_art_creator/README.md)
writes these folders with **Export to game**, and the
[Tile Art Viewer](../../../dev_tools/tile_art_viewer/README.md) shows them as
the game does. Every image here is new art; none is copied from the original
game.
