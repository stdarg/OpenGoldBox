# The whole map with one party (2026-10-25)

One party plays Pool of Radiance's map from Mendor's Library to Tyranthraxus's
fall and beyond, in one session: `whole_map` in `tests/expedition_tests.cpp`,
run by `opengold_expedition_tests` after the expedition chain (the Slums,
Kuto's Well, Podol Plaza, the Cadorna Textile House and Mendor's Library). The
per-area tests each start a new party; this run carries one party, its
experience, treasure, companions and the scripts' flags from area to area, and
earns every commission through City Hall's clerk instead of setting its flag.

## The route

Each stage starts with a save, so `--whole-map SAVE STAGE` plays on from one.

1. **home:** out of Kuto's Well, through the Slums to New Phlan; City Hall
   pays for the library, Podol Plaza and the textile house.
2. **auction:** back through the Slums and Kuto's Well to Podol Plaza, where
   the party sneaks to the auction on the council's mission; City Hall pays.
3. **sokal:** Sokal Keep by the harbor master's boat; City Hall pays.
4. **temple:** Tyr's bishop, Dirten, the Temple of Bane, home by the boat at
   the city's eastern edge.
5. **nomads:** the nomad camp.
6. **kovel:** a night at the inn, then Kovel Mansion; City Hall pays for the
   temple, Kovel and the nomads, and offers the graveyard's commission.
7. **pyramid:** the Pyramid, its pumping room's lizard men freed.
8. **kobolds:** the kobold caves; the party keeps the brass bottle.
9. **lizards:** the lizard men's keep, whose old lizard man knows the freed
   slaves' friend-word.
10. **graveyard:** City Hall's commission and its enchanted weapon, then
    Valhingen Graveyard, with Samir Ahwahl from the party's own bottle.
11. **sites:** the dragon Diogenes and the Buccaneers' base, the Bivant heir
    freed; City Hall pays.
12. **evidence:** through Stojanow Gate by the trader's wagon, the scribes'
    building in Valjevo Castle's northeast quarter searched until it gives up
    the evidence of Cadorna's treachery, and back south, taking the gate.
13. **outpost:** City Hall's message for the Zhentil keep, then the outpost.
14. **castle:** through the taken gate, the castle and Tyranthraxus; the game
    is won and the party walks back to New Phlan.
15. **south:** the old town, then home.

## What the original asks, found on the way

- **City Hall closes** from the fourteenth hour to midnight (`ECL3:0`
  `0x9920`): the party waits at the inn.
- **The clerk names three commissions a visit**, the first three still open,
  and repeats them until the party leaves the hall. The Slums stay open
  for a party that cannot win there again (below), so later commissions wait
  for earlier ones to be done.
- **Cadorna's commissions** are never offered to a party that opened his
  family treasure (`0x4AB8` = 255). Such a party gets the Zhentil keep's
  message only after the castle's evidence against Cadorna (`0x4ABE` = 254,
  `ECL5:4` `0xADA6`), and the clerk gives no commission once Tyranthraxus is
  beaten.
- **The Slums are cleared** at twenty-five wins (`0x4ABB`), but roaming groups
  stop after fifteen (`0x4A80`) unless the party murders the fortune teller,
  which resets the count. The expedition chain's party has seventeen and its
  roaming wins spent: it leaves the Slums commission open.
- **The old town's alarms** (`0x4A6A` bits 1 and 2, raised by its temple and
  smithy, never cleared) keep the castle's inner gate from asking its
  password; the old town comes after the victory.
- **Going back south through Stojanow Gate** raises its alarm; taking the gate
  opens its passage for good.
- **The smith and his fire giants** work only until the fourteenth hour.
- **The maze's disorientation squares** send the party only to one another;
  map 3's, at (12,11), is beside the inner tower's lower level.

## Engine changes the run needed

1. City Hall pays every reward through one TREASURE instruction, so a script
   that reaches a treasure again pays again ([QUESTS](../QUESTS.md#reward)).
2. A town building's own script gives a fixed item list as script treasure
   (City Hall's enchanted weapon); the town script's lists stock its shops.
3. Leaving the Temple of Bane's block for the wilderness loads the wilderness
   script's map before handing over to it.
4. Loot goes to player characters before companions, who leave with what they
   carry (Fatima took the brass bottle).
5. A night at the inn passes without a Long Rest's benefits when no one may
   take another within a day.
6. The Pyramid's exploding equipment deals bludgeoning damage.
7. The combat AI uses an area item only on an enemy it can aim at in range and
   sight; an out-of-reach target had it aim and cancel forever.
8. A fight's command list no longer reads every carried item's key for a
   spell no item casts or for a member who reads no scrolls, which a
   well-stocked party made slow.

## Stand-ins

- **Experience:** `reach_level()` tops the party up to each area's audit level
  where its own fights fall short, for the roaming fights and training a
  player has between areas.
- **Healing:** the party is healed between fights and areas, as the per-area
  tests do.
- **Companions:** the mad man who joins in New Phlan is left with Tyr's priest
  as he asks; the mystic stays behind before the castle so Genheeris has an
  NPC place. The chain's Knock wizard is its own recruit.
