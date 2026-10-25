# Kobold caves audit (2026-10-25)

What playing the kobold caves needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL8.DAX 13`, the
eastern wilderness's `ECL8.DAX 27` (the way in), `opengold_creatures GAME`,
`opengold_maps GAME`, coab's `CMD_Rob`, and SRD 5.2.1.

## The area

- **Map:** `GEO8.DAX:13`, the kobold king's caves, with wall banks `LOAD
  PIECES 17,18,255`. One script, `ECL8.DAX:13`.
- **The way in:** the eastern wilderness's two caves at (6,15): the small,
  hidden one opens at (6,15) facing north, the large one at (10,15) past a
  drunken kobold.
- **The way out:** the entrances ask whether to leave; leaving, Princess
  Fatima takes her leave of the party if she is with it.
- **Events** (the jump table at `0x9964`):
  - the pool (6,13): a member falls in, and items are lost recovering them;
  - the mine tailings (6,11), searched: a kobold's drawing (journal entry 42);
  - the truce kobold (8,12): agreed, it leads the party past its traps (a
    net, spikes, a deadfall) and a boulder to the wyvern's roost, then flees;
  - the king's hall (4,3)-(6,3): a horde, two Zhentil envoys and the ancient
    king on his throne. The king's ballista fire between waves; beaten, the
    king flees to his quarters (7,0), where his guards fight, and dies in his
    own spiked pit (10,1). The community's wealth lies beyond (13,1).
  - Princess Fatima (1,3), a barbarian captive, offers to join.
  - the brass bottle (12,0), searched: the efreeti Samir Ahwahl, who seeks
    vampires and returns to his bottle (item list 50), or fights.
  - the wyvern's roost and its treasure (14,6), the barracks and its map
    (journal entry 28), a crippled kobold's tale (entry 20), living areas.
  - **Traps** with a chance to see them (Find Traps, or the party's best
    chance to find traps) and disarm them (the chosen member's chance).
- **Surrender:** the king offers it after his ballista fire; surrendering, the
  party is robbed, thrown in a cell, and may fight its way out with Fatima.

## Monster conversions (MON8CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 0, 1 | Kobold, Kobold Leader | the Slums' |
| 31 | Troll | the Troll |
| 87 | Envoy (level-6 fighter and magic-user, HP 45) | the Warrior Veteran with 45 Hit Points |
| 104 | Princess Fatima, fought (HP 33) | the Warrior Veteran with 45 Hit Points |
| 120 | Wild Boar | the western wilderness's Boar |
| 121 | Wyvern | the eastern wilderness's Wyvern |
| 124 | Efreeti (HP 55) | **new:** SRD 5.2.1 Efreeti with 55 Hit Points: three attacks, Heated Blade or Hurl Flame (its spells and Magic Resistance not modeled) |

## The companion

Princess Fatima (`ADD NPC 104`): a level-4 fighter with Strength 18,
Dexterity 18, Constitution 16, Intelligence 14, Wisdom 10 and Charisma 15. She
joins as a level-4 Fighter with those scores (a Soldier) and her short sword,
leather armor and shield on.

## Engine support

Missing before this audit, now done:

1. **The area row** and its conversions.
2. **Princess Fatima** and her leaving, by the record-blanking Dirten uses.
3. **`ROB` of one member** (form 0): that member's share of the party's purse.
4. **More than 56 monsters loaded:** the fight takes the first 56, as the
   original's monster slots did.
5. **The traps' damage types** (the roof, spikes, thrown rocks).

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [KOBOLD-1](../SRD-DECISIONS.md#kobold-1-2026-10-25-the-kobold-caves).
  Played (`opengold_expedition_tests`, a new level-7 party from the harbor's
  eastern boat): the small cave, the pool, the truce kobold's escort and the
  wyvern, the king's hall and his ballista, Fatima joining, the king's
  quarters, his death and the community's wealth, the wyvern's treasure, the
  brass bottle searched out, and the way out with Fatima leaving.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)). Loot
  now goes to player characters first, so the brass bottle stays with the party
  when Fatima leaves.
