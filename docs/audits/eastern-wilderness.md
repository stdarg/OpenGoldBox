# Eastern wilderness audit (2026-10-25)

What travelling the wilderness map's eastern window needs, from the user's
DOS installation and the current engine. Sources: `opengold_scripts --inspect
GAME ECL8.DAX 27`, New Phlan's `ECL3.DAX 0` (the harbor), `opengold_creatures
GAME`, `opengold_maps GAME`, and SRD 5.2.1.

## The window

- **The window** (`ECL8:27`) looks onto the wilderness map's columns 26-41.
  Stepping west off its third column enters the central window at column 14.
  New Phlan's harbor master sells passage east, landing at (9,29).
- **Sites** (`0x9A9C`, by the tables at `0xABA6`-`0xABC1`):
  - (11,8): a ruined castle in the quicksand, the lizard men's keep
    (`ECL8:16`, in the game since 2026-10-25: [lizard men's
    keep](lizard-keep.md)).
  - (6,14), (7,14), (5,15), (7,15), (6,16), (7,16): a screaming horde of
    kobolds (25 and 5 leaders), once.
  - (6,15): the kobolds' two caves, small and large (`ECL8:13`, in the game
    since 2026-10-25: [kobold caves](kobold-caves.md)).
  - (9,29): a boat back to New Phlan.
- **Roaming groups** (`0x9EA7`): one step in twenty, a group by the table at
  `0xABF7`: kobolds, gnolls, lizard men, giant lizards, hippogriffs, wyverns,
  trolls, phase spiders or minotaurs; or, one roll in 420, the wyverns' cave.
- **The wyverns' cave** (`GEO8:27`, wall banks 17): four passages, each opening
  on a quarter: wyverns and their treasure, giant lizards and minotaurs,
  gnolls and trolls, mad kobolds and the remains of their victims.

## Monster conversions (MON8CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 0, 1 | Kobold, Kobold Leader | the Slums' |
| 31 | Troll | the Troll |
| 57 | Lizardman | the Lizardfolk |
| 59 | Giant Lizard | the Giant Lizard |
| 62 | Minotaur | the Pyramid's Minotaur of Baphomet |
| 73 | Gnoll | the Gnoll Warrior |
| 113 | Hippogriff (HP 18) | **new:** SRD 5.2.1 Hippogriff with 18 Hit Points and its Flyby |
| 116 | Phase Spider (HP 35) | **new:** SRD 5.2.1 Phase Spider with 35 Hit Points, its Ethereal Jaunt and its Bite's paralyzing venom ([MONSTER-3](../SRD-DECISIONS.md#monster-3-2026-10-28-the-last-monster-abilities)) |
| 121 | Wyvern (HP 42) | **new:** SRD 5.2.1 Wyvern with 42 Hit Points: Bite and Sting, which poisons until the start of the wyvern's next turn |

## Status

- **Increment 1 (done, 2026-10-25):** the window, its sites and cave, and the
  conversions above, by
  [EAST-1](../SRD-DECISIONS.md#east-1-2026-10-25-the-eastern-wilderness).
  Played (`opengold_expedition_tests`, a new level-6 party): the harbor's
  eastern boat, the ruined castle and the kobold caves refused for now, the
  kobold horde, the wyverns' cave found by a roaming roll and left, and the
  boat home to New Phlan.
