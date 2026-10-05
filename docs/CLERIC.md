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

## Channel Divinity

- Two uses from level two; a Short Rest restores one, a Long Rest all
  ("Channel Divinity" in the rest and combat resources).
- **Divine Spark** (Action, another creature the Cleric can see within 30
  feet): roll 1d8 + Wisdom modifier. An ally regains that many Hit Points; an
  enemy makes a Constitution save and takes that much Radiant damage, half on a
  success. Necrotic is used instead when the target resists Radiant more (the SRD
  leaves the choice to the Cleric; this takes the better one).
- **Turn Undead** (Action, offered when an Undead enemy is within 30 feet): each
  Undead enemy within 30 feet makes a Wisdom save or is turned for 1 minute:
  Frightened and Incapacitated, it can only move as far from the Cleric as it
  can, takes no Action or Reaction, and the effect ends when it takes damage or
  the Cleric drops to 0 Hit Points. The current content has no Undead yet.
- In the game: A cycles to **Divine Spark** (click the creature) or **Turn
  Undead** (Space). The party AI uses Divine Spark on an ally below half Hit
  Points before spending a spell slot, and Turn Undead whenever it is offered.

Verification: `opengold_cleric_channel_tests` (no Channel Divinity at level one,
two uses, healing, harming with a save, Turn Undead needing an Undead, a turned
zombie fleeing then ending its turn, a checkpoint, damage ending it) and
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
- **Lesser Restoration** (level 2, Bonus Action, touch): ends Blinded on an
  ally, the only one of its conditions the game has so far, so it is offered
  only on a Blinded ally. The AI uses it whenever it is offered.
- Verification: `opengold_cleric_channel_tests` (the always prepared spells,
  Disciple of Life's +3 from a level-one slot on identical rolls, Preserve
  Life, Lesser Restoration ending Blinded) and the updated preparation checks in
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
- Not yet: casting Aid from the Camp dialog (it would need several targets
  there).
- Verification: `opengold_cleric_channel_tests` (two creatures raised by 5, a
  checkpoint, the campaign maximum and its end after 8 hours).

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
- **Silence** (level 2, Action, 120 feet, Concentration up to 10 minutes),
  aimed like Entangle: a 20-foot-radius sphere, tinted on the map. No spell with
  a Verbal component can be cast from inside it (smites and Favored Enemy's
  Hunter's Mark included), and creatures inside take no Thunder damage.
  Deafened is not modeled.
- Combat checkpoints become `OGCOMBAT 37` (Resistance's once-per-turn use).
- Verification: `opengold_cleric_channel_tests` (eleven Resistance entries, the
  ward and its 1d4, Silence's sphere and no Verbal spells inside it), and the
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
