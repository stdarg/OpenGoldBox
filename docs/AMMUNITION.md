# Unlimited ranged ammunition — approved SRD exception

On 2026-09-25 the user explicitly replaced #57's ammunition tracking requirement:

> Let's do away with ammunition for ranged weapons and assume, if they have the
> weapon, they also have the ammunition.

Ranged ammunition is therefore unlimited. A character needs the weapon, not a
matching ammunition stack. Firing does not select or consume inventory ammunition;
there is no ammunition recovery decision, recovery calculation or recovery time.
The proposed Ranged feedback changes and recovery dialog are withdrawn. This
retains existing gameplay rather than introducing an ammunition system.

This is a deliberate exception to SRD 5.2.1's expenditure/recovery rules, not a
claim to implement them. [#57](https://github.com/stdarg/OpenGold/issues/57) is no
longer planned under the changed requirement. Other weapon properties remain
separate. Since [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)
(2026-09-30), [Thrown weapons](THROWN-WEAPONS.md) are treated the same way: a
throw never removes, drops or spends the weapon, which stays held or carried.
Existing action costs, attack rolls, damage, range, proficiency, grip
and shield behavior remain in effect.

## Existing saves and verification

Ordinary ammunition records remain inventory items, including original arrows
and quarrels and their provenance. These records are not prerequisites for firing
and are not depleted by ranged attacks. No expenditure/recovery implementation,
pending recovery state or new control was added.

- `opengold_weapon_catalog_tests` exercises all 38 weapons, including all nine
  ammunition weapons, for all twelve classes without supplying ammunition.
  Normal/critical/miss attacks, range, action expenditure, stale-command rejection
  and combat save continuation are covered.
- `opengold_ammunition_tests` checks all five inert ammunition inventory types
  across all twelve classes, source-backed original conversions, rejected Equip
  atomicity, and campaign/combat continuation.
- `opengold_thrown_weapon_tests` verifies that throwing keeps the weapon.

All 50 native/tool tests passed on runtime `79558fd`. The three focused tests
above passed again after the user selected the exception, with unchanged runtime
inputs: `ctest --test-dir build/mac-check --output-on-failure -R
'^opengold_(ammunition|weapon_catalog|thrown_weapon)_tests$'`.
Log: `/tmp/ammunition-policy-checks.log`. No UI changes require new layout approval
or renders for this policy decision.

## Execution record

One batch and owner, branch `codex/srd-ammunition`; no new issues/tasks/agents.
Recorded Astra/high assignment was retained for the save-compatibility work;
actual settings were not independently verified and no model/runtime switch
occurred. Preparation began 17:58:09 UTC; prior-writer captures committed in
`1fe531d`; inventory compatibility committed in `79558fd` around 18:18 UTC.
Then execution awaited AMMO-1/AMMO-2. The user's explicit requirement change was
handled around 18:51 UTC, before the original 18:58:09 checkpoint. The initial
fixture experiment used unsupported authored keys and was corrected to use
original-item acquisition before freezing; no production codec or compatibility
check was weakened. Preparation is not counted as delivery of SRD ammunition
tracking, and administrative closure is not a new implemented feature.
