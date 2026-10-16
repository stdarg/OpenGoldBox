# Wizard

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 77–80. The spellbook, preparation and cantrip replacement are
described in [Wizard spell choices](WIZARD-SPELL-CHOICES.md).

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting, Ritual Adept, Arcane Recovery | Delivered earlier (Ritual Adept removed by DM-1) |
| 2 | Scholar | Delivered earlier ([Scholar](SCHOLAR.md)) |
| 3 | Wizard Subclass (Evoker) | Delivered (rules 0.6.93); Evocation Savant removed by the 2026-09-30 simplification |
| 4 | Ability Score Improvement | Delivered earlier |
| 5 | Memorize Spell, level-3 spells | Delivered (rules 0.6.217) |
| 6 | Sculpt Spells (Evoker) | Delivered (rules 0.6.217; before, at level 3) |
| 7 | Level-4 spells | Delivered (rules 0.6.218) |
| 8 | Ability Score Improvement | Delivered (rules 0.6.218) |
| 9 | Level-5 spells | Delivered (rules 0.6.220) |
| 10 | Empowered Evocation (Evoker), fifth cantrip | Delivered (rules 0.6.220) |
| 11 | Level-6 spells | Delivered (rules 0.6.221) |
| 12 | Ability Score Improvement | Delivered (rules 0.6.221) |

## Levels 5–15

The Wizard advances to level 12 for now; level 13 waits for its level-7 spells
([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

- **Spellcasting** (SRD 5.2.1 p. 77): level-3 slots from 5 (two, three from
  6), level-4 slots from 7 (one, two from 8), level-5 slots from 9 (one, two
  from 10); prepared spells 9, 10, 11, 12, 14 and 15 at levels 5–10; the
  spellbook gains two spells per level, of
  any level the Wizard has slots for (half its level, rounded up); a fifth
  cantrip at 10.
- **Level-3 spells** (5): Animate Dead, Bestow Curse, Blink, Counterspell,
  Dispel Magic, Fear, Fireball, Haste, Hypnotic Pattern, Lightning Bolt, Magic
  Circle, Protection from Energy, Remove Curse, Sleet Storm, Stinking Cloud,
  Tiny Hut and Vampiric Touch (below; the shared ones work as for the
  [Cleric](CLERIC.md#the-new-level-3-spells)). Slow waits for the decision on
  how its spell failure becomes a save (LEVELS-1). Clairvoyance, Fly, Gaseous
  Form, Glyph of Warding, Major Image, Nondetection, Phantom Steed, Sending,
  Speak with Dead, Tongues and Water Breathing are cut.
- **Memorize Spell** (5): when a Short Rest ends (after its Hit Dice), the
  rest's spell window opens, titled Short Rest: the Wizard may change one
  prepared spell for another in its spellbook. Campaign saves keep the
  pending window (format 31).
- **Arcane Recovery** restores any expended slots of levels 1 to 5 whose
  levels add up to half the Wizard level, rounded up: at 5 and 6 one level-3
  slot, or a level-2 and a level-1, and so on. The choices beyond the
  original three read "Spell slots of levels 3" or "… of levels 2, 1".
- **Sculpt Spells** moves to level 6, where the SRD puts it; it was given at
  level 3 before. Potent Cantrip stays at 3.
- **Level-4 spells** (7): Banishment, Black Tentacles, Blight, Charm Monster,
  Confusion, Dimension Door, Fire Shield, Greater Invisibility, Ice Storm,
  Phantasmal Killer, Polymorph, Resilient Sphere, Stoneskin, Vitriolic Sphere
  and Wall of Fire, and Conjure Minor Elementals and Faithful Hound (LEVELS-4)
  (below). Arcane Eye, Control Water, Divination, Fabricate,
  Hallucinatory Terrain, Locate Creature, Private Sanctum, Secret Chest and
  Stone Shape are cut.
- **Ability Score Improvement** (8): a feat or ability points.
- **Level-5 spells** (9): Animate Objects, Arcane Hand, Cloudkill, Cone of
  Cold, Conjure Elemental, Dominate Person, Hold Monster, Mislead, Summon
  Dragon, Telekinesis, Wall of Force and Wall of Stone (below). Contact Other
  Plane, Creation, Dream, Geas, Legend Lore, Modify Memory, Passwall, Planar
  Binding, Scrying, Seeming, Telepathic Bond and Teleportation Circle are cut.
- **Empowered Evocation** (Evoker 10): the Intelligence modifier is added to
  the damage roll of a Wizard Evocation spell (Fire Bolt, Fireball, Cone of
  Cold…). Magic Missile's darts, each its own roll, take none.
- **A fifth cantrip** (10).
- **Level-6 spells** (11): one level-6 slot; prepared spells 16 at 11 and 12.
  Chain Lightning, Circle of Death, Contingency, Create Undead, Disintegrate,
  Eyebite, Flesh to Stone, Freezing Sphere, Globe of Invulnerability,
  Irresistible Dance, Magic Jar, Sunbeam, True Seeing and Wall of Ice (below;
  the Cleric's are described [there](CLERIC.md#the-level-6-spells)). Arcane
  Gate, Drawmij's Instant Summons, Find the Path, Guards and Wards, Move
  Earth, Programmed Illusion and the like are cut.
- **Ability Score Improvement** (12): a feat or ability points.
- Verification: `opengold_wizard_levels_tests` (the campaign's stop at 6,
  prepared counts, level-3 spells in the level-5 spellbook choice, slots, a
  campaign round trip, Memorize Spell's one change and the campaign's Short
  Rest window, Arcane Recovery of a level-3 slot, each level-3 and level-4
  spell, each level-5 spell and its summoned creatures, Sculpt Spells at 6
  and not 5, Empowered Evocation and the fifth cantrip at 10) and
  `opengold_wizard_spell_tests`.

### The level-3 spells

- **Fireball** (Action, 150 feet): a 20-foot-radius Sphere; Dexterity save,
  8d6 Fire, half on a success; 1d6 more for each slot level above 3.
- **Lightning Bolt** (Action): a 100-foot, 5-foot-wide Line from the Wizard;
  Dexterity save, 8d6 Lightning, half on a success; 1d6 more per slot level.
- **Counterspell** (Reaction, 60 feet): when a foe the Wizard can see within
  60 feet casts a spell, the Wizard is asked (React/Decline, "Cast
  Counterspell"); it spends a level-3 slot or higher and the Reaction, and
  the caster makes a Constitution save against the Wizard's spell DC. On a
  failure the spell dissipates: the caster's action or Bonus Action is spent,
  its slot is not. Sunbeam's later Lines and Dragon's Breath are not castings.
- **Haste** (Action, 30 feet, Concentration up to 1 minute): a willing ally
  the Wizard can see doubles its Speed, gains +2 AC and Advantage on
  Dexterity saves, and on each of its turns a second action for one weapon
  attack, Dash, Disengage, Hide or taking off a shield. A Dash, Disengage or
  Hide uses that action first; an attack uses it once the Action is spent,
  so Extra Attack stays with the Action. When Haste ends, a wave of lethargy
  leaves the creature Incapacitated with a Speed of 0 until the end of its
  next turn.
- **Hypnotic Pattern** (Action, 120 feet, Concentration up to 1 minute): a
  30-foot Cube; each creature in it that can see makes a Wisdom save or is
  Charmed, Incapacitated and at Speed 0 ("Charmed (Hypnotic Pattern)") until
  it takes damage or anyone within 5 feet spends an action on **Shake
  awake**.
- **Fear** (Action, Concentration up to 1 minute): a 30-foot Cone; each
  creature in it makes a Wisdom save or is Frightened ("Frightened (Fear)").
  While it sees the Wizard it can only flee, as Command's Flee does: moves
  away, then Dash. Out of the Wizard's sight at the end of its turn it saves
  again to end it. Dropping what it holds is left out.
- **Sleet Storm** (Action, 150 feet, Concentration up to 1 minute): a
  20-foot-radius area, Heavily Obscured and Difficult Terrain. A creature
  that enters it or starts its turn there makes a Dexterity save, once a
  turn, or falls Prone and loses Concentration.
- **Stinking Cloud** (Action, 90 feet, Concentration up to 1 minute): a
  20-foot-radius Sphere, Heavily Obscured. A creature that starts its turn
  in it makes a Constitution save or is Poisoned and takes no action or
  Bonus Action that turn.
- **Vampiric Touch** (Action, Concentration up to 1 minute): a melee spell
  attack for 3d6 Necrotic (1d6 more per slot level above 3); the Wizard
  regains half the damage dealt. On later turns **Vampiric Touch** (a Magic
  action) touches again.
- **Blink** (Action, 1 minute): at the end of each of the Wizard's turns, a
  d6; on 4–6 it slips into the Ethereal Plane, out of reach of everything,
  and returns at the start of its next turn to its square, or the nearest
  free one.
- **Tiny Hut** (camp, the whole party): for 8 hours no rest is interrupted
  (LEVELS-1).

### The level-6 spells

- **Chain Lightning** (Action, 150 feet): choose a creature, then up to three
  more within 30 feet of it (one more per slot level above 6); each makes a
  Dexterity save against one roll of 10d8 Lightning, half on a success. A
  chosen creature farther from the first is spared.
- **Circle of Death** (Action, 150 feet): a 60-foot-radius Sphere;
  Constitution save, 8d8 Necrotic, half on a success; 2d8 more per slot level
  above 6.
- **Contingency** (camp): one entry per spell and trigger, "Contingency: False
  Life (when a fight starts)"; the triggers are a fight starting, dropping to
  half Hit Points and dropping to 0 (LEVELS-3). It spends a level-6 slot and
  one for the stored spell, any spell of level 5 or lower the Wizard can cast
  on itself with an action and no further choice (False Life, Mage Armor,
  Blur, Haste, Stoneskin, Greater Invisibility, Cure Wounds…). The spell takes
  effect on the Wizard alone, once, and lasts its usual time; a new
  Contingency replaces an old one. It keeps for 10 days.
- **Disintegrate** (Action, 60 feet): a Dexterity save or 10d6 + 40 Force (3d6
  more per slot level above 6); a creature it drops to 0 Hit Points turns to
  dust, dead at once, and only Resurrection revives it (LEVELS-3): Revivify,
  Raise Dead and Animate Dead cannot. Objects and a Wall of Force have
  nothing to target it with yet.
- **Eyebite** (Action, Concentration up to 1 minute): one entry per effect on
  a creature within 60 feet: **asleep** (Sleep's Unconscious, ended by damage
  or a shake), **panicked** (Frightened, fleeing the Wizard with the Dash
  action until it stands 60 feet away out of the Wizard's sight) or
  **sickened** (Poisoned), on a failed Wisdom save. On later turns a Magic
  action targets another creature; one that saved is not offered again.
- **Flesh to Stone** (Action, 60 feet, Concentration up to 1 minute): a
  Constitution save or Restrained; a success leaves Speed 0 until the
  Wizard's next turn, and a Construct always succeeds. The Restrained
  creature saves again at the end of each of its turns: three successes end
  the spell, three failures turn it to stone (Petrified: Incapacitated, Speed
  0, Resistance to all damage, failing Strength and Dexterity saves, attacked
  with Advantage). Held for the full minute, the stone lasts until **Greater
  Restoration: Petrified**, and a foe of stone counts as out of the fight.
- **Freezing Sphere** (Action, 300 feet): a 60-foot-radius Sphere;
  Constitution save, 10d6 Cold, half on a success; 1d6 more per slot level
  above 6. Freezing water and keeping the globe for later are left out.
- **Globe of Invulnerability** (Action, Concentration up to 1 minute): a still
  10-foot sphere around where the Wizard stands. A spell of level 5 or lower
  (one more per slot level above 6) cast from outside is not offered against
  a creature inside, and its area spares those inside.
- **Irresistible Dance** (Action, 30 feet, Concentration up to 1 minute): a
  failed Wisdom save sets the creature dancing: Charmed, Speed 0,
  Disadvantage on Dexterity saves and attack rolls, attacked with Advantage;
  **Try to stop dancing** (its action) repeats the save. A success leaves it
  dancing in place (Speed 0) until the end of its next turn.
- **Magic Jar** (Action, 100 feet): a Humanoid that fails a Charisma save is
  possessed: the player controls it as Dominate Person's creature, while the
  Wizard's body lies helpless (Incapacitated, Speed 0) (LEVELS-3). Damage
  lets the host save again, as Dominate does; **Return to your body** (the
  host's Magic action) ends it. If the host dies, the Wizard makes a Charisma
  save against its own spell DC: its soul returns on a success, and it dies
  on a failure. It ends with the fight.
- **Wall of Ice** (Action, 120 feet, Concentration up to 10 minutes): a wall
  100 feet long in any of the four directions, or a ring 20 feet across
  (LEVELS-3), drawn as obstacles; it blocks movement and effects but not
  sight. A creature in its way is pushed to the nearest free square and makes
  a Dexterity save against 10d6 Cold (2d6 more per slot level above 6), half
  on a success. Its sections cannot be attacked, so its frigid air never
  forms.

### Summoned creatures (LEVELS-4)

Summon Dragon's spirit, Animate Objects' objects and Mislead's double join the
fight as new combatants on the Wizard's side, right after the Wizard in the
turn order, controlled by the player. Their attacks use the Wizard's spell
attack bonus. They vanish when the Wizard's Concentration ends, when they drop
to 0 Hit Points, and with the fight. The Draconic Spirit wears the icon of
Tyranthraxus, the original's dragon; an Animated Object, the Skeleton's;
Mislead's double, the Wizard's own.

### The level-5 spells

- **Animate Objects** (Action, Concentration up to 1 minute): as many Medium
  objects as the Intelligence modifier rise in the free squares nearest the
  Wizard (LEVELS-1): AC 15, 10 Hit Points, a Slam for 1d4 + 3 Force (1d4 more
  per slot level above 5).
- **Arcane Hand** (Action, 120 feet, Concentration up to 1 minute): a hand of
  force in a free square. As it appears, and later with a Bonus Action, it
  moves up to 60 feet beside a creature and **Clenched Fist** (a melee spell
  attack, 5d8 Force, 2d8 more per slot level above 5) or **Forceful Hand** (a
  Strength save or a push of 5 feet plus five times the Intelligence
  modifier, away from the hand). The hand cannot be attacked: the game has no
  attacks on objects. Grasping Hand and Interposing Hand stay cut.
- **Cloudkill** (Action, 120 feet, Concentration up to 10 minutes): a
  20-foot-radius fog, Heavily Obscured. A creature in it as it appears, that
  it drifts over, that enters it or ends its turn there makes a Constitution
  save once a turn against 5d8 Poison, half on a success (1d8 more per slot
  level above 5). It drifts 10 feet away from the Wizard at the start of each
  of the Wizard's turns.
- **Cone of Cold** (Action): a 60-foot Cone; Constitution save, 8d8 Cold, half
  on a success; 1d8 more per slot level above 5.
- **Conjure Elemental** (Action, 60 feet, Concentration up to 10 minutes): one
  entry per element: air (Lightning), earth (Thunder), fire (Fire) or water
  (Cold). A Large spirit, two squares by two, drawn as spirits. While it
  holds no one, an enemy that enters its space or starts its turn within 5
  feet of it makes a Dexterity save or takes 8d8 of its type and is
  Restrained; at the start of each of its turns the held creature saves
  again, taking 4d8 on a failure and breaking free on a success. 1d8 more per
  slot level above 5.
- **Dominate Person** (Action, 60 feet, Concentration up to 1 minute):
  Dominate Beast's control (LEVELS-1) for a Humanoid.
- **Hold Monster** (Action, 90 feet, Concentration up to 1 minute): Hold Person
  for any creature; one more creature per slot level above 5.
- **Mislead** (Action, Concentration up to 1 hour): the Wizard turns Invisible
  (until it attacks or casts) and an illusory double appears in the nearest
  free square. The double takes no turns and nothing harms it; a foe's attack
  on it passes through, and foes ignore it after (LEVELS-1). **Move the
  double** (a Magic action) moves it up to twice the Wizard's Speed, beside a
  creature the Wizard can see.
- **Summon Dragon** (Action, 60 feet, Concentration up to 1 hour): one entry
  per type, Acid, Cold, Fire, Lightning or Poison, which is both the spirit's
  breath and the Resistance the Wizard shares. The Draconic Spirit (SRD
  5.2.1 p. 166): AC 14 + the spell's level, 50 Hit Points and 10 more per
  level above 5, Resistance to Acid, Cold, Fire, Lightning and Poison, and as
  many Rends (1d6 + 4 + the level, reach 10 feet) as half the spell's level;
  **Breath Weapon**, once on each of its turns besides its Rends, a 30-foot
  Cone for 2d6, a Dexterity save against the Wizard's spell DC halving it.
  The SRD lets the breath's type and the shared Resistance be chosen apart.
- **Telekinesis** (Action, Concentration up to 10 minutes): pick a Huge or
  smaller creature within 60 feet, then the square it should go to, up to 30
  feet away; a failed Strength save moves it there in a straight line and
  leaves it Restrained until the end of the Wizard's next turn. A wall or a
  creature in the way stops it, with PUSH-1's damage for the distance it
  could not cover. Later, **Telekinesis** (a Magic action, no slot) seizes it
  or another creature again. Objects are left out (LEVELS-1).
- **Wall of Force** (Action, 120 feet, Concentration up to 10 minutes): a wall
  100 feet long in any of the four directions, or a ring 20 feet across
  (LEVELS-3), drawn as obstacles. Nothing passes through it: no movement,
  attack, spell or area. A creature in its line is pushed to the nearest free
  square beside it.
- **Wall of Stone** (Action, 120 feet, Concentration up to 10 minutes): a
  stone wall 100 feet long in any of the four directions; it blocks
  movement, sight and every effect, and leaves out squares a creature stands
  in. Its panels cannot be attacked, and it never becomes permanent: fights
  end long before its 10 minutes.

### The level-4 spells

- **Conjure Minor Elementals** (Action, Concentration up to 10 minutes): one
  entry per type, Acid, Cold, Fire or Lightning, chosen at the casting
  (LEVELS-4); while it lasts, each of the Wizard's hits on a creature within
  15 feet of it deals 2d8 more of that type (1d8 more per slot level above
  4), and the ground there is Difficult Terrain for its enemies.
- **Faithful Hound** (Action, 30 feet, 8 hours): a phantom watchdog in a free
  square, drawn as a guardian, that nothing harms. At the start of each of
  the Wizard's turns it bites the nearest enemy within 5 feet: a Dexterity
  save against 4d8 Force. **Move Faithful Hound** (a Magic action) moves it
  up to 50 feet, beside an enemy the Wizard can see (LEVELS-4). It lasts the
  fight; its watch over a camp has nothing to act on.
- **Black Tentacles** (Action, 90 feet, Concentration up to 1 minute): a
  20-foot square, Difficult Terrain. A creature in it when it appears, that
  enters it or ends its turn there makes a Strength save, once a turn, or
  takes 3d6 Bludgeoning and is Restrained ("Restrained (Black Tentacles)");
  **Escape the tentacles** spends an action on an Athletics check against the
  spell's DC, as Entangle's vines do.
- **Blight** (Action, 30 feet): a Constitution save against 8d8 Necrotic,
  half on a success; a Plant fails it; 1d8 more per slot level above 4. The
  withering of plants has nothing to act on.
- **Charm Monster** (Action, 30 feet): Charm Person for any creature, one more
  for each slot level above 4.
- **Confusion** (Action, 90 feet, Concentration up to 1 minute): a
  10-foot-radius Sphere (5 feet more per slot level above 4); a Wisdom save
  or Confused: no Bonus Actions or Reactions, and a d10 at the start of each
  turn: on 1 it wanders north, east, south or west as far as its movement
  goes (provoking no Opportunity Attacks); on 2–6 it does nothing; on 7–8 it
  makes one melee attack against a random creature within reach, if any; on
  9–10 it acts as it likes. It saves again at the end of each of its turns.
- **Dimension Door** (Action, 500 feet): the Wizard teleports to any free
  square on the battlefield, seen or not; **Dimension Door with** an ally
  within 5 feet takes that ally too, to the nearest free square. With two
  allies beside it, a click starts the entry of the first; the A cycle lists
  both.
- **Fire Shield** (Action, 10 minutes): **(warm)** gives Resistance to Cold,
  **(chill)** to Fire; a creature within 5 feet that hits the Wizard in melee
  takes 2d8 Fire from the warm shield or 2d8 Cold from the chill one. Its
  light has nothing to act on.
- **Greater Invisibility** (Action, touch, Concentration up to 1 minute): the
  creature is Invisible, and attacking or casting does not end it.
- **Ice Storm** (Action, 300 feet): a 20-foot-radius area; Dexterity save,
  2d10 Bludgeoning and 4d6 Cold, half on a success (1d10 more Bludgeoning per
  slot level above 4); the hail is Difficult Terrain until the end of the
  Wizard's next turn.
- **Phantasmal Killer** (Action, 120 feet, Concentration up to 1 minute): a
  Wisdom save against 4d10 Psychic (1d10 more per slot level above 4); on a
  failure the creature has Disadvantage on attack rolls and saves again at
  the end of each of its turns, taking the damage again on a failure; a
  success, at the casting or later, ends the spell.
- **Polymorph** (Action, 60 feet, Concentration up to 1 hour): a creature the
  Wizard sees makes a Wisdom save or becomes one of the game's Beast forms,
  Wild Shape's (CLASS-10): **Polymorph: Wolf**, Boar, Giant Lizard or Giant
  Snake. It takes the Beast's statistics and attacks, cannot cast spells, and
  gains Temporary Hit Points equal to the Beast's Hit Points (11, 13, 19 and
  13). The spell ends when those are gone or Concentration ends. The forms
  are all Challenge Rating 1/4; the game's stat blocks carry no Challenge
  Rating, so the SRD's limit to a form no stronger than the creature is not
  checked.
- **Resilient Sphere** (Action, 30 feet, Concentration up to 1 minute): a
  Large creature or smaller makes a Dexterity save or is sealed in a sphere.
  Nothing reaches it from outside (attacks, spells, areas), and it reaches
  nothing outside; on its turn it only rolls the sphere, at half Speed, which
  takes its action.
- **Vitriolic Sphere** (Action, 150 feet): a 20-foot-radius Sphere; Dexterity
  save, 10d4 Acid and 5d4 more at the end of its next turn, half the first
  only on a success; 2d4 more initial Acid per slot level above 4.
- **Wall of Fire** (Action, 120 feet, Concentration up to 1 minute): a wall 60
  feet long in any of the four directions, or a ring 20 feet across
  (LEVELS-3), drawn as a wall; each entry names the side that burns
  ("Wall of Fire (vertical, east side burning)", "… (ring, inner side
  burning)"). Each creature in it as it rises makes a Dexterity save against
  5d8 Fire, half on a success. A creature that enters the wall (once a turn)
  or ends its turn in it or within 10 feet on the burning side takes 5d8 Fire.
  1d8 more per slot level above 4. The wall hides the creatures in it, as fog
  does; it does not block sight beyond it.

## Spells

- The Wizard list in the game: the cantrips Chill Touch, Fire Bolt, Poison
  Spray, Ray of Frost and Shocking Grasp; level 1 Fog Cloud, Longstrider, Magic
  Missile and Protection from Evil and Good; level 2 Blindness, Hold Person and
  Scorching Ray. The shared spells work as described for the
  [Paladin](PALADIN.md#buffs-and-concentration), [Ranger](RANGER.md) and
  [Cleric](CLERIC.md#hold-person-and-paralyzed).
- Level-up and rest defaults learn and prepare the first available spells, so a
  default Wizard now learns more than Magic Missile.
- The other CRPG-relevant Wizard spells come in later increments.

## Burning Hands, Thunderwave and Shatter

- Area spells are aimed as recorded in
  [CLASS-5](SRD-DECISIONS.md#class-5-2026-10-05-aiming-area-spells). Cones and
  cubes extend from the caster toward the aimed square.
- **Burning Hands** (level 1, Action): a 15-foot cone (seven squares, those
  within 15 feet and about 30 degrees of the aim); each creature in it makes a
  Dexterity save, 3d6 Fire damage, half on a success (+1d6 from a level-two
  slot).
- **Thunderwave** (level 1, Action): a 15-foot cube with a face against the
  Wizard, on the aimed side (diagonals take the corner); Constitution save, 2d8
  Thunder damage, half on a success, and a failed save also pushes the creature
  10 feet straight away while the way is open (+1d8 from a level-two slot). Its
  sound is not modeled.
- **Shatter** (level 2, Action, 60 feet): a 10-foot-radius sphere; Constitution
  save, 3d8 Thunder damage, half on a success. Its Disadvantage for creatures of
  inorganic material is not modeled.
- The caster is never caught in its own area; allies are, unless an Evoker
  sculpts the spell.
- Walls block areas (SRD 5.2.1 line of effect): a square is in the area only
  if a straight line from the point of origin reaches it, the caster for a
  cone, line or cube, the aimed square for a sphere. A sphere can be centered
  only on a square the caster has a clear line to. Partial cover is not
  modeled.
- Verification: `opengold_wizard_spell_tests` (the cone's seven squares and who
  is hit, the cube and a 10-foot push, the sphere and who is hit, a wall
  cutting a sphere off).

Verification: the Wizard spellbook, preparation and advancement checks in
`opengold_spell_access_tests`, `opengold_advancement_tests`,
`opengold_rules_tests`, `opengold_spell_component_tests`,
`opengold_poison_spray_tests` and `opengold_spell_table_tests` (offer
baseline), and `tests/wizard_choices_view_tests.gd`.

## Evoker (level 3)

- The Evoker is the SRD's only Wizard subclass, taken automatically at level
  three.
- **Potent Cantrip**: when a damaging cantrip misses, or its target succeeds on
  the save, the target still takes half the damage, without the cantrip's other
  effects.
- **Sculpt Spells** (level 6): in an Evocation area spell (Burning Hands,
  Thunderwave, Shatter, Fireball) up to 1 + the spell's level allies the
  Evoker can see are spared: they take no damage and no other effect.
  Adaptation: the allies are chosen automatically.
- Verification: `opengold_wizard_spell_tests` (a missed Fire Bolt at levels one
  and three, a saved-against Acid Splash at levels one and three, an ally in
  Burning Hands at levels three and six, and only two of three allies spared). The cantrip
  tests' exact-damage checks no longer pin a level-three miss.

## Mage Armor, False Life and Expeditious Retreat

- **Mage Armor** (level 1, Action, touch, 8 hours): a willing creature wearing
  no armor has base AC 13 + Dexterity modifier (a shield still adds). It is
  offered only on an unarmored character not already warded. Casting it from
  the Camp dialog is not available yet.
- **False Life** (level 1, Action, self): 2d4 + 4 Temporary Hit Points, 5 more
  from a level-two slot; when the Wizard already has some, the usual
  keep-or-replace choice appears.
- **Expeditious Retreat** (level 1, Bonus Action, self, Concentration up to 10
  minutes): the Wizard Dashes at once, and on later turns **Expeditious
  Retreat: Dash** is a Bonus Action.
- Verification: `opengold_wizard_spell_tests` (AC 13 + Dexterity and no second
  cast, 6–12 Temporary Hit Points, the first and a later Dash, a checkpoint).
  The generic spell table probes now also swap a Wizard's book entry.

## Ray of Sickness, Ice Knife, Chromatic Orb and Acid Splash

- **Ray of Sickness** (level 1, 60 feet): a spell attack for 2d8 Poison; a hit
  leaves the target **Poisoned** (Disadvantage on attack rolls, shown as a
  condition) until the end of the Wizard's next turn. Protection from Poison
  ends it. Disadvantage on ability checks is not modeled yet.
- **Ice Knife** (level 1, 60 feet): a spell attack for 1d10 Piercing; hit or
  miss, the target and every creature within 5 feet make a Dexterity save or
  take 2d6 Cold (3d6 from a level-two slot).
- **Chromatic Orb** (level 1, 90 feet): a spell attack for 3d8 of the chosen
  type, offered once per type ("Chromatic Orb: Fire" and so on). The leap to
  another creature on matching dice is not modeled.
- **Acid Splash** (cantrip, Wizard and Sorcerer): a 5-foot-radius sphere aimed
  within 60 feet; each creature makes a Dexterity save or takes 1d6 Acid (an
  Evoker's Potent Cantrip deals half on a success). Area cantrips spend no slot.
- Area spells of level two now spend a level-two slot (Shatter and Silence spent
  a level-one slot before).
- Verification: `opengold_wizard_spell_tests` (Poisoned and its Disadvantage,
  the burst on two creatures, six orb entries and a level-one slot, Acid
  Splash's sphere without a slot, Shatter's level-two slot), the Sorcerer's six
  cantrip options.

## Sleep, Hideous Laughter and Color Spray

- **Sleep** (level 1, 60 feet, Concentration up to 1 minute): aimed like other
  areas, a 5-foot-radius sphere. Each enemy in it (allies are never chosen)
  makes a Wisdom save or is **Incapacitated** while drowsy; at the end of its
  next turn it saves again and on a failure falls **Unconscious** (Prone, Str
  and Dex saves fail, attacks against it have Advantage). Damage, the end of
  Concentration or a companion's **Shake awake** Action (within 5 feet) ends
  it. Elves, Undead and Constructs are unaffected.
- **Hideous Laughter** (level 1, 30 feet, Concentration up to 1 minute): an
  enemy makes a Wisdom save or falls **Prone and Incapacitated**, repeating the
  save at the end of each of its turns and, with Advantage, whenever it takes
  damage. The extra target from a higher slot is not modeled.
- **Color Spray** (level 1, 15-foot cone): each creature in the cone makes a
  Constitution save or is **Blinded** until the end of the Wizard's next turn.
- An Incapacitated creature can only end its turn and takes no Reactions. The
  computer wakes its sleeping companions and casts Hideous Laughter at an
  enemy without conditions.
- Verification: `opengold_wizard_spell_tests` (Sleep passes over allies and
  is shaken off, a laughing creature can only end its turn, the cone's Blinded
  creature, checkpoints of both effects).

## Grease and Web

- **Grease** (level 1, 60 feet, 1 minute, no Concentration): a 10-foot square
  of Difficult Terrain. Each creature in it when it appears, entering it or
  ending its turn in it makes a Dexterity save or falls **Prone**. The grease
  stays when the Wizard's Concentration on another spell ends and vanishes
  after a minute of combat time.
- **Web** (level 2, from Wizard level 3, 60 feet, Concentration): a 20-foot
  cube on the ground of Difficult Terrain. Each creature in it when it appears,
  entering it or starting its turn in it makes a Dexterity save or is
  **Restrained**; **Escape the webs** spends the Action on an Athletics check
  against the spell DC. A creature caught while moving stops where it entered.
  The webs' Light Obscurement, anchoring and burning are not modeled.
- Checkpoints become `OGCOMBAT 38`: each zone records when it ends (0 while
  Concentration holds it).
- Verification: `opengold_wizard_spell_tests` (Grease's square, Prone, a
  checkpoint and its expiry; Web's level-two slot and cube, the save on entry,
  the stop and Escape the webs).

## Shield

- **Shield** (level 1, Reaction): when an attack roll hits a Wizard who has
  Shield prepared, a Reaction and a slot, play stops and the Wizard is asked
  "You are hit. Cast Shield (+5 AC) or decline." The attack total is not shown,
  as at a table where the DM only says the attack hits. **React** casts it:
  +5 AC until the start of the Wizard's next turn, including against the
  triggering attack, which may now miss. Being targeted by Magic Missile asks
  the same way, and a shielded Wizard takes no Magic Missile damage.
- Only one spell slot may be spent per turn, whoever's turn it is: a Wizard
  who spent a slot on its own turn is not asked about Shield before that turn
  ends (an Opportunity Attack as it walks away, for example), but is on the
  next creature's turn.
- A critical hit is not asked about, since +5 AC cannot stop it. A Wizard who
  declines is not asked again during the same command (the later rays of one
  Scorching Ray, for example).
- How it works: the attack sits deep inside the command, so the command is
  undone at the hit, the question is asked, and the answer replays it with the
  same dice. The log shows the attack once, after the answer.
- Computer-controlled casters always cast Shield when asked.
- Checkpoints become `OGCOMBAT 39`: a pending Shield question records the
  asked creature, the command to replay and who has declined.
- Verification: `opengold_wizard_spell_tests` (the question and its
  checkpoint, React's +5 AC and slot with one resolved attack, Decline, and
  Shield blocking Magic Missile).

## Misty Step

- **Misty Step** (level 2, from Wizard level 3, Bonus Action):
  chosen from the A cycle, it is aimed like an area spell, but the preview
  starts on the Wizard and only unoccupied squares within 30 feet that the
  Wizard can see (not Heavily Obscured, not walls) can be chosen; Cast is
  unavailable until one is. The Wizard teleports there without provoking
  Opportunity Attacks, keeping the Action.
- Aimed spells may now be Bonus Actions: casting spends the Bonus Action
  instead of the Action.
- Verification: `opengold_wizard_spell_tests` (occupied and out-of-range
  squares refused, the teleport, the Bonus Action and level-two slot, no
  Opportunity Attack).

## Acid Arrow, Mind Spike and Ray of Enfeeblement

- **Acid Arrow** (level 2, 90 feet): a spell attack for 4d4 Acid, and 2d4 Acid
  more at the end of the target's next turn. A miss splashes half the initial
  damage and nothing later.
- **Mind Spike** (level 2, 120 feet, Concentration): a Wisdom
  save against 3d8 Psychic, half on a success. Knowing the target's location
  has no use here yet.
- **Ray of Enfeeblement** (level 2, 60 feet, Concentration): on a failed
  Constitution save the target is **Enfeebled**: Disadvantage on Strength
  saves and on melee weapon attacks (taken as its Strength-based attacks), and
  1d8 less damage on each attack that hits. It repeats the save at the end of
  each of its turns. On a success it still has Disadvantage on its next attack
  roll before the start of the Wizard's next turn. Strength checks and the
  damage of its spells and saves are not reduced yet.
- Fix: single-target save spells with Concentration now start it, so a second
  Hideous Laughter ends the first.
- The computer casts Acid Arrow, Mind Spike and Ray of Enfeeblement.
- Verification: `opengold_wizard_spell_tests` (the hit and the later burn,
  the splash on a miss, Mind Spike's save, both outcomes of the ray and their
  Disadvantage, a checkpoint, Concentration replacing Hideous Laughter).

## Blur, Mirror Image and Magic Weapon

- **Blur** (level 2, self, Concentration up to 1 minute): attack
  rolls against the Wizard have Disadvantage. No creature has Blindsight or
  Truesight yet, so none ignores it.
- **Mirror Image** (level 2, self, 1 minute): three duplicates, shown as
  "Mirror Image (3 duplicates)". Each hit rolls a d6 per duplicate left; any 3
  or higher and a duplicate takes the hit instead and vanishes. A Blinded
  attacker is not fooled. Shield is asked about before the duplicates roll.
- **Magic Weapon** (level 2, Bonus Action, touch): the creature's weapon
  attacks gain +1 to attack and damage rolls for an hour; casting it again
  ends the earlier one. Adaptation: the bonus follows the creature's weapon
  attacks rather than one particular weapon.
- Verification: `opengold_wizard_spell_tests` (Disadvantage against a blurred
  Wizard, a duplicate taking a hit and its checkpoint, Magic Weapon's Bonus
  Action and +1 to the attack roll).

## Invisibility, See Invisibility and Darkness

- **Invisibility** (level 2, touch, Concentration up to 1 hour): the creature
  is **Invisible**: attack rolls against it have Disadvantage, its own have
  Advantage, spells that need sight cannot target it, and it can leave an
  enemy's reach without an Opportunity Attack. It ends right after the
  creature makes an attack roll (which still has Advantage) or casts a spell.
  The extra targets from higher slots are beyond levels 1-4.
- **See Invisibility** (level 2, self, 1 hour): Invisible creatures are seen as
  if visible.
- **Darkness** (level 2, 60 feet, Concentration up to 10 minutes):
  aimed like other areas, a 15-foot-radius sphere that is Heavily Obscured,
  like Fog Cloud: creatures in it can neither see nor be seen. Casting it on
  an object and dispelling light spells are not modeled.
- Verification: `opengold_wizard_spell_tests` (Disadvantage against and
  Advantage for an Invisible Wizard, no Opportunity Attack, the attack ending
  it; a foe that can't be targeted until See Invisibility; Darkness's sphere
  hiding a creature).

## Flaming Sphere

- **Flaming Sphere** (level 2, 60 feet, Concentration up to 1 minute): aimed
  at an unoccupied square (the preview starts beside the nearest enemy), it is
  drawn as an orange ball. Any creature ending its turn within 5 feet makes a
  Dexterity save against 2d6 Fire, half on a success.
- **Roll Flaming Sphere** (Bonus Action on later turns): choose a creature
  within 30 feet of the sphere; it rolls into that creature, which saves at
  once, and stops in the open square beside it. Rolling it to an empty square,
  jumping pits and its light are not modeled.
- Verification: `opengold_wizard_spell_tests` (the open square, the burn at
  the end of a turn, the Bonus Action roll, a checkpoint).

## Knock

- **Knock** (level 2): at a locked door the menu offers
  **Knock** beside Bash and Pick when an active member has it prepared and a
  level-two slot. The first such member casts it, the slot is spent, and the
  door opens: "Wizard casts Knock, and the lock opens." It is never offered in
  combat. The loud knock it makes draws no attention yet, and wizard-locked
  doors (Arcane Lock) are not modeled.
- Rules modules gain `can_cast_exploration_spell` and `cast_exploration_spell`
  for spells cast while exploring.
- Verification: `opengold_wizard_spell_tests` (no Knock without the spell, the
  cast with its level-two slot, never offered in combat).

## Enlarge/Reduce

- **Enlarge/Reduce** (level 2, 30 feet, Concentration up to 1 minute): offered
  as **Enlarge** on allies (willing, so no save) and **Reduce** on enemies
  (Constitution save), since the other pairings only hurt the caster's side.
  Enlarged: Advantage on Strength saves and 1d4 more damage on weapon hits.
  Reduced: Disadvantage on Strength saves and 1d4 less (not below 1).
  The size change itself, Strength checks and objects are not modeled.
- Verification: `opengold_wizard_spell_tests` (Enlarge without a save and its
  extra damage; Reduce offered only on enemies, its save and lowered damage).

## True Strike

- **True Strike** (cantrip, Wizard and Sorcerer): with a melee weapon in hand, **True Strike** and **True Strike
  (Radiant)** are offered against each enemy in reach. The attack uses the
  spellcasting modifier for its attack and damage rolls and deals the weapon's
  type or Radiant damage. It is a cantrip choice at creation (Sorcerers now
  have seven options). Ranged weapons are not supported yet; the extra Radiant
  damage starts at level 5, beyond this milestone.
- Verification: `opengold_wizard_spell_tests` (no weapon, no True Strike; both
  damage types against a Radiant-immune target), `opengold_sorcerer_cantrip_tests`.

## Dragon's Breath

- **Dragon's Breath** (level 2, Bonus Action, touch, Concentration up to 1
  minute): offered once per type ("Dragon's Breath: Fire" and so on: Acid,
  Cold, Fire, Lightning, Poison) on an ally within 5 feet, including the
  Wizard. While it lasts, that creature has **Exhale (Dragon's Breath)** as an
  Action, aimed like Burning Hands: a 15-foot cone where each creature makes a
  Dexterity save against the Wizard's spell DC, taking 3d6 of the chosen type,
  half on a success.
- Verification: `opengold_wizard_spell_tests` (five options, the Bonus Action,
  the ally's exhale and its saves).

## Charm Person

- **Charm Person** (level 1, 30 feet, 1 hour): a Humanoid makes a Wisdom save,
  with Advantage because the Wizard's side is fighting it (always, in combat),
  or is **Charmed** by the Wizard: it is offered no attack, spell or other
  command against the Wizard and makes no Opportunity Attack against the
  Wizard, though it may still fight the Wizard's allies. From a level-two slot
  two creatures are chosen, as with other multi-creature spells.
- Adaptation: any damage ends the charm, not only damage from the Wizard's
  side (in practice the same, since a creature's allies rarely harm it).
  Social Advantage outside combat has no use yet.
- Verification: `opengold_wizard_spell_tests` (the save, no command against
  the charmer, attacks on others, damage ending it, two targets from a
  level-two slot).

## Gust of Wind

- **Gust of Wind** (level 2, Concentration up to 1 minute): aimed from the
  Wizard like Burning Hands, a Line 60 feet long and two squares (10 feet)
  wide. Each creature in it makes a Strength save or is pushed 15 feet away
  from the Wizard; a creature ending its turn in the line saves again.
- Adaptations: the line stays where it was cast; its squares are Difficult
  Terrain for all movement (the wind only slows movement toward the caster);
  changing its direction as a Bonus Action and its effects on gas and flames
  are not modeled.
- Verification: `opengold_wizard_spell_tests` (the line's squares, saves only
  in it, the push, the save at the end of a turn).
