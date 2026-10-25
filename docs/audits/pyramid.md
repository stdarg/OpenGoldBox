# Pyramid audit (2026-10-24)

What playing Yarash's Pyramid needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL7.DAX 22` and
`ECL7.DAX 23`, the Central Wilderness's `ECL7.DAX 26` (the way in and the
valley's healing), `opengold_creatures GAME`, `opengold_maps GAME`, coab's
`MovePartyForward`, `TryStepForward` and `WallDoorFlagsGet`, and SRD 5.2.1.

## The area

- **Two scripts and maps**, both with the wall banks `LOAD PIECES 1,3,255`
  (bank 255 loads nothing):
  - `ECL7:22` on `GEO7:22`, the prison of souls: an outer corridor, a maze of
    rooms joined by secret doors and teleporters, and the souls Yarash keeps.
  - `ECL7:23` on `GEO7:23`, his laboratories, joined by more secret doors,
    teleporters and rocks.
- **The way in:** the wilderness map's (6,14) offers a rowboat in the reeds;
  across, the Pyramid's secret door (`NEW ECL 22`) opens on the outer corridor
  at (15,0) facing south.
- **The way out:** the outer corridor's secret doors at (15,0) north and
  (15,15) south, once found, lead back to the wilderness map.
- **Secret doors** (events 1-11 outside, 1-10 inside): each square's door in
  one direction is held shut (`0x6DC9` = 255) until found. A search finds it:
  each living member rolls d6, needing at most 1 when searching (0 otherwise),
  plus 1 for an elf or half-elf.
- **Teleporters:** the outer (14,8) to (5,11), (14,9) to (0,0) and (6,1) into
  the laboratories at (0,12); inside, (15,8) to (3,13), and three to the dial's
  room (6,0), whose dial (blue, copper, silver, gold) sends the party back
  outside or to three of the laboratories' rooms.
- **Rocks** (the outer (14,7), the inner (0,2), (15,12) and (12,3)): stepped
  on, they send the party on by the parity of a flag (`0x4A4D` bit 1), which
  each use flips. Searching (or once known) the party feels a tingle and may
  go back, move on, or throw a rock, which flips the flag without moving.
- **The prison's souls** (`0xA169`): one step in three while searching (one in
  twenty-one otherwise) meets a group by the table at `0xA58D`: human fighters,
  displacer beasts, dwarven fighters, lizard men, driders, thieves, stirges,
  minotaurs, elves (Diane's band) or kobolds, met in peace or fought; and a
  human priest who asks the party to break through his tunnel.
- **The laboratories:**
  - five lizard men, wary of Yarash's servants; a torture room; vats where
    pain-crazed mutants surface; an alchemist's lab and its notebook;
    storerooms of magical junk with treasure.
  - **The password door** (1,1): a sign of six dwarvish runes, NOKNOK. The
    wrong word brings a monster that kills the whole party (201 damage).
  - **The fountain chamber** (2,1): destroying its equipment, before the
    pressure builds and it explodes (3d10 to everyone), stops the pollution
    (`0x4AB3` = 254).
  - **The pumping room** (5,4): three chained lizard men, freed for their
    tribe's story (journal entry 35), or enraged.
  - **Yarash** (4,1), behind a locked door: the ancient mage and six mutated
    guards. Beaten, he too stops the pollution.
  - **His desk** (6,1): searched, five papers for the journal.
- **The valley's healing:** back on the wilderness map with the pollution
  stopped, the Central Wilderness's script patches the original's colours of
  36 valley tiles, moves the Pyramid's landmarks and opens the rivers
  (`0x4AA2` = 255).

## Monster conversions (MON7CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 57, 58, 61, 68, 69, 123 | lizard men, mutants, stirges, displacer beasts, driders, kobolds | the Central Wilderness's |
| 62 | Minotaur (HP 33) | **new:** SRD 5.2.1 Minotaur of Baphomet with 33 Hit Points, its Abyssal Glaive (Gore not modeled) |
| 75 | 6th Level Fighter (HP 45) | **new:** SRD 5.2.1 Warrior Veteran with 45 Hit Points |
| 76, 80 | 7th Level Dwarf Fighters (HP 50) | **new:** SRD 5.2.1 Warrior Veteran with 50 Hit Points |
| 77 | 7th Level Thief (HP 28) | the Spy with 28 Hit Points, as Kovel Mansion's |
| 78 | 2nd Level Cleric (HP 8) | **new:** SRD 5.2.1 Priest Acolyte with 8 Hit Points |
| 79 | Diane (level-5 magic-user, HP 20) | the nomad shaman's level-4 Wizard profile |
| 81 | Yarash (level-9 magic-user, HP 27) | **new:** SRD 5.2.1 Mage with 27 Hit Points: Arcane Burst, Fireball, Invisibility, Cone of Cold, Misty Step and Protective Magic (the game has no Fly) |

## Engine support

Missing before this audit, now done:

1. **The area rows** for both scripts, with **wall bank 255** loading nothing.
2. **Search mode**, the original's Search command: each step searches as
   Look does (`0x6DCA` = 1) and takes ten minutes more (coab
   `MovePartyForward`); the S key turns it on or off, and it is saved
   (campaign format 40).
3. **Dwarvish runes** for `PROTECTION`: one glyph a letter, A at 0x79
   (the Pyramid's NOKNOK, Valjevo's RHODIA).
4. **Cells the host does not model:** the tile colours the valley's healing
   patches are accepted and dropped.
5. **The explosion's harm** (since 2026-10-25, met by the whole-map run): the
   fountain's equipment blowing up deals bludgeoning damage.

The step itself was checked against coab: the original stops a step only on
the wall on the party's side of the edge, as the engine has since Stojanow
Gate; every Pyramid door has a door on both sides.

## Party level and balance

The prison's groups and Yarash with his guards suit a level-6 party at the
default encounter challenge.

## Status

- **Increment 1 (done, 2026-10-24):** all of the above, by
  [PYRAMID-1](../SRD-DECISIONS.md#pyramid-1-2026-10-24-the-pyramid). Played
  (`opengold_expedition_tests`, a new level-6 party): the rowboat and the
  secret door, the prison's teleporters and the passage into the
  laboratories, the rocks in Search mode and a rock thrown, the password read
  from its runes, the fountain's equipment destroyed, Yarash beaten behind
  his locked door, his desk searched, the dial's teleporter back out, the
  north door found, and the valley healing.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)). The
  test now frees the pumping room's chained lizard men (`0x4A52` bit 1), whose
  friend-word the lizard men's keep asks.
