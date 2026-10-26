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
claim to implement them. [#57](https://github.com/stdarg/OpenGoldBox/issues/57) is no
longer planned under the changed requirement. Other weapon properties remain
separate. Since [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)
(2026-09-30), [Thrown weapons](THROWN-WEAPONS.md) are treated the same way: a
throw never removes, drops or spends the weapon, which stays held or carried.
Existing action costs, attack rolls, damage, range, proficiency, grip
and shield behavior remain in effect.

## No ammunition items (2026-10-23)

The user went further on 2026-10-23: "We're not dealing with ammo. You buy darts
and you have them. You buy a bow, but not arrows. Keep javelins and darts, but
all other ammo should be removed from the game." Arrows and quarrels, plain,
silver or magic (original item types 73 and 28, `original_item::is_ammunition`),
no longer exist in the game:

- Shops do not sell them: they are out of the `data/shops` records, the game
  skips any a record lists, and the Shop & Temple Manager no longer offers them.
- Fixed treasure lists and monsters' gear leave them out, and a random treasure
  roll for them is rolled again.
- Starting kits hold a bow or crossbow and no ammunition.
- A carried bow is drawn to shoot without them. A shot has the bow's own magic:
  Arrow +1 is gone, and Protection From Normal Missiles stops a shot from a bow
  without magic.
- Campaign format 38 drops older saves, which could hold arrows.

Darts and javelins stay: they are thrown weapons, not ammunition. Each ranged
weapon still records the kind it shoots, which tells a firearm from a bow for
Weapon Mastery.

## Verification

- `opengold_weapon_catalog_tests` exercises all 38 weapons, including all nine
  ammunition weapons, for all twelve classes without supplying ammunition.
  Normal/critical/miss attacks, range, action expenditure, stale-command rejection
  and combat save continuation are covered.
- `opengold_fire_and_acid_tests` draws a carried bow with no arrows, and checks
  that a plain longbow's shot cannot touch a target warded by Protection From
  Normal Missiles while a Longbow +1's can.
- `opengold_thrown_weapon_tests` verifies that throwing keeps the weapon.

The 2026-09-25 checks ran on runtime `79558fd`; the ammunition item test they
included was removed with the items.

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
