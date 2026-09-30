# Sneak Attack

Rules 0.6.52 implements #112 through ordinary Rogue levels 1–4, including the
acceptance of #220/#221. See the [frozen batch](ROGUE-ATTACKS.md) for approvals,
verification and remaining Steady Aim demo placement. Full Rogue completion #116,
Thief #115, Hide #219 and weapon mastery #60 remain separate requirements.

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
pp.16 and61–63. The sourced level-one grant is `feature:sneak_attack`.

## Player behavior

An eligible weapon hit offers the approved Sneak Attack dialog before damage.
Use adds 1d6 at levels1/2 or 2d6 at levels3/4; Escape/decline preserves the use.
Misses and ineligible attacks do not spend it. Each combatant turn resets the
allowance, so an opportunity hit during another turn can use Sneak again.
Action/Reaction remains spent and interrupted movement waits for all decisions.

A Finesse or Ranged weapon qualifies with net Advantage, or with a capable ally
within five feet of the target and no net Disadvantage. The ally query excludes
the attacker and Incapacitated allies, without adding sight/reach requirements.
Thrown Dagger and Dart qualify; thrown Handaxe and unarmed/spell attacks do not.
A ranged weapon's unarmed melee fallback does not qualify. Opposed Advantage and
Disadvantage cancel normally.

Criticals double the extra dice. Blowgun's fixed base remains fixed. Savage
Attacker follows Sneak and rerolls only weapon dice; its display keeps the extra
component separate. Signed weapon damage and Sneak damage combine before a
single zero floor and same-type resistance calculation.

## Rules boundary and persistence

Eligibility, rolls, budgets, advancement and pending-hit validation live in the
statically linked SRD module. Core transports generic commands/state; main and
demo UI present the optional decision. PC42/combat27 preserve the new Rogue
fields; unsupported grants or level/profile combinations reject.

## Verification

`tests/rogue_attack_checks.h` (training target) exercises ordinary advancement,
critical/canceled rolls, sleeping allies, thrown weapons, actual opportunity
attacks, separate Savage rolls, resistance, exact pending-state continuation,
campaign/rest/reload and invalid-command atomicity. Existing independent catalog
predicates remain in `damage_tests.cpp`.

`rogue_attack_view_tests.gd` exercises both-size main EN/ES and demo EN hit dialogs,
keyboard decisions and exact native continuation. `rogue_advancement_view_tests.gd`
uses ordinary campaign level-up controls through level4, Cancel/Confirm, available
ASI controls and save/reload; native comparison verifies the complete resulting
campaign. The advancement note explicitly preserves unavailable Thief/Hide/mastery.
See the batch evidence and coverage record for tested revision and limitations.
