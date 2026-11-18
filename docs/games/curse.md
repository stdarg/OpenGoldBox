# Curse of the Azure Bonds: how its files differ

*Curse of the Azure Bonds* (1989) runs on the next version of *Pool of
Radiance*'s engine. This page records what its files change, as the census
(`opengold_census`, [GAMES.md](../GAMES.md)) reads them. The facts come from the
reconstructed source of the DOS release 1.3 and from the files themselves;
the wording is our own.

Since 2026-11-12 every file of the game decodes.

## The files

| Files | What they hold | Change from *Pool of Radiance* |
|---|---|---|
| `GEO2`–`GEO6` | 16 maps | None |
| `ECL1`–`ECL6` | 25 scripts | A new memory bank and three new commands (below) |
| `MON1CHA`–`MON6CHA` | 81 creatures | A new record of 422 bytes (below) |
| `MON*ITM`, `ITEM*` | Creatures' items, item lists | None: 63 bytes an item |
| `MON*SPC` | Creatures' effects | None: 9 bytes an effect |
| `ITEMS` | Item types | None |
| `HEAD2`–`HEAD6`, `BODY2`–`BODY6`, `PIC*` | Each disk's portraits and close-ups for the scripts | Same format; a number can mean another picture on another disk, and none are for the player |
| `TITLE` | Title pictures | Four pictures instead of two |
| `BIGPIC1`, `2`, `6` | Four large pictures, such as the overland map | New file; ordinary pictures |
| `SKY` | The north sky, the sun and the horizon (records 250 to 252) | New file; ordinary pictures |
| `TILES` | The copy protection's symbols: sets of 26 and 22 symbols, 24 by 24 pixels | New file; the combat icons' format |
| `CHEAD`, `CBODY`, `CPIC*`, `COMSPR` | Combat icons | None |
| `DUNGCOM`, `WILDCOM`, `RANDCOM` | Battlefield tiles | None |
| `SPRIT*` | Monsters seen at a distance | None |
| `8X8D1` | The font (201), the symbols of set 4 (202) and the shared wall tiles (203) | Holds only those three |
| `8X8D2`–`8X8D6`, `WALLDEF2`–`WALLDEF6` | Each disk's wall tiles and walls | None: the same pairing of blocks and tiles |
| `START.EXE` | Item and spell names, and the sound effects | The sound driver moved (below) |

*Pool of Radiance*'s `ICON`, `BACPAC`, `SQRPACI` and `FINAL` files have no
counterpart.

The original shows a portrait from the current disk's `HEAD` and `BODY`
files. *Curse*'s characters are not given portraits: the record's portrait
fields come only from a *Pool of Radiance* character brought across.

## Scripts

- A script loads at **8000h** and may run to 9DFFh. *Pool of Radiance*'s load at
  9900h and run to B6FFh. Everything else about a record is the same: two
  header bytes, five entry jumps, then the body.
- Operands are encoded as in *Pool of Radiance*.
- **ECL CLOCK** (34h) takes two operands instead of one.
- Three commands are new:

| Code | Command | Operands |
|---|---|---|
| 3Eh | DUMP | none |
| 3Fh | FIND SPECIAL | 1 |
| 40h | DESTROY ITEMS | 1 |

DUMP sends the selected character (the last LOAD CHARACTER's) out of the
party with what it carries, as Alias and Dragonbait leave at the Pit of
Moander's way out; its slot stays empty. DESTROY ITEMS takes every item of a
type from the party. FIND SPECIAL is not run yet.

### Script damage

The tower's explosive runes select the reader with `DAMAGE A0h,6,4,6,80h`
and other healthy members with `90h,6,4,6,84h`; the next stair trap uses
`D0h,3,6,3,00h` for the whole party. These forms must complete rather than
roll back the event. In Curse's
[CMD_Damage](https://github.com/simeonpilgrim/coab/blob/9dc46f1d5911710fb2fcb7a9c0ec0ef74264d17c/engine/ovr003.cs#L1595),
the chosen-member form's low save category is one-based (zero means none).
Bit `10h` keeps full damage even after a save; the later engines' half-damage
interpretation does not apply. Core translates the operands into existing
rules hazards; the selected rules module resolves saving throws and damage.
Synthetic event tests exercise chosen and whole-party forms, successful and
failed original saves, no-save damage and unaffected nonselected members.
A read-only inventory of all installed Curse scripts finds 24 DAMAGE calls
using 13 constant operand forms; all pass the dispatcher's form checks after
this correction (`/tmp/opengold-curse-damage-inventory.txt`).

### Script memory

Scripts read and write the game through numbered cells. *Curse* moved
*Pool of Radiance*'s zones of cells whole, and each cell kept its place within
its zone (`game_address`):

| Zone | *Pool of Radiance* | *Curse* |
|---|---|---|
| The game's state (map, clock, flags) | 4900h–4CFFh | 4B00h–4EFFh |
| The current character, then the party's and the fight's cells | 6B00h–6EFFh | 7C00h–7FFFh |
| Strings | 9700h–98FFh | 7A00h–7BFFh |
| The script itself | 9900h–B6FFh | 8000h–9DFFh |

Every cell *Curse*'s source names (the map loaded, the clock, morale,
surprise, the party's size and the rest) is where this puts it. The cells
outside the zones, such as the party's position at C04Bh, did not move. The
current character's cells at the start of zone 1 are not the record's bytes:
the original serves each from a field of the record (SpecialReadZ1), at the
same offsets as *Pool of Radiance* (Constitution at +18h, race at +72h, thief
skills at +A5h, coins from +BBh, status at +100h, side at +10Ch), so they move
with the zone. *Curse*'s scripts also read Intelligence (+15h), class (+73h),
magic-user level (+C9h), gender (+D6h) and a script flag (+E4h); OpenGoldBox
gives them Intelligence and gender, and the class and level once the
characters carry the original's classes.

## Creatures

The record grew from 285 to 422 bytes: each ability now keeps a permanent and
a current score, a creature can know 100 spells and hold 84 memorized ones,
and a character remembers its levels before a change of class. Byte offsets:

| Offset | Field |
|---|---|
| 0 | Name: a length byte and up to 15 letters |
| 16–27 | Six abilities (Strength, Intelligence, Wisdom, Dexterity, Constitution, Charisma), each a permanent and a current score |
| 28, 29 | Exceptional Strength: current, permanent |
| 30–113 | Memorized spells, one a slot; bit 7 while still being memorized |
| 114 | Hours of rest before memorizing finishes |
| 115 | THAC0, as 60 minus the value |
| 116, 117 | Race, class |
| 118 | Age (two bytes) |
| 120 | Maximum hit points |
| 121–220 | Known spells |
| 221 | Attack level |
| 223–227 | Five saving throws |
| 228 | Movement |
| 229–233 | Highest level, level at the change of class, drained levels, drained hit points, undead level |
| 234–241 | Thief skills |
| 247 | NPC flag |
| 251–264 | Coins: copper, silver, electrum, gold, platinum, gems, jewelry (two bytes each) |
| 265–272 | Levels: cleric, druid, fighter, paladin, ranger, magic-user, thief, monk |
| 273–280 | The same levels before the change of class |
| 281–283 | Gender, type, alignment |
| 284, 285 | Attacks per two rounds, first and second attack |
| 286–291 | Their dice: two counts, two sides, two modifiers |
| 292 | Armor Class, as 60 minus the value |
| 295 | Experience (four bytes) |
| 300 | Hit points rolled |
| 301–315 | Spell slots: cleric, druid and magic-user, levels 1 to 5 |
| 316 | Experience for defeating it (two bytes), then 318 experience per hit point |
| 319–324 | Portrait head and body, combat icon head and body, order, icon size |
| 325–330 | Combat icon colors |
| 332 | Number of items |
| 389–392 | Hands in use, saving throw bonus, encumbrance (two bytes) |
| 405–408 | Status, in play, hostile, quick fight |
| 409–421 | Current THAC0, front and rear Armor Class, attacks, their dice, hit points and movement |

The other bytes hold pointers and the running game's bookkeeping, which the
files leave meaningless. `curse::decode_creature` reads the record into the
fields *Pool of Radiance*'s records fill (`por::CharacterRecord`), with the
current abilities, the levels before a change of class and the longer spell
lists besides.

## Modify Character

The party menu's Modify changes a new character's scores, Hit Points and
name while its experience is still what creation gives, 25,000 (12,500 or
8,333 for each class of a multi-class character), or none, as a character
brought from *Pool of Radiance* before its first fight has (`chargen.pas`,
`ModifyCharacterMenu`). The Hit Points reach each class's die and
Constitution at every level creation trained it to. OpenGoldBox's party
screen offers it so
([ORIGINAL-RULES.md](../ORIGINAL-RULES.md#modify-character)).

## Sound effects

The PC speaker's driver is linked into `START.EXE` as in *Pool of Radiance*, but
at a different place, so OpenGoldBox finds it by its first bytes (a tick divider
of 13 and a coded error message). Its addresses count from the start of the
code segment it is linked into, which its own instructions name: the voice
blocks lie 56h bytes into the driver, and the song table 332h bytes after them.
The table holds 21 songs of four voices; sound command *n* plays song *n* − 1.
The sequencer and its commands are *Pool of Radiance*'s, and so are the first
twelve sounds, tick for tick.

## The opening

A new game starts on disk 2: the original runs script 1 of `ECL2.DAX`, and
only its load entry. That entry loads map `GEO2:1` with the wall banks 1, 2
and 3, puts the party on square (7,13) facing east, says the party wakes in a
small room, shows picture 10 of `PIC2.DAX` (the arm marked with the azure
bonds) and redraws the view (CALL 2E10h). The script's other entries run when
the party first steps. Before it runs, the original's script setup marks a 3D
map as shown and no portrait up (cells 4BE6h and 7EE1h).

`load_campaign(folder, GoldBoxGame::curse_of_the_azure_bonds)` plays it with
the engine *Pool of Radiance* uses: the shared area loader reads the map, its
walls, sprites and pictures, and the session binds the script memory where
*Curse* keeps it. *Curse*'s areas are listed in `campaign_session.cpp`; only the
opening room so far.

## Areas and walls

A script's number is the same on every disk: scripts 1 to 4 are on disk 2,
16 to 21 on disk 3, 32 to 37 on disk 4, 48 to 53 on disk 5, 64 to 69 on disk 6
and 80 to 82 on disk 1. Map numbers are unique too. A script loads maps of its
own disk with LOAD FILES, and the view's three wall slots with LOAD PIECES,
whatever the map: the same map can show other walls for another script (map 1
takes banks 1, 2 and 3 for script 1, and 1, 2 and 4 for script 2).
`campaign_session.cpp` reads every script's LOAD FILES and LOAD PIECES, loads
each map it names and every wall set, and LOAD PIECES switches between them.

## Area names

The game shows no area's name; the exploration screen names each map as the
cluebook's maps and the scripts' own words do. Area 0, which has no map, is
the Overland. Two maps hold two places each, told apart by the script that
plays them: map 1 is Tilverton, or its Thieves' Guild for script 2, and map 32
Zhentil Keep, or the Shrine of Bane for script 33. Map 50 is the Village of
Haptooth, or the Cave of the Dracolich for script 50. Three maps serve several
towns: the wilderness scripts keep the town the party is at in cell 4C9Bh,
and the maps' scripts branch on it, as the screen does: map 21 is Voonlar's
ruins (town 5) or Phlan's, map 69 Teshwave's (town 7) or Hillsfar's, and map
53 the dungeon of Shadowdale (town 1), Ashabenford (2) or Essembra (8).

| Map | Name | Map | Name |
|---|---|---|---|
| 1 | Tilverton, Thieves' Guild | 33 | Cave of the Beholder |
| 3 | Tilverton Sewers | 37 | Oxam's Tower |
| 4 | Fire Knife Hideout | 50 | Village of Haptooth, Cave of the Dracolich |
| 16 | Yulash | 51 | Wizard's Tower |
| 17 | Pit of Moander | 53 | Shadowdale, Ashabenford or Essembra Dungeon |
| 21 | Voonlar or Phlan Ruins | 64 | Burial Glen |
| 32 | Zhentil Keep, Shrine of Bane | 66 | Ruins of Myth Drannor |
| | | 67 | Grand Ruined Temple |
| | | 69 | Teshwave or Hillsfar Ruins |

## The ending

When Tyranthraxus falls the script runs PROGRAM 8. *Curse*'s WinGame tells
its ending in six screens of four lines, Pascal strings in GAME.OVR (at
265C6h in the release OpenGoldBox reads, 272,137 bytes); OpenGoldBox reads
them from the player's copy and tells them, followed by "You have won.".
Another release shows only that line.

## The route

`opengold_curse_route` plays the story from the opening to the ending
([GAMES.md](../GAMES.md), step 3.7). Each fight is played up to 120 times
from the party as it stood (40 dice, three tactics each: every member
casting by Quick, the magic-user alone casting, a player's own choices) and
the best play kept, as a player reloads. A fight won with members dead
counts as won: the clerics raise them at the next rest or temple, and the
last fight wins the game whatever it costs, as the original counts a fight
won with one member standing.

### Stand-ins re-planned (2026-11-16)

On origin/main 4b5b95686 the route leaned on these stand-ins:

| Stand-in | Measured then |
|---|---|
| Test experience: 60,000 each six times and 400,000 twice at the guild's secret hall, to levels 10 and 11, with 60,000 gold | Kept, below |
| Test gear: weapons +2, armor and shields +1 from the start | Replaced |
| Heals by the test outside the game's commands before 11 beats (Hap's patrols, Crimdrac, Dracandros, the Yulash mounds, the Pit, Dexam, the caves' exit, the temple) | Replaced |
| Mogion and the pseudopods: the test set the Gauntlet's cells; the route never walked to Mogion's altar | Replaced |
| The cultists' last stand, won by the test | Its player won every play (53% with no one dead) |
| Dexam's lair: the test cleared cell 0x4C03 | Not needed (below) |
| The final battle, won by the test | Its player won no play of 120 (the best killed 10 of 28 margoyles) |

What changed:

- **Mogion and the pseudopods.** Mogion's altar is event 12 of ECL3.DAX 18.
  The assisted route used `(7,1)` in the western half of the shared map;
  the physically reachable lower altar is `(11,3)/(12,3)` in its eastern half.
  Played there, the party attacks
  Mogion, her cultists and mounds, and the three pseudopods that follow, and
  finds the Gauntlet in their remains. The route goes back up the stairs and
  declines the question to go down again, so the next camp can start.
- **The cell before Dexam.** The original's NEW ECL runs EclInit, which clears
  4C00h to 4C1Fh unless a saved game was just loaded (`eclmenu.pas`,
  `EclInit`). OpenGoldBox has done this since 2026-11-14
  (`cleared_by_new_script`), after the stand-in was set. The caves' new
  script clears the hooded woman's 0x4C03, and Dexam's lair meets the party.
  This was an old stand-in, not a difference from the original. The scripts
  use the 32 cells as each area's own flags.
- **Rests instead of heals.** Every map but the ruined temple lets a camp
  rest ("YOUR BANDS WILL NOT LET YOU REST" is ECL6.DAX 67's alone), so the
  route rests where a player would:
  - Hap's patrols come into its camps, which only adds patrols the party
    hunts anyway.
  - In Yulash a camp is refused in the Red Plumes' quarters ("HEY, NO
    SLEEPING HERE!", x up to 4 and y up to 5) and broken by wandering
    monsters in the streets. The barracks (event 25, at 5,2) are the spot
    they point out (ECL3.DAX 16's camp entries), and the party rests there
    before the mounds and before the Pit.
  - A camp in the Pit draws its wanderers (up to 50 fights a rest before the
    last stand, all won).
  - A camp in the Caves of the Beholder draws theirs night after night, so
    the party leaves first and rests at Zhentil Keep's edge.
  - In the ruins of Myth Drannor the parley keeps the rakshasas away.
  - In the temple the clerics cast their cures before the final battle.
- **The game's own gear.** The party starts with a long sword (a cleric's
  mace, a magic-user's dagger), plate mail and a shield, unenchanted. It
  takes what its fights leave and readies each in place of what it carries:
  - Dracandros: Bracers AC 2 and a Ring of Wizardry for the magic-user, a
    Ring of Protection +3, and Wands of Ice Storm and Fireballs.
  - The Yulash spies: broad swords +1, splint mail +2 and a mace +1.
  - The mounds: a Wand of Lightning besides the Wand of Defoliation, which
    the magic-user readies again before Mogion.
  - Mogion: plate armor +3, a shield +2, a mace +3 and a Cloak of
    Displacement.
  - Fzoul's men in the shrine: plate armor +1.
  - The temple's chapel: plate armor +2.

  The Hap dark elves' Drow weapons do not last: Curse's scripts take item
  types 94, 96 and 97 away (DESTROY ITEMS in ECL5.DAX 48 and 53).
- **The final battle.** Tyranthraxus (100 Hit Points, 16d6 lightning at a
  square and along 10 more in each of the first four rounds, `magic.pas`,
  `EffThrowLightning`), eight high priests and 28 margoyles. The party
  starts at the bottom of the field, rows 13 and 14, with the enemy packed
  in rows 7 to 12. The player's tactic follows the cluebook:
  - fireballs (the magic-user's four, then the wand) at the square whose
    burst takes the most margoyles and priests and no member;
  - a cleric's Prayer once;
  - Hold Person on the high priests, who are people.

  The margoyles shrug off Hold Person, which the old tactic wasted on them.
  13 plays of 120 win, all with a member or two dead, and the fight is
  reloaded until won.

### Win rates (2026-11-16)

Each figure is plays won of 120, in the full route on its own dice, with
the plays won with no one dead in brackets.

| Fight | Won (no one dead) |
|---|---|
| The king's guards, the Fire Knife in the guild, the Fire Knives' leader | 100% (92% to 100%) |
| The wilderness's ambushes and the black dragons | 100% |
| Hap's four patrols and the barn | 100% |
| The caves' guards at Hap's gate | 59% (54%) |
| Camps on the way to the tower (eight fights) | 64% to 100% |
| Crimdrac's tower, Dracandros | 100% (58%) |
| Crimdrac on the road back | 92% (61%) |
| The Zhentil spies | 100% (99%) |
| The shambling mounds over the dead cleric | 43% (8%) |
| Mogion, her cultists and mounds | 48% (0%) |
| The three pseudopods | 8% (0%), reloaded until won |
| The Pit's wanderers in camp (50 fights) | 100% |
| The cultists' last stand | 100% (18%) |
| Fzoul's men | 100% |
| Dexam's guard and Dexam | 99% and 100% (47%, 58%) |
| The temple's chapel | 100% (77%) |
| Tyranthraxus, the high priests and margoyles | 11% (0%), reloaded until won |

### The stand-in left: test experience

The party still trains to levels 10 and 11 at the guild's secret hall on
1,160,000 test experience and 60,000 gold, for a party as strong as one a
player brings from *Pool of Radiance* and trains on the way. The route's
own fights earn each member about 160,000 more, and the original has no
other training hall on the way before Voonlar's. These were measured:

- **No test experience.** The level-5 party wins Tilverton, the wilderness,
  Hap's patrols and the barn, then dies to the caves' guards at Hap's gate.
  Patrolling Shadow Gap's forest for experience (PATROL FOREST) kills five
  of six at level 5.
- **360,000 only (levels 9 and 10).** The caves' guards win 30% to 37% of
  plays, the mounds 50%, Mogion 15%, and the pseudopods none of 120.

Replacing this stand-in needs either a *Pool of Radiance* party brought
across at the end of that game's route, or the optional fights a player
takes on the way: the sewers' trolls, the hideout's rooms, and the
dungeons below Shadowdale, Ashabenford and Essembra, which the cluebook
says are there to add experience.


### Earned sewer progression (2026-11-18)

`opengold_curse_sewers` (`opengold_gold_box_games_tests --curse-sewers`)
checks one earned progression segment independently of the assisted full route.
The party uses its creation money (300 platinum per character) to buy ordinary
weapons, plate and shields at Tilverton's actual shop. It receives no extra
experience, money, items, healing, quest flags or combat victories.

From the sewer arrival at (0,0), movement uses actual turns and steps, including
locks, checkpoints and the original script's folded corridors. ECL2:3 connects
southward steps at (2,15), (5,15) and (9,15) to (9,0), (11,0) and (15,0);
the northward passages reverse those connections. They are script transitions,
not ordinary coordinate wrapping. The route refuses checkpoint surrender,
bargains with the otyughs offering a trade, burns the scattered troll pieces,
clears four lairs and rests in cleared rooms and the hidden guild chambers.
Paid training advances both clerics to level 6 on earned experience.

The observed run wins seven fights with no lost fight and retains all six
characters; each finishes with 30,912 experience. This is **not a campaign-win
certificate**. At the initial checkpoint `b54db7ba`, the Tilverton opening
still relocated between event squares and entered the sewers directly; that
opening assistance is removed by the subsequent work below. The full story
route retains its progression grants. The test separately asserts earned experience and cleric advancement,
rather than accepting only successful event decoding or a count of encounters.


### Physical opening and sewer progression (2026-11-18)

The earned route now starts in the inn at (7,13), walks out to hear the
innkeeper's explanation, and buys equipment by stepping west from (3,12)
into the town shop. The shop event also appears in disconnected geometry;
the assisted story's (12,7) was not reachable from the inn. The sage is visited
by stepping east from (5,5). Both services leave the party outside their doors,
so navigation must visit the doorway, rather than insist on occupying a shop's
event square.

The party approaches the royal guards, is sent back, witnesses the carriage
attack, surrenders and follows the jail escape and guide into the guild.
Navigation recognizes script-driven transfers. Guild corridors and fights
lead to the southern exit at (10,15), whose script brings the party into the
sewers at (0,0). It then clears the earned progression segment above. The
route rejects any test relocation or direct area entry, as well as injected
rewards, healing, flags or victories.

Observed fresh run: eight fights won, zero losses, all six characters alive,
31,876 XP each and both clerics at level 6. Legal original Modify controls and
combat retries remain allowed. This proves the opening and optional sewer
progression, **not the remainder of the campaign**. The existing full story
route still uses injected progression and relocation.


### Earned first bond (2026-11-18)

`opengold_curse_first_bond` (`opengold_gold_box_games_tests --curse-first-bond`)
extends the fresh physical opening through the first bond. After earned sewer
training, the party walks to (13,15), steps south and follows ECL2:3 into the
Fire Knives' hideout at (11,0). It walks the hideout to its leader at (3,13),
wins the encounter and reaches the wilderness's Journey menu.

A fresh run wins twelve fights, loses none and keeps all six player characters
alive, each at 35,912 XP; the magic-user ends unconscious. The test reads the
first-bond flag `4C2A = 1` after the original event sets it and requires the
wilderness area. No rewards, healing, positions, flags or combat results are
injected. The verified twelve-beat opening save replays the hideout
successfully. The first-bond endpoint stops at Journey, where the route
harness does not export a checkpoint (`can_leave` is false). An earlier claim
that a fourteen-beat endpoint file existed was incorrect; that file was not
written. A resumed diagnostic's four suffix
fights alone do not satisfy the fresh route's minimum fight count.

This proves one bond from legal new characters, not the remaining four or the
campaign ending. The assisted full story route remains a separate regression.


### Earned journeys and Hap (2026-11-18)

`opengold_curse_hap` (`opengold_gold_box_games_tests --curse-hap`) continues
the same earned party overland via Shadowdale, Ashabenford, the Standing Stone
and Essembra to Hap. Real camps replenish spells; clerics spend actual cure
spells after resting. Hap's real arrival is (3,0); its barn is at (3,13),
not the disconnected event copy at (8,4) used by the assisted route.

The party camps and fights until at least four patrols have fallen, collects
and readies their Drow Long Swords and Maces, then walks to and defeats the
barn's efreeti and dark elves. Loot selections apply to camp encounters too.
The test requires the patrol counter `4C47 >= 4`, barn-clear flag `4C5E = 1`,
six living player characters and real combat victories. It supplies no rewards,
healing, positions, flags or outcomes. NPCs do not replace a missing PC in the
cohort check.

The suffix from the verified Hap arrival wins six fights without a loss;
all six PCs finish alive at 53,073 XP. Its actual twenty-nine-beat save was
verified at `/tmp/opengold-curse-earned-hap-barn.sav`. The full fresh registered route wins twenty-one fights with no loss, retains
all six PCs at full HP and the same XP, and passes its neighboring regressions
(2026-11-18, working tree based on `0cd46cbd`).
This does not certify the caves, Dracandros or the campaign ending.


### Earned second bond and recovery (2026-11-18)

`opengold_curse_second_bond` (`opengold_gold_box_games_tests --curse-second-bond`)
extends the earned opening through paid Shadowdale advancement, the caves,
Crimdrac, the roof's dragon parley, four actual downward stairs and Dracandros.
Training occurs after four Hap patrols and before the barn: leaving Hap destroys
Drow weapons, so the final patrol and barn replace them after the party returns.
Permanent troll-lair bracers protect the mage. No rewards, movement, quest flags,
healing or battle outcomes are injected.

The tower stairs are (3,1) north, (5,4) west, (14,1) north and (9,12) west.
Their original events, including the burly guard, illusion, explosive runes and
spike trap, complete normally. The two DAMAGE forms documented above formerly
rolled these events back. The party prepares at (4,12), outside the courtyard:
actual Resist Fire, Enlarge, Bless, Prayer and Haste spells. Interrupted camps
replenish the planned spells before proceeding. The boss is entered by one
southward step, avoiding an unnecessary second look after victory.

Dracandros can be beaten at level 6 with casualties. The route reloads actual
combat attempts, takes his real treasure, rests, uses the secret passage at
(7,15) east and returns to Hap. The general store is reached from (2,5) west.
The mage pays 200 gp to identify the older bracers and sells them for 6,750 gp,
keeping Dracandros's stronger pair. This pays the Temple of Sune's 5,500 gp
Raise Dead fee for each of the three casualties. The temple is entered from
(3,8) west. Recovery uses the same ID/sale and temple services as the game UI.

Acceptance requires six living PCs, earned XP and paid advancement, actual
Dracandros victory and Helm flags (`4C60 = 1`, `4C59 = 1`), and zero battle
defeats. Fights with casualties are counted separately; death followed by a
paid resurrection does not turn a won battle into a defeat. The verified
suffix ends with all six alive in Hap and exports the sixty-four-beat format-53
checkpoint `/tmp/opengold-curse-second-bond-all-six.sav`. The fresh registered
milestone passes in 264.43s: 47 real victories, no defeats, one fight with three
casualties, and all six PCs alive after recovery. The two paladins and mage end
at 120,502 XP; the ranger and clerics, who died in the boss fight, have 106,648.
The game is rebuilt and all 341 registered checks pass across the focused
24 and remaining 317 checks, including 171 Godot checks, with zero skips. This is the second bond, not the campaign ending.

### Camp healing and shared-map saves (2026-11-18)

A journey menu can retain a selected character record across a camp cure.
The live party healed, but the next LOAD CHARACTER read back the older HP,
undoing the consumed spell. `cast_outside_combat` now refreshes that record's
HP through the active paused request. The machine validates every host update
before writing, rejects stale tickets and program addresses, and retains the
same request and control flow. A synthetic paused-menu regression casts the
actual original-rules cure and checks both retained healing and consumption.

The lower Pit's script 18 uses NEW ECL without LOAD FILES, retaining script
17's map and walls. It now appears among map 17's shared scripts. This repairs
ordinary saves which were written successfully but rejected on reload as a
district/script mismatch. The installed-data regression enters the lower
floor, saves and reloads the actual party, position and script. Existing
format-53 earned lower-floor saves also reload; the save format is unchanged.

### Earned Moander route (2026-11-18)

`opengold_curse_third_bond` extends the earned route through Crimdrac's revenge,
paid advancement, Yulash, the Wand of Defoliation and Alias/Dragonbait's actual
recruitment. Crimdrac resists cold; Magic Missile and the real Fireball wand
won from Dracandros work. The route prefers Yulash's open streets over locked
house shortcuts. It enters the Pit by stepping north from `(11,0)` and takes
the reachable upper stair `(7,14)` to the lower floor.

Camp in the side room `(13,6)`, where interruptions are occasional, rather
than the altar corridor, where they are guaranteed. Reload interrupted
preparation camps; retained battles still require real victories. At `(12,4)`
north, cast Bless, Enlarge, Prayer and Haste without intervening Look actions.
Look costs ten minutes; Search adds ten minutes to even the final six-second
step, so turn Search off before entering the altar. The actual fight snapshot
then retains Bless, Prayer and Haste instead of silently expiring them.
Fireball and holds interrupt Mogion's human casters; the Defoliation wand
attacks plants.

The fresh registered route wins both chained fights and receives the actual
Gauntlet (`4C5B = 1`, `4C2D = 1`) in 364.64s: 56 retained victories, zero
battle defeats and two fights with casualties. All six PCs remain in the
party. The mage (6/41 HP) and Dragonbait (50/50) stand; paladin 2 and the
ranger are dead, while paladin 1, both clerics and Alias are unconscious.
A second fresh replay on the final test player reproduces that endpoint and
saves `/tmp/opengold-curse-third-bond-earned.sav` at beat 101. Recovery and the
remaining campaign are still open. All 342 registered checks have passing
coverage across the broad run and final affected replays; no additional whole
campaign is certified here.
See [the delivery record](../PLAYABILITY.md) for current checks and limitations.
