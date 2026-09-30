# Savage Attacker

[FT03 / #76](https://github.com/stdarg/OpenGold/issues/76) completes the feat's
combat decisions. Authority: [SRD 5.2.1 p. 87](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=87).
It is a nonrepeatable Origin feat with no additional prerequisite. A weapon hit
can use it once per turn, including another creature's turn. It affects weapon
damage dice; spell damage, an ordinary Unarmed Strike, and fixed Blowgun damage
do not acquire extra dice from the feat.

## Player decisions

The user approved the two-stage centered dialog on 2026-09-23:

1. After an eligible hit, show its weapon dice, modifier and first damage result.
   Choose **Use Savage Attacker** or **Keep damage; save feat**.
2. Using the feat spends it for this turn and rolls the second set of weapon
   damage dice. Choose either result, including the lower one. The buttons show
   their damage amounts.

An attack's Action or Reaction is spent before either decision. Other combat
commands and movement wait. HP, Temporary HP and defenses are unchanged until
the final choice. A skipped use rolls no additional dice. Critical hits double
weapon dice in both sets, with the ability modifier added once to each total;
Versatile uses the current grip. Defenses and Temporary HP apply to the chosen
result once. Both dialogs use standard buttons and keyboard focus. The game
provides English/Spanish text; the older native demo retains its English UI and
offers the same decisions. Neither exposes player saving during combat.

Opportunity-hit decisions retain the pending movement and its reactor position.
The spent Reaction remains spent across both stages. After damage, continue to
the next reactor or resume the movement once; incapacitating the mover stops it.
Enemy AI chooses to use the feat and retains the higher result. Player choice
is not replaced by that AI policy.

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

The internal combat checkpoint records an optional pending weapon hit. It
records the participants, attack roll/mode, weapon attack kind,
first damage and optional second damage. Dice, modifiers and eligibility derive
from the validated actor/equipment state. Restoring validates the hit, roll
bounds, feat use, spent Action/Reaction and any movement queue before publishing
the session. It never rerolls, refunds an action, or applies damage early.

Campaign saves preserve all grants, equipment, wounds, pools and clocks.
Player saving remains restricted to camping or an inn.

## Verification

`savage_attacker_tests.cpp` uses independent fixed-roll expectations across all
twelve classes, both decision stages, lower/critical results, ranged/melee and
Versatile weapons, misses and exclusions, defenses/Temporary HP, queued and
lethal opportunity hits, malformed checkpoints and real advancement grants.
`savage_view_tests.gd` exercises actual buttons, keyboard
focus, both supported window sizes, deferred target HP and internal checkpoint
continuation. The normal party and advancement walkthroughs exercise acquisition
and use through the existing game flow.
