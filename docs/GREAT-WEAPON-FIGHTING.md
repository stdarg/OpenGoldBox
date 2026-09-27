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
and fixed damage. Actual rules 0.6.29 files from `88c2649` preserve a wounded
Fighter with starting Defense, level-four Archery and Soldier Savage Attacker.
The combat files record a critical Greatsword hit before the Savage decision,
after the second roll and after accepting it. Current combat reproduces these
files apart from module identity on later writers, including action expenditure, HP, grants and random state.

## Playable routes

Rules 0.6.53 adds selection through Fighter starting Training/Review Training,
Paladin/Ranger level-two advancement, and the independent level-four feat choice.
Fighters can replace their class-granted style when gaining levels 2–4. The
class entitlement and feat entitlement stay distinct and cannot duplicate a feat.

STYLE-1 approves automatically applying the beneficial replacement. Eligible
Melee attacks held in two hands use it for each damage die, including critical
and Savage Attacker rolls. One-handed, Ranged, thrown, spell and unarmed attacks
do not benefit. The existing Savage dialog displays the resulting totals.
There are no new combat saving controls.

PC36 validates the new grants; internal combat21 retains signed pending damage
and the exact RNG continuation. Campaign17 stores a class style choice separately
from the level-four feat and from locked starting training. Older recipes and
campaign formats retain their existing validation and support. The real 0.6.29
fixtures above remain unchanged.

The [delivery packet](FIGHTING-STYLE-ROUTES.md) records acceptance and verification;
[coverage](SRD-COVERAGE.md) is the completion record. Full Paladin/Ranger features,
other styles, mastery, level5+ and multiclassing remain outside this increment.
