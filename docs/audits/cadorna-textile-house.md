# Cadorna Textile House audit (2026-10-24)

What playing the Cadorna Textile House needs, from the user's DOS installation
and the current engine. Sources: `opengold_scripts --inspect GAME ECL4.DAX 2`
(and City Hall's `ECL3.DAX 8`), `opengold_creatures GAME`, `opengold_maps
GAME`, the item lists `ITEM4.DAX` 38 and 39 decoded with `START.EXE`'s item
words, the cluebook's Cadorna Textile House page (p15-16, reference only), and
SRD 5.2.1.

## The area

- **Map:** `GEO4.DAX:2`, a 16 x 16 block south of Podol Plaza. One script,
  `ECL4.DAX:2`, drives it: `LOAD FILES 2,2,255` (`0x9920`) and wall banks
  `LOAD PIECES 1,3,20` (`0x9927`). 104 of the 252 walkable squares are
  interior. The map's event bytes (low seven bits) lay it out:
  - **0** (169 squares): the ruined streets and yards around the house, where
    roaming creatures meet the party.
  - **22** (54): the textile house's halls (rows 8-15, columns 2-13). They
    roll for wandering hobgoblins while the ogre chief lives.
  - **1** (4,10), (8,10), (8,13): the weedy courtyard, where a drum beats and
    hobgoblins attack.
  - **2** (5,9), (7,9): the barracks; **5** (2,8): the tapestry room; **8**
    (2,14), (3,14): the graffiti room. Each holds a hobgoblin garrison.
  - **17** (2,15): the small dark room where Skullcrusher lies chained.
  - **18** (3,11), (2,12): Grishnak's makeshift temple; **19** (2,13): the
    blood-smeared room.
  - **20** (11,14): where Skullcrusher points out the secret door; **21**
    (12,14): the ogre chief's hall.
  - Text only: **3** (5,14), (7,14) stench; **4** (3,9) a deteriorated
    building; **6** (5,15) baby hobgoblins flee; **7** (6,14) "Skullcrusher
    was here" carved in the wall; **12** (11,9) a dark object to the west;
    **13** (9,9) a dead hobgoblin (search, destroy or leave it); **14**
    (10,11), (9,12) a kitchen; **15** (10,12) a smoking fire; **16** (11,11)
    bread, meat and worse.
  - **23** (seven squares): a diary page, journal entry 11, found once.
  - **24** (1,3): the dry well marked "Thieves only".
  - 16 door faces have code 2 (locked); none is wizard-locked.
- **Entries:**
  - Podol Plaza (`ECL1:18`): its south edge (`0x9A00`, bank 4, `NEW ECL 2`).
  - Mendor's Library (`ECL2:15`) and the wilderness map (`ECL7:26`) lead
    here too; neither is in the game yet.
- **Exits** (the step entry `0x995B`, by facing at `0x996D`):
  - north: bank 1, `NEW ECL 18`, Podol Plaza (`0x997F`), already loaded;
  - east: bank 2, `NEW ECL 15`, Mendor's Library (`0x9989`);
  - south: nothing (`0x9A07`): the river;
  - west: bank 7, overland position (11,28), `NEW ECL 26`, the wilderness
    (`0x9993`).
- **Script entries** (the five jumps at `0x9900`): `0x995B` before each step
  (exits), `0x99A9` on arrival at a square (the event dispatch, `ON GOTO` on
  the event byte at `0x99B6`), `0x993D` before camping, `0x9957` when a camp
  is interrupted, `0x9914` on entering the area.

### Cadorna's commission

City Hall (`ECL3:8`) announces it (`0xAA52`): "Junior Councilman Cadorna has a
special commission for you", when the party has not been given it
(`0x4A97` ≠ 255), a later council flag is below 254 (`0x4ABE`, set by
another mission) and Cadorna's payment is not settled (`0x4AB8` ≠ 255); it sets `0x4A97` = 254. In
his chambers (`0xAF7D`) Cadorna asks the party to bring back a family treasure
hidden in the western building of the textile complex, which a servant sent
for it never returned with; `0x4A97` = 255.

- **The box** (`0x4AC8`): 0 not found, 1 sealed (taken unopened, or resealed
  by the thieves), 128 opened, 255 settled.
- **Opened:** on the party's next entry into City Hall (`0x9A72`) the payment
  and the box are settled (`0x4AB8` = `0x4AC8` = 255), so his commission is
  never offered again, and, unless `0x4ABE` ≥ 254, Cadorna confronts the
  party: "You have taken my family treasure. I don't forget such things."
- **Sealed:** the clerk's reward list has an entry for it, slot 18 at
  `0xA389`, "Councilman Cadorna left this payment for some service you had
  rendered", worth 20 gems by the clerk's tables. But no script in `ECL1`-`ECL8` sets
  `0x4AB8` to 254, so the original never pays it, though the cluebook says the
  clerk rewards an unopened box. Question 8.

### Events

- **Roaming creatures** (events 0 and 22, `0x9A08`): each step rolls 0-30
  (0-5 in search mode, `0x6DCA` = 1) and a 1 meets something.
  - In the halls (22) while the chief lives: 8 hobgoblins and a leader.
  - Elsewhere, unless the block is cleared (`0x4AE6` ≥ 254): a roll of 0-10
    plus the count of roaming fights so far (`0x4AE4`) picks the group: above
    17 hobgoblins, 13-17 undead, 8-12 ghouls, 4-7 scorpions, else frogs, so
    the creatures grow tougher with each fight. After nine fights no more
    come until the chief is dead; with him dead, the next meeting after the
    ninth fight clears the block (`0x4AE6` = 254) instead. City Hall then pays the clearing
    reward (`0x9E21`, slot 10, as for Podol Plaza).
- **The courtyard** (event 1, `0x9C27`, while `0x4A74` = 0): a drum beats and
  12 hobgoblins and 2 leaders attack. Won, the drum falls silent (`0x4A74` =
  1). Fled, the party is chased into the streets at (4,2) or (12,2).
- **The garrisons** (events 2, 5 and 8): the encounter menu at `0xA9D1`. Any
  choice but fleeing brings a fight (a parley gets a battle cry). Fleeing, or
  fleeing the fight, escapes to one of (4,11), (8,8), (4,14) or (12,13). A won
  fight empties the room for good (`0x4AE1`, `0x4A02` or `0x4AE3`).
- **Skullcrusher** (event 17, `0xA109`; `0x4A73`: 0 chained, 128 freed, 255
  settled). A muscular man lies chained, and a page beside him becomes
  journal entry 47 (head 32, body 3 in the portrait). Choices:
  - **Free:** unlock (needs the Brass Key, item type 75, from Grishnak's
    treasure), pick the lock ("No one is able to pick all the padlocks") or
    break the chains ("Despite great effort, the chains hold"). Only the key
    frees him (`0x4A73` = 128).
  - **Attack:** he dies in his chains (`0x4A73` = 255).
  - **Parley:** five answers, none of which frees him.
  - **Leave.**
  - Freed: attack (he fights, MON4CHA 27, morale 100), wait, flee, parley or
    leave. Any parley but an abusive one: "Thanks for freeing me. I'm Lord
    Cadorna's servant." Add him? Yes, with room in the party (fewer than
    eight), he names himself Skullcrusher, tells of a secret door to the
    southeast, and joins: `ADD NPC 27,100`. An abusive answer brings a fight.
  - With him in the party (found by scanning `LOAD CHARACTER` slots 0-8 for
    the name "SKULLCRUSHER"), the carving (event 7) and the dead hobgoblin
    (event 13) gain a line, and at (11,14) he points out the secret door.
- **Grishnak** (event 18, `0xA60C`, once the drum is silent, `0x4A74` = 1):
  an ancient hobgoblin priestess at a bone altar attacks with 3 leaders and 12
  hobgoblins (morale 100). `TREASURE`: 100 cp, 20 sp, 30 ep, 40 gp, 8 pp and
  list 38: two Magic User Scrolls of 3 spells (Blink, Burning Hands, Mirror
  Image; Fireball, Detect Invisibility, Reduce), Scale Armor, a Stone
  Statuette, a Quarter Staff, a Potion of Healing, the Brass Key and a Cursed
  Necklace. Won, `0x4A74` = 2.
- **The ogre chief** (event 21, `0xA7D0`, while `0x4A75` = 0): the party
  turns to face south; a ten-foot ogre and his troops: 1 ogre, 18 hobgoblins
  and 4 leaders (morale 100). Won, `0x4A75` = 255, and the party finds an iron
  box sealed with the Cadorna crest:
  - **Open it:** `0x4AC8` = 128, `0x4A17` = 1, and `TREASURE` 450 cp, 20 sp,
    400 pp, 8 gems, 4 jewelry and list 39: Gauntlets of Ogre Power, a Gold
    Banded Wand, a Jewel Studded Bowl, a Jewelled Dragon Statuette and a Ring.
  - **Take it unopened:** `0x4AC8` = 1.
- **The dry well** (event 24, `0xABD3`): "Thieves only". Leave, or climb down:
  `WHO` picks a living climber, and d100 below the character's climb-walls
  chance (byte `0xAB`) reaches the bottom. Otherwise the climber falls:
  - If the table at `0x4900`-`0x493F` holds 27 beside the climber's slot
    (meaning unverified; a falling-magic effect, by the outcome), "You float
    down the well", and arrows from the archway (`DAMAGE` 8 arrows of 1d6).
  - Otherwise "You hit an unyielding stone floor", `DAMAGE` 15d6 to the
    climber (form `0xA0`). A dead climber's body floats back up; a survivor
    meets a laughing thief, whose ring lifts the climber out.
  - A climber who reaches the bottom meets **Restal**, the old thief (head 52,
    body 8):
    - Without Cadorna's commission: "Perhaps we shall meet again", and his
      ring lifts the climber out.
    - On the commission, before the box is found: he offers to lead the party
      in unseen. Yes, and yes to a blindfold: the party is led to (6,11) facing
      north, inside the house past the courtyard (`0x4A74` = 1).
    - With the sealed box, before any deal (`0x4A17` ≠ 1): he offers to open
      it and copy the seal for half the treasure. Yes: `TREASURE` 200 pp, 4
      gems, 2 jewelry and list 39, the box sealed again (`0x4AC8` = 1).
- **Camping** (`0x993D`): every 12 five-minute steps, a 6% chance of a roaming
  fight (`0x9957`, the same roll as a step). None once the block is cleared.
- **Quest flags other areas read:** `0x4A97` (the commission), `0x4AC8` (the
  box), `0x4AB8` (Cadorna's payment), `0x4AE6` (the block cleared).
- **Art:** `WALLDEF4`/`8X8D4` (banks 1, 3, 20), `SPRIT4` (6, 14, 18, 34, 37,
  72), `CPIC4` (2, 6, 7, 8, 11, 18, 20, 34, 35, 37, 72), `HEAD4` (32
  Skullcrusher, 52 Restal), `BODY4` (3, 8), `MON4CHA`, `ITEM4` are all present.

## Fights

| Fight | `LOAD MONSTER` (MON4CHA record × count) |
| --- | --- |
| Frogs (roaming, `0x9AB1`) | Poisonous Frog (38) × 8 |
| Scorpions (`0x9BE2`) | Large Scorpion (18) × 3, Huge Scorpion (39) × 1 |
| Ghouls (`0x9B91`) | Wight (20) × 1, Ghoul (72) × 5 |
| Undead (`0x9B3C`) | Wight (20) × 1, Skeleton (34) × 10, Zombie (35) × 8 |
| Hobgoblins (`0x9AF7`; the halls) | Hobgoblin (6) × 8, Hobgoblin Leader (7) × 1 |
| The courtyard (`0x9C2F`) | Hobgoblin (6) × 12, Hobgoblin Leader (7) × 2 |
| Barracks / tapestries / graffiti | Hobgoblin (6) × 14 / 10 / 12, Leader (7) × 3 / 2 / 2 |
| Grishnak (`0xA6F0`) | Grishnak (28) × 1, Leader (7) × 3, Hobgoblin (6) × 12; `TREASURE` list 38 |
| The ogre chief (`0xA8EA`) | Ogre (8) × 1, Hobgoblin (6) × 18, Leader (7) × 4 |
| Skullcrusher (`0xA45E`) | Skullcrusher (27) × 1 |

As in the earlier areas, every fight is trimmed to the party's XP budget and to
one creature per living character (`encounter_challenge`), each group keeping
at least one.

## Monster conversions

MON4CHA 6-8 match MON2CHA 6-8 (the Slums' hobgoblins and ogre).

| Record | Original | Conversion |
| --- | --- | --- |
| MON4CHA 6 | Hobgoblin (HP 6, AC 5, 1d8, long sword) | `slums-hobgoblin` (SRD 5.2.1 Hobgoblin Warrior), 100 XP |
| MON4CHA 7 | Hobgoblin Leader (HP 6, AC 5, 1d8, long sword, long bow) | `slums-hobgoblin`, fit 100, award 200 |
| MON4CHA 8 | Ogre (HP 21, AC 5, 1d10) | `ogre` (SRD 5.2.1), 450 XP |
| MON4CHA 34 | Skeleton (HP 5, AC 7, 1d6) | **new:** SRD 5.2.1 Skeleton (CR 1/4, 50 XP) |
| MON4CHA 35 | Zombie (HP 10, AC 8, 1d8) | `zombie` (SRD 5.2.1, Animate Dead's), 50 XP |
| MON4CHA 72 | Ghoul (HP 10, AC 6, two claws 1d3, bite 1d6) | `ghoul` (SRD 5.2.1, Create Undead's; its paralysing Claw is left out), 200 XP |
| MON4CHA 38 | Poisonous Frog (HP 4, AC 8, a 1-point bite) | **new; question 2:** SRD 5.2.1 Giant Frog (CR 1/4, 50 XP) |
| MON4CHA 39 | Huge Scorpion (HP 20, AC 4, two claws 1d8, sting 1d3) | **new; question 3:** SRD 5.2.1 Giant Scorpion (CR 3, 700 XP) |
| MON4CHA 20 | Wight (HP 23, AC 5, 1d4) | **new; question 1:** SRD 5.2.1 Wight (CR 3, 700 XP, 82 HP) |
| MON4CHA 18 | Large Scorpion (HP 10, AC 5, two claws 1d4, sting) | **new; question 4** |
| MON4CHA 28 | Grishnak (HP 24, AC 5, 1d4; INT 16, WIS 16) | **new; question 5** |
| MON4CHA 27 | Skullcrusher (HP 39, AC 10, STR 18/81, fists 2d1+2) | **new; questions 6 and 7** |

## Engine support

Supported today: every command the script uses runs (`ENCOUNTER MENU`,
`PARLAY`, `HORIZONTAL MENU`, `WHO`, `FIND ITEM`, `TREASURE` with an `ITEMn`
list, `SETUP MONSTER` and head-and-body portraits, `CALL 0xC01E` and `CALL
0x2C90`, `DAMAGE` arrow volleys, combat results 128 and 129, morale, camp
interruptions at any interval and chance, Bash and Pick at locked doors). City
Hall's reward list and its clearing reward run as for Podol Plaza.

Missing:

1. **Area loading.** No row for the house in the area table
   (`rolf_tour.cpp`, `AreaSource`): script 2, bank 4, map 2 with pieces {1, 3,
   20}, and conversion rows for the MON4CHA records. Podol Plaza's south exit
   is refused today ("The way to Cadorna Textile House is not in the game
   yet").
2. **Exits to unloaded areas.** East (Mendor's Library) and west (the
   wilderness) need the same refusal; south leads nowhere.
3. **The fall** (`DAMAGE 160,15,6,0,128`): form `0xA0` throws "Unsupported
   DAMAGE form". It needs 15d6 to the chosen climber (SRD falling: 1d6 per 10
   feet, so a 150-foot fall).
4. **The climber's chance** (character byte `0xAB`): not written, so every
   climber falls. Question 8.
5. **Scanning slot 8.** The Skullcrusher check loads slots 0-8; slot 8 throws
   "Invalid ECL party position" when no monster is staged. It should read as
   empty.
6. **Skullcrusher joining** (`ADD NPC 27,100`): recruiting is wired only for
   `MON3CHA` records with an authored profile, and no profile exists for him.
   Question 7.
7. **The falling-magic table at `0x4900`.** It reads the script's own
   variables there, which hold no such effects in this engine; the float-down
   branch would run only if a stored value happened to be 27. To verify before
   relying on it.
8. **Cadorna's payment.** Question 9.
9. **Roaming fights.** Every roaming group in the streets holds a creature
   that waits on questions 1-4; until then a step that meets one fails and is
   rolled back, so the way in from Podol Plaza stays closed.

## Party level and balance

The arc to here is the Slums (levels 1-3), Kuto's Well (2-4) and Podol Plaza
(3-5); the house follows at about level 4-6. At the default encounter
challenge a level-5 party of six has a budget of about 1,250 XP: the garrisons
shrink to six hobgoblins, Grishnak's band to her, a leader and four
hobgoblins, and the chief's to him, a leader and four hobgoblins. The undead
group (a Wight, a Skeleton and a Zombie at least) and a lone Giant Scorpion are
the hardest roaming fights. The campaign-arc simulator should get a Cadorna
stage after Podol Plaza.

## Decisions needed

1. **The Wight (MON4CHA 20).** The SRD 5.2.1 Wight is CR 3 (700 XP) with 82
   Hit Points, against the original's 23: two attacks with its Necrotic Sword
   (1d8 + 2 Slashing and 1d8 Necrotic) or Necrotic Bow, one of which may be
   Life Drain. Recommend Life Drain as written (Constitution save DC 13, 1d8 +
   2 Necrotic, and the Hit Point maximum drops by the damage until a long
   rest), leaving out a slain humanoid rising as a Zombie and, as for kobolds,
   Sunlight Sensitivity.
2. **The Giant Frog's grab (MON4CHA 38).** Recommend its Bite (+3, 1d6 + 2
   Piercing) without the Grapple and Swallow, as the Bugbear's Grab is a plain
   attack.
3. **The Giant Scorpion's attacks (MON4CHA 39).** The engine's Multiattack
   repeats one attack. Recommend teaching it the real mix, two Claws (+5, 1d6 +
   3 Bludgeoning) and a Sting (+5, 1d8 + 3 Piercing and 2d10 Poison), leaving
   out the Claws' grapple.
4. **The Large Scorpion (MON4CHA 18).** The SRD has only the Tiny Scorpion
   (CR 0) and the Giant Scorpion. Recommend the SRD 5.2.1 Giant Centipede (CR
   1/4, 50 XP, 9 Hit Points, a Bite that leaves the target Poisoned until its
   next turn), nearest the original's 10 Hit Points.
5. **Grishnak (MON4CHA 28).** Recommend the SRD 5.2.1 Priest (CR 2, 450 XP,
   38 Hit Points): two attacks with her Mace (1d6 + 3 Bludgeoning and 2d4
   Radiant) or Radiant Flame (2d10 Radiant, 60 feet), without her spells
   (Spirit Guardians and Divine Aid), which the engine's monsters do not cast.
6. **Skullcrusher as a foe (MON4CHA 27).** Recommend the SRD 5.2.1 Tough (CR
   1/2, 100 XP, 32 Hit Points, nearest his 39); the Warrior Veteran (CR 3) is
   far stronger.
7. **Skullcrusher as a companion.** Recommend a level-4 SRD Fighter built from
   his record's scores (Strength 18, Dexterity 16, Constitution 18,
   Intelligence 10, Wisdom 8, Charisma 12), unarmored and unarmed as found,
   whom the player controls like any member.
8. **Climbing the well.** Recommend: a Rogue wearing no armor or Light armor
   makes a Strength (Athletics) check against DC 15 to climb down (the SRD's
   climbing check); anyone else falls, as in the original. The fall is the
   original's 15d6 Bludgeoning, also the SRD's damage for 150 feet.
9. **Cadorna's payment.** Recommend the clerk pays the 20 gems of slot 18 when
   the party brings back the box sealed, as the cluebook says, though the
   original's scripts never set it.

## Status

- **Increment 1 (done, 2026-10-24):** the area row (`rolf_tour.cpp`): script 2,
  bank 4, map 2 with wall banks 1, 3, 20, and every MON4CHA record the script
  loads converting by CADORNA-1. Podol Plaza's south edge leads in, and the
  house's north edge back; east (Mendor's Library) and west (the wilderness)
  are refused with a notice. The fall is `DAMAGE` to the chosen member, the
  climbing chance is the Rogue's Strength (Athletics) check written to byte
  `0xAB`, the script's cells from `0x4900` are bound (the falling-charm search
  finds nothing), slot 8 reads as empty, names reach scripts in capitals, and
  `ADD NPC` builds Skullcrusher (`companions.cpp`) from the area's own MON4CHA.
  City Hall pays Cadorna's 20 gems for the box brought back sealed. Played
  (`opengold_expedition_tests`, after Podol Plaza at level 4): a save in the
  house, the refused exits, a fighter's fall down the well, the courtyard,
  Grishnak and her Brass Key, the ogre chief and the box taken sealed,
  Skullcrusher unlocked and joining as a level-4 Fighter, and the way back.
  A save just after Skullcrusher joins is reloaded (`reload_cadorna_chain`,
  issue #4): the drum, Grishnak, the chief, the sealed box and Skullcrusher
  are all still done, and walking back through their rooms fights nobody,
  adds no coins or gems and recruits no second Skullcrusher. The route counts nine roaming fights as done, so it meets none of the
  streets' frogs, scorpions or undead; their stat blocks are checked in
  `opengold_slums_creatures_tests`.
- **Fixed on the way:** a fight reported an item it had used up again after
  every action, and the campaign then failed on the missing item; a potion
  drunk in combat (here Grishnak's) or the last Basic Poison would trigger it.
