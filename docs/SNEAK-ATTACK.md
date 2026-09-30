# Sneak Attack

Rules 0.6.52 implements #112 through ordinary Rogue levels 1–4, including the
acceptance of #220/#221. See the [frozen batch](ROGUE-ATTACKS.md) for approvals,
verification and remaining Steady Aim demo placement. Full Rogue completion #116,
Thief #115, Hide #219 and weapon mastery #60 remain separate requirements.

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
pp.16 and61–63. The sourced level-one grant is `feature:sneak_attack`.

## Player behavior

Sneak Attack applies automatically to the first eligible weapon hit each turn
([AUTO-1](SRD-DECISIONS.md#auto-1-2026-09-30-automatic-choices-with-logging),
superseding the ROGUE-1 dialog). It adds 1d6 at levels1/2 or 2d6 at levels3/4,
and the combat log shows it, for example
`Vex adds Sneak Attack: 2d6 for 7 extra damage.` Misses and ineligible attacks
do not spend it. Each combatant turn resets the allowance, so an opportunity hit
during another turn can use Sneak again. The attack's Action/Reaction is spent
as usual and interrupted movement continues once the hit resolves.

A Finesse or Ranged weapon qualifies with net Advantage, or with a capable ally
within five feet of the target and no net Disadvantage. The ally query excludes
the attacker and Incapacitated allies, without adding sight/reach requirements.
Thrown Dagger and Dart qualify; thrown Handaxe and unarmed/spell attacks do not.
A ranged weapon's unarmed melee fallback does not qualify. Opposed Advantage and
Disadvantage cancel normally.

Criticals double the extra dice. Blowgun's fixed base remains fixed. Savage
Attacker rerolls only the weapon dice; the Sneak dice are rolled once and added
to the kept weapon total. Signed weapon damage and Sneak damage combine before a
single zero floor and same-type resistance calculation.

## Rules boundary and persistence

Eligibility, rolls, budgets and advancement live in the statically linked SRD
module. Core transports generic commands/state; the UI only shows the log. The
combat checkpoint keeps the once-per-turn flag; nothing is pending between the
attack roll and damage. Unsupported grants or level/profile combinations reject.

## Verification

`tests/rogue_attack_checks.h` (training target) exercises ordinary advancement,
the logged extra dice at each level, critical/canceled rolls, sleeping allies,
thrown weapons, actual opportunity attacks, the once-per-turn limit, separate
Savage rolls (HP loss equals the kept weapon roll plus the Sneak dice),
resistance, checkpoint continuation and campaign/rest/reload. Existing
independent catalog predicates remain in `damage_tests.cpp`.

`rogue_attack_view_tests.gd` exercises the Steady Aim and Cunning Action
controls. `rogue_advancement_view_tests.gd`
uses ordinary campaign level-up controls through level4, Cancel/Confirm, available
ASI controls and save/reload; native comparison verifies the complete resulting
campaign. The advancement note explicitly preserves unavailable Thief/Hide/mastery.
See the batch evidence and coverage record for tested revision and limitations.
