# Valjevo Castle audit (2026-10-25)

What playing Valjevo Castle, the game's last area, needs, from the user's DOS
installation and the current engine. Sources: `opengold_scripts --inspect
GAME ECL5.DAX` 3, 4, 5 and 7, the old town's `ECL5.DAX 6`, `opengold_creatures
GAME`, `opengold_maps GAME`, coab's `CMD_Program` (8), `CMD_LoadFiles` and
`CMD_Damage`, the Cluebook's castle maps (for the layout only), and SRD 5.2.1.

## The area

- **Four quarters on four maps**, all with `LOAD PIECES 24,25,1`: map 3 the
  northwest, map 4 the northeast, map 5 the southeast, and the old town's map
  6 the southwest. Their outer rings are the castle grounds; their inner parts
  form the maze of hedges inside the walls; the four corners at the middle
  form the inner tower's lower level. Map 7 holds the tower's upper level.
- **Four scripts share the maps:**
  - `ECL5:3` runs the grounds of the northwest and southeast quarters;
    walking off its maps leads to `ECL5:4` (the northeast) or back to the old
    town.
  - `ECL5:4` runs the northeast quarter's grounds.
  - `ECL5:5` runs the maze and the tower's lower level across all four
    quarters, moving the party between their maps as it walks off one; on the
    grounds' squares it hands back to `ECL5:3`.
  - `ECL5:7` runs the tower's upper level.
- **The way in:** the old town's north edge (3,0) opens on the northwest
  quarter at (3,15).
- **The inner gate** (map 3, (3,7)), met facing east, asks for the password
  (RHODIA, which Cadorna, rescued from his cell, can give); forced, it brings
  boulders down from its top. Its gates never open otherwise. Past it, the
  maze.
- **The maze:** hedges with poisonous thorns (cutting through, someone may be
  pricked for 201 damage, unless the saving throw succeeds), towers of giants
  and trolls, giant guards who ask the password, and squares where the party
  feels a sudden disorientation and finds itself elsewhere: two of the four
  places are inside the inner tower.
- **The inner tower's lower level:** Al-Hyam Dazid, a captive mage who gives
  his notes on the Pool of Radiance (journal entry 30); the false
  Tyranthraxus on his throne, an actor who gives up when pressed; and stairs
  up, met facing east.
- **The upper level:** stairs, secret doors and a trap door over the medusa's
  chamber (a member who looks down may turn to stone); a waiting room where a
  messenger, spoken to civilly, is called away; Genheeris's office, where the
  second in command takes the party for Tyranthraxus's assassins and offers to
  join them against him (`ADD NPC 88`); and the audience hall, open to the
  sky, around the Pool of Radiance.
- **The finale:** Tyranthraxus, possessing an ancient bronze dragon, sends his
  guards, then offers the party command of his armies. Each member votes; the
  party attacks, and beaten, Tyranthraxus is destroyed: the game is won
  (`PROGRAM 8`), and the party walks back to the civilized city (`NEW ECL 0`).

## Monster conversions (MON5CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 31 | Troll | the Troll |
| 45 | 1st Level Thief (HP 4) | Kovel Mansion's thief |
| 49 | Medusa (HP 30) | **new:** SRD 5.2.1 Medusa with 30 Hit Points: two Claws or Poison Rays and its Petrifying Gaze (Snake Hair's poison not modeled) |
| 55, 56 | Hill Giant, Fire Giant | Stojanow Gate's and the old town's |
| 60 | Giant Snake (HP 25) | the western wilderness's Giant Venomous Snake |
| 66 | Tyranthraxus (HP 80) | **new:** SRD 5.2.1 Adult Bronze Dragon with 80 Hit Points: three Rends and its Lightning Breath (10d10 in a 90-foot Line, save DC 19, Recharge 5-6) |
| 75, 84, 87 | 6th Level Fighters, Envoy (HP 45) | the Pyramid's Warrior Veteran |
| 85 | 8th Level Fighter (HP 87) | **new:** SRD 5.2.1 Knight with 87 Hit Points (Parry not modeled) |
| 86, 88 | Al-Hyam Dazid, Genheeris (HP 22) | **new:** the nomad shaman's level-4 Wizard profile with 22 Hit Points |
| 92 | the false Tyranthraxus (HP 40) | **new:** the Warrior Veteran with 40 Hit Points |
| 93 | 6th Level Thief (HP 28) | Kovel Mansion's master thief |

## The companion

Genheeris (`ADD NPC 88`): a magic-user with Intelligence 18 and 10 in the
rest. He joins as a level-6 Wizard with those scores (a Sage).

## Engine support

Missing before this audit, now done:

1. **Maps shared by scripts:** a district names the other scripts that load
   its map and holds their creatures too, so the castle's scripts load each
   other's quarters and the old town's map.
2. **The area rows**, the conversions and Genheeris.
3. **`PROGRAM 8`, the game won:** every living member regains its Hit Points,
   the won cell (`0x4AFD`, coab's area byte `0x3FA`) is set, and the closing
   words wait for a key press before the script goes on.
4. **The castle's harm:** a bucket of sand, boulders and falling stones are
   bludgeoning missiles; drowning and the trap door's fall bludgeoning; the
   gnoll's scalding pot fire; the hedge's thorn poison.
5. **The location label** names the castle.
6. **The original's closing story** (Tyranthraxus's spirit leaving the dragon
   and called back through the pool) is read from the player's `GAME.OVR` at
   the victory and told before the closing words; nothing of it is stored in
   the repository, and another release's file leaves only the closing words.
7. **A monster's breath weapon** (a `breath` row): Tyranthraxus's Lightning
   Breath, taken with his action and recharging on a 5 or 6.

## Not done

- **The dead are not raised by the victory:** the original restores every
  member (see [DEVIATIONS](../DEVIATIONS.md)).

## Party level and balance

With rests between fights, a level-9 party reaches and beats Tyranthraxus and
his guards at the default encounter challenge.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [CASTLE-1](../SRD-DECISIONS.md#castle-1-2026-10-25-valjevo-castle). Played
  (`opengold_expedition_tests`, a new level-9 party through Stojanow Gate and
  the old town): the inner gate opened with RHODIA, Al-Hyam Dazid's notes, the
  maze into the inner tower, the stairs up, the messenger called away,
  Genheeris joining, Tyranthraxus beaten and the game won, and the walk back
  to the civilized city.
