# Valhingen Graveyard audit (2026-10-25)

What playing Valhingen Graveyard needs, from the user's DOS installation and
the current engine. Sources: `opengold_scripts --inspect GAME ECL4.DAX 10`,
the Central Wilderness's `ECL7.DAX 26` (the way in), `opengold_creatures
GAME`, `opengold_maps GAME`, coab's `LoadWalldef`, `CMD_LoadFiles`,
`CMD_Call` and `CMD_FindItem`, and SRD 5.2.1.

## The area

- **Map:** `GEO4.DAX:10`, the graveyard turned upside down by the undead,
  with `LOAD PIECES 21,22,20`. One script, `ECL4.DAX:10`.
- **The way in:** the wilderness map's (12,26) and (13,26), the city's north
  edge, offer the city's northern gates or the graveyard. The graveyard opens
  at (0,4), under a storm and thunder.
- **The way out:** the west edge at (0,4) leads back to the city's north
  edge.
- **Events** (by each square's event number, the jump table at `0x99E2`):
  - skeletons, zombies, ghouls and "claws from the soil" among the graves;
  - an enclave of skeletons (5,6) and a giant skeleton with its retainers
    (7,6)-(7,8), its marble chest holding treasure;
  - a shuffling zombie horde down some stairs (10,15) and a juju zombie and
    its zombies (9,15), with scattered objects;
  - **three towers**, each with its spectre: the bones and noxious fumes
    (1,12)-(0,15), the hair and fur (5,9)-(4,10), the static electricity and
    its wights (9,5)-(9,7). Each spectre guards treasure.
  - the mummies' sarcophagi (9,11);
  - the brass doors (9,1), met facing east: spectres flow out;
  - the huge tower (15,6): spirits who fought the dragons warn the party
    off; wights (15,7); the paladin's crypt (15,8), guarded by a wraith, whose
    body may be stripped of its gear;
  - the knight's tomb (8,2): a voice gives the party his weapons;
  - **the mystic** (8,9), once the party has beaten three spectres: he asks to
    join (`ADD NPC 24`) against the undead, and attacks if sent away. With
    him, he names the vampire behind the curse and, at (15,1), the way to it.
  - the black marble crypt (12,4): a scroll (journal entry 43) and the
    vampire's coffin, which the party may examine, overturn or sanctify.
  - **the vampire** in its pit (15,0), down wooden stairs to (14,0), with his
    wolves. Beaten, he turns into a vapor cloud and flees to his coffin, where
    the party finds him again: with only 15 Hit Points if the coffin was
    sanctified. His treasure trove lies there.
  - A party carrying the efreeti Samir Ahwahl's bottle (from the kobold
    caves) has his help against the vampire in the pit: he fights on the
    party's side.
- **Two wall slots for one bank:** wall bank 21 is two blocks, its tiles in
  `8X8D4` records 211 and 212, and fills the first two wall slots; the script
  sets `0x49E7` and `0x49E8` so that the original loads no bank of its own
  into the second (coab `CMD_LoadFiles`).

## Monster conversions (MON4CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 17 | Spectre (HP 38) | Mendor's Library's Specter |
| 20 | Wight (HP 23) | the Wight |
| 21 | Wraith (HP 24) | **new:** SRD 5.2.1 Wraith with 24 Hit Points: Life Drain, flying, Incorporeal Movement (Create Specter not modeled) |
| 22 | Giant Skeleton (HP 28) | **new:** SRD 5.2.1 Minotaur Skeleton with 28 Hit Points, its Slam (the charging Gore not modeled) |
| 23 | Vampire (HP 43) | **new:** SRD 5.2.1 Vampire with 43 Hit Points: two Grave Strikes (its grapple, Bite, Beguile and Charm not modeled) |
| 24 | Level 6 MU (HP 23) | **new:** the nomad shaman's level-4 Wizard profile with 23 Hit Points |
| 29 | Juju Zombie (HP 24) | **new:** SRD 5.2.1 Ogre Zombie with 24 Hit Points, Medium as the original's is |
| 34, 35, 72 | Skeleton, Zombie, Ghoul | the Slums' |
| 70 | Efreeti (an ally) | the kobold caves' Efreeti, fighting on the party's side |
| 105 | Vampire (HP 15) | **new:** the Vampire with 15 Hit Points |
| 106 | Wolf (HP 18) | **new:** SRD 5.2.1 Wolf with 18 Hit Points |
| 114 | Mummy (HP 33) | **new:** SRD 5.2.1 Mummy with 33 Hit Points (Dreadful Glare and the Rotting Fist's curse not modeled) |

## The companion

The mystic (`ADD NPC 24`): a level-6 magic-user with Intelligence 15, 10 in
the rest, and a Ring of Protection +2. He joins as a level-6 Wizard with those
scores (a Sage) and his ring on.

## Engine support

Missing before this audit, now done:

1. **The area row**, its conversions and the mystic.
2. **A wall bank of several blocks**, filling that many wall slots.
3. **The storm's thunder** (sound selector `0x03DE` = 10) plays the
   original's fireball sample from the player's game, as the original did.
4. **The towers' damage types:** noxious fumes are poison, the sharp crack
   lightning.
5. **A saving throw against a script's harm** reports its DC beside the roll.
6. **Allied monsters:** a record whose team byte (`0x10E`) is 0, the
   efreeti, fights on the party's side, beside it, and the computer fights
   it, as the original's quick fight did (since 2026-10-25, after the
   graveyard's first increment).

## Party level and balance

With rests between fights, a level-8 party beats the spectres, the vampire and
his wolves at the default encounter challenge; the pit's 50 wolves are trimmed
to the party's budget.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [GRAVE-1](../SRD-DECISIONS.md#grave-1-2026-10-25-valhingen-graveyard).
  Played (`opengold_expedition_tests`, a new level-8 party from the harbor's
  western boat): the city's north edge, the brass doors' spectres and a
  tower's spectre, the mystic joining, the coffin sanctified after its scroll
  is read, the vampire beaten in his pit with Samir Ahwahl beside the
  party, destroyed at his coffin, and the west edge back out.
