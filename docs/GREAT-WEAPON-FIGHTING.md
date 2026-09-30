# Great Weapon Fighting

[SRD 5.2.1 p. 88](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=88)
requires the Fighting Style feature. For an attack with a Melee weapon held in
two hands, the feat permits treating each damage die showing 1 or 2 as 3. The
weapon must have Two-Handed or Versatile. This replaces die values, not rolls;
it is not the older reroll rule. Feats are nonrepeatable unless stated otherwise.

## Delivered foundation

The internal [damage roller](../src/OpenGold.Rules.Srd5/src/damage_roll.h) supports
an explicit normal or Great Weapon Fighting die rule. Combat selects its die rule from validated feats, weapon properties and grip.
Normal rolls preserve existing behavior and random continuation. The helper doubles dice for critical hits, adds the flat modifier
once and floors only the final total at zero. Choosing the replacement consumes
exactly the same random draws. It can be used for each damage component of an
eligible attack when additional attack damage is implemented.

[Damage tests](../tests/damage_tests.cpp) use independent face and seeded-sum
expectations, including two successive critical damage rolls, negative modifiers
and fixed damage.

## Playable routes

Rules 0.6.53 adds selection through Fighter starting Training,
Paladin/Ranger level-two advancement, and the independent level-four feat choice.
Fighters can replace their class-granted style when gaining levels 2–4. The
class entitlement and feat entitlement stay distinct and cannot duplicate a feat.

STYLE-1 approves automatically applying the beneficial replacement. Eligible
Melee attacks held in two hands use it for each damage die, including critical
and Savage Attacker rolls. One-handed, Ranged, thrown, spell and unarmed attacks
do not benefit. The Savage Attacker log line shows the resulting totals.
There are no new combat saving controls.

PC42 validates the new grants; internal combat27 retains signed pending damage
and the exact RNG continuation. Campaign19 stores a class style choice separately
from the level-four feat and from locked starting training.

The [delivery packet](FIGHTING-STYLE-ROUTES.md) records acceptance and verification;
[coverage](SRD-COVERAGE.md) is the completion record. Full Paladin/Ranger features,
other styles, mastery, level5+ and multiclassing remain outside this increment.
