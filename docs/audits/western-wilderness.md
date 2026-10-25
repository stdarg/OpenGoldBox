# Western wilderness audit (2026-10-25)

What travelling the wilderness map's western window needs, from the user's
DOS installation and the current engine. Sources: `opengold_scripts --inspect
GAME ECL6.DAX 25`, City Hall's `ECL3.DAX 8` (the commissions that open its
sites), `opengold_creatures GAME`, `opengold_maps GAME`, and SRD 5.2.1.

## The window

- **The window** (`ECL6:25`) looks onto the wilderness map's columns 0-15,
  as the central window (`ECL7:26`) does columns 13-28: stepping east off its
  last column enters the central window at column 3, and stepping west off
  the central window's second column enters this one at column 14.
- **Sites** (`0x9A77`, by the tables at `0xADD6`-`0xAE14`):
  - (10,8), (11,8), (9,9), (11,9), (9,10), (10,10): a dragon seen over the
    mountains; (10,9): the dragon lands at its cave (`ECL6:19`, not in the
    game yet).
  - (8,24): two ankhegs erupt from the ground, once.
  - (12,30), (13,30), (11,31), (13,31), (12,32), (13,32): a slaver caravan
    riding to the Buccaneers' base; (12,31): the base, whose riders escort the
    party in (`ECL6:1`, not in the game yet). Both need City Hall's
    commission to rescue the Bivant heir (`0x4A8C` = 255).
  - (2,31), (3,31), (2,32), (4,32), (2,33), (3,33): armed guards riding by;
    (3,32): the Zhentil outpost's riders, who take the party for New Phlan's
    envoys (City Hall's message for the Zhentil keep, `0x4A98` = 255) and
    escort it in (`ECL6:28`, not in the game yet), or else warn it off.
- **Roaming groups** (`0xA0B3`): one step in twenty, a group by the table at
  `0xAE2A`: quicklings, wild boars, giant mantises, tigers, thri-kreen,
  ankhegs, centaurs, Zhentil soldiers, thieves, buccaneers or giant snakes,
  sized by the party's strength; or, one roll in 420, the dark cave.
- **The dark cave** (`GEO6:25`, wall banks 17): entered at one of four
  passages, each opening on a quarter of the cave: wild boars and a sow with
  her young, an ankheg and its victims' treasure, giant snakes, and thri-kreen
  guarding a chest. Each passage leads back outside.

## Monster conversions (MON6CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 10 | Quicklings (HP 7) | **new:** SRD 5.2.1 Sprite with 7 Hit Points (Enchanting Bow's charm and Invisibility not modeled) |
| 45, 99 | 1st Level Thief, Buccaneer (HP 4) | the Bandit with 4 Hit Points, as Kovel Mansion's thief |
| 51 | 6th Level Thief (HP 28) | the Spy with 28 Hit Points |
| 54 | Corporal (HP 13) | **new:** SRD 5.2.1 Guard with 13 Hit Points |
| 60 | Giant Snake (HP 25) | **new:** SRD 5.2.1 Giant Venomous Snake with 25 Hit Points |
| 65 | Ankheg (HP 40) | **new:** SRD 5.2.1 Ankheg with 40 Hit Points (Acid Spray and its grapple not modeled) |
| 67 | Centaur (HP 20) | **new:** SRD 5.2.1 Centaur Trooper with 20 Hit Points (Trampling Charge not modeled) |
| 74 | Giant Mantis (HP 55) | **new:** the Giant Scorpion's two Claws with 55 Hit Points (SRD 5.2.1 has no mantis) |
| 100 | 3rd Level Fighter (HP 18) | the Bandit Captain with 18 Hit Points |
| 118 | Thri-kreen (HP 33) | **new:** the Warrior Veteran with 33 Hit Points (SRD 5.2.1 has no thri-kreen) |
| 119 | Tiger (HP 38) | **new:** SRD 5.2.1 Tiger with 38 Hit Points: Rend knocks a Large or smaller target Prone; Nimble Escape |
| 120 | Wild Boar (HP 18) | **new:** SRD 5.2.1 Boar with 18 Hit Points: its charge and Bloodied Fury |

## Engine support

Missing before this audit, now done:

1. **The area row** for the window and its cave.
2. **The beast forms' traits as monster rows**: Bloodied Fury, the Boar's
   charge, a hit that knocks Prone, and a new Nimble Escape (Disengage or Hide
   as a Bonus Action).
3. **Sprites with fewer distance frames** (the centaur's two) show their
   farthest frame beyond them.
4. **Names for the areas beyond** in the notice that refuses them.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [WEST-1](../SRD-DECISIONS.md#west-1-2026-10-25-the-western-wilderness).
  Played (`opengold_expedition_tests`, a new level-6 party): the step west off
  the central window, the Buccaneers' base, the Zhentil outpost and the
  dragon's cave refused for now, the dragon over the mountains, the ankhegs,
  the dark cave found by a roaming roll and left by its passage, and the step
  back east.
