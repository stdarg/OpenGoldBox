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
