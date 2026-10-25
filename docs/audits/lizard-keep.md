# Lizard men's keep audit (2026-10-25)

What playing the lizard men's keep needs, from the user's DOS installation and
the current engine. Sources: `opengold_scripts --inspect GAME ECL8.DAX 16`,
the eastern wilderness's `ECL8.DAX 27` (the way in), `opengold_creatures
GAME`, `opengold_maps GAME`, coab's `Area1.can_cast_spells`, and SRD 5.2.1.

## The area

- **Two maps, one script** (`ECL8.DAX:16`):
  - `GEO8.DAX:16`, the ruined castle's first level, with wall banks `LOAD
    PIECES 9,20,23`;
  - `GEO8.DAX:30`, its flooded lower level, with `LOAD PIECES 9,17,23`.
- **The way in:** the eastern wilderness's (11,8), the ruined castle in the
  quicksand; the drawbridge sets the party at (8,14) facing north. The entry
  says the party senses an anti-magic shell. The original only prints this:
  the script never sets the area's spell ban (coab's `can_cast_spells`), so
  spells work in the keep, as they did in the original.
- **The way out:** the moat of sludge and thorns stops the party except at the
  drawbridge (8,15), which leads back to the wilderness map (`NEW ECL 27`).
- **The old lizard man** waits at one of several places on the first level.
  Attacked, he dies. Heard out, he asks for his tribe's friend-word, which the
  party learns by freeing the Pyramid's chained lizard men. The wrong word
  sends him away, calling the party pirates. With SAVIOR he tells his story
  (journal entry 31). Then Drythh challenges him, and the old lizard man asks
  the party to be his champion:
  - Championed, the duel is one member against Drythh. Beaten, his
    followers withdraw, and the old lizard man promises that his people will
    not join Tyranthraxus (`0x4A60` = 255).
  - Refused, the old one fights Drythh himself and loses, and Drythh's
    followers attack.
- **The first level's ruins:**
  - a roofless building with two lizard men and a giant lizard;
  - the central keep, where hidden lizard men throw javelins and giant
    lizards charge out of tunnels;
  - an L-shaped building with three giant lizards;
  - hidden tunnels and old stairs leading down, and some treasure.
- **The lower level:**
  - lizard men slither out of the water and throw javelins, then dive back or
    fight;
  - deep pools to dive into and search by touch: lizard men attack the
    diver, and one pool hides treasure;
  - stairs and sloping tunnels back up to the first level.

## Monster conversions (MON8CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 57 | Lizardman | the Lizardfolk, as in the eastern wilderness |
| 59 | Giant Lizard | the Giant Lizard |
| 95 | Drythh (HP 18; a 1d10 weapon and two 1d4 attacks) | the Lizardfolk with 18 Hit Points, the Central Wilderness's mutant lizard men |

## Engine support

Missing before this audit, now done:

1. **The area row** for both maps and its conversions; the eastern
   wilderness already ran `NEW ECL 16`.
2. **What a volley throws:** each shot's report names arrows unless its call
   names another missile: the lizard men's javelins, Kovel Mansion's sling
   bullets, the kobold king's ballista bolts and thrown rocks.

## Party level and balance

With rests between fights, a level-7 party survives the keep's ambushes and
the duel at the default encounter challenge.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [LIZARD-1](../SRD-DECISIONS.md#lizard-1-2026-10-25-the-lizard-mens-keep).
  Played (`opengold_expedition_tests`, a new level-7 party that freed the
  Pyramid's lizard men): the drawbridge, the old lizard man and SAVIOR, the
  duel with Drythh, the stairs down, a pool dived into under javelins, the
  tunnel back up, and the drawbridge out.
