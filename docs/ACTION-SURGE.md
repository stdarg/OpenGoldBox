# Fighter Action Surge through level four

Issue [#86](https://github.com/stdarg/OpenGold/issues/86). Authority:
[SRD 5.2.1 p. 48](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=48).

## Rules

Rules 0.6.24 grants `feature:action_surge`, sourced to `class:fighter` at level
2, through ordinary advancement. It has one use through level 4, restored by
both Short and Long Rests. Other classes and level-one Fighters gain no use.

Activation is available on the Fighter's turn and spends its use immediately.
It adds one non-Magic action without consuming the ordinary action, Bonus Action,
Reaction or movement. It can precede or follow the ordinary action. The separate
allowance expires at the end of that turn, with no refund if unused. Non-Magic
actions spend this allowance first, preserving the ordinary action for Magic.
Magic never spends the restricted allowance. Activation consumes no dice or time.
Pending reactions, Temporary HP decisions and Savage Attacker choices resolve
before activation or another action.

The current non-Magic actions include attacks, Dash, Dodge and Disengage. Every
extra attack rolls independently; an extra Dash adds the normal Speed to movement.
Two Dashes and an Orc's separate Adrenaline Rush can provide four times Speed.
Savage Attacker retains its once-per-turn limit across both actions. Spending
either attack can pause for its existing damage-choice dialog while retaining
an unspent action. The first use does not refresh any other resource.

## Persistence and architecture

`ActionBudget` is a rules-owned value with separate ordinary and restricted
allowances. Core and Godot continue to consume legal commands, display snapshots
and named recovery pools; no new runtime or ownership mechanism is introduced.

PC42 validates the class grant. OGCOMBAT 31 stores remaining uses, activation
this turn and the unspent extra allowance. SRD11 vital continuation stores spent
uses alongside existing slots, Second Wind, Hit Dice, mortality clocks,
Temporary HP, Orc uses and effects.

A Fighter acquires the fixed feature when its recorded advancement reaches
level 2. Expenditure
survives training, advancement, healing, elapsed time and campaign handoff.
Saving remains restricted to camping or an inn; internal combat checkpoints are
for deterministic continuation and tests.

## Verification and remaining integration

[Native tests](../tests/action_surge_tests.cpp) cover independent grants and
allowances, both action orders, Magic exclusion, exact extra-attack dice, three
Dash sources, exhaustion, turn expiry, reactions, Savage Attacker, malformed
state, real PC/NPC campaign handoff, advancement, training, rests and replay.

Validation passed 38 native/tool checks and 15 Godot runtime checks, including
six native fixture prerequisites for the latter. Both native Godot extensions
built successfully. Localization checks validate 739 English/Spanish messages.

The approved combat button appears beside Adrenaline Rush for eligible Fighters,
shows remaining uses and stays visible but disabled while unavailable. Mouse,
focused Enter/Space and the existing A/Space action cycle activate the same native
command immediately. Its tooltip explains the Magic restriction and recharge.
[The Godot check](../tests/action_surge_view_tests.gd) verifies levels 1–4, another
class, spent uses, pending damage decisions, mouse and keyboard activation, two
Dash actions and English/Spanish layouts at 1120 × 800 and 1920 × 1080. The UI
follow-up passed eight affected Godot checks and five native fixture checks;
the rendered control check also passed. #86's through-level-four scope is complete.
The existing Short Rest and Hit Dice controls remain #192. Higher-level second
uses remain #178; multiclass acquisition remains #179–184. This increment does
not claim the rest of the Fighter feature set or the all-class milestone.
