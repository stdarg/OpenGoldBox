# Kuto's Well audit (2026-10-08)

What playing Kuto's Well needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL8.DAX 29`,
`opengold_creatures GAME`, `opengold_maps`, the Gold Box Explorer map names in
`demos/godot/scripts/por_map_names.gd`, and SRD 5.2.1.

## The area

- **Maps:** `GEO8.DAX:29` (the plaza around Kuto's Well) and `GEO8.DAX:32`
  (the catacombs beneath). One script, `ECL8.DAX:29`, drives both: `LOAD
  FILES 29` and `LOAD FILES 32` switch between them as the party climbs the
  well's rungs.
- **Entry:** the Slums script already leads here (`NEW ECL 29` in `ECL2:20`).
  Kuto's Well leaves through `NEW ECL` to the script stored in variable
  `0x6E82`.
- **Story:** roaming kobolds climb out of the well; a band of sickly kobolds can
  be questioned or released; a lizardman leads giant lizards before a nailed
  door; a seer woman gives a prophecy; arrow ambushes. Below, the bandit gang
  of Norris the Gray offers "Surrender or die". Clearing it yields the bandits'
  treasure, a curious message (journal entry 50) and a hideout where the party
  "may rest undisturbed". A rug hides a stash of arms and armor.
- **Art:** `WALLDEF8`/`8X8D8` (wall banks `LOAD PIECES 3,20,1` and `18,17,1`),
  `SPRIT8`, `CPIC8`, `PIC8`, `HEAD8`, `BODY8`, `MON8CHA`, `ITEM8` are all
  present in the installation.

## Fights

| Fight | `LOAD MONSTER` (MON8CHA record × count) |
| --- | --- |
| Norris the Gray's band (catacombs) | Norris (32) ×1, Lizardman (57) ×5, Kobold Leader (1) ×9 |
| Kobolds climb from the well | Kobold (0) ×6, Kobold Leader (1) ×3 |
| Waiting kobolds | Kobold (0) ×2, Kobold Leader (1) ×4 |
| Lizardman patrol at the nailed door | Lizardman (57) ×1, Giant Lizard (59) ×4 |
| Hidden arms and armor | guarded; `TREASURE` item list 58 |
| Roaming | Kobold Leader (1) ×3 and a strength-sized group read from a script table: gnolls (73), kobolds or lizardmen |

As in the Slums, every fight is trimmed to the party's XP budget and to one
creature per living character (`encounter_challenge`).

## Monster conversions

| Record | Original | Proposed SRD conversion |
| --- | --- | --- |
| MON8CHA 0, 1 | Kobold, Kobold Leader | the Slums kobold profiles, if MON8 and MON2 records agree (to check) |
| MON8CHA 57 | Lizardman (HP 11, AC 4, 1d8) | **SRD 5.1 Lizardfolk** (CR 1/2) via Open5E: SRD 5.2.1 has no lizardfolk; precedent: the Slums Orc is SRD 5.1 |
| MON8CHA 59 | Giant Lizard (HP 16, AC 5, 1d8) | SRD 5.2.1 Giant Lizard (CR 1/4) |
| MON8CHA 73 | Gnoll | SRD 5.2.1 Gnoll Warrior (CR 1/2) |
| MON8CHA 32 | Norris the Gray: long sword +1, shield, chain armor | SRD 5.2.1 **Bandit Captain** (CR 2) wearing his record's chain mail and shield, as Slums leaders wear theirs; an `equipment` metal-armor row |

## Engine support

Supported today: the script interpreter runs every command the script uses
(`SUBTRACT`, `COMPARE AND`, `ON GOSUB`, `OR` included); the host `CALL`
services (movement and redraws), `APPROACH`, `SPRITE OFF`, `DELAY`, `PICTURE`,
`PARTY STRENGTH`, `SURPRISE`, `PARTY SURPRISE`, `LOAD CHARACTER`, coin
`TREASURE`, and dungeon combat arenas from any GEO map.

Missing:

1. **Area loading.** The Slums district is loaded by hand
   (`rolf_tour.cpp`); Kuto's Well needs its script, two maps, wall banks,
   sprites, pictures, monster bank and combat icons. Best done as a small
   data-driven area table so later areas are rows, not code.
2. **Travel.** `NEW ECL 29` fails ("Travel outside New Phlan (script 29)");
   the program registry knows only New Phlan and the Slums. `LOAD FILES`
   accepts only the town and Slums profiles, and must switch between two maps
   in one area.
3. **Slums-only branches.** `LOAD MONSTER`, `ENCOUNTER MENU`, `COMBAT` and two
   other host paths check `current_area_ == 20`; the creature, XP and loot
   tables are Slums-only.
4. **`DAMAGE`** (the arrow volleys): no handler.
5. **`ROB`** (surrendering to Norris): throws "Pickpocket money and item
   removal".
6. **Fixed treasure item lists** (`TREASURE` list 58, the hidden arms and
   armor) are unsupported outside shops.
7. **The hideout:** after Norris falls, the catacombs become a safe rest spot;
   the camp-interruption profile needs that rule.
8. **Journal entry 50** must be recorded like the existing journal entries.

## Party level and balance

The arc to here is the Slums at levels 1-3; Kuto's Well follows at about
level 2-4. Norris (CR 2, 450 XP) alone exceeds a level-two party's trimmed
budget, so his fight keeps Norris plus one of each other group. The
campaign-arc simulator should get a Kuto's Well stage to measure it.

## Status

- **Increment 1 (done, 2026-10-08):** areas beyond New Phlan load from a table
  (`rolf_tour.cpp`): the Slums and Kuto's Well, each map a district with its
  script, archive bank and wall banks. Leaving the Slums by a west doorway
  enters Kuto's Well, `LOAD FILES` switches between its plaza and catacombs,
  saves made there load back, and `NEW ECL` writes the destination's archive
  bank to `0x6E12`. The script's display cells `0xC059` and `0xC05F` are
  stored but have no effect yet (meaning unverified). Fights, traps,
  robbery and fixed treasure still stop with a diagnostic.

## Suggested increments

1. Data-driven area table; load Kuto's Well (both maps) and travel to and from
   the Slums. Exploration only, events that need missing commands report a
   clear diagnostic.
2. Creature conversions and equipment rows; general `LOAD MONSTER`,
   `ENCOUNTER MENU`, `COMBAT`, XP and loot for any area; the well and patrol
   fights.
3. `DAMAGE` traps, `ROB`, fixed treasure lists, journal entry 50 and the
   hideout; Norris's fight.
4. Campaign-arc simulation and a play-test through the area.
