# Default parties

The startup menu's **Play with Default Party** starts a new game with a
ready-made party of six, so the player can begin adventuring at once, as each
original game's pre-rolled party allowed. OpenGoldBox ships its own parties in
`data/default_parties/`, one file per game folder (`POOLRAD.json`,
`CURSE.json`, `SECRET.json`, `GATEWAY.json`, `DARKNESS.json`,
`TREASURE.json`, `UA.json`). They are OpenGoldBox's own characters: none of
their names, scores or items comes from the games' shipped saves, and the
player's `SAVE` folder is never read.

Each file holds six people. Each person has one name, gender, portrait and
combat icon, and a character under each rule set: the `original` rules and
the `srd` rules (SRD 5.2.1). The SRD character is the same person as the
nearest SRD species and class. *Gateway*, *Pools of Darkness* and *Treasures*
have their original-rules characters only: their SRD rules wait (GAMES.md,
phase 9), so their files have no `srd` part, and under the SRD rules the
button says the party has no SRD characters yet. To change a party, edit its
file; the game reads it when the button is pressed, and
`opengold_default_party_tests` builds every party under each rule set it has.

## How a party is made

Each member is made by the game's own creation rules for the chosen rule set
(`opengold::add_default_party`), as a player's new character is:

- **Original rules:** race, class, gender and alignment are chosen as on the
  creation pages, and the character is rolled by the game's own roll (seeded
  by its place in the party, so it is the same every time): its Hit Points,
  age, exceptional Strength and gold. The file's six scores then take the
  place of the rolled scores (`scores`, Strength first, after the race's
  adjustment). The starting age and the race's and class's limits still
  apply, so a shown score can differ by a point. Each game's starting
  experience, training, gold and platinum apply: level 1 in *Pool of
  Radiance*, 25,000 experience in *Curse*, 200,000 in *Secret*, 3,000 in
  *Gateway*, 1,500,001 in *Pools of Darkness* and 22,600 in *Treasures*, each
  trained at once as far as it reaches. Items are numbered by the game's own
  item types (*Pool of Radiance*, *Curse* and *Gateway* share theirs;
  *Secret* and *Treasures* share theirs), with the name each shows. *Pools of
  Darkness* gives each new character its equipment by its class
  (`outfit_original_character`), so its file lists none.
- **Creation's spell choices:** a *Treasures* magic-user chooses its first-
  to third-level spells on a page after the roll, slots x 4 / 3 of each level
  (ORIGINAL-RULES.md). Its `learned` list holds the choices; each level gets
  exactly as many as the page asks for, or the member is refused. It knows
  Read Magic besides.
- **Standing:** in *Pools of Darkness* and *Treasures* every character stands
  on one square in a fight, the dwarves and halflings too (their
  `chargen.pas`); their icons are drawn short as in every game.
- **SRD:** made as a hand-made companion is (`build_companion`, as in
  `data/companions`): level 1, the `rolled` scores plus the background's +2
  and +1, the class's and background's training choices by default, the
  listed cantrips and spells (a choice the list leaves short takes its first
  other options), and 3d6 x 10 gold pieces. Items are SRD equipment keys.
- **Items** are given, not bought: the creation money is kept. Items marked
  `"readied": true` are worn or held; the rest are packed.
- **Spells:** under the original rules the casters memorize the listed spells
  at a Long Rest the party takes before the game begins, as a new party does
  at its first camp (a spell listed twice is memorized twice). Under the SRD
  the cleric, wizard, paladin and ranger prepare theirs at creation.

## Pool of Radiance (`POOLRAD.json`)

| Name | Original rules | SRD 5.2.1 |
|---|---|---|
| Brannoc Stoutmere (male) | Dwarf Fighter, Lawful Good; Str 18 Dex 13 Con 17 Int 9 Wis 11 Cha 8; Long Sword, Chain Mail, Shield | Dwarf Fighter, Lawful Good, Soldier; rolled Str 15 Dex 13 Con 15 Int 9 Wis 11 Cha 8 (+2 Strength, +1 Constitution); Longsword, Chain Mail, Shield |
| Hilde Varrow (female) | Human Fighter, Lawful Neutral; Str 17 Dex 15 Con 16 Int 10 Wis 12 Cha 11; Long Sword, Chain Mail, Shield, Long Bow (packed), Arrows x40 (packed) | Human Fighter, Lawful Neutral, Soldier; rolled Str 15 Dex 15 Con 14 Int 10 Wis 12 Cha 11 (+2 Strength, +1 Constitution); Longsword, Chain Mail, Shield, Longbow (packed) |
| Aveline Harrow (female) | Human Cleric, Lawful Good; Str 14 Dex 10 Con 14 Int 11 Wis 17 Cha 13; Mace, Chain Mail, Shield; memorizes bless, cure light wounds, cure light wounds | Human Cleric, Lawful Good, Acolyte; rolled Str 14 Dex 10 Con 14 Int 11 Wis 15 Cha 12 (+2 Wisdom, +1 Charisma); Mace, Scale Mail, Shield; spells guidance, sacred flame, resistance, bless, cure wounds, guiding bolt, shield of faith |
| Corvin Mallory (male) | Human Magic-User, Neutral Good; Str 9 Dex 14 Con 13 Int 17 Wis 12 Cha 11; Quarterstaff, Dagger (packed); memorizes sleep | Human Wizard, Neutral Good, Sage; rolled Str 8 Dex 14 Con 13 Int 15 Wis 12 Cha 11 (+2 Intelligence, +1 Constitution); Quarterstaff, Dagger (packed); spells fire bolt, ray of frost, shocking grasp, magic missile, shield, sleep, mage armor, burning hands, detect magic |
| Tamsin Burrows (male) | Halfling Thief, Neutral; Str 11 Dex 18 Con 14 Int 12 Wis 10 Cha 10; Short Sword, Leather Armor, Sling (packed) | Halfling Rogue, Neutral, Criminal; rolled Str 10 Dex 15 Con 13 Int 12 Wis 10 Cha 10 (+2 Dexterity, +1 Constitution); Shortsword, Leather Armor, Sling (packed) |
| Sylvaine Ashgrove (female) | Elf Fighter/Magic-User, Chaotic Good; Str 16 Dex 17 Con 13 Int 16 Wis 10 Cha 12; Long Sword, Short Bow (packed), Arrows x40 (packed); memorizes sleep | Elf Fighter, Chaotic Good, Soldier; rolled Str 14 Dex 15 Con 13 Int 13 Wis 10 Cha 12 (+2 Dexterity, +1 Strength); Longsword, Studded Leather Armor, Shortbow (packed) |

## Curse of the Azure Bonds (`CURSE.json`)

| Name | Original rules | SRD 5.2.1 |
|---|---|---|
| Roderic Vance (male) | Human Paladin, Lawful Good; Str 17 Dex 12 Con 15 Int 10 Wis 14 Cha 17; Long Sword, Plate Mail, Shield | Human Paladin, Lawful Good, Soldier; rolled Str 15 Dex 12 Con 14 Int 10 Wis 12 Cha 15 (+2 Strength, +1 Constitution); Longsword, Chain Mail, Shield; spells bless, cure wounds |
| Wren Falconer (female) | Human Ranger, Chaotic Good; Str 16 Dex 16 Con 15 Int 13 Wis 14 Cha 10; Long Sword, Scale Mail, Long Bow (packed), Arrows x40 (packed) | Human Ranger, Chaotic Good, Soldier; rolled Str 13 Dex 15 Con 14 Int 12 Wis 14 Cha 10 (+2 Dexterity, +1 Constitution); Longsword, Scale Mail, Longbow (packed); spells cure wounds, longstrider |
| Durgan Flintcairn (male) | Dwarf Fighter, Neutral Good; Str 18 Dex 12 Con 18 Int 8 Wis 10 Cha 7; Battle Axe, Banded Mail, Shield | Dwarf Fighter, Neutral Good, Soldier; rolled Str 15 Dex 12 Con 15 Int 8 Wis 10 Cha 7 (+2 Strength, +1 Constitution); Battleaxe, Chain Mail, Shield |
| Mirela Sunward (female) | Human Cleric, Neutral Good; Str 13 Dex 11 Con 15 Int 12 Wis 18 Cha 14; Mace, Chain Mail, Shield; memorizes bless, cure light wounds, cure light wounds, cure light wounds, protection from evil, hold person, hold person, hold person, silence 15 radius, spiritual hammer, prayer, dispel magic | Human Cleric, Neutral Good, Acolyte; rolled Str 13 Dex 11 Con 14 Int 12 Wis 15 Cha 13 (+2 Wisdom, +1 Charisma); Mace, Scale Mail, Shield; spells guidance, sacred flame, resistance, bless, cure wounds, guiding bolt, shield of faith |
| Thalion Vey (male) | Elf Magic-User, Chaotic Good; Str 9 Dex 17 Con 12 Int 18 Wis 12 Cha 13; Quarterstaff, Dagger (packed); memorizes magic missile, magic missile, sleep, sleep, stinking cloud, stinking cloud, fireball | Elf Wizard, Chaotic Good, Sage; rolled Str 8 Dex 15 Con 12 Int 15 Wis 12 Cha 13 (+2 Intelligence, +1 Constitution); Quarterstaff, Dagger (packed); spells fire bolt, ray of frost, shocking grasp, magic missile, shield, sleep, mage armor, burning hands, detect magic |
| Nim Quickfinger (male) | Gnome Thief, Neutral; Str 12 Dex 18 Con 15 Int 13 Wis 10 Cha 9; Short Sword, Leather Armor, Sling (packed) | Gnome Rogue, Neutral, Criminal; rolled Str 10 Dex 15 Con 13 Int 13 Wis 10 Cha 9 (+2 Dexterity, +1 Intelligence); Shortsword, Leather Armor, Sling (packed) |

## Secret of the Silver Blades (`SECRET.json`)

| Name | Original rules | SRD 5.2.1 |
|---|---|---|
| Ysolde Marrick (female) | Human Paladin, Lawful Good; Str 16 Dex 13 Con 15 Int 11 Wis 14 Cha 17; Long Sword, Plate Mail, Shield | Human Paladin, Lawful Good, Soldier; rolled Str 14 Dex 13 Con 14 Int 11 Wis 12 Cha 15 (+2 Strength, +1 Constitution); Longsword, Chain Mail, Shield; spells bless, cure wounds |
| Orrin Hale (male) | Human Ranger, Neutral Good; Str 17 Dex 15 Con 16 Int 13 Wis 14 Cha 11; Long Sword, Scale Mail, Long Bow (packed), Arrows x40 (packed) | Human Ranger, Neutral Good, Soldier; rolled Str 14 Dex 15 Con 14 Int 12 Wis 14 Cha 11 (+2 Strength, +1 Dexterity); Longsword, Scale Mail, Longbow (packed); spells cure wounds, longstrider |
| Bruni Hammerfell (female) | Dwarf Fighter, Lawful Neutral; Str 18 Dex 13 Con 18 Int 9 Wis 11 Cha 8; Battle Axe, Banded Mail, Shield | Dwarf Fighter, Lawful Neutral, Soldier; rolled Str 15 Dex 13 Con 15 Int 9 Wis 11 Cha 8 (+2 Strength, +1 Constitution); Battleaxe, Chain Mail, Shield |
| Anselm Brightwater (male) | Human Cleric, Lawful Good; Str 14 Dex 10 Con 15 Int 12 Wis 18 Cha 13; Mace, Chain Mail, Shield; memorizes bless, cure light wounds, cure light wounds, cure light wounds, protection from evil, hold person, hold person, hold person, silence 15 radius, spiritual hammer, prayer, dispel magic, cure disease, remove curse, cure serious wounds, cure serious wounds, neutralize poison | Human Cleric, Lawful Good, Acolyte; rolled Str 14 Dex 10 Con 14 Int 12 Wis 15 Cha 12 (+2 Wisdom, +1 Charisma); Mace, Scale Mail, Shield; spells guidance, sacred flame, resistance, bless, cure wounds, guiding bolt, shield of faith |
| Liriel Moonbrook (female) | Elf Magic-User, Neutral Good; Str 8 Dex 17 Con 12 Int 18 Wis 13 Cha 12; Quarterstaff, Dagger (packed); memorizes magic missile, magic missile, sleep, shield, stinking cloud, mirror image, invisibility, fireball, lightning bolt, haste, confusion, minor globe of invulnerability, hold monster | Elf Wizard, Neutral Good, Sage; rolled Str 8 Dex 15 Con 12 Int 15 Wis 13 Cha 12 (+2 Intelligence, +1 Constitution); Quarterstaff, Dagger (packed); spells fire bolt, ray of frost, shocking grasp, magic missile, shield, sleep, mage armor, burning hands, detect magic |
| Fenn Applewood (male) | Halfling Thief, Neutral; Str 11 Dex 18 Con 15 Int 12 Wis 10 Cha 10; Short Sword, Leather Armor, Sling (packed) | Halfling Rogue, Neutral, Criminal; rolled Str 10 Dex 15 Con 13 Int 12 Wis 10 Cha 10 (+2 Dexterity, +1 Constitution); Shortsword, Leather Armor, Sling (packed) |

## Gateway to the Savage Frontier (`GATEWAY.json`)

Original rules only. Each starts with 3,000 experience; a fighter, paladin,
ranger, cleric and magic-user reach level 2, the thief level 3.

| Name | Original rules |
|---|---|
| Kestrel Dunmore (female) | Human Ranger, Chaotic Good; Str 17 Dex 16 Con 16 Int 13 Wis 14 Cha 10; Long Sword, Scale Mail, Long Bow (packed), Arrows x40 (packed) |
| Garran Holt (male) | Human Paladin, Lawful Good; Str 17 Dex 12 Con 16 Int 10 Wis 13 Cha 17; Long Sword, Plate Mail, Shield |
| Thrain Coppervein (male) | Dwarf Fighter, Lawful Neutral; Str 18 Dex 13 Con 17 Int 9 Wis 11 Cha 8; Battle Axe, Banded Mail, Shield |
| Maren Ashcombe (female) | Human Cleric, Neutral Good; Str 14 Dex 11 Con 15 Int 12 Wis 17 Cha 13; Mace, Chain Mail, Shield; memorizes bless, cure light wounds, cure light wounds, protection from evil |
| Elowen Brightleaf (female) | Elf Magic-User, Chaotic Good; Str 8 Dex 17 Con 12 Int 18 Wis 12 Cha 13; Quarterstaff, Dagger (packed); memorizes magic missile, sleep |
| Bram Thistlewood (male) | Halfling Thief, Neutral; Str 11 Dex 18 Con 14 Int 12 Wis 10 Cha 10; Short Sword, Leather Armor, Sling (packed) |

## Pools of Darkness (`DARKNESS.json`)

Original rules only. Each starts with 1,500,001 experience and 200 platinum:
the paladin reaches level 12, the ranger 13, the cleric and magic-user 14,
the thief 16, and the dwarf fighter stops at his race's 9. The game's
creation gives each its equipment by class: a fighter's long sword, plate
mail, shield, long bow and arrows; a thief's bracers, ring, long sword and
sling; a cleric's mace, plate mail, shield, staff sling and scrolls; a
magic-user's bracers, ring, quarterstaff and darts.

| Name | Original rules |
|---|---|
| Aldric Stormvale (male) | Human Paladin, Lawful Good; Str 17 Dex 13 Con 16 Int 10 Wis 14 Cha 17; memorizes bless, cure light wounds, hold person, spiritual hammer |
| Sera Nightbrook (female) | Human Ranger, Neutral Good; Str 16 Dex 17 Con 16 Int 13 Wis 14 Cha 11; memorizes magic missile, shield, mirror image, entangle, faerie fire, barkskin |
| Hrolf Granitehand (male) | Dwarf Fighter, Lawful Neutral; Str 18 Dex 12 Con 18 Int 9 Wis 10 Cha 8 |
| Ilona Dawnmere (female) | Human Cleric, Lawful Good; Str 13 Dex 10 Con 15 Int 12 Wis 18 Cha 14; memorizes bless x2, cure light wounds x4, protection from evil x2, hold person x4, silence 15' radius, spiritual hammer x2, resist fire, dispel magic x2, prayer x2, remove curse, cure disease, cure blindness, cure serious wounds x4, neutralize poison, protection from evil 10' radius, flame strike, cure critical wounds, raise dead, heal, blade barrier |
| Varis Quillon (male) | Human Magic-User, Neutral Good; Str 9 Dex 16 Con 13 Int 18 Wis 12 Cha 11; memorizes magic missile x4, shield, stinking cloud x2, mirror image x2, invisibility, fireball x2, lightning bolt, haste, dispel magic, ice storm x2, minor globe of invulnerability, fire shield, cone of cold x3, cloudkill, disintegrate, globe of invulnerability, delayed blast fireball |
| Tobble Fennick (male) | Gnome Thief, Neutral; Str 12 Dex 18 Con 15 Int 13 Wis 10 Cha 9 |

## Treasures of the Savage Frontier (`TREASURE.json`)

Original rules only. Each starts with 22,600 experience and 200 platinum: the
paladin reaches level 4, the ranger, fighter, cleric and magic-user 5, the
thief 6. Its battle axe takes both hands, so the dwarf carries no shield.

| Name | Original rules |
|---|---|
| Brenna Hawkmoor (female) | Human Paladin, Lawful Good; Str 17 Dex 12 Con 15 Int 11 Wis 14 Cha 17; Long Sword, Plate Mail, Shield |
| Oswin Greaves (male) | Human Ranger, Chaotic Good; Str 16 Dex 16 Con 15 Int 13 Wis 14 Cha 10; Long Sword, Scale Mail, Long Bow (packed), Arrows x40 (packed) |
| Dain Ironfold (male) | Dwarf Fighter, Neutral Good; Str 18 Dex 11 Con 18 Int 8 Wis 11 Cha 7; Battle Axe, Banded Mail |
| Halvard Penn (male) | Human Cleric, Neutral Good; Str 14 Dex 10 Con 15 Int 11 Wis 18 Cha 13; Mace, Chain Mail, Shield; memorizes bless x2, cure light wounds x3, hold person x3, silence 15' radius, spiritual hammer, dispel magic, prayer |
| Neris Silverfrond (female) | Elf Magic-User, Chaotic Good; Str 8 Dex 17 Con 12 Int 18 Wis 13 Cha 12; Quarterstaff, Dagger (packed); chooses burning hands, charm person, magic missile, shield, sleep, mirror image, stinking cloud and fireball at creation; memorizes magic missile x2, sleep, shield, stinking cloud, mirror image, fireball |
| Quill Bramblefoot (male) | Halfling Thief, Neutral; Str 11 Dex 18 Con 15 Int 12 Wis 10 Cha 10; Short Sword, Leather Armor, Sling (packed) |

## Unlimited Adventures (`UA.json`)

A design's party is made under the kit's rules with the design's own start
(its game record's experience, platinum, gems, jewelry and set of
equipment, as each new character of the design gets them,
`ua::outfit_new_character`), so the file lists no items: the design arms
them. Its casters memorize only first-level spells, which a design starting
its characters at level 1 still lets them. A design plays by the original
rules only; the SRD characters are there as in every file.

| Name | Original rules | SRD 5.2.1 |
|---|---|---|
| Garrick Thornwall (male) | Human Paladin, Lawful Good; Str 17 Dex 12 Con 16 Int 10 Wis 14 Cha 17 | Human Paladin, Lawful Good, Soldier; rolled Str 15 Dex 12 Con 14 Int 10 Wis 13 Cha 15 (+2 Strength, +1 Constitution); spells bless, cure wounds |
| Dagna Ironvein (female) | Dwarf Fighter, Lawful Neutral; Str 18 Dex 13 Con 17 Int 9 Wis 11 Cha 9 | Dwarf Fighter, Lawful Neutral, Soldier; rolled Str 15 Dex 13 Con 15 Int 9 Wis 11 Cha 9 (+2 Strength, +1 Constitution) |
| Bertram Ashby (male) | Human Cleric, Neutral Good; Str 14 Dex 10 Con 15 Int 11 Wis 17 Cha 13; memorizes cure light wounds | Human Cleric, Neutral Good, Acolyte; rolled Str 14 Dex 10 Con 14 Int 11 Wis 15 Cha 12 (+2 Wisdom, +1 Charisma); spells guidance, sacred flame, resistance, bless, cure wounds, guiding bolt, shield of faith |
| Maelis Starfall (female) | Elf Fighter/Magic-User, Chaotic Good; Str 16 Dex 17 Con 13 Int 16 Wis 10 Cha 12; memorizes sleep | Elf Fighter, Chaotic Good, Soldier; rolled Str 14 Dex 15 Con 13 Int 13 Wis 10 Cha 12 (+2 Dexterity, +1 Strength) |
| Pell Quickfoot (male) | Halfling Thief, Neutral; Str 11 Dex 18 Con 14 Int 12 Wis 10 Cha 10 | Halfling Rogue, Neutral, Criminal; rolled Str 10 Dex 15 Con 13 Int 12 Wis 10 Cha 10 (+2 Dexterity, +1 Constitution) |
| Ilsabet Vane (female) | Human Magic-User, Neutral Good; Str 9 Dex 15 Con 13 Int 17 Wis 12 Cha 11; memorizes sleep | Human Wizard, Neutral Good, Sage; rolled Str 8 Dex 14 Con 13 Int 15 Wis 12 Cha 11 (+2 Intelligence, +1 Constitution); spells fire bolt, ray of frost, shocking grasp, magic missile, shield, sleep, mage armor, burning hands, detect magic |

## Open choices

- *Pool of Radiance*'s elf fighter/magic-user, Sylvaine, is an SRD Fighter:
  the SRD has no multiclass character at level 1.
- The SRD has no holy symbols or spellbooks as items, so its casters carry
  none; the original rules have none either.
- Hit Points under the original rules come from the game's own roll; *Pool of
  Radiance*'s magic-user, Corvin, starts with few, as a first-level
  magic-user often does.
