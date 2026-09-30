# Typed damage and defenses

[F05 #33](https://github.com/stdarg/OpenGold/issues/33) supplies typed damage,
resistance, vulnerability and immunity, including the Poison resistance part of
Dwarven Resilience. It uses the existing C++20 rules module, value containers and
Godot combat log/Modifiers dialog. There are no new controls or layout choices.

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
damage and defenses pp. 16–17, Dwarf p. 84, weapons p. 91, Magic Missile p. 146.

## Resolution

`damage.h/.cpp` defines all thirteen damage types and a pure resolver. A damage
instance may contain several types. Callers first apply damage-roll bonuses,
penalties, critical dice and save multipliers. The resolver combines components
of each type, halves resistant damage with rounding down, doubles vulnerable
damage and prevents immune damage. Matching type-specific and all-type sources
apply once each; neither resistance nor vulnerability stacks. Resistance and
vulnerability both apply rather than canceling the rounding step. The SRD's
28 minus 5 example resolves to 23, then 11, then 22.

Amounts are nonnegative and checked for overflow. Zero damage remains zero:
it cannot wake or destabilize a creature, add death failures or cause massive
damage. HP/death transitions use the resolved amount through the shared life
cycle. Resolution consumes no RNG and does not replenish actions or resources.

All currently supported weapons carry their SRD damage type. Unarmed strikes
are Bludgeoning. Curated creature attacks have explicit authored damage-type
rows. Fire Bolt and each Scorching Ray attack deal Fire; each Magic Missile dart
deals Force. Darts resolve resistance separately before their results are summed,
retaining the existing per-dart dice and simultaneous HP application. The broader
Magic Missile conformance work (including simultaneous-dart mortality boundaries,
split targets, Shield and other casting requirements) remains #168/#42/#41.
Existing attack rolls, critical dice, Savage Attacker and spent turn resources
are preserved. When defenses change an amount, the existing combat log shows
the type and before/after values, localized in English and Spanish.

## Dwarves and content

Every created or reconstructed Dwarf receives
`trait:dwarven_resilience`, source `species:dwarf`, acquisition level 1. The
existing racial explanation lists Poison resistance, and combat uses it for any
class. Dwarven Toughness continues independently. This does not complete the
Poisoned-condition saving-throw advantage, Darkvision or Stonecunning; those
remain #66/#35. Other species' choices and resistances remain their own issues.

Creature content can declare:

```text
damage_types creature_key melee_type ranged_type
affinity creature_key source_id resistance|vulnerability|immunity damage_type|all
```

Types are lowercase rules IDs. Rows require an existing creature; duplicate type
rows, duplicate source IDs, unknown types/kinds, missing fields and trailing data
reject. A creature has at most 128 affinity sources. Several distinct sources may
grant the same defense without multiplying it. Older authored packs without a
type row retain the Bludgeoning default; the installed pack supplies explicit
rows for every current profile. No original AD&D numbers imply a new SRD defense.
Original script HP assignments remain explicit state assignments, not inferred
Poison damage or an invented hazard conversion.

## Saves

**PC42** recipes require the sourced Dwarf resistance. Records must contain it;
missing, forged or wrongly sourced grants reject. A save loads identically on
the next pass.

## Verification

`opengold_damage_tests` checks independent SRD arithmetic, all damage types,
overlap, mixed instances, separate-instance rounding, malformed data and overflow.
It runs actual Poison melee attacks against newly created Dwarves in all twelve
classes, compares them with Human damage, and checks action expenditure, RNG,
checkpoint continuation and stale-command rejection. An independent weapon table
checks all 26 supported damaging weapons against an immune target; Fire Bolt,
Scorching Ray and Magic Missile exercise the same live path. Training and
advancement are covered.
The game `--party-check` now creates a Dwarf, verifies the existing Modifiers
explanation, and continues through shopping, combat, advancement and recovery.

Temporary HP and replacement choices are separate F05b #34 work. This increment
adds no new player spell entitlement, creature catalog encounter or UI action.

Delivery checks: both native Godot libraries built; all 16 selected native suites
and all six Godot control/ownership checks passed. The game Dwarf route and the
existing demo party route passed with the current pack, as did the localization
UI check and all 574 English/Spanish catalog entries. The game party route still
reports the previously observed eight ObjectDB instances at shutdown; the
dedicated node ownership check passes. This increment does not claim to repair
that existing integration-route cleanup warning.

## Dwarven Toughness source display

[#210](https://github.com/stdarg/OpenGold/issues/210) corrects the racial section
of the character sheet after leveling. It now shows the attained contribution
(+1 through +4 maximum HP), using the same translated message as combat HP
sources. The former level-one-only explanation could remain visible even when
the underlying maximum HP already included the later species bonuses.
Authority: [SRD 5.2.1 p. 84](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=84).

This is a presentation correction: HP arithmetic, wounds, resources, grants and
save formats are unchanged. Reconstructing existing saved advancement choices
also reconstructs the corrected explanation. Advancement tests verify all twelve
classes at creation, Fighter/Cleric/Wizard levels 1–4, independent HP totals,
Constitution increases, wounded/unconscious characters and campaign reloads.
Other classes' advancement remains in their own issues. Dwarf tracker #66 still
requires Darkvision, Stonecunning and Poisoned-save Advantage.

Validation for #210: rebuilt advancement, character and save tests pass; the
Godot advancement check now advances a Dwarf Wizard, verifies the actual Modifiers
text contains +4, and reloads the campaign to verify the same dialog text.
English/Spanish localization validates 743 messages. Existing control placement
and styling are unchanged.
