# Stojanow Gate audit (2026-10-25)

What playing Stojanow Gate needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL2.DAX 9`, Podol
Plaza's `ECL1.DAX 18` (the way in), `opengold_creatures GAME`, `opengold_maps
GAME`, coab's `TryStepForward` and its reading of the facing cell (`0xC04D`,
the direction halved), and SRD 5.2.1.

## The area

- **Map:** `GEO2.DAX:9`, the gate across the river's bend that cuts the old
  town and the castle off from the rest of the city, with `LOAD PIECES
  1,9,23`. One script, `ECL2.DAX:9`.
- **The way in:** Podol Plaza's north edge opens on the gate's south side at
  (4,15).
- **The ways out:** the south edge leads back to Podol Plaza; the north edge
  to Valjevo Castle's old town (`ECL5:6`, in the game since 2026-10-25:
  [Valjevo old town](valjevo-old-town.md)).
- **The gate** (0x4A77 holds its state): a wall with two towers, its passage
  shut by gates that never open from the south. Bugbears lounge before it;
  hill giant patrols led by a human fighter roam, and give a password test
  (HARASH, then on whose authority: TYRANTHRAXUS). The ways through:
  - a trader's wagon, bought for 250 gold pieces, and the toll of 15 gold
    the bugbears take once the party has watched a trader pay it; a bugbear
    may see through the party and attack instead;
  - or a fight with the bugbears before the gate (taking a punch avoids it).
- **The towers:** on the north side, the stairs at (4,5) and (11,5), climbed
  facing south, lead up to each tower's guard room: a magic-user, an aide and
  three ettins, overheard plotting the assault on Phlan.
- **Taking the gate:** with both garrisons beaten, the passage opens south
  from the north side (each square's own side is open), and the bugbears
  waiting beyond fight; beaten, the gate is taken and the council's flag set
  (`0x4AB9` = 254). With the bugbears beaten first, the garrisons come down
  together from the north side's (7,6) instead.
- **Alarms** (`0x4A64`): once raised, the garrisons charge when the party
  nears the towers; rocks and arrows fall on a party lingering by the wall.

## Monster conversions (MON2CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 24 | Level 6 MU (HP 23) | Valhingen Graveyard's mystic profile |
| 53 | Aides (HP 18) | the Central Wilderness's Bandit Captain |
| 55 | Hill Giant (HP 41) | **new:** SRD 5.2.1 Hill Giant with 41 Hit Points: Tree Club and Trash Lob (the Poisoned of Trash Lob not modeled) |
| 63 | Bugbear (HP 16) | the Slums' Bugbear Warrior |
| 71 | Ettin (HP 50) | **new:** SRD 5.2.1 Ettin with 50 Hit Points: Battleaxe and Morningstar, each hit knocking Prone (the Morningstar's Disadvantage not modeled) |
| 75 | 6th Level Fighter (HP 45) | the Pyramid's Warrior Veteran |
| 89 | 5th Level MU (HP 20) | the nomad shaman's level-4 Wizard profile |

## Engine support

Missing before this audit, now done:

1. **The area row** and its conversions.
2. **A step is stopped only by the wall on the party's side** of the edge
   (coab `TryStepForward`), so the passage opens southward from the north
   side though its gates stay shut from the south. The engine had also
   checked the far side's wall.
3. **The gate's harm:** the rocks from its top name their missile, and a
   bugbear's punch is bludgeoning.
4. **The location label** names the areas added since the nomad camp: the
   Pyramid, the wilderness caves, the kobold caves, the lizard men's keep,
   Valhingen Graveyard and Stojanow Gate.

## Party level and balance

With rests between fights, a level-7 party beats each tower's garrison and
the bugbears at the default encounter challenge.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [GATE-1](../SRD-DECISIONS.md#gate-1-2026-10-25-stojanow-gate). Played
  (`opengold_expedition_tests`, a new level-7 party by way of Podol Plaza):
  the wagon bought and the toll paid, both towers climbed and their
  garrisons beaten, the way north refused, the passage south and the bugbears
  beyond, the gate taken, and the way back to Podol Plaza.
