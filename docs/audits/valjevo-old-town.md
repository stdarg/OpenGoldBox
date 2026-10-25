# Valjevo old town audit (2026-10-25)

What playing the old town below Valjevo Castle needs, from the user's DOS
installation and the current engine. Sources: `opengold_scripts --inspect
GAME ECL5.DAX 6`, Stojanow Gate's `ECL2.DAX 9` (the way in), `opengold_creatures
GAME`, `opengold_maps GAME`, coab's `CMD_Damage`, `CMD_AndOr` and its player
fields (`0xD8`, alignment), and SRD 5.2.1.

## The area

- **Map:** `GEO5.DAX:6`, the old town Tyranthraxus holds below the castle,
  with `LOAD PIECES 24,25,1`: bank 24 is two blocks and fills the first two
  wall slots, as Valhingen Graveyard's bank 21 does. One script,
  `ECL5.DAX:6`.
- **The way in:** Stojanow Gate's north edge opens on (4,15).
- **The ways out:** the south edge, stepped off facing south, leads back to
  the gate; the north edge at (3,0) leads into Valjevo Castle (`ECL5:3`, in
  the game since 2026-10-25: [Valjevo Castle](valjevo-castle.md)), and the
  east edge at (15,12) into its maze.
- **Sites** (by each square's event number, the table at `0x9BF2`):
  - **the washerwomen** (12,13): spoken to, they offer their clothes as a
    disguise (`0x4A65` bit 1); threatened and left free, they talk, and the
    disguise is lost; tied up or killed, they keep silent or scream;
  - **the Temple of Bane** (1,2), a barracks turned temple: the priest and two
    acolytes offer Bane's blessing. Each member who is not evil doubles up in
    agony (2d10 + 2), and the priest, seeing impostors, throws a fireball
    (15 damage twice, the first allowing a saving throw) and attacks.
  - **the altar** (1,0): an offering box (item list 37) and two crossed
    swords (item list 35); something immense and evil stares down at the
    party (2d10 + 2 to everyone).
  - **the smithy** (0..3,14..15): the smith and three fire giants forging
    armor; talked to, the smith may see through a disguise; fought, they
    guard 20 platinum pieces, and the piled weapons are item list 33.
  - the smokehouse (0,6) and coal stores (9,5), (1,13).
- **Patrols** of hill giants led by a fighter roam the streets and ask the
  day's password (HARASH) and on whose authority (TYRANTHRAXUS), as at the
  gate.

## Monster conversions (MON5CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 41 | 4th Level Fighter (HP 30) | the nomad guards' Bandit Captain |
| 55 | Hill Giant (HP 41) | Stojanow Gate's Hill Giant |
| 56 | Fire Giant (HP 59) | **new:** SRD 5.2.1 Fire Giant with 59 Hit Points: two Flame Sword or Hammer Throw attacks (the throw's push and Disadvantage not modeled) |
| 75 | 6th Level Fighter (HP 45) | the Pyramid's Warrior Veteran |
| 90 | 1st Level Cleric (HP 8) | the Pyramid's Priest Acolyte |
| 91 | Level 5 Cleric (HP 29) | **new:** the Temple of Bane's Priest with 29 Hit Points |

## Engine support

Missing before this audit, now done:

1. **The area row** and its conversions.
2. **A member's alignment** in the character record the scripts read
   (`0xD8`, coab's order from lawful good to chaotic evil): the temple's
   blessing tests it.
3. **The temple's damage types:** Bane's blessing and the evil stare are
   necrotic, the priest's fireball fire.
4. **The location label** names the old town.

The two fireball calls are the original's: coab deals both.

## Party level and balance

With rests between fights, a level-8 party beats the temple's priests and the
smithy's fire giants at the default encounter challenge.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [TOWN-1](../SRD-DECISIONS.md#town-1-2026-10-25-valjevos-old-town). Played
  (`opengold_expedition_tests`, a new level-8 party through Stojanow Gate):
  the disguise, Bane's blessing and the priest's fireball, the altar, the
  smithy fought, the castle refused, and the way back to the gate.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)). The
  smith and his fire giants work only until the fourteenth hour; its temple's
  and smithy's alarms keep the castle's inner gate shut, so the run plays the
  old town after the victory.
