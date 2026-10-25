# Podol Plaza audit (2026-10-22)

What playing Podol Plaza needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL1.DAX 18` (and the
scripts that lead to it), `opengold_creatures GAME`, `opengold_magic_items
GAME`, `opengold_maps`, the script's data tables read from the record bytes,
the Gold Box Explorer map names in `demos/godot/scripts/por_map_names.gd`, the
cluebook's Podol Plaza page (p13, reference only), coab (the Curse of the Azure
Bonds reimplementation) for engine behaviour, and SRD 5.2.1. The script calls
the place "Podal Plaza"; this audit uses the cluebook's spelling.

## The area

- **Map:** `GEO1.DAX:18`, a 16 x 16 block of the old trading district. One
  script, `ECL1.DAX:18`, drives it: `LOAD FILES 18,2,255` (`0x9971`) and wall
  banks `LOAD PIECES 1,3,5` (`0x9978`). 120 of 256 squares are interior. The
  map's event bytes (low seven bits) lay it out:
  - **0** (79 squares): the open plaza, where roaming groups meet the party.
  - **11** (54): the streets around the plaza; nothing happens there, so
    hugging the walls avoids fights, as the cluebook advises.
  - **9** (80, all interior): building squares; they only record the square
    in `0x4A1A`, which suppresses the shop messages below while the party
    stays inside.
  - **7** (15) and **8** (14): a looted shop and a rat-infested ruin, text
    only.
  - **1** (7,6), (7,7), (7,8): the auction block.
  - **2** (4,8): the Pitt, a tavern; **3** (5,8): the sign over its door.
  - **5** (13,8): a small shrine whose Ilmater symbols were replaced by
    Bane's; **6** (14,8): the robed orc inside it.
  - **4** (1,15): the hidden temple of Ilmater; **10** (six squares around it,
    (0..1,13..14), (0,15), (2,15)): its rooms. Both doors into it, the west
    faces of (2,13) and (3,15), have door code **3** (wizard-locked).
- **Entries:** four scripts lead here.
  - Kuto's Well (`ECL8:29`): its west edge (table `0xAFC7`, facing 3 gives
    bank 1, script 18). The party wraps to Podol's east openings, (15,4) and
    (15,11).
  - Stojanow Gate (`ECL2:9`, `0x9C68`): any edge but the north one (which
    goes to Valjevo Castle, `ECL5:6`).
  - Cadorna Textile House (`ECL4:2`, `0x997F`): its north edge.
  - The wilderness map (`ECL7:26`, `0x9EE2`): at the city's western edge,
    choosing the north entrance puts the party on (0,4).
- **Exits:** every edge has two openings (x or y = 4 and 11). The step entry
  (`0x99B9`) runs when the party walks off the map: it steps the party
  (`CALL 0xC01E`), sets `0x6DC9` = 255, writes the destination's archive bank
  to `0x6E12` and calls `NEW ECL` by facing (`0x99D4`):
  - north: bank 2, `NEW ECL 9`, Stojanow Gate (`0x99EC`);
  - east: bank 8, `NEW ECL 29`, Kuto's Well (`0x99F6`), the only neighbour
    loaded today;
  - south: bank 4, `NEW ECL 2`, Cadorna Textile House (`0x9A00`);
  - west: bank 7, overland position (11,28) in `0x49C3`/`0x49C4`,
    `NEW ECL 26`, the wilderness map (`0x9A16`).
- **Script entries** (the five jumps at `0x9900`): `0x99B9` before each step
  (exits, the plaza menu), `0x9AB7` on arrival at a square (the event
  dispatch, `ON GOTO` on the event byte at `0x9AC0`), `0x9914` before camping,
  `0x9960` when a camp is interrupted, `0x9971` on entering the area.

### Two ways in: on the council's mission or not

City Hall (`ECL3:8`, `0xA9B0`) offers a commission: a powerful weapon is to be
auctioned in the plaza; find out what it is. It sets `0x4AB0` = 1. On entry
(`0x9971`), when the previous script was not this one (`0x49F2` ≠ 18):

- **On mission** (`0x4AB0` = 1): `0x4A34` = 64 (mission mode) and `0x4A35` =
  0 (approach not yet chosen).
- **Not on mission:** `0x4A35` = 255, and `0x4A34` = 0 unless the plaza is
  already cleared (`0x4AE7` ≥ 254). `0x4A34` then counts won roaming fights,
  so clearing has to happen in one visit: every re-entry restarts the count.

`0x4A35` holds the party's approach: 0 unchosen, 1 disguised, 2 sneaking, 254
the auction is over, 255 striding openly or cover blown.

### Events

- **The crowded plaza** (`0x9A36`): on mission, the first step after entering
  asks how to proceed: stride boldly, disguise the party as monsters, or sneak
  unseen. `GETTABLE` at `0xB255` (255, 1, 2) stores the answer in `0x4A35`.
  Before that, `0x9A1A` sets cell `0x49FD` to 11 before 14:00 and 8 after
  (meaning unknown; Cadorna's script writes the same cell).
- **Roaming groups** (event 0, `0x9AEA`): each plaza step rolls 0-25. Off
  mission a group comes on 0 (1 in 26); on mission, 0-2 overhear a rumour and
  22-25 bring a group (4 in 26). Original Search mode (`0x6DCA` = 1) raises
  the odds of a group in both cases. After nine won fights off mission (`0xB241`), `0x4AE7` = 254: the
  plaza is cleared and no more groups come (the cluebook says ten).
  - **The group** (`0x9F86`): a 0-8 roll picks goblins (4 in 9), orcs (3 in 9),
    hobgoblins or ogres (1 in 9 each) through the tables at `0xB260` (type:
    rolls 0-8 give 0, 0, 1, 1, 2, 2, 3, 4, 5),
    `0xB269` (MON1CHA record 2, 2, 4, 4, 6, 8) and `0xB26F` (speed 6 or 9). Its
    size is `PARTY STRENGTH` / 1, 1, 1, 1, 2 or 4 (`0xB275`) / 3 = n, then n
    to 2n, doubled when a mission party's cover is blown, at least 2 and at
    most 50. Groups of at least 40, 40, 30, 30, 20 (`0xB287`) bring 5, 5, 4,
    4 or 3 leaders (`0xB28D`, the next record); ogres never do. Morale
    (`0x6DC6`) is 30, 35, 40, 45, 45 or 50 by type (`0xB281`), +10 with
    leaders.
  - **Disguised** (`0x9B53`): the group is spotted at a distance and comes
    closer. The chance of being questioned is 5% plus 5% for each of the eight
    party slots whose character byte `0x72` is not 7 (the race, coab `Race`; 7
    being human in the Gold Box order; empty slots count). Otherwise the group
    walks on. Questioned, the party parleys (`PARLAY` at `0x9C31`): Haughty or
    Abusive pass if `PARTY STRENGTH` beats a 0-50 roll, Sly passes if the
    spokesman's reaction score (`0x6DCF`) beats 0-30 ("here for the boss"),
    Nice and Meek give the party away; failure means a fight.
  - **Sneaking** (`0x9D15`): one in six the group questions the party as above,
    two in six they stop and it moves on, three in six the party slips past.
  - **Striding, unchosen or blown** (`0x9D6F`): the encounter menu
    (`ENCOUNTER MENU` at `0x9D86`, distance 2, 0 on mission). Its parley
    (`0x9EA8`) scores Haughty 10, Sly, Nice or Meek 20, Abusive 30, plus 0-50,
    plus twice `0x6DCF`, plus a group modifier (`0xB27B`: goblins 30 or 20,
    orcs 20 or 10, hobgoblins and ogres 0): 112 and up they let the party go,
    90-111 they move off with an insult, below 90 they fight. A blown mission
    party gets no parley. A successful flight (`0x9E23`) puts the party on
    one of eight fixed squares from (4,0) to (12,7) (table `0xB258`).
  - **The fight** (`0x9DE2`, `0xA0A0`): `CLEAR MONSTERS`, `LOAD MONSTER`
    for the group and its leaders, `COMBAT`. A win counts toward clearing
    (`0x9E0C`). After any fight on an open-air square a disguised or sneaking
    party's cover is blown (`0x9E68`).
- **Rumours** (`0xA121`): on mission, the party overhears the group in turn
  wonder whether the boss will show up, complain that the giants who took the
  castle bring more ogres, describe someone melting on touching "the thing",
  say the boss will be the one bidding up the price, and say the Bane shrine
  is recruiting for Mace, to Grishnak's displeasure.
- **The auction** (event 1, `0xA2A3`): only while `0x4A35` is 0-2, so only on
  mission, disguised or sneaking; striding in forfeits it. It shows
  `PICTURE 16`: an auctioneer offering a wand or staff. Choices: stand and
  listen, move in closer, listen to comments (two random remarks naming it a
  wand of illumination or a staff of command, `0xA74F`), or leave.
  - Listening (`0xA38A`): a plain-clothed man beside an ogre bids 5,000 gold.
    The party may bid, wait for the winner (the man and the ogre trade the
    wand for a bag and vanish), or try to leave. Bidding: the man darkens the
    block, the ogre runs; the party may chase the ogre (he is caught and has
    nothing; the crowd looks for a scapegoat), slip away, or look for the man
    (the auctioneer curses "Garwin", who has escaped).
  - Moving closer reveals the wand is only a wand of fear.
  - Each risky choice (bid, move closer, chase, leave) runs the cover check
    (`0xA875`): a sneaking party is always noticed, a disguised one one time
    in six. Noticed, the crowd attacks, the auction is cancelled and the party
    meets the last group rolled this visit (`0xA7D1`).
  - Any ending sets `0x4A35` and `0x4AB0` to 254 (`0xA82B`, `0xA86B`): the
    mission is done. City Hall then rewards it: `ECL3:8` scans its flag table
    at `0x4AA6` for 254 (index 10 is `0x4AB0`; its line at `0xA15A`), pays
    reward slot 10 (`0x9EC2`: 250 gp and 200 pp) and sets the flag to 255.
- **The Pitt** (event 2, `0xA8B0`; its sign is event 3, `0xAD7B`):
  - First visit (`0x4A37` = 0): the party bumps into a drunk buccaneer (head
    6 and body 3 in `0x6DE1`, `SETUP MONSTER 19,0,3`) who demands that one of
    them answer for it. Choices:
    - **Challenge** (`0xA985`): `WHO` picks a living champion (`0x6C00` > 0),
      then `CLEAR MONSTERS`, `CALL 0x8001` (coab's one-on-one duel: only the
      chosen member fights), the buccaneer (MON1CHA 41) alone, `COMBAT`.
      `0x4A37` = 1. A result above 128 (in coab, the party fled) makes him
      call it settled and leave.
    - **Attack** (`0xAA6A`): the patrons join in (morale 30): 4 hobgoblins,
      12 goblins, the buccaneer and 8 orcs. Afterward the bar lies wrecked
      (`0x4A37` 2, then 3).
    - **Flee:** the escape at `0x9E23`.
    - **Bribe** (`0xAAD0`): he asks a tenth of the party's money; yes is
      `ROB 1,10,10` (10% of coins and, in the original, of items). He leaves,
      but `0x4A37` stays 0, so he is back on the next visit.
  - Later visits (`0xAB1A`, also right after the duel or a bribe): the
    creatures ignore the party. Mix with them (a sneaking or exposed party is
    attacked; otherwise they trade insults, and attacking or insulting back
    starts a free-for-all, trying to leave brings the patrons' attack),
    drink heavily, attack (the patrons), or leave (`0xAC9C`, back to the
    previous square from `0x49F0`/`0x49F1`).
    - **Drinking** (`0xAC34`): the party wakes on (2,10) facing south, robbed
      by `ROB 1,100,50` (all coins, half the items in the original); a mission
      party's cover is blown.
    - **Free-for-all** (`0xACAF`, morale 50): 25 goblins, 20 hobgoblins and 5
      ogres; then 21 random picks among monster slots 8-52 get record byte
      `0xB8` = `0xB2` (coab's berserk-NPC control value: they attack whoever
      is nearest) and byte `0x10C` = 128 (meaning unverified), through
      `LOAD CHARACTER` on the monster's slot.
  - Won fights leave the buccaneer's gear (MON1ITM 41: Long Sword +1, Chain
    Mail +1) as loot, as the cluebook promises.
- **The hidden temple of Ilmater** (event 4, `0xADAB`): behind the
  wizard-locked doors. A young healer (head 48, body 25, `PICTURE 25`) offers
  help; yes sets `0x6DE2` = 1 (`0xAE55`) and calls `COMBAT` with no monsters,
  which opens the temple's services. Either way she says the party may rest
  as long as it likes (`0x4A1A` = 4). A revisit while `0x4A1A` still holds 4
  first says the party feels safe here. Camping on its squares (raw event
  bytes 132 and 138) is never interrupted (`0x992B`, `0x9938`).
- **The Bane shrine** (event 5, `0xAE9B`, text) **and the robed orc** (event
  6, `0xAEDB`, `SETUP MONSTER 4,1,4`):
  - First meeting (`0x4A38` = 0): leave, greet or attack. A party that is
    striding, exposed, off mission (all 255) or sneaking (2) fights whatever it
    picks; only a disguised party can talk.
  - Greeting (`0xAF4F`): the orc recruits creatures for Bane's temple to the
    north-east or, once that temple has fallen (`0x4ABD` ≠ 0, set by
    `ECL1:24` at `0xABC8`), for retaking it. Yes or no, he names a meeting in
    two hours: `0x4A38` = 1, meeting hour `0x4A19` = hour + 2 (mod 24), and he
    shows the party out (back one square).
  - **The loyalty test** (`0xB0B6`): once the hour has passed (strictly after
    it; across midnight it fires at once), many creatures have gathered and
    the orc orders them to prove their loyalty on the party: 8 orcs, the orc
    leader and 3 ogres, morale 50.
  - **The orc alone** (`0xB181`): the Orc Leader (MON1CHA 5), morale 90.
  - Both fights carry `TREASURE` list 22: six Leather Holy Symbols (item type
    74). The Temple of Bane's blind orc lets in a party carrying one (`ECL1:24`
    `FIND ITEM 74` at `0x9A1E` and `0xA5CF`). `0x4A38` = 2 afterward.
- **Camping** (`0x9914`): every 2 five-minute steps, a 16% chance of a roaming
  group (`0x9960`: the group roll, then straight to the fight). None in the
  temple or once the plaza is cleared. The mission-mode branch (`0x9953`: 1
  and 99) is overwritten by the next two instructions, so it has no effect in
  the original either.
- **Quest flags other areas read:** `0x4AB0` (the auction commission: City
  Hall sets 1, Podol sets 254, City Hall pays and sets 255), `0x4AE7` (the
  plaza cleared: Podol sets 254; City Hall announces a reward for clearing it
  at `ECL3:8 0x9DFC`, sets 255 and pays the same reward slot 10; this branch
  sets `0x4A00` to 2, which skips the table write at `0x9F5A`, so `0x4AB0` is
  left as it was), `0x4ABD` (read: the Temple of
  Bane has fallen), item type 74 (the holy symbols for `ECL1:24`).
- **Art:** `WALLDEF1`/`8X8D1` (banks 1, 3, 5), `SPRIT1` (2, 4, 6, 8, 9),
  `CPIC1` (icons 2-9 match the Slums' `CPIC2` byte for byte, plus 16, the
  buccaneer), `PIC1` (close-ups 2, 4, 6, 8; 16, the auctioneer), `HEAD1`/
  `BODY1` (6+3 the buccaneer, 48+25 the healer, 13+7 presumably Mace),
  `MON1CHA`, `MON1ITM` and `ITEM1` are all present. `SPRIT1` has no record 19,
  the sprite the Pitt's `SETUP MONSTER` names; at distance 0 the original
  shows the head-and-body portrait instead. The temple's place record already
  exists: `data/shops/podol-temple-ilmater.json` marks (1,15) on `GEO1:18`
  with the shop manager's Ilmater portrait. No NPC portrait in
  `art/portraits/NPCs` covers this area.

## Fights

| Fight | `LOAD MONSTER` (MON1CHA record × count) |
| --- | --- |
| Roaming goblins (`0xA0CC`, `0xA0FA`) | Goblin Guard (2) × n; Goblin Leader (3) × 5 when n ≥ 40 |
| Roaming orcs | Orc (4) × n; Orc Leader (5) × 4 when n ≥ 30 |
| Roaming hobgoblins | Hobgoblin (6) × n; Hobgoblin Leader (7) × 3 when n ≥ 20 |
| Roaming ogres | Ogre (8) × n, no leaders |
| Camp interruption (`0x9960`) | a roaming group |
| Auction cancelled (`0xA831`) | the last roaming group rolled this visit |
| Buccaneer's duel (`0xA9D7`) | 4th Level Fighter (41) × 1, one party member |
| Patrons attack (`0xAA98`) | Hobgoblin (6) × 4, Goblin Guard (2) × 12, 4th Level Fighter (41) × 1, Orc (4) × 8 |
| Free-for-all (`0xACD8`) | Goblin Guard (2) × 25, Hobgoblin (6) × 20, Ogre (8) × 5; 21 random picks berserk |
| The robed orc (`0xB188`) | Orc Leader (5) × 1; `TREASURE` list 22 |
| Loyalty test (`0xB148`) | Orc (4) × 8, Orc Leader (5) × 1, Ogre (8) × 3; `TREASURE` list 22 |

n is the roaming group's size from `PARTY STRENGTH` (above). As in the Slums
and Kuto's Well, every fight is trimmed to the party's XP budget and to one
creature per living character (`encounter_challenge`), each group keeping at
least one.

## Monster conversions

MON1CHA records 2-8 are byte-for-byte the Slums' MON2CHA records 2-8, and
their combat icons are the same, so the Slums rows apply unchanged (the orc
leader shoots a bow with icon 5, as the robed orc's `LOAD MONSTER 5,1,5`
uses).

| Record | Original | Conversion |
| --- | --- | --- |
| MON1CHA 2 | Goblin Guard (HP 4, AC 7, 1d6) | `slums-goblin` (SRD 5.2.1 Goblin Warrior), 50 XP |
| MON1CHA 3 | Goblin Leader (HP 7, AC 5, 1d6, bow) | `slums-goblin-leader`, fit 50, award 100 |
| MON1CHA 4 | Orc (HP 5, AC 6, 1d8) | `slums-orc` (SRD 5.1 Orc), fit 100, award 75 |
| MON1CHA 5 | Orc Leader (HP 8, AC 5, 1d8, chain, bow) | `slums-orc-leader` (archer with icon 5), fit 100, award 150 |
| MON1CHA 6 | Hobgoblin (HP 6, AC 5, 1d8) | `slums-hobgoblin` (SRD 5.2.1 Hobgoblin Warrior), 100 XP |
| MON1CHA 7 | Hobgoblin Leader (HP 6, AC 5, 1d8, bow) | `slums-hobgoblin`, fit 100, award 200 |
| MON1CHA 8 | Ogre (HP 21, AC 5, 1d10) | `ogre` (SRD 5.2.1), 450 XP |
| MON1CHA 41 | 4th Level Fighter, the buccaneer (HP 30, AC 8 unarmored; Long Sword +1, Chain Mail +1, shield) | **new; user decision** (question 1): an SRD 5.2.1 human warrior. The literal match, Warrior Veteran (CR 3, 700 XP), is deadly in a one-on-one duel with a level 3-5 member; a CR 1-2 block suits it: the SRD 5.2.1 Pirate (p. 314: CR 1, 200 XP, AC 14, HP 33, two Dagger attacks at +5 for 1d4 + 3, Enthralling Panache), or the Bandit Captain (CR 2, 450 XP), as Norris. His magic gear is loot, not worn. |

MON1CHA 9 (Ogre Leader) is never loaded: the ogre row of the leader table is
0. MON1CHA 4th Level Fighter records also appear in MON3, MON5, MON6 and MON7,
so this choice will likely serve New Phlan, Valjevo Castle, the Buccaneer Base
and the Nomad Camp too.

## Engine support

Supported today: the interpreter runs every command the script uses
(`GETTABLE`, `MULTIPLY`, `DIVIDE`, `COMPARE AND`, `AND`, `OR`, `ON GOTO`,
string `SAVE`, `PARLAY` as a five-choice menu); `VERTICAL MENU` and
`HORIZONTAL MENU`; `CALL 0xC01E` (step) and `CALL 0x2C90` (redraw at a pose
the script wrote, used by the escape, the drinking and the leaving);
`APPROACH`, `DELAY`, `SPRITE OFF`, `PICTURE` (whole pictures and head-and-body
portraits); `SETUP MONSTER` with a `SPRIT` record; `ENCOUNTER MENU` (its
results 0 leave, 1 fight, 2 fled, 3 parley match the script's `ON GOTO` at
`0x9DD0`); `PARTY STRENGTH` (coab's formula); `LOAD CHARACTER` for party slots
0-7 (the disguise check reads the race at byte `0x72`; see
[PARTY.md](../PARTY.md#ecl-adapter));
`WHO`; `CLEAR MONSTERS`, `LOAD MONSTER`, `COMBAT`, XP and loot in any area
with conversion rows; `TREASURE` with a fixed `ITEMn` list in an area;
`ROB 1,x,y` (coins only, by the user's Kuto's Well choice); `COMBAT` with
`0x6DE2` = 1 opens a temple, using the place record at (1,15); the previous
square (`0x49F0`/`0x49F1`); morale from `0x6DC6`; `NEW ECL` to a loaded area.

Missing:

1. **Area loading.** No row for Podol Plaza in the area table
   (`rolf_tour.cpp`, `AreaSource`). It needs script 18, bank 1, map 18 with
   pieces {1, 3, 5}, and conversion rows for MON1CHA 2-8 and 41. Until then
   Kuto's Well's west edge stops with "Travel outside New Phlan (script 18)"
   (`phlan_session.cpp`, `NEW ECL`).
2. **Exits to unloaded areas.** North (`NEW ECL 9`), south (`NEW ECL 2`) and
   west (`NEW ECL 26`) stop with the same "Travel outside New Phlan"
   diagnostic. Stojanow Gate, Cadorna Textile House and the wilderness map are
   later areas; until then those exits need a clear refusal.
3. **Camp interruption profile.** `rest_interruption` accepts only the city
   watch, the Slums street (24/24) and Kuto's Well plaza (12/12) profiles;
   Podol's 2 steps / 16% throws "Unsupported probabilistic camp interruption".
   A general rule (any interval and chance) would cover every later area.
4. **Wizard-locked doors** (door code 3, the temple's two doors). Only code 2
   is a lock (`locked_door = 2`); code 3 on a wall is solid, so the temple
   reports "The way is blocked." and cannot be reached. How they open is
   question 2.
5. **The Pitt's portrait.** `SETUP MONSTER 19,0,3` throws "Unsupported
   encounter sprite 19": `SPRIT1` has no record 19, and `SETUP MONSTER` never
   composes the head-and-body portrait (`0x6DE1` = 6, body 3) that the
   original shows at distance 0. This blocks the whole tavern, since every
   later visit needs `0x4A37` ≥ 1.
6. **The duel** (`CALL 0x8001`): "Unsupported native service in tour: 32769".
   Needed: only the `WHO` champion fights, the rest of the party stands
   aside, and losing the champion is not a party defeat. Lizard Man Keep
   (`ECL8:16 0xA344`) uses the same call for its champion.
7. **Combat result codes.** The engine writes 0 (won) or 128 (fled). coab
   sets 0x81 when the party flees and 0x80 when a duel is lost, and Podol tests
   `> 128` after five fights (`0x9DFD`, `0xAABB`, `0xAD34`, `0xB175`,
   `0xB1A7`). With 128 for flight, a party that flees the patrons' brawl or
   the free-for-all is recorded as having wrecked the bar, and no fleeing
   party is moved to an escape square. To verify across areas before relying
   on it.
8. **The free-for-all.** `LOAD CHARACTER` on monster slots 8-52 throws
   "Invalid ECL party position" (`party_character_reply` takes slots 0-7
   only), and the engine has no berserk control (attack the nearest creature,
   any side). Question 4.
9. **Robbery of items.** `ROB 1,10,10` and `ROB 1,100,50` would also take 10%
   and 50% of the items in the original; the engine takes only coins.
   Question 3.
10. **The cancelled auction's group.** If no roaming group was rolled this
    visit, the encounter menu at `0xA831` reads the group cells
    `0x4A01`-`0x4A0B` left by another area (or 0), so it fails ("Unsupported
    roaming encounter sprite") or meets a stranger's group. Question 5.
11. **Search mode** (`0x6DCA` = 1, which raises roaming odds) is never set;
    the same gap as Kuto's Well. Low priority.

## Party level and balance

The arc to here is the Slums (levels 1-3) and Kuto's Well (2-4); Podol Plaza
follows at about level 3-5, with the council's mission in hand. At the
default encounter challenge (33% of the Moderate budget) a level-4 party of
six has 742 XP. Roaming groups shrink to six goblins, orcs or hobgoblins, or
one ogre. The loyalty test becomes 2 orcs, the orc leader and an ogre (750
XP); at level 3 every group keeps its first creature, so it stays at an orc,
the leader and an ogre (650 XP against 445). The patrons' attack becomes a
hobgoblin, 2 goblins, the buccaneer and an orc; the free-for-all 3 goblins, 2
hobgoblins and an ogre. The duel is not fitted (one creature), so its balance
rests on question 1. The campaign-arc simulator should get a Podol Plaza
stage after Kuto's Well.

## Decisions needed

1. **The buccaneer's stat block (MON1CHA 41).** Recommend an SRD 5.2.1 CR 1
   block: the SRD 5.2.1 Pirate (p. 314, CR 1, 200 XP; else the Bandit Captain),
   not the Warrior Veteran, because a single level 3-5 member must survive
   the duel.
2. **Wizard-locked doors.** Recommend Knock opens them, and Bash or Pick
   succeed at DC 25 (Arcane Lock adds 10 to the DC 15 of an ordinary locked
   door).
3. **Items taken by robbery.** Recommend keeping the Kuto's Well rule (coins
   only) for the bribe and the drunken stupor, for consistency.
4. **The free-for-all.** Recommend a simplification: all 50 are enemies,
   with no berserk picks, documented in `DEVIATIONS.md`; fitting already cuts
   the fight to six creatures.
5. **The cancelled auction with no group rolled.** Recommend rolling a fresh
   plaza group (the script's own `0x9F86` roll) when none was rolled this
   visit, documented in `DEVIATIONS.md`.

## Status

- **Increment 1 (done, 2026-10-22):** Podol Plaza has an area row
  (`rolf_tour.cpp`): script 18, bank 1, map 18 with wall banks 1, 3, 5, and
  MON1CHA 2-8 converting as the Slums' records. Kuto's Well's west edge enters
  it at (15,4) and its east edge leads back; a save made there loads back. The
  north, south and west exits are refused with a notice naming Stojanow Gate,
  Cadorna Textile House or the wilderness, and the party stays put. Camp
  interruptions accept any interval and chance, so the plaza's 2 steps / 16%
  works. Played from the installed game (`opengold_expedition_tests`, after
  Kuto's Well, with the party raised to level 4): a roaming group is met and
  beaten on the plaza, a camp there is attacked by a roaming group, and the
  robed orc fights a party that is not disguised and leaves his six leather
  holy symbols (`0x4A38` = 2). At level 2 the expedition party lost a camp
  fight there: with a check every ten minutes, almost every rest on the
  plaza is attacked, as in the original, whose only safe rest is the temple.
- **Increment 2 (done, 2026-10-22):** wizard-locked doors (door code 3) offer
  Bash, Pick and Knock under "Wizard locked."; Bash and Pick need DC 35 and
  Knock always opens (PODOL-1). Played: Bash fails at the temple's west door,
  a recruited Wizard's Knock opens it, the healer's YES opens the temple of
  Ilmater at its own record's prices (Cure Light Wounds bought), and three
  rests inside are never interrupted.
- **Increment 3 (done, 2026-10-22):** combat results are the original
  engine's (coab `ovr006`): 129 when the party flees, 128 when it falls or
  loses a duel. The engine wrote 128 for a flight, so scripts testing "above
  128" treated fleeing as winning: New Phlan's tavern brawl sent the city
  watch after a fleeing party, and fleeing a Slums fight counted toward
  clearing the Slums. The buccaneer is the SRD Pirate (`buccaneer` in
  `combat.rules`; Enthralling Panache came later, see below). With SPRIT1 lacking
  sprite 19, `SETUP MONSTER` at distance 0 shows the head-and-body portrait.
  `CALL 0x8001` makes the next `COMBAT` a duel: only the member `WHO` chose
  fights; a lost duel leaves the champion on 1 Hit Point and is not the
  party's defeat. A staged monster's record (slot 8 and up) loads and stores
  without effect, so the free-for-all's berserk picks change nothing. Played
  from a save on entering the plaza: the bribe (a tenth of the coins, no
  items), the duel won, then drinking heavily (all coins taken, waking on
  (2,10)), the patrons' attack (the bar wrecked), the free-for-all, and a
  duel lost by a champion on 1 Hit Point.
- **Increment 4 (done, 2026-10-22):** the council's mission. City Hall
  offers the auction commission only after one of its first three
  commissions is done, which the expedition route does not do, so the test
  sets its flag (`0x4AB0` = 1) through a test-only friend
  (`tests/tour_access.h`), by the user's choice; the same friend reseeds the
  script's dice, so the test tries a few seeds until a disguise or a sneak
  holds long enough (a fight on the plaza blows the party's cover). The auctioneer's picture
  (`PIC1` 16) is an animation; its first frame stands for it. When the
  auction opens with no roaming group rolled this visit, the engine rolls one
  from the script's own tables (`0x9F86`), so a cancelled auction meets a
  plaza group (PODOL-1); host replies can now set text cells, for the group's
  name. Played from a level-4 save in Kuto's Well: the plaza menu (disguised
  and sneaking), a rumour overheard, the auction waited out (`0x4AB0` and
  `0x4A35` = 254), the robed orc's greeting and, three hours later, the
  loyalty test fought (`0x4A38` = 2), and a sneaking party's cancelled
  auction attacked by a fresh group.
- **Race byte:** `LOAD CHARACTER` now writes each member's Gold Box race
  at `0x72`, so humans no longer count against a disguised party: six humans
  are questioned 15% of the time (5% plus 5% for each of the two empty slots),
  not 45%.
- **Rewards and clearing (2026-10-23):** City Hall's clerk, at (5,5) past
  the guards at (4,5), pays 250 gp and 200 pp once for the auction mission
  and once for clearing the plaza, played from a new party in New Phlan with
  Podol's flag set. Clearing is played off the mission with the win count
  (`0x4A34`) set to eight: the ninth win, then one plaza step, marks the plaza
  cleared (`0x4AE7` = 254), and a camp there is not interrupted.
- **Enthralling Panache (2026-10-23):** in melee the buccaneer trades the
  first of its two Dagger attacks for the charm when its target is not
  already charmed by it and it can see the target: a Wisdom save against DC
  12, or the target is Charmed until the start of the buccaneer's next turn
  and cannot attack it. Damage does not end this charm (Charm Person's does).
  Fey Ancestry gives Advantage on the save, Countercharm a second try, and
  Mindless Rage, Aura of Devotion and Mind Blank make the target immune.
- **Group table, roll by roll (2026-10-24):** from the save made on entering,
  each of the nine group rolls is forced by reseeding the script's dice before
  one step on open plaza (row 9). Each must print its type's name, roll a size
  and leaders that fit its divisor and threshold, fight only its own creatures
  and set its type's morale (`podol_group_rolls` in
  [expedition_tests.cpp](../../tests/expedition_tests.cpp)).
- **Gaps:** none known.

## Suggested increments

1. Area row and travel: Podol Plaza loads (map, pieces, sprites, pictures,
   item lists) with MON1CHA 2-8 reusing the Slums conversions; Kuto's Well's
   west edge and Podol's east edge connect; the other three exits refuse
   clearly; the camp interruption rule accepts any interval and chance. Tests:
   enter from Kuto's Well, the plaza menu, a roaming fight of each kind, the
   rumours, the auction to its end (`0x4AB0` = 254) and City Hall's reward,
   the robed orc and the loyalty test with the holy symbols, nine wins
   clearing the plaza, camping.
2. Wizard-locked doors and the temple of Ilmater: reach (1,15), heal through
   its place record, rest uninterrupted there.
3. The Pitt: the head-and-body portrait at distance 0 for `SETUP MONSTER`
   with a missing sprite, the buccaneer's conversion, the one-on-one duel
   with its result codes (also Lizard Man Keep's), the patrons' attack, the
   bribe and the drinking.
4. The free-for-all and the cancelled-auction group, as decided; the combat
   result code check across all loaded areas.
5. Campaign-arc simulator stage and a play-test script through the area, as
   for Kuto's Well.
