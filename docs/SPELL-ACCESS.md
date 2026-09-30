# Wizard spell access

Issue [#199](https://github.com/stdarg/OpenGold/issues/199) separates knowledge
from preparation for the existing playable Wizard spells. It is the first part
of [F07 #36](https://github.com/stdarg/OpenGold/issues/36), not complete Wizard
spellcasting. Authority: [SRD 5.2.1 pp. 77–78](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=77).

## Implemented scope

Rules 0.6.22 adds explicit Fire Bolt/Poison Spray starting cantrip choices through
the approved Spell Choices step. Preset Wizards receive both available cantrips.
Missing catalog choices stay pending.
Magic Missile remains a spellbook entry and prepared spell. See
[Poison Spray](POISON-SPRAY.md) for this increment and its open integrations. Existing advancement
selections can also learn Scorching Ray and Blindness from Wizard level 3.
Changing those selections retains every previously learned book entry and its
first acquisition level. An unprepared book entry cannot be cast. Known cantrips
remain available with no slots; a prepared leveled spell still spends a slot.
An explicitly empty Wizard preparation no longer silently enables Magic Missile.

The rules module derives `SpellAccess` from the existing `FeatureGrant` ledger
and `CharacterSheet.prepared_spells`. Each spell grant has an ID, the source
`class:wizard:spellcasting`, an acquisition level, and an `access` choice of
`cantrip` or `spellbook`. Validation rejects duplicates, wrong sources,
unavailable acquisition levels, malformed choices, and preparation of a spell
absent from the book. No pointer ownership or additional runtime is introduced.

| Wizard level | Cantrip entitlement | Total book selections from levels | Prepared entitlement |
| --- | --- | --- | --- |
| 1 | 3 | 6 | 4 |
| 2 | 3 | 8 | 5 |
| 3 | 3 | 10 | 6 |
| 4 | 4 | 12 | 7 |

These capacities remain explicit even though the current catalog cannot fill
them. New book selections are limited to six at level 1 and two per later
level, with spell level checked at acquisition. Existing Modifiers text shows
known cantrips, book entries/acquisition levels, and pending counts in English
and Spanish. It does not select missing spells on the player's behalf.

## Cleric extension

Rules 0.6.23 adds a sourced starting Sacred Flame choice using the same control.
Cleric grants use `class:cleric:spellcasting`; the base cantrip entitlement is
3 through level 3 and 4 at level 4. Presets receive the supported choice.
Leveled preparation, Divine Order and
level-up replacement remain #91. See [Sacred Flame](SACRED-FLAME.md).

## Persistence

**PC42** combat recipes store an explicit spell ID list; the existing grant
section contains the spell sources and the spell list must agree with them.
Campaign saves store explicit starting cantrip choices. Campaigns replay creation
and recorded advancement to reconstruct knowledge, including now-unprepared
spells, then validate their grant ledger. Missing choices remain pending.
Loading does not replenish resources or roll dice. See [saves](SAVES.md#pre-10-format-policy).

Campaign saves and reloads preserve wounds,
equipment, spent slots, Hit Dice, Adrenaline Rush, Temporary HP and clocks.
Player saving remains restricted to camping or an inn.

## Remaining work

The [level-four spell inventory](SPELL-INVENTORY.md) enumerates all required
spells and their source routes. The five Wizard spells described here are only
a subset of that catalog; missing choices are not evidence of a smaller SRD
entitlement.

- [#37](https://github.com/stdarg/OpenGold/issues/37): full learning and
  preparation controls and filling pending choices. This increment retains the
  existing advancement selection flow; it does not add a Long Rest editor.
- [#97](https://github.com/stdarg/OpenGold/issues/97): copying costs/time,
  book objects, replacement books and the complete Wizard preparation lifecycle.
- [#200](https://github.com/stdarg/OpenGold/issues/200): independent free casts,
  source-specific use/recharge and an actual granting feature. No reserved
  free-cast fields or test-only grants are counted as support.
- Other class policies, all missing spells, rituals and subclass grants remain
  their named increments. Parent #36 stays open.

## Verification

`spell_access_tests.cpp` covers the independent entitlement table, knowledge
retention, actual casting, slot exhaustion, empty preparation, invalid grants,
atomic advancement/preview and campaign replay.
`localization_tests.gd` creates a Wizard through existing controls and checks
the translated knowledge, source level and pending counts in Modifiers.

Rules 0.6.25 adds Ray of Frost as a third supported starting Wizard choice. New
presets receive all three supported choices; existing explicit selections stay
unchanged.
See [Ray of Frost](RAY-OF-FROST.md) for effects and remaining source integrations.
