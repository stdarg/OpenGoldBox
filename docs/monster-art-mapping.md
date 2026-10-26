# Monster-to-art mapping investigation

## Scope and reproducibility

Inspected the locally supplied Pool of Radiance installation. Results are
research candidates, not gameplay-verified runtime bindings. No original art or
full monster database is bundled in the repository.

Run from the repository root:

```powershell
godot_console --headless --path demos/godot --script ../../tools/research_monster_art.gd
```

The tool honors `OPENGOLD_GAME_DIR`, falling back to the Godot project setting.
It writes `user-data/monster-art-candidates.csv` and
`user-data/monster-art-unresolved.csv` into the ignored local-data directory.
The candidate CSV uses one row per monster/art/pose association and explicitly
marks each association `data_consistent_candidate`.

## Findings

- All 172 inspected `MON*CHA.DAX` records are 285 bytes long. Byte 0 gives the
  name length; the following bytes contain the ASCII name (up to 15 bytes).
- Across `CPIC1.DAX` through `CPIC8.DAX`, 109 archive-local base images have
  partners at **base record ID + 128**. All pairs decode with matching dimensions.
  These represent 58 distinct base IDs, with resources repeated across banks.
- Matching monster IDs to base CPIC IDs gives candidate names for 56 of those
  58 IDs, covering 89 of the 172 monster records. The other 83 records are
  explicitly left unresolved. CPIC base IDs 16 and 37 have no same-ID monster
  name in the inspected monster archives.
- Twelve visually inspected pairs show consistent creatures in different poses:
  kobold, goblin guard, orc, hobgoblin, ogre, giant skeleton, vampire, basilisk,
  troll, minotaur, ahnkheg (source spelling), and wyvern. This supports the
  ready/action interpretation but does not prove every monster binding.
- The initial speculation that byte 159 selects a CPIC bank is **rejected as a
  general rule**. For example, it is 11 for a basilisk and 17 for a minotaur,
  while the installation has only eight numbered CPIC archives. Its meaning
  remains unverified; it is not used by the mapping generator.
- Base ID 24 has different decoded pixels in `CPIC3.DAX` and `CPIC4.DAX`.
  Same-ID matches in different archives must therefore remain separate
  candidates. The generator does not guess which archive the game selects.

## Visually reviewed examples

Names below come from matching monster record IDs; pose roles remain inferred
from the displayed images, not traced in the original game.

| Candidate name | Art archive | Ready candidate | Action candidate |
| --- | --- | --- | --- |
| Kobold | CPIC2.DAX | 0 | 128 |
| Goblin Guard | CPIC1.DAX | 2 | 130 |
| Orc | CPIC1.DAX | 4 | 132 |
| Hobgoblin | CPIC1.DAX | 6 | 134 |
| Ogre | CPIC1.DAX | 8 | 136 |
| Giant Skeleton | CPIC4.DAX | 22 | 150 |
| Vampire | CPIC4.DAX | 23 | 151 |
| Basilisk | CPIC2.DAX | 26 | 154 |
| Troll | CPIC2.DAX | 31 | 159 |
| Minotaur | CPIC7.DAX | 62 | 190 |
| Ahnkheg | CPIC6.DAX | 65 | 193 |
| Wyvern | CPIC8.DAX | 121 | 249 |

The local visual review sheet is `user-data/monster-art-review.png`.

## Remaining work

1. Confirm runtime archive selection with original-game observations. The game
   now draws each fight's icon from `LOAD MONSTER`'s icon operand in the area's
   own `CPIC` archive ([encounter conversion](EXPEDITION.md#encounter-conversion)).
2. Resolve aliases and assembled icons. For example, monster ID 44 is named
   ORC but lacks a same-ID CPIC pair, whereas ID 4 has an orc pair. Similar
   names alone are not sufficient evidence to assign an alias.
3. Identify the two base art IDs without same-ID names and verify all remaining
   visual candidates in gameplay.
4. Investigate encounter-art bindings independently; this report maps CPIC
   candidates only. It does not label `SPRIT`, `COMSPR`, `CHEAD`, or `CBODY`.
5. Promote only verified bindings into runtime metadata. The demo is deliberately
   unchanged by this investigation so tentative associations are not presented
   as established monster names.

## Research lead

The [Pool of Radiance Hub discussion](https://forums.goldbox.games/index.php?topic=1912.0)
contains a first-hand investigation of IDs 22/150 and 23/151 and an offset-159
hypothesis. The ID+128 relationship was independently checked against every
local CPIC pair here; the offset hypothesis failed the broader local check.
