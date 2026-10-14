# Hide

Scope: [#219](https://github.com/stdarg/OpenGold/issues/219). SRD 5.2.1's Hide
action, the Rogue's Cunning Action Hide and the Search action, with the choices
recorded in [HIDE-1](SRD-DECISIONS.md#hide-1-2026-10-14-finding-hidden-creatures).

## Hiding

A party character may Hide as an Action, and a Rogue from level 2 as a Bonus
Action (Cunning Action), when no conscious enemy has a clear line of sight to
it: a wall between them, or either square Heavily Obscured (Fog Cloud,
Darkness). The game has no partial cover ([COVER-1](SRD-DECISIONS.md#cover-1-2026-10-14-no-half-or-three-quarters-cover)),
so an enemy that can see the square blocks hiding. A Halfling's Naturally
Stealthy also counts a creature at least one size larger, friend or foe, as
blocking the line.

The Dexterity (Stealth) check is against DC 15, with Disadvantage in armor that
hinders Stealth; Halfling Luck and Bardic Inspiration apply as to other checks.
On a success the creature is **Hidden** and has the Invisible condition: attack
rolls against it have Disadvantage, its own have Advantage, and spells that
need sight cannot target it. Its total is the DC to find it. Monsters do not
hide.

## Finding and ending

Hiding ends when the creature:

- makes an attack roll (after the roll, which keeps its Advantage);
- casts any spell, including a reaction spell such as Shield (HIDE-1);
- uses a Breath Weapon;
- is found.

An enemy finds a hidden creature in its clear view when its Passive Perception
(10 + Perception) reaches the Stealth total; this is checked after every
command, so walking into view of a watchful enemy ends hiding at once. The
Search action (an Action, offered while an enemy is hidden) rolls Wisdom
(Perception) against each hidden enemy in clear view. A character's Perception
and Stealth come from its skills; a monster's Perception is its stat block's
skill (the `perception` rows in `combat.rules`) or else its Wisdom save.

The AI never targets or approaches a hidden creature; with no other enemy
left, it searches. Hiding does not outlast the fight: the saved character state
after combat drops it.

## Controls

The Rogue's Bonus Action dropdown lists Hide after Dash and Disengage; it is
disabled while an enemy can see the Rogue. Any character reaches Hide (and
Search, when offered) with A and uses it with Space. The combatant shows
"Hidden" while it lasts.

## Verification

[hide_tests.cpp](../tests/hide_tests.cpp) checks who may hide, the Bonus Action
and Action costs, the shown condition, Passive Perception finding a Rogue that
steps into view (and a dull enemy not finding it), Advantage and the end of
hiding on an attack, the AI searching instead of attacking with both outcomes,
and a cantrip without an attack roll ending hiding. The Godot Cunning Action and
Rogue attack checks cover the dropdown.
