# Magic Initiate

[FT02 / #75](https://github.com/stdarg/OpenGold/issues/75) delivers the Origin
feat, and with it the first source-specific free cast
([#200](https://github.com/stdarg/OpenGold/issues/200)). Authority:
[SRD 5.2.1 p. 87](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=87).

## The feat

The character chooses the Cleric, Druid or Wizard spell list, then:

- **Two cantrips** from that list.
- **One level-one spell** from that list. It is always prepared. It can be cast
  once per Long Rest without a spell slot, and also with any slot the character
  has.
- **A spellcasting ability** for these spells: Intelligence, Wisdom or
  Charisma. It sets their spell attack bonus, save DC and any modifier they add
  (a healing spell's, Shillelagh's or True Strike's).

The feat repeats only with another list. Each list is its own feat id
(`feat:magic_initiate_cleric`, `_druid`, `_wizard`), so the grant ledger
rejects a second feat with the same list.

## Where it comes from

| Route | Feat | Choices made |
| --- | --- | --- |
| Acolyte background | Magic Initiate (Cleric) | Spell Choices step at creation |
| Sage background | Magic Initiate (Wizard) | Spell Choices step at creation |
| Level-four feat | Any list not already held | The level-up dialog's spell page |
| Warlock's Lessons of the First Ones (level 2) | Any list not already held | The level-up dialog's spell page |

The Human's Versatile Origin feat is [#71](https://github.com/stdarg/OpenGold/issues/71);
Human traits are not in the game yet.

## Choices

Following [CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes),
the feat uses the existing Spell Choices groups, with no new layout:

- "Magic Initiate (Cleric): spellcasting ability", one of three.
- "Magic Initiate (Cleric) cantrips", two.
- "Magic Initiate (Cleric) level 1 spell", one.

A class without spells of its own (an Acolyte Fighter, a Sage Rogue) now has
a Spell Choices step at creation, and a spell page after choosing the feat at
level four. The class's own spellcasting ability is listed first. Ready-made
characters take the first options, so a Character Pool Acolyte Cleric casts
with Wisdom.

A spell is learned only once. The feat does not offer a spell the character
already knows from its class. Picking the same spell in two groups is
rejected.

## In play

- **Combat.** The feat's cantrips join the Spell dropdown. Its level-one spell
  is offered from a slot as usual, and once per Long Rest as
  "Guiding Bolt (Magic Initiate)". The free cast spends no slot. It is
  therefore still offered after the turn's one spell slot is spent.
- **Shield.** A Shield taught by the feat is cast as a reaction with the free
  cast first, and from a slot only after the free cast is spent. Only the free
  cast is limited to Shield; a slot could have cast any spell.
- **Camp.** Cure Wounds, Healing Word and Goodberry from the feat are offered
  in the Camp dialog's Cast / Use list with the free cast.
- **Resources.** Each feat's free cast is a pool named for its spell, such as
  "Magic Initiate: Guiding Bolt 1/1". The pool is shown in combat and on the
  rest screen. A Long Rest restores it. A feat gained at a new level arrives
  with its free cast.
- **Character sheet.** The Modifiers text lists each spell the feat teaches,
  with that feat's spell attack bonus.

## Persistence

Grants record the feat and every choice: the ability as
`spellcasting_ability:<ability>` and each spell as `spell:<id>`. Both are
sourced from the feat id. The level-one spell's access is `always_prepared`
and a cantrip's is `cantrip`. Profiles reject:

- a spell from another list
- a third cantrip or a second level-one spell
- an ability other than the three
- a feat's spells without the feat
- one list taken twice

The unspent free casts are a bit set in the vital state (`SRD12`) and in combat
checkpoints (`OGCOMBAT 50`). Rules 0.6.158.

## Not offered

The feat's **Spell Change**, replacing one of its spells on gaining a level, is
not offered. No class can replace a spell, cantrip or invocation on level-up
yet; the 2026-10-11 audit tracks this as a shared gap.

## Verification

`opengold_magic_initiate_tests` covers:

- the offered groups for each background
- grants and their provenance
- the always-prepared spell
- invalid and duplicate picks
- the feat's ability in a spell attack and a save DC
- the free cast in combat, its checkpoint, the next fight and its Long Rest
- a caster's free cast after spending the turn's slot
- Shield as a reaction
- the level-four feat and its repeat rule
- Lessons of the First Ones
- camp healing
- tampered grants

`opengold_character_tests` walks an Acolyte Cleric through creation. The
creation, level-up and rest Godot checks pass with the original game data.
