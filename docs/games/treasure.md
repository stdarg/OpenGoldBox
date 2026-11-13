# Treasures of the Savage Frontier: how its files differ

*Treasures of the Savage Frontier* (1992) follows *Gateway to the Savage
Frontier* and runs on the engine *Pools of Darkness* also uses. This page
records what its files change, as the census (`opengold_census`,
[GAMES.md](../GAMES.md)) reads them. The facts come from the reconstructed
source of the DOS release and from the files themselves; the wording is our
own.

## Scripts, creatures and items

Since 2026-11-13 the scripts (`ECL1`, 30), creatures (`MON1CHA`, 102), their
items (`MON1ITM`, 46 lists), the item definitions (`ITEM0`) and the item types
(`ITEMS`) decode. Their layouts are *Pools of Darkness*' ([darkness.md](darkness.md)),
with the differences below; OpenGoldBox reads both games with
`late_records.h`.

### Scripts

The commands are *Pools of Darkness*' with three more, and a script may be
longer: it loads at 8000h and may run to B6B0h (the original cuts a longer
one there). A new game starts with script 16.

| Code | Command | Operands |
|---|---|---|
| 42h | LOAD WALL SETS | 4: a map (below 7Fh, indoors only) and the wall sets for the three slots; 0 or FFh empties a slot |
| 43h | FIND NAMED ITEM | 4: an item type (0 for any) and its three name words, the third first (a third word of 0 matches any name); the equal flag says whether a party member carries one |
| 44h | TAKE NAMED ITEM | 4: as FIND NAMED ITEM, and takes the first such item from the party |

As in *Pools of Darkness*, CLEAR AND EXIT (23h) ends a script, and the census
decodes the scripts without running them.

### Creatures

The record is *Pools of Darkness*' 510 bytes, except that a character knows
126 spells, at 179–304, and so has no attack level. Every other field is at
the same place. The orc, creature 12, has 1 Hit Die, THAC0 18, AC 6 and 4 hit
points.

### Items and item types

As in *Pools of Darkness*: 17-byte items, 254 definitions in `ITEM0`, and 128
item types in `ITEMS` with no header.
