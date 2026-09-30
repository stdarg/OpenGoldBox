# Savage Attacker

[FT03 / #76](https://github.com/stdarg/OpenGold/issues/76) completes the feat's
combat decisions. Authority: [SRD 5.2.1 p. 87](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=87).
It is a nonrepeatable Origin feat with no additional prerequisite. A weapon hit
can use it once per turn, including another creature's turn. It affects weapon
damage dice; spell damage, an ordinary Unarmed Strike, and fixed Blowgun damage
do not acquire extra dice from the feat.

## Automatic reroll

Since [AUTO-1](SRD-DECISIONS.md) (2026-09-30) the feat has no dialog: taking
the higher roll is the only sensible choice. On the first eligible weapon hit
each turn the weapon damage dice are rolled twice and the higher total is kept.
The combat log records it, for example
`Rolf rerolls weapon damage (Savage Attacker): 9 and 10, keeps 10.`, and the
hit line that follows carries the `(Savage Attacker)` marker. This supersedes
the two-stage dialog approved on 2026-09-23 (Q8).

Critical hits double the weapon dice in both sets, with the ability modifier
added once to each total. A Versatile weapon rerolls the die of the grip it was
used with. Sneak Attack dice are rolled once and added to the kept total;
defenses and Temporary HP apply to the final damage once. The attack's Action
or Reaction is spent as usual, and an opportunity hit resolves at once, so the
interrupted movement continues or stops immediately. Enemies with the feat
follow the same rule.

## Grants and integration boundaries

The existing Soldier background grants the feat to all twelve classes, with
`background:soldier` provenance. The existing level-four selector can acquire
it for Fighter, Cleric and Wizard through their advancement entitlement; preview,
confirmation and campaign reconstruction preserve its source and level.
Duplicate grants and acquisitions reject. The feature/feat foundation remains
the authority for entitlement and repeatability.

This closes the feat's behavior, not all granting packages. Human Origin-feat
selection remains #71, unfinished starting-class choices remain #84–164, and
the remaining Soldier package remains #64. Extra Attack and other features
that add attacks retain their separate increments. The once-per-turn spent
flag is already part of combat state; those features must respect it.

## Persistence

Nothing is pending between the attack roll and damage, so the combat checkpoint
records only the spent once-per-turn flag. Campaign saves preserve all grants,
equipment, wounds, pools and clocks. Player saving remains restricted to
camping or an inn.

## Verification

`savage_attacker_tests.cpp` checks, with fixed rolls across all twelve classes,
that the higher roll applies at once with the expected RNG use and log line,
critical and lower-reroll cases, Versatile dice, misses and exclusions,
defenses/Temporary HP, opportunity hits (queued and lethal), the once-per-turn
refresh and real advancement grants. The normal party and advancement
walkthroughs exercise acquisition and use through the existing game flow.
