# Temporary Hit Points

The native foundation [#196](https://github.com/stdarg/OpenGold/issues/196) supplies
the shared pool. [#197](https://github.com/stdarg/OpenGold/issues/197) adds a playable
source: a normally created Orc of any of the twelve classes can use Adrenaline
Rush in combat. This is the activated-trait portion of [Orc #72](https://github.com/stdarg/OpenGold/issues/72),
not Darkvision or Relentless Endurance.

Authority: [SRD 5.2.1 pp. 17–18 and 86](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

## Adrenaline Rush and controls

The sourced level-one Orc trait grants uses equal to proficiency bonus (two at
the currently supported levels). It spends one use and a Bonus Action, performs
Dash using the actor's actual Speed, and grants Temporary HP equal to proficiency
bonus. Ordinary Action Dash can also be used, for three movement allowances.
Movement still provokes opportunity attacks. Short and Long Rests restore all
uses. Training, advancement and new encounters preserve expenditure; advancement
adds only any newly gained capacity. Curated original-game Orc monsters do not
silently acquire the player-species trait.

Adrenaline Rush is visibly labeled beside Dash beneath the existing turn
controls, with remaining/capacity counts, keyboard focus and an explanatory
tooltip. The older action grid remains hidden. A first pool applies immediately.
An existing pool opens a centered window showing both amounts and sources, with
Keep current and Use new buttons. Neither is automatically submitted. The grant's
use and Bonus Action remain spent while other commands and movement wait. A
smaller new pool is a valid choice. Stale command tickets cannot duplicate the
grant or decision. Automated combat adjudicates its own choice by retaining the
larger pool; the player's interface always asks.

Ordinary HP is green at full health, yellow when wounded above 20%, and red at
20% or below (including zero). Separate Temporary HP is green on character sheets
and the combat roster. HP hints identify Constitution, Dwarven Toughness and the
Temporary HP source where applicable. No current status effect reduces maximum
HP. Missing ordinary HP is identified as such: historical attacks and script HP
losses have not been individually recorded, so hints do not invent their cause.
An unrelated condition such as Blinded is not described as an HP penalty.

## Rules and caller contract

Temporary HP is a separate nonnegative amount with a bounded source identifier.
It may exceed maximum HP. A granting feature establishes entitlement, spends
its action/resources and obtains an explicit keep-current or use-new decision.
`grant_temporary_hit_points` commits that decision atomically; it never adds the
two amounts or automatically chooses the larger one. Keeping requires an
existing pool, and a new grant must be positive and have a valid source. The
shared operation grants no species, spell or class entitlement by itself.

Typed damage adjustments, resistance, vulnerability and immunity resolve first.
The resulting damage consumes Temporary HP before ordinary HP; any remainder
uses the existing unconsciousness and massive-damage transitions. Depletion
clears the exhausted source. Healing, Hit Dice, temple services, advancement and
training completion preserve the pool. An original script's absolute HP
assignment changes actual HP and bypasses the damage buffer.

Temporary HP does not heal, stabilize or wake a creature. Absorbing HP loss does
not mean that no damage was taken: at zero HP, positive resolved damage still
ends Stable and causes one death-save failure, or two for a Critical Hit. Damage
at zero HP reaching the HP maximum still causes death. Damage reduced to zero
by defenses has none of these consequences. Future concentration checks must
likewise use damage taken, not just the ordinary HP lost.

The default pool lasts until depleted or until its recipient completes a Long
Rest. Waiting eight hours, a Short Rest or another member's Long Rest does not
expire it. Existing campaign rest eligibility determines which active members
complete the rest; reserves and ineligible members retain their pools. Features
with a specific overriding lifetime must implement that lifetime when added.

## Persistence and architecture

In **OGCOMBAT 28**, in addition to the sourced
pool, actor rows retain Adrenaline Rush uses and whether its movement allowance
was used this turn. A final section stores the optional unresolved offer. Restore
validates entitlement, amount/source, spent costs, movement capacity, conscious
recipient and absence of a competing opportunity queue before committing.
**PC42** requires the fixed sourced Orc grant.
**SRD9** records uses with existing resources, pool, Hit Dice, mortality clocks
and effects. Orc use counts survive reloads.
Malformed counters, source IDs, pending offers and trailing fields reject before
replacing live state.

The source and amount are available through `RecoveryInfo` and `CombatantView`.
The shared life-cycle value type owns the pool; no ownership uses raw pointers,
and Core/Godot do not decode the SRD resource string. Rules remain C++20 and
independent of Godot. No new runtime or framework is introduced. Godot controls display rules-owned
queries; Core and presentation never parse SRD9 themselves.

## Verification and remaining delivery

`opengold_temporary_hp_tests` uses independent absorption and massive-damage
examples, including fully absorbed hits at zero HP, explicit smaller replacement,
healing and script-assignment distinctions, overflow and rejection. Actual
combat tests apply Dwarf Poison resistance before consuming the buffer and
continue identically after a checkpoint. Campaign tests cover PC/NPC/reserve
state, training, advancement, Hit Dice and individual Long Rest expiry.

`opengold_adrenaline_tests` covers ordinary creation in all twelve classes,
shared Bonus Action costs, both Dash allowances, explicit lower/equal/higher
replacement, stale commands, pending save/load, opportunity continuation,
resource validation, training/advancement, campaign continuation and rests. The Godot
check drives the feature buttons, keyboard replacement, and checkpoint handlers;
`--party-check --adrenaline-check` exercises normal Orc creation, shops, campaign
combat, advancement and recovery using the existing game route. English and
Spanish catalogs cover the new labels, hints and decision text.

## Remaining integration boundaries

Campaign saves retain resolved pools and spent uses between encounters. The
existing campaign engine rejects saving during an active battle; this increment
does not add mid-encounter campaign checkpoints. Standalone training combat's
checkpoint codec preserves an unresolved choice exactly for internal continuation
and regression checks. Per the user's save policy, no combat Save/Load controls
are exposed; player saving belongs at camp or an inn.

Activating Adrenaline Rush during exploration, before combat, remains missing.
Its resource/HP effect and an outstanding replacement choice must be owned by the
campaign, and exploration time/action semantics need to be defined. Keep parent
[#34](https://github.com/stdarg/OpenGold/issues/34) open for that integration;
additional granting spells/features remain their own tracked increments.

Delivery verification: all 18 selected native suites and seven Godot checks
(including project preparation) passed. The normal Orc game route passed creation,
shops, both combats, advancement, interrupted camping, temple healing, inn
recovery and companion/reserve mortality. The demo party route also passed.
Rendered English and Spanish decisions/buttons fit at 1920×1080 and 1120×800;
the actual campaign roster displays separate green HP/Temporary HP and the
Adrenaline Rush source hint. Both Godot libraries build and the 591-entry
English/Spanish catalog validates. Ownership remains value-based native state
and Godot-owned scene nodes; no runtime or architecture substitution is made.

The baseline Dwarf game route still reports the previously observed five
ObjectDB instances at shutdown. Its gameplay assertions and the dedicated
node-ownership check pass; this increment does not claim to fix that cleanup
warning.
