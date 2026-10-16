# Cleric

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 34–37. Spell preparation, cantrips and Divine Order are described
in [Cleric preparation](CLERIC-PREPARATION.md).

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting, Divine Order | Delivered ([Cleric preparation](CLERIC-PREPARATION.md)) |
| 2 | Channel Divinity | Delivered (rules 0.6.82) |
| 3 | Cleric Subclass (Life Domain) | Delivered (rules 0.6.83; Aid 0.6.84) |
| 4 | Ability Score Improvement | Delivered earlier |
| 5 | Sear Undead, level-3 spells | Delivered (rules 0.6.213) |
| 6 | Blessed Healer (Life), Channel Divinity 3 | Delivered (rules 0.6.213) |
| 7 | Blessed Strikes, Divine Spark 2d8, level-4 spells | Delivered (rules 0.6.213) |
| 8 | Ability Score Improvement | Delivered (rules 0.6.213) |
| 9 | Level-5 spells | Delivered (rules 0.6.214) |
| 10 | Divine Intervention | Delivered (rules 0.6.214) |
| 11 | Level-6 spells, cantrip damage | Delivered (rules 0.6.215) |
| 12 | Ability Score Improvement | Delivered (rules 0.6.215) |
| 13 | Level-7 spells, Divine Spark 3d8 | Delivered (rules 0.6.216) |
| 14 | Improved Blessed Strikes | Delivered (rules 0.6.216) |
| 15 | Level-8 spells | Delivered (rules 0.6.216) |

## Levels 5–15

The Cleric advances to the game's cap of 15
([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15),
[LEVELS-3](SRD-DECISIONS.md#levels-3-2026-10-15-the-open-decisions-of-levels-1115)).

- **Spellcasting:** level-3 slots from 5 (two, three from 6), level-4 slots
  from 7 (one, two from 8); prepared spells 9, 10, 11, 12 at levels 5–8.
- **Level-3 spells** (5): Animate Dead, Beacon of Hope, Bestow Curse, Dispel
  Magic, Magic Circle, Protection from Energy, Remove Curse, Revivify and
  Spirit Guardians, and Mass Healing Word; the Life Domain always prepares
  Mass Healing Word and Revivify. Clairvoyance, Create Food and Water,
  Daylight, Glyph of Warding, Meld into Stone, Sending, Speak with Dead,
  Tongues and Water Walk are cut (LEVELS-1).
- **Level-4 spells** (7): Aura of Life, Banishment, Death Ward, Freedom of
  Movement and Guardian of Faith; the Life Domain always prepares Aura of Life
  and Death Ward. Control Water, Divination, Locate Creature and Stone Shape
  are cut.
- **Sear Undead** (5): Turn Undead rolls a d8 for each point of the Wisdom
  modifier (at least one) once; each Undead that fails its save takes that
  much Radiant damage, and stays turned.
- **Blessed Healer** (Life 6): when a spell cast with a slot restores Hit
  Points to another creature, the Cleric regains 2 + the slot's level.
- **Channel Divinity:** three uses from level 6.
- **Blessed Strikes** (7), chosen in the level-up Training dropdown:
  - **Divine Strike:** once on each of its turns, after a weapon hit, the
    Bonus Action list's smite window offers **Divine Strike (Radiant)** and
    **Divine Strike (Necrotic)**: 1d8 more damage of that type, part of the
    hit (a Critical Hit doubles it). The party AI takes Radiant.
  - **Potent Spellcasting:** the Wisdom modifier is added to the damage of
    Cleric cantrips (Sacred Flame).
- **Divine Spark** rolls 2d8 from level 7.
- **Ability Score Improvement** (8): a feat or ability points.
- **Level-5 spells** (9): one level-5 slot at 9, two at 10; prepared spells
  14 and 15. Contagion, Dispel Evil and Good, Flame Strike, Greater
  Restoration, Insect Plague, Mass Cure Wounds and Raise Dead; the Life Domain
  always prepares Greater Restoration and Mass Cure Wounds. Commune, Geas,
  Hallow, Legend Lore, Planar Binding and Scrying are cut (LEVELS-1).
- **Divine Intervention** (10): once per Long Rest ("Divine Intervention" in
  the resources), a Magic action casts any Cleric spell of level 5 or lower,
  prepared or not, without a slot: the A cycle lists each such spell again as
  "… (Divine Intervention)". A Bonus Action spell cast this way takes the
  Action. Revivify, Raise Dead and Magic Circle are not offered through it.
- **Level-6 spells** (11): one level-6 slot; prepared spells 16 at 11 and
  12. Blade Barrier, Create Undead, Harm, Heal, Heroes' Feast, Sunbeam, True
  Seeing and Word of Recall (below). Find the Path, Forbiddance and Planar Ally
  are cut.
- **Cantrips** deal two dice from level 5 and three from 11 (Sacred Flame 3d8
  at 11), as every class's do (see [Spell slots](SPELL-SLOTS.md)).
- **Ability Score Improvement** (12): a feat or ability points.
- **Level-7 spells** (13): one level-7 slot; prepared spells 17 at 13 and
  14, 18 at 15. Conjure Celestial, Divine Word, Fire Storm, Regenerate and
  Resurrection (below). Etherealness, Plane Shift and Symbol are cut.
- **Divine Spark** rolls 3d8 from level 13.
- **Improved Blessed Strikes** (14): Divine Strike deals 2d8. With Potent
  Spellcasting, after a Cleric cantrip (Sacred Flame) deals damage, the
  Bonus Action list offers **Potent Spellcasting: Temporary Hit Points** for
  the Cleric or an ally within 60 feet, once that turn: twice the Wisdom
  modifier. The Cleric is asked whether to keep Temporary Hit Points it
  already has; an ally keeps the larger amount.
- **Level-8 spells** (15): one level-8 slot. Antimagic Field, Earthquake,
  Holy Aura and Sunburst (below). Control Weather is cut.
- Verification: `opengold_cleric_levels_tests` (the campaign's stop at 15,
  features and prepared counts by level, the level-3 and level-4 spells,
  slots and Channel Divinity uses, each spell below, Sear Undead, Blessed
  Healer, Divine Strike once a turn, a campaign round trip, each level-5 spell
  and Divine Intervention, each level-6 spell, Sacred Flame's dice at 4, 5
  and 11, each level-7 and level-8 spell, Resurrection and Regenerate in
  camp, Improved Blessed Strikes' Temporary Hit Points) and
  `opengold_word_of_recall_tests` (with the installed game: the
  refusal without a sanctuary, preparing it at the Temple of Tyr, the trip
  back from the training schools).

### The new level-3 spells

- **Mass Healing Word** (Bonus Action, 60 feet): up to six creatures, chosen
  one click at a time as for Bless, each regain 2d4 + the Wisdom modifier
  (1d4 more for each slot level above 3).
- **Spirit Guardians** (Action, Concentration up to 10 minutes): spirits in a
  15-foot Emanation around the Cleric, drawn on the map. The Cleric's allies
  are unaffected. An enemy's Speed is halved there (its squares count as
  Difficult Terrain for it); each enemy the Cleric can see makes a Wisdom
  save the first time in a turn the Emanation reaches it or it ends its turn
  there, taking 3d8 Radiant damage, half on a success (1d8 more for each slot
  level above 3). An evil Cleric's guardians deal Necrotic damage instead
  (the character profile carries the alignment, recipe `PC43`).
- **Bestow Curse** (Action, touch, Concentration up to 1 minute; none from a
  level-5 slot): the A cycle offers each curse against an adjacent creature,
  which makes a Wisdom save: Disadvantage on one ability's saves (pick the
  ability), Disadvantage on its attacks against the Cleric, a Wisdom save at
  the start of each of its turns or only the Dodge action, or 1d8 more
  Necrotic damage whenever the Cleric damages it with an attack roll or a
  spell. Remove Curse ends it.
- **Animate Dead** (a minute's casting, 10 feet): the A cycle offers it
  against an enemy's corpse, a Small or Medium Humanoid; a higher slot takes
  two more corpses for each level above 3. As with Magic Circle, the casting
  holds Concentration and takes the Cleric's Action on each turn of its
  minute (ten rounds, LEVELS-1); broken off, it fails and keeps its slot.
  Then each corpse rises as a Zombie (SRD stat block, with Undead Fortitude)
  that fights for the party right after the Cleric, under the player's
  control (LEVELS-1). The Zombies fall again when the fight ends (LEVELS-3),
  and count as defeated enemies for experience.
- Combat checkpoints keep each actor's Divine Strike and controlled-creature
  state (`OGCOMBAT 56`, unchanged).

### The level-5 spells

- **Flame Strike** (Action, 60 feet): aimed like Entangle, a 10-foot-radius
  column; each creature in it makes a Dexterity save against 5d6 Fire and
  5d6 Radiant damage, half on a success, each type resisted on its own (1d6
  of each more for each slot level above 5).
- **Insect Plague** (Action, 300 feet, Concentration up to 10 minutes): a
  20-foot-radius swarm, Difficult Terrain, drawn on the map. Each creature in
  it when it appears, entering it or ending its turn there makes a
  Constitution save, once a turn, against 4d10 Piercing damage, half on a
  success (1d10 more for each slot level above 5). As with the game's other
  areas the caster is spared. Its light obscurement has nothing to act on.
- **Mass Cure Wounds** (Action, 60 feet): aimed at a point, a 30-foot-radius
  Sphere; up to six allies in it regain 5d8 + the Wisdom modifier (1d8 more
  for each slot level above 5). Adaptation: the six are picked for the
  player, the most hurt allies first, as the caster's choice of creatures in
  the Sphere would be.
- **Greater Restoration** (Action, touch): the A cycle offers what afflicts an
  ally: **Charmed** (also ends a domination) or **curse** (Bestow Curse,
  Hex). Exhaustion, ability score and Hit Point maximum reductions and
  Petrified have nothing to act on yet.
- **Contagion** (Action, touch): one entry per ability; a creature that fails
  a Constitution save takes 11d8 Necrotic damage and is Poisoned, with
  Disadvantage on that ability's saves. It saves again at the end of each of
  its turns: three successes end it, three failures make it last seven days.
  Lesser Restoration ends its Poisoned only if the creature succeeds on a
  Constitution save.
- **Dispel Evil and Good** (Action, Concentration up to 1 minute):
  Celestials, Elementals, Fey, Fiends and Undead attack the Cleric with
  Disadvantage. A Magic action ends it early with **Break Enchantment** (an
  ally within 5 feet is freed from such creatures' charm or fear) or
  **Dismissal** (such a creature within 5 feet makes a Charisma save or is
  sent home, counting as defeated, as a banished one is).
- **Raise Dead** (an hour's casting, in camp): from the Camp dialog's Cast /
  Use list, on a member dead up to ten days, for an offering of 300 gp times
  its level (LEVELS-1). It returns with 1 Hit Point and a −4 penalty to
  attack rolls, saving throws and Initiative that lessens by 1 with each Long
  Rest.

### The level-6 spells

- **Blade Barrier** (Action, 90 feet, Concentration up to 10 minutes): a wall
  of blades 100 feet long, horizontal, vertical or along either diagonal, or
  a ring 60 feet across (LEVELS-3), Difficult Terrain, drawn as a wall. Each
  creature in it when it rises, entering it or ending its turn there makes a
  Dexterity save, once a turn, against 6d10 Force damage, half on a success.
  Its cover has nothing to act on (COVER-1).
- **Create Undead** (a minute's casting, 10 feet): as Animate Dead, chosen
  one corpse at a time: up to three corpses rise as Ghouls (four from a
  level-7 slot, five from a level-8 one), at any hour (LEVELS-3). The Ghoul is
  the SRD stat block: two Bites, each 1d6 + 2 Piercing and 1d6 Necrotic; its
  paralyzing Claw, a second action, is left out. Ghasts and Wights have no
  stat blocks in the game yet.
- **Harm** (Action, 60 feet): a Constitution save against 14d6 Necrotic
  damage; on a failure the Hit Point maximum drops by the damage taken, never
  below 1, until Greater Restoration ends it ("Greater Restoration: Hit Point
  maximum"). Aura of Life prevents the drop.
- **Heal** (Action, 60 feet): 70 Hit Points (10 more for each slot level
  above 6) and the end of Blinded and Poisoned.
- **Heroes' Feast** (camp, the whole party): each member gains for a day
  Poison Resistance, Immunity to Frightened and Poisoned, and 2d10 more Hit
  Points at most, healed at once. Its bowl is free (LEVELS-3).
- **Sunbeam** (Action, Concentration up to 1 minute): a 5-foot-wide, 60-foot
  Line; each creature in it makes a Constitution save against 6d8 Radiant
  damage and is Blinded until the Cleric's next turn on a failure, half and
  not Blinded on a success. While it lasts, **Sunbeam: a new Line** (a Magic
  action, no slot) fires another. Its sunlight has nothing to act on yet.
- **True Seeing** (Action, touch, 1 hour): the creature sees Invisible and
  hidden creatures and through magical Darkness.
- **Word of Recall** (camp): cast from the Camp dialog's Cast / Use list. On
  the square where the party just used the Temple of Tyr (or another temple
  of New Phlan) it prepares the party's sanctuary; anywhere else it takes the
  whole party back there (LEVELS-3), from a district by running the town's
  entry script as the gate does. Without a prepared sanctuary it is refused
  and spends nothing. The sanctuary is kept in campaign saves (format 30).

### The level-7 spells

- **Conjure Celestial** (Action, 90 feet, Concentration up to 10 minutes): a
  pillar of light 10 feet in radius, drawn as moonlight. The light falls on
  each creature the Cleric can see in it when it appears, when the pillar
  moves onto it, when it enters, and when it ends its turn there, once a
  turn: the Cleric's allies regain 4d12 + the Wisdom modifier (Disciple of
  Life adds its 2 + the slot level), the others make a Dexterity save against
  6d12 Radiant damage, half on a success; 1d12 more of each for each slot
  level above 7. The SRD lets the caster choose the light for each creature;
  the game heals allies and sears the rest. Once the Cleric has moved on its
  turn, **Move the pillar of light** moves it up to 30 feet, onto a creature
  the Cleric can see.
- **Divine Word** (Bonus Action, 30 feet): each foe in range makes a Charisma
  save. On a failure, by its current Hit Points: 20 or fewer, it dies; 21–30,
  Blinded and Stunned for an hour; 31–40, Blinded for 10 minutes; 41–50,
  Deafened, which is not modeled (DEAF-1). A Celestial, an Elemental, a Fey or
  a Fiend that fails is sent back to its plane and counts as defeated, as
  after Banishment (LEVELS-1). The SRD's "creatures of your choice" are the
  Cleric's foes.
- **Fire Storm** (Action, 150 feet): up to ten 10-foot cubes, each touching
  another, placed one at a time (LEVELS-3). The board splits into cubes two
  squares by two; aiming starts with one cube on the nearest foe. A click on
  a free cube that touches a placed one adds it; a click on the cube placed
  last removes it, and a click on another placed cube selects it; while only
  one cube is placed, a click anywhere moves it. The arrow keys step a whole
  cube. Each creature in the storm makes a Dexterity save against 7d10 Fire
  damage, half on a success. Flammable objects have nothing to burn.
- **Regenerate** (a minute's casting, touch): in a fight the casting takes
  Concentration and the Cleric's Action on each of its turns for 10 rounds,
  as Animate Dead's does; broken, it fails and keeps its slot. Then the
  creature regains 4d8 + 15 Hit Points (Disciple of Life adds its share) and,
  for an hour, 1 Hit Point at the start of each of its turns ("Regenerate"
  in its conditions). In camp, the hour of regaining 1 Hit Point a turn heals
  the member fully. Severed limbs have nothing to regrow.
- **Resurrection** (camp, an hour's casting): any dead member, however long
  dead, returns with all its Hit Points and the −4 penalty Raise Dead gives,
  which lessens by 1 with each Long Rest. It takes Raise Dead's offering, 300
  gp × the member's level, and can revive a creature turned to dust
  (LEVELS-3). The SRD's century and old-age limits never arise.

### The level-8 spells

- **Antimagic Field** (Action, Concentration up to 1 hour): a 10-foot
  Emanation around the Cleric, drawn as a ward. No one in it casts a spell,
  the Cleric included; no spell is cast on a creature in it; spell areas and
  lasting spell zones (Spirit Guardians, Insect Plague, a Moonbeam and the
  like) do not affect a creature in it. A creature's spell effects (Bless,
  Hold Person, True Seeing and every other effect Dispel Magic could end) are
  suppressed while it stands in the field: they do nothing, their time still
  runs, and they return when it leaves or the field ends. No Magic action or
  other magic is used in it or on a creature in it either (LEVELS-4): Channel
  Divinity, a spell's later actions (a Sunbeam's new Line, moving a Moonbeam),
  Lay On Hands, Wild Shape, Breath Weapon and smites. Magic items, which do
  not work there either, and teleportation have nothing to act on yet.
- **Earthquake** (Action, 500 feet, Concentration up to 1 minute): a tremor
  100 feet in radius, Difficult Terrain. When it is cast and at the end of
  each of the Cleric's turns, every creature in it makes a Dexterity save; on
  a failure it falls Prone and loses Concentration, the Cleric too.
  Fissures and collapsing structures are left out: the battlefield has no
  depth or buildings with Hit Points.
- **Holy Aura** (Action, Concentration up to 1 minute): a 30-foot Emanation
  around the Cleric, drawn as a ward. The Cleric's allies in it (the SRD's
  "creatures of your choice") have Advantage on every saving throw, and
  attack rolls against them have Disadvantage. A Fiend or an Undead that hits
  one of them with a melee attack makes a Constitution save or is Blinded
  until the end of its next turn.
- **Sunburst** (Action, 150 feet): a 60-foot-radius Sphere; each creature in
  it makes a Constitution save against 12d6 Radiant damage and is Blinded for
  a minute on a failure, saving again at the end of each of its turns; half
  and not Blinded on a success. It ends spell Darkness in the Sphere. Allies
  in the Sphere are not spared.

## Channel Divinity

- Two uses from level two; a Short Rest restores one, a Long Rest all
  ("Channel Divinity" in the rest and combat resources).
- **Divine Spark** (Action, another creature the Cleric can see within 30
  feet): roll 1d8 + Wisdom modifier. An ally regains that many Hit Points; an
  enemy makes a Constitution save and takes that much Radiant damage, half on a
  success. The SRD lets the Cleric choose Necrotic instead; the game always
  deals Radiant
  ([SPARK-1](SRD-DECISIONS.md#spark-1-2026-10-14-divine-spark-always-radiant)).
- **Turn Undead** (Action, offered when an Undead enemy is within 30 feet): each
  Undead enemy within 30 feet makes a Wisdom save or is turned for 1 minute:
  Frightened and Incapacitated, it can only move as far from the Cleric as it
  can, takes no Action or Reaction, and the effect ends when it takes damage or
  the Cleric is Incapacitated (Paralyzed, Sleep, Hideous Laughter or 0 Hit
  Points). The current content has no Undead yet.
- In the game: A cycles to **Divine Spark** (click the creature) or **Turn
  Undead** (Space). The party AI uses Divine Spark on an ally below half Hit
  Points before spending a spell slot, and Turn Undead whenever it is offered.

Verification: `opengold_cleric_channel_tests` (no Channel Divinity at level one,
two uses, healing, harming with a save, Turn Undead needing an Undead, a turned
zombie fleeing then ending its turn, a checkpoint, damage ending it, uses spent
in fights kept by a campaign save, one back on a Short Rest and all on a Long
Rest) and
`opengold_godot_divine_spark` (A cycle and click, English and Spanish).

## Life Domain (level 3)

- The Life Domain is the SRD's only Cleric subclass, taken automatically at
  level three.
- **Disciple of Life**: a healing spell cast with a slot restores 2 + the
  slot's level more, in combat and in camp.
- **Preserve Life** (Channel Divinity, Action; offered when a Bloodied ally,
  the Cleric included, is within 30 feet): restores five times the Cleric level,
  divided among Bloodied allies within 30 feet, none above half its Hit Point
  maximum. Adaptation: the division is automatic, the most hurt first. Undead
  and Constructs are not healed. The AI uses it whenever it is offered.
- **Life Domain spells**: Aid, Bless, Cure Wounds and Lesser Restoration are
  always prepared from level three and not counted; earlier preparations of
  them free their places.
- **Lesser Restoration** (level 2, Bonus Action, touch): ends one condition on
  an ally: Blinded (Blindness or Color Spray), Paralyzed (Hold Person) or
  Poisoned. It is offered once for each of these the ally has, as **Lesser
  Restoration: Blinded** and so on; nothing deafens yet. The AI uses it whenever
  it is offered.
- Verification: `opengold_cleric_channel_tests` (the always prepared spells,
  Disciple of Life's +3 from a level-one slot on identical rolls, Preserve
  Life, a Lesser Restoration choice for each condition) and the updated preparation checks in
  `opengold_spell_access_tests` and `opengold_spell_component_tests`.

## Aid

- **Aid** (level 2, Action, 30 feet, 8 hours, no Concentration): up to three
  creatures, chosen as for Bless
  ([CLASS-2](SRD-DECISIONS.md#class-2-2026-10-04-choosing-several-targets)),
  raise their Hit Point maximum and current Hit Points by 5. A creature already
  aided gains nothing more. The higher maximum is kept in the character's vital
  state, so it lasts after the fight; when Aid ends, Hit Points above the sheet's
  maximum go with it. Higher slots wait for level-three slots.
- `RulesModule::hit_point_maximum` and `CampaignParty::hit_point_maximum` give
  the raised maximum; the campaign's checks, healing, the temple, the rest
  dialog, the party panel and the combat roster use it.
- **In camp** Aid is in the Camp dialog's Cast / Use list. As Pool of
  Radiance's group spells do, the game picks its targets: the three
  members with the lowest Hit Point maximum not already aided, the caster
  included ([AID-1](SRD-DECISIONS.md#aid-1-2026-10-14-aid-in-camp-chooses-its-targets)).
- Verification: `opengold_cleric_channel_tests` (two creatures raised by 5, a
  checkpoint, the campaign maximum and its end after 8 hours) and
  `opengold_camp_action_tests` (the camp targets and a second cast).

## Guiding Bolt, Bane and Spare the Dying

- **Guiding Bolt** (level 1, Action, 120 feet): a spell attack for 4d6 Radiant
  (1d6 more from a level-two slot). On a hit, the next attack roll against the
  target before the end of the Cleric's next turn has Advantage; the mark is
  shown as a condition and spent by that roll. The AI casts it like its other
  damage spells.
- **Bane** (level 1, Action, 30 feet, Concentration up to 1 minute): up to three
  enemies, chosen as for Bless, make a Charisma save; on a failure they subtract
  1d4 from attack rolls and saving throws (shown as a condition). A level-two
  slot adds a creature.
- **Spare the Dying** (cantrip, Action, 15 feet): an ally at 0 Hit Points
  becomes Stable. It is a Cleric and Druid cantrip, so Blessed Warrior and
  Druidic Warrior may learn it too. The AI uses it on a dying ally.
- Cleric cantrip choices now offer Sacred Flame and Spare the Dying.
- Verification: `opengold_cleric_channel_tests` (Guiding Bolt's Advantage and
  its end, Bane's -1d4 on an attack, Spare the Dying and no second offer), the
  updated cantrip and preparation checks.

## Hold Person and Paralyzed

- **Hold Person** (level 2, Action, 60 feet, Concentration up to 1 minute): a
  Humanoid the Cleric can see makes a Wisdom save or is **Paralyzed**,
  repeating the save at the end of each of its turns. A higher slot's extra
  target waits for level-three slots. The AI casts it on a creature with no
  condition, as it does Blindness.
- **Paralyzed** (shown as a condition): Incapacitated (it can only end its turn
  and takes no Reactions) with Speed 0; it automatically fails Strength and
  Dexterity saves, attacks against it have Advantage, and a hit from within 5
  feet is a critical hit.
- Verification: `opengold_cleric_channel_tests` (Humanoids only, Paralyzed, a
  checkpoint, only ending its turn, the repeated save, a failed Dexterity save).

## Sanctuary, Warding Bond and Protection from Poison

- **Sanctuary** (level 1, Bonus Action, 30 feet, 1 minute): an enemy that makes
  an attack roll against the warded creature, or harms it with a spell, first
  makes a Wisdom save; on a failure the attack or spell is lost
  ([CLASS-9](SRD-DECISIONS.md#class-9-2026-10-05-sanctuary)). Areas are not
  stopped. It ends when the warded creature attacks or casts a spell.
- **Warding Bond** (level 2, Action, touch, 1 hour; another creature): +1 AC
  and saves and Resistance to all damage; the Cleric takes the same damage the
  creature takes. It ends when the Cleric drops to 0 Hit Points or is more than
  60 feet away (checked at each turn), or when either is bonded again. Its rings
  are not required ([CLASS-8](SRD-DECISIONS.md#class-8-2026-10-05-costly-components-that-are-not-consumed)).
- **Protection from Poison** (level 2, Action, touch, 1 hour): Resistance to
  Poison damage. The game has no Poisoned condition yet, so its other benefits
  wait for it.
- Effects that grant Resistance join the creature's own damage affinities.
- Verification: `opengold_cleric_channel_tests` (a lost attack after a failed
  Wisdom save, Warding Bond's AC, resisted and shared damage and no bond on the
  caster itself, resisted Poison damage). The generic spell table checks now
  prepare each row explicitly, so every castable row is probed.

## Resistance and Silence

- **Resistance** (cantrip, Action, touch, Concentration up to 1 minute): once
  per turn the creature takes 1d4 less damage of the chosen type, before other
  resistances apply. The A cycle offers it once per type, "Resistance: Fire" and
  so on ([CLASS-7](SRD-DECISIONS.md#class-7-2026-10-05-choosing-resistances-damage-type)),
  then click the creature. It is a Cleric and Druid cantrip, so Blessed and
  Druidic Warriors may learn it.
- **Guidance** (cantrip, Action, touch, Concentration up to 1 minute): the
  creature adds 1d4 to ability checks with the chosen skill. The A cycle offers
  "Guidance: Athletics" (escaping Ensnaring Strike, Entangle and Web) and
  "Guidance: Medicine" (stabilizing), then click the creature
  ([GUIDE-1](SRD-DECISIONS.md#guide-1-2026-10-12-guidance-restored)). The 1d4 is
  rolled with the check and logged. It is a Cleric and Druid cantrip too.
- **Silence** (level 2, Action, 120 feet, Concentration up to 10 minutes),
  aimed like Entangle: a 20-foot-radius sphere, tinted on the map. Creatures
  inside take no Thunder damage. Spells have no components
  ([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)), so
  it does not stop spellcasting. Deafened is not modeled
([DEAF-1](SRD-DECISIONS.md#deaf-1-2026-10-13-blindnessdeafness-without-deafness)).
- Combat checkpoints become `OGCOMBAT 37` (Resistance's once-per-turn use).
- Verification: `opengold_cleric_channel_tests` (eleven Resistance entries, the
  ward and its 1d4, Silence's sphere and a spell cast inside it), and the
  creation checks with three Cleric cantrips.

## Spiritual Weapon

- **Spiritual Weapon** (level 2, Bonus Action, Concentration up to 1 minute):
  click an enemy within 65 feet; the spectral force appears in the open square
  beside it nearest the Cleric (drawn as a gold disc) and makes a melee spell
  attack for 1d8 + Wisdom modifier Force. On later turns **Spiritual Weapon
  attack** (Bonus Action, no slot) moves the force beside an enemy within 25
  feet of it and attacks again. Adaptation: the force goes to the square beside
  the chosen enemy rather than being placed by hand. The force ends with the
  Cleric's Concentration. A higher slot's extra die waits for level-three slots.
  The AI strikes whenever the attack is offered.
- Verification: `opengold_cleric_channel_tests` (placement beside the target, a
  checkpoint, the later Bonus Action attack, Concentration ending it).

## Prayer of Healing

- **Prayer of Healing** (level 2, ten minutes): a camp spell, used from the Camp
  dialog's Cast / Use row ([CLASS-3](SRD-DECISIONS.md#class-3-2026-10-05-holy-water-and-casting-outside-combat)).
  The five most hurt active members it has not healed since their last Long Rest
  each regain 2d8 + Wisdom modifier (Disciple of Life adds 4). No member is
  chosen in the dialog: the target list is hidden for it, and the result line
  shows the party's total. Adaptation: the members are chosen automatically.
  A higher slot's extra die waits for level-three slots.
- Rules: `RulesModule::use_party_camp_action` handles camp actions that affect
  several members (`CampAction::whole_party`).
- Verification: `opengold_camp_action_tests` (a whole-party action, several
  members healed from one level-two slot, refused again before a Long Rest
  without change, healing again after one).
