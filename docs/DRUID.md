# Druid

Levels 1–15 of the SRD 5.2.1 Druid (pp. 41–46). Wisdom is the spellcasting
ability.

## Spellcasting, Primal Order and the Circle of the Land

- Druids prepare spells from the Druid list like Clerics: two cantrips (three
  from level 4) and 4/5/6/7 prepared spells, with full-caster slots, chosen at
  creation and on gaining a level; a Long Rest may change them. The
  implemented Druid spells through level 2 are on the list. A cantrip may be
  replaced on gaining a level ([Replacing choices](LEVEL-SWAPS.md)).
- **Primal Order** (level 1), chosen with the Training controls:
  **Magician** learns one more Druid cantrip; **Warden** trains Martial
  weapons and Medium armor.
- **Produce Flame** (cantrip, Bonus Action, 10 minutes): a flame in hand,
  shown as a condition. While it lasts, "Hurl flame" is an Action (a Magic
  action, not a casting) making a ranged spell attack for 1d8 Fire within 60
  feet; the flame stays. Casting it again replaces it.
- **Shillelagh** (cantrip, Bonus Action, 1 minute): offered only while holding
  a Club or Quarterstaff. Melee attacks with it use the spellcasting ability
  for the attack and damage rolls and a d8. "Melee attack (Force)" deals Force
  damage instead of the weapon's Bludgeoning
  ([CLOSE-1](SRD-DECISIONS.md#close-1-2026-10-29-the-last-srd-choices)).
- **Barkskin** (level 2, Bonus Action, touch, 1 hour): the creature's AC is 17
  if it was lower.
- **Circle of the Land** (level 3), the SRD's only Druid subclass: the land
  type is a Training choice at level-up. Its Circle Spells are always
  prepared: Arid (Blur, Burning Hands, Fire Bolt), Polar (Fog Cloud, Hold
  Person, Ray of Frost), Temperate (Misty Step, Shocking Grasp, Sleep) or
  Tropical (Acid Splash, Ray of Sickness, Web). After a Long Rest, not a
  Short Rest, the rest window offers the land again (rules 0.6.175); a spell
  the new land makes always prepared leaves the prepared list
  (`opengold_druid_tests`, `land_rest_checks`).
- Druidic and Wild Companion are removed (SRD-DECISIONS). Wild Shape and
  Land's Aid are below.
- Verification: `opengold_druid_tests` (Produce Flame and its hurl,
  Shillelagh's Wisdom attack bonus, Magician's cantrip and Warden's armor,
  the Arid Circle Spells and Fire Bolt, Barkskin's AC, a third cantrip at
  level 4, the level-one counts and refused preparations),
  `opengold_character_tests` (a Druid made through the ordinary creator),
  `opengold_spell_table_tests` (Druid probes).

## Flame Blade, Moonbeam, Spike Growth and Heat Metal

- **Flame Blade** (level 2, Bonus Action, Concentration): a fiery blade in hand,
  shown as a condition. While it lasts, "Flame Blade attack" is an Action (a
  Magic action) making a melee spell attack for 3d6 + the spellcasting modifier
  Fire against a creature within 5 feet. The free-hand requirement is not
  checked.
- **Moonbeam** (level 2, Concentration): an aimed 5-foot-radius beam within 120
  feet. A creature makes a Constitution save against 2d10 Radiant, half on a
  success, when the beam appears or moves onto it, when it enters the beam and
  when it ends its turn there, at most once a turn. On later turns "Move
  Moonbeam" is a Magic action that moves the beam onto a creature within 60
  feet of it. Shape-shifting is not affected yet.
- **Spike Growth** (level 2, Concentration): an aimed 20-foot-radius Sphere
  within 150 feet becomes Difficult Terrain; a creature takes 2d4 Piercing for
  each square it moves into or within it on its own move. Forced movement and
  the camouflage's Perception check are not modeled.
- **Heat Metal** (level 2, Druid and Bard, Concentration): offered only against
  a creature wearing metal armor (Medium or Heavy armor other than Hide), as
  the user decided on 2026-10-06. Player characters are judged from their
  equipment, monsters from their `equipment` rows in combat.rules
  ([creature equipment](CREATURE-EQUIPMENT.md)): the Slums goblin and orc
  leaders wear metal. The armor deals 2d8 Fire; a failed Constitution save gives
  Disadvantage on attack rolls until the start of the caster's next turn (worn
  armor cannot be dropped). "Heat Metal again" repeats it as a Bonus Action on
  the caster's later turns within 60 feet. Ability checks are not affected.
- Verification: `opengold_druid_tests` (the blade's Bonus Action and attack,
  the beam's saves on appearing, entering, ending a turn and moving, Difficult
  Terrain and spike damage, Heat Metal on a character in Chain Mail but not on
  a monster, and its later Bonus Action).

## Wild Shape and Land's Aid

- **Wild Shape** (level 2): two uses, one back on a Short Rest and all on a
  Long Rest, kept in the store Channel Divinity and Rage use. The Bonus Action
  dropdown offers "Wild Shape: Wolf", "...: Boar", "...: Giant Lizard" and
  "...: Giant Snake", and "Leave Wild Shape" while in a form (CLASS-10: the
  Beasts with game art that the SRD allows at levels 2-4; every Druid knows
  all four).
- In a form the Druid keeps its Hit Points, mental scores, proficiencies and
  features and gains Temporary Hit Points equal to its Druid level. The
  Beast's AC, Speed, size, Strength, Dexterity, physical saves and attack
  replace the Druid's; equipment merges into the form (so Heat Metal finds no
  armor); no spells are cast, but Concentration continues. Taking or leaving a
  form mid-turn changes the movement left by the difference in Speed; movement
  already spent stays spent.
  - Wolf: AC 12, Speed 40, Bite +4 for 1d6 + 2 Piercing; Pack Tactics, and a
    hit knocks a Medium or smaller creature Prone.
  - Boar: AC 11, Speed 40, Gore +3 for 1d6 + 1 Piercing; Bloodied Fury
    (Advantage at half its Hit Points or fewer). Charge (rules 0.6.171): a
    Gore hit right after moving 20 feet or more in a straight line, each step
    5 feet closer to a Medium or smaller target, deals 1d6 more Piercing
    damage and knocks the target Prone. Any command between the run and the
    Gore, other than moving, ends the run; combat checkpoints keep it
    (`OGCOMBAT 51`).
  - Giant Lizard (Large): AC 12, Speed 40, Bite +4 for 1d8 + 2 Piercing.
  - Giant Snake (the SRD Constrictor Snake, Large): AC 13, Speed 30, Bite +4
    for 1d8 + 2 Piercing. Constrict is not offered: grappling is not planned
    (SCOPE-2), and its Swim Speed goes unused.
  - Every form's original art is wide, so the form fills two squares side by
    side, its top-left one the Druid's square
    ([FOOT-1](SRD-DECISIONS.md#foot-1-2026-10-26-creatures-fill-the-squares-their-art-fills));
    without room there, the Druid takes the nearest space with room. A move
    puts the form's left square on the square chosen.
  - A form wears the Combat Sprite Catalog's new art of the creature drawn
    with its original icon (the wolf's, the wild boar's), as that creature
    stands on the battlefield; without it, the original icon
    ([#274](https://github.com/stdarg/OpenGoldBox/issues/274)). Summoned
    creatures and Animate Objects' objects do the same.
- The form ends on "Leave Wild Shape", another Wild Shape, 0 Hit Points, being
  Incapacitated at the start of the Druid's turn, fleeing the battlefield, or
  when the fight ends
  ([WILD-1](SRD-DECISIONS.md#wild-1-2026-10-13-wild-shape-lasts-until-the-fight-ends),
  a departure from the SRD's hours; rules 0.6.168).
- The combat view draws the Druid as the Beast's original combat icon (the
  art research's candidates: Wolf CPIC4 106, Wild Boar CPIC6 120, Giant
  Lizard CPIC8 59, Giant Snake CPIC5 60).
- **Land's Aid** (Circle of the Land, level 3): a Magic action spending a use
  of Wild Shape, aimed at a point within 60 feet. Each enemy in the 10-foot
  Sphere makes a Constitution save against the spell save DC, taking 2d6
  Necrotic, half on a success; the most wounded ally in it regains 2d6 Hit
  Points. The enemies and the ally are chosen automatically.
- Verification: `opengold_druid_tests` (each form's AC, Speed and attack bonus
  with the Druid's Hit Points kept, the Wolf's Temporary HP, no spells, a
  checkpoint, the bite and its Prone, Concentration on Faerie Fire continuing,
  Heat Metal finding no armor on a shaped Druid, leaving the form; the form
  ending at 0 Hit Points, on a turn begun Incapacitated and when it lapses; and
  Land's Aid's thorns, healing and spent use). A form carried into a new fight
  applies from its start (rules 0.6.167; it used to wait for the Druid's turn).

## Levels 5–15

The Druid advances to level 15, the cap ([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

- **Spellcasting** (SRD 5.2.1 p. 41): a full caster's slots; prepared spells
  9, 10, 11, 12, 14 and 15 at levels 5–10; a fourth cantrip at 10. The kept
  Druid spells open with their slots: at 5 Call Lightning, Conjure Animals,
  Dispel Magic, Plant Growth, Protection from Energy, Revivify, Sleet Storm and
  Wind Wall; at 7 Blight, Charm Monster, Confusion, Conjure Minor Elementals,
  Conjure Woodland Beings, Dominate Beast, Fire Shield, Freedom of Movement,
  Giant Insect, Ice Storm, Polymorph, Stoneskin and Wall of Fire; at 9
  Antilife Shell, Cone of Cold, Conjure Elemental, Contagion, Greater
  Restoration, Insect Plague, Mass Cure Wounds, Reincarnate and Wall of
  Stone; the cut spells stay cut.
- **Circle Spells:** arid Fireball (5), Blight (7), Wall of Stone (9); polar
  Sleet Storm, Ice Storm, Cone of Cold; temperate Lightning Bolt, Freedom of
  Movement (Tree Stride is cut); tropical Stinking Cloud, Polymorph, Insect
  Plague, always prepared.
- **Call Lightning** (Action, Concentration up to 10 minutes): a storm cloud
  forms over the Druid; a bolt strikes a point within 60 feet of the square
  under the cloud's centre: each creature within 5 feet of it makes a
  Dexterity save against 3d10 Lightning (1d10 more per slot level above 3),
  half on a success. **Call Lightning: another bolt** (a Magic action) calls
  one more under the cloud, wherever the Druid has gone. There is no weather,
  so the storm's extra 1d10 is never gained.
- **Giant Insect** (Action, Concentration up to 10 minutes): one entry each
  for a centipede, a spider and a wasp. It joins the fight right after the
  Druid, played by the player (LEVELS-4), drawn as the game's Large
  Scorpion: AC 11 + the slot level, 30 Hit Points and 10 more per slot level
  above 4, half the slot level in Poison Jabs (reach 10 feet, the Druid's
  spell attack, 1d6 + 3 + the slot level Piercing and 1d4 Poison); the spider
  also has Web Bolt (60 feet, 1d10 + 3 + the slot level Bludgeoning). The Web
  Bolt's Speed 0, the centipede's Venomous Spew and the wasp's flight are
  left out. It vanishes at 0 Hit Points or with the spell.
- **Antilife Shell** (Action, Concentration up to 1 hour): a 10-foot
  Emanation around the Druid. No creature but the Druid, a Construct or an
  Undead walks across its edge, in or out, and no such creature's melee
  attack reaches across it; spells and ranged attacks pass. If the Druid
  moves so that a creature passes through it, the spell ends.
- **Wild Resurgence** (5): with no Wild Shape use left, **Wild Resurgence: a
  level-N slot for Wild Shape**, no action, once a turn, trades a slot for a
  use; once per Long Rest, **Wild Resurgence: Wild Shape for a level-1 slot**
  trades a use for a spent level-1 slot.
- **Natural Recovery** (Land 6): once per Long Rest, any levelled Circle
  Spell is also offered as "<spell> (Natural Recovery)", cast without a slot;
  after a Short Rest, the rest's recovery choices recover spent slots of
  levels 1–5 adding up to half the Druid level, rounded up, as Arcane Recovery
  does for a Wizard.
- **Wild Shape** has three uses from 6, and from 8 the **Tiger** (CR 1; the
  game's Tiger art): AC 13, 30 Hit Points, Speed 40, Rend +5 for 2d6 + 3
  Slashing, knocking a Large or smaller creature Prone. Its Nimble Escape is
  left out; no other CR 1 Beast has art.
- **Elemental Fury** (7): a level-up choice. **Potent Spellcasting** adds the
  Wisdom modifier to a Druid cantrip's damage; **Primal Strike**: once on each
  of the Druid's turns, right after a weapon or Beast form hit, **Primal Strike
  (Cold)**, **(Fire)**, **(Lightning)** or **(Thunder)** adds 1d8 of it,
  doubled on a critical hit.
- **Ability Score Improvement** (8).
- **Nature's Ward** (Land 10): the Druid cannot be Poisoned (any Poisoned
  effect falls away at once), and resists its land's element: Fire (arid),
  Cold (polar), Lightning (temperate) or Poison (tropical).
- **Land's Aid** deals and heals 3d6 from 10.
- **Level-6 to level-8 spells:** at 11 Conjure Fey, Flesh to Stone, Heal,
  Heroes' Feast, Sunbeam and Wall of Thorns; at 13 Fire Storm, Regenerate and
  Reverse Gravity; at 15 Befuddlement, Earthquake, Incendiary Cloud,
  Sunburst and Tsunami (the shared ones work as for the
  [Cleric](CLERIC.md#levels-515) and the [Wizard](WIZARD.md#levels-515)).
- **Animal Shapes** (Action, 30 feet): one entry per Beast form (Wolf, Boar,
  Giant Lizard, Giant Snake, Tiger), then the willing creatures are chosen,
  the Druid among them if it likes. Each takes the form, keeping its own Hit
  Points, with Temporary Hit Points equal to the form's, and casts no spells.
  It lasts the fight (LEVELS-3); **Leave the animal shape** (a Bonus Action)
  ends it sooner, and the Temporary Hit Points go with it. Changing forms with a
  later Magic action is not offered; the Giant Scorpion, the one CR 2–4 Beast
  with art, is left out, its claws and poison sting not modelled.
- **Conjure Fey** (Action, Concentration up to 10 minutes): a fey spirit
  appears beside a creature and makes a melee spell attack for 3d12 + the
  Wisdom modifier Psychic (1d12 more per slot level above 6); a hit leaves the
  creature Frightened of the Druid until the start of the Druid's next turn.
  On later turns **Fey spirit attack** (a Bonus Action) teleports it up to 30
  feet and attacks again. It is drawn as Spiritual Weapon's force is.
- **Wall of Thorns** (Action, Concentration up to 10 minutes): a wall 60 feet
  long in any of four directions or a ring 20 feet across (LEVELS-3); it hides
  what is in it, its squares cost 4 feet of movement a foot, and as it rises
  each creature in it makes a Dexterity save against 7d8 Piercing, half on a
  success. Entering it or ending a turn in it is a Dexterity save against
  7d8 Slashing, once a turn (1d8 more of each per slot level above 6).
- **Reincarnate** (camp, an hour's casting): from the Camp dialog's Cast /
  Use list, on a member dead up to ten days, for Raise Dead's offering of
  300 gp times its level (LEVELS-5). A d10 names the new body's species (a 1
  rolls again: Dragonborn, Dwarf, Elf, Gnome, Goliath, Halfling, Human, Orc
  or Tiefling). The creation pages open for it: the race page, with only that
  species, the name and the gender fixed, for the species' choices; a
  Human's Versatile Magic Initiate spells, when chosen; the combat icon; the
  portrait, starting on a face of the new species; then the sheet, whose
  **Reincarnate** casts the spell. **Cancel** goes back to town with nothing
  spent; the dice are only read until the spell is cast, so casting again
  forms the same species. The member returns with all its Hit Points and no
  penalty; class, level, scores, background, skills and items stay, and its
  levels are made again on the new body. A skill or cantrip it already has
  is not offered among the species' choices, and a skill of the old species
  that it built Expertise on stays ("Kept from the former body").
- **Tsunami** (Action, Concentration up to 6 rounds): a wall of water across
  the field in any of four directions, 10 feet thick (LEVELS-5: the SRD's
  minute of casting and 50 feet would not fit a fight or a battle map); its
  second row lies on the side away from the Druid. As it rises each creature
  in it makes a Strength save against 6d10 Bludgeoning, half on a success. At
  the start of each of the Druid's later turns it moves 50 feet away from
  the Druid, carrying the creatures in it or in its way, each of which makes
  a Strength save or takes 5d10 Bludgeoning; each move deals 1d10 less, and
  after the 1d10 move it subsides at the end of the turn. Carried into an
  obstacle, a creature takes the push damage of PUSH-1; the field's edge
  stops it without harm, and a wave that leaves the field ends. A creature
  in the water moves only after a Strength (Athletics) check against the
  spell's DC, once a turn; a failure holds it for the turn.
- **Ability Score Improvement** (12).
- **Nature's Sanctuary** (Land 14): **Nature's Sanctuary**, a Magic action
  and a Wild Shape use, raises a 15-foot cube of spectral trees within 120
  feet for a minute: allies among them share the Druid's Nature's Ward
  Resistance; **Move Nature's Sanctuary** (a Bonus Action) moves it up to 60
  feet. Its Half Cover is cut (COVER-1).
- **Land's Aid** deals and heals 4d6 from 14.
- **Improved Elemental Fury** (15): Primal Strike deals 2d8; Potent
  Spellcasting's Druid cantrips with a range of 10 feet or more reach 300 feet
  farther, across any battle map (#185).
- Verification: `opengold_druid_levels_tests` (the campaign to 15,
  prepared counts, the features' grants, level 5's description, a level-10
  round trip, Fireball as an arid Circle Spell and Natural Recovery's free
  cast once, Natural Recovery's Short Rest choice, Wild Resurgence's slot,
  Primal Strike, the Tiger from 8, Call Lightning and its second bolt, Giant
  Insect, Antilife Shell keeping a brute out, Conjure Fey's two attacks, Wall
  of Thorns, Nature's Sanctuary and its move, Animal Shapes' Tiger and its
  end).
