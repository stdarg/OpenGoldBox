# Save migration fixtures

`campaign-v1.ogs` and `campaign-v2.ogs` retain the earlier authored campaign
formats used by the save tests.

`campaign-v6-low-con.ogs` was written by rules module 0.6.2, built from commit
`c59ce07`, before the HP-history correction. Do not regenerate it with the
current writer. It contains six level-four Wizards created through the normal
character/advancement APIs, with Sage bonuses excluding Constitution and a
level-four +2 Constitution increase:

| Member | Species | Starting Constitution | Old maximum/current HP | State |
| --- | --- | --- | --- | --- |
| 1 | Human | 3 | 6 / 6 | Healthy |
| 2 | Human | 3 | 6 / 4 | Wounded |
| 3 | Human | 3 | 6 / 0 | Unconscious, one success/two failures |
| 4 | Human | 3 | 6 / 0 | Dead, one success/three failures |
| 5 | Dwarf | 3 | 10 / 8 | Wounded |
| 6 | Human | 15 | 30 / 28 | Wounded; unaffected by the correction |

All have one remaining slot at each spell level. Migration must produce maxima
9, 9, 9, 9, 13 and 30; current HP 9, 7, 0, 0, 11 and 28; and preserve the resource
strings and dead flag. Subsequent saves must reload without applying the
correction again. The fixture uses no original game assets.


## Combat opportunity-reaction migration

The three `combat-v5-*.save` fixtures were written by rules **0.6.4** from
commit `1c75229`, before removal of facing-triggered reactions. Preserve the
original writer bytes, including trailing spaces. They contain no original assets.

- `combat-v5-facing.save`: a Vanguard at (2,3) has turned left and attacked.
  Its action and Bonus Action are spent, Second Wind is exhausted, HP is 20,
  and 25 feet of movement remain. Guards 3 and 4 await facing-only reactions.
  Migration cancels that queue and changes ticket 4 to 5, preserving every
  other actor, log, RNG, effect and clock field.
- `combat-v5-movement.save`: the same wounded/healed actor attacked to the right,
  then tried to move from (2,2) to (1,2). Guard 3 already declined; guards 4 and
  2 remain queued before the step. Migration must preserve that exact queue.
- `combat-v5-movement-resolved.save`: the old writer's reference continuation
  after guards 4 and 2 take their opportunity attacks. Both miss, movement
  finishes at (1,2), and 25 feet remain. New rules must match all actor, log,
  RNG, clock and resource fields after applying the same commands.

## Versatile grip migration

`campaign-v6-grips.ogs` and `combat-v7-grips.save` were written by the **0.6.6**
libraries built from commit `5daabbb`, using that commit's headers. Do not
regenerate them with the current writer. They contain no original assets.
Members 1–7 carry Quarterstaff, Spear, Battleaxe, Trident, Longsword, Warhammer
and War Pick respectively; member 7 is a recruited NPC. The old hand use is
two, two, two, two, one, one, one. Member 1 is wounded at 5 HP with one Second
Wind remaining. The combat seed is 42 and Battleaxe wielder 3 acts first.
The campaign asset identity is `grip-fixture`. Migration preserves hand use,
wounds and recovery, introduces the corrected two-handed melee dice, and
retains deterministic subsequent saves and combat commands.

## Feature and feat grant migration

`campaign-v7-grants.ogs`, `combat-v8-grants.save` and
`combat-v8-grants-continued.save` were written by rules **0.6.7** from commit
`abda7ea`, using its headers and compiled libraries. Do not regenerate with the
current writer. The campaign asset identity is `grant-fixture`; all data is authored.

| Member | Class/background | Level | Level-four choice | Max/current HP | AC |
| --- | --- | --- | --- | --- | --- |
| 1 | Fighter/Soldier | 1 | None | 12/9 | 16 |
| 2 | Fighter/Soldier | 4 | +2 Constitution | 40/37 | 16 |
| 3 | Fighter/Sage | 4 | Defense | 40/37 | 17 |
| 4 | Fighter/Sage | 4 | Savage Attacker | 40/37 | 16 |
| 5 | Dwarf Wizard/Sage | 4 | +2 Constitution | 38/35 | 12 |
| 6 | Cleric/Soldier | 3 | None | 24/21 | 16 |

Fighters have one Second Wind remaining; the casters have one slot of each
available level remaining. Everyone carries a Longsword; all except the Wizard
wear Chain Mail. The combat seed is 42. Its continued reference applies twelve
commands, choosing the first legal melee attack or otherwise End Turn. Migration
must preserve the exact resulting damage, per-turn feat usage, resource/turn
budgets, RNG and log. Campaign history identifies the grants' sources; the older
combat recipes only encode effects and must not acquire fabricated provenance.

## Training migration

`campaign-v8-training.ogs` and `combat-v8-training.save` were written by rules
**0.6.8** from commit `58e8955`, before training choices existed. The original
headers and compiled libraries produced these files; do not regenerate them with
the current writer. The authored campaign uses asset identity `training-fixture`.
Its four Human characters are Rogue/Criminal level 1, Rogue/Soldier level 1,
Fighter/Soldier level 4 with Defense, and Wizard/Sage level 4 with +2 Constitution.
All are wounded by 2 HP; the Fighter has one Second Wind and the Wizard has one
slot at each spell level remaining. Combat seed 42 retains their original PC6
recipes and state. Migration must not invent optional training choices, heal
wounds, refill resources or rewrite the paused combat recipes.

## Rest resource migration

`campaign-v9-rest.ogs`, `combat-v8-rest.save` and
`combat-v8-rest-continued.save` were written by rules **0.6.9** from commit
`7b55636`, before spendable Hit Dice existed. The original libraries and headers
produced these files; do not regenerate them with the current writer. They use
asset identity `rest-fixture` and authored Human characters: level-four Fighter,
Wizard and Cleric, plus level-one Rogue/Criminal. All training is selected. The
Fighter uses Defense and a Longsword in two hands; both casters increased
Constitution at level four. Each character is wounded by 5 HP. Fighter has one
Second Wind, Wizard one slot of each level and an active Blinded effect, Cleric
two level-one slots and no level-two slots. Campaign time and prior rest offsets
are nonzero. The combat starts with the Fighter's departure from the enemy's
reach; the second file resolves that pending opportunity attack using the old
writer. Migration must preserve every original field and outcome while adding
only unspent Hit Dice to format-nine combat actors.

## Recovery-clock migration

`campaign-v10-recovery.ogs`, `combat-v9-recovery.save` and
`combat-v9-recovery-continued.save` come from commit `1adfc63`, module **0.6.10**.
A frozen copy of that commit's headers, content and built libraries generated
these files; do not regenerate them with the current writer. Asset identity is
`recovery-fixture`. Five authored level-two Human PCs include a Stable Fighter
with one spent Hit Die/Second Wind, an unstable Wizard with two successes/one
failure and Blinded/spent slots, a dead Fighter, a conscious Fighter and an
unstable reserve. Campaign time is 1234 minutes plus 5678 ms, with a prior rest
completion on the Stable Fighter. All language choices are completed.

The combat contains the three living active PCs and an authored enemy. Initiative
order is IDs 4, 1, 99, 2. It pauses on Fighter 4's departure from enemy reach; the
second file declines the reaction without elapsed time. Old data has no recovery
clocks. Migration adds a known 4500 ms until Wizard 2's next initiative slot and
a deferred legacy recovery roll for Stable Fighter 1, preserving every other
field and the old writer's pending-movement continuation. Campaign migration
retains its original opaque states without a load-time roll.

## Typed damage and Dwarf resistance migration

`campaign-v10-damage.ogs`, `combat-v10-damage.save` and
`combat-v10-damage-continued.save` were generated by the frozen **0.6.12** writer
from commit `4557cf2`, before any F05 code/content changes. A separate copy of its
headers, content and built libraries generated them; do not regenerate with the
current writer. Asset identity is `damage-fixture`. The five level-two members
alternate Dwarf/Human, with a Stable Dwarf's rolled 4321000 ms remaining and one
spent Hit Die/Second Wind, an unstable Human Wizard with Blinded/spent slots, a
dead Dwarf, a conscious Human and an unstable Dwarf reserve. All languages are
selected. The combat pauses on member 4's departure from enemy reach and its
continued file declines that reaction. New campaign reconstruction adds only the
fixed species resistance; combat continuation retains the prior writer's actor
rows/recipes, timers, resources, RNG and movement, replacing only the identity.

## Temporary HP migration

`campaign-v10-temporary-hp.ogs`, `combat-v10-temporary-hp.save` and
`combat-v10-temporary-hp-continued.save` were generated by the frozen **0.6.13**
writer at commit `eea7368`. A separate copy of its headers, content and built
libraries generated these before the Temporary HP changes. Do not regenerate
them with the current writer. Asset identity is `temporary-hp-fixture`.
They use the same five-member mortality/resource scenario as the damage fixtures,
but contain validated PC8 Dwarf resistance grants. None has Temporary HP because
the old writer did not support it. Combat migration appends only the empty pool
to each actor and updates format/module identity; declining the saved opportunity
decision matches the frozen continuation. Campaign migration changes only module
identity and retains all existing state.

### Adrenaline Rush introduction (0.6.14 writer)

`campaign-v10-adrenaline.ogs`, `combat-v11-adrenaline.save` and
`combat-v11-adrenaline-continued.save` were generated from commit **efbe48d**
before the #197 writer changed. Source and static libraries were frozen under
`/tmp/opengold-adrenaline-legacy`; its generator checked module version 0.6.14.
These are five level-two normally created characters (alternating Orc and Dwarf,
one Wizard and four Fighters), including an Orc reserve. They retain wounds,
spent Wind/slots/Hit Dice, zero-HP recovery deadlines, a deceased character and
Blinded. The first Orc also has 7 Temporary HP from `spell:fixture`. The campaign
uses time 1234 minutes plus 5678 ms and an earlier individual Long Rest.

The combat pauses before actor 4 leaves an enemy's reach; the continued file
records the old writer's Decline result. Migrating both must produce the same
continuation without changing RNG, time, pool, existing resources or Dwarf
grants. Orc use capacity is new and starts full; the campaign independently
checks the exact resulting SRD7 state. Do not regenerate these files with the
current writer.

## Heavy weapon requirements (0.6.15 writer)

`campaign-v10-heavy.ogs`, `combat-v12-heavy.save` and
`combat-v12-heavy-continued.save` were generated with frozen source and static
libraries from commit **42a7f6c**, module **0.6.15**, before #55 changes. The
generator checked that version. Do not regenerate these with the current writer.

The campaign contains a Dwarf Fighter (Strength 12, Dexterity 15, Greatsword)
and an Orc Wizard (Strength 15, Dexterity 12, Longbow), both authored through
normal character rules with selected languages. Second Wind, Magic Missile and
Adrenaline Rush have spent resources; the Orc retained 7 Temporary HP from
`spell:fixture` through Keep current. Existing PC9 species grants must survive
migration. Asset identity is `heavy-fixture`.

The combat uses seed 1 and pauses an enemy's movement out of the Dwarf's reach.
The continued file records Decline. Migration changes only module identity and
must reproduce that exact continuation. Choosing Opportunity Attack instead
now correctly selects 11 over 16 with Disadvantage; the old writer omitted that
penalty. New save/restore continuation must agree on the corrected outcome.

## Complete weapon catalog (0.6.16 writer)

`campaign-v10-catalog.ogs`, `combat-v12-catalog.save` and
`combat-v12-catalog-continued.save` were generated by the frozen source/static
libraries of commit **93d3c32**, module **0.6.16**. The generator checked its
module identity; do not regenerate with the current writer. Asset identity is
`catalog-fixture`.

Three normally authored characters carry the old `longbow` key: a Dwarf Fighter
with original type-45 provenance (named Heavy crossbow by the fixture's legacy
generic label), an Orc Wizard with Longbow/type-43 provenance, and a Human
Fighter with original type 45 but no source record. All three retain Longbow:
the campaign's reviewed type-45 interpretation is Fine Composite Long Bow, and
a display label alone is not evidence for changing an original conversion.
All have wounds; the Dwarf has one spent Wind, while the Orc has one spent slot,
one spent Rush and 7 Temporary HP. Time is 123 minutes plus 456 milliseconds.
Campaign migration preserves the original resource descriptions and changes
only module identity.

The two combat files record seed 13 before and after one ranged attack. Combat
recipes lack original-item provenance, so they retain their recorded Longbow
and match the old writer's exact result, resources and random continuation.

`weapons-srd-5.2.1.tsv` is a separate, source-verified table of all 38 weapons
from SRD 5.2.1 p.91. Tests compare every definition/property, including exact
quarter-pound/copper units, without reading expected values from implementation.

## Armor catalog compatibility

`campaign-v10-armor.ogs`, `combat-v12-armor.save` and
`combat-v12-armor-continued.save` were written by **0.6.17**, using headers and
compiled libraries frozen from commit `5dfabb7`. Do not regenerate these with
current code. Asset identity is `armor-fixture`; there are no original assets.
Three normally created characters (Dwarf Fighter, Orc Wizard, Human Fighter)
carry Longswords. The Fighters wear Chain Mail; the Wizard wears untrained
Leather. The first two armor items retain authored original type 55/50 source
records. Everyone is wounded; Second Wind, slots and Adrenaline Rush have
spent uses, the Orc has sourced Temporary HP, and the clock is 123 minutes plus
456 milliseconds. The combat uses seed 13 and records an actual melee attack
continuation. Migration changes only module identity; it must not rewrite
resources, descriptions, equipment, grants, RNG or clocks.

`armor-srd-5.2.1.tsv` is an independent transcription of the SRD 5.2.1 p. 92
table. Its thirteen rows cover twelve suits and Shield. Times are seconds;
Shield's zero times denote a Utilize action, not a free equipment change.


## Wizard spell knowledge compatibility

`campaign-v10-spells.ogs`, `combat-v12-spells.save` and
`combat-v12-spells-continued.save` were written by **0.6.18**, using headers and
libraries frozen from commit **e3440d9**. Do not regenerate with current code.
Asset identity is `spells-fixture`; the fixtures contain no original assets.

Three Sage Wizards (Dwarf level 1, Orc level 3, Human level 4) carry Wands and
have two wounds each. The old creation preset implicitly enabled Fire Bolt and
Magic Missile. All advancement histories select Magic Missile at level 2. At
level 3 the Orc selected Scorching Ray/Blindness, while the Human selected Magic
Missile/Scorching Ray and then only Blindness at level 4. Migration must recover
book entries from those actual choices, retaining spells no longer prepared
without filling missing SRD selections. Their spent slots, Hit Dice, sourced
Temporary HP, Orc Rush use, equipment and clock (123 minutes + 456 ms) remain.

The combat uses the Orc and a Vanguard target, seed 13. The second file records
an actual Scorching Ray cast. Old PC9 recipes lack book history and remain
unchanged; only module identity changes. Spell damage, spent resources, action
state, RNG and clock must match the frozen writer exactly.

With 0.6.19, older equipment/resource campaign fixtures also gain explicit Fire
Bolt and Magic Missile grants. `campaign_fixture.h` constructs that limited
expected ledger change independently, retaining every other body byte. Earlier
sections' identity-only migration descriptions refer to their delivery versions.

## Savage Attacker compatibility

`campaign-v10-savage.ogs` and the four `combat-v12-savage*.save` files were
written by **0.6.19**, using headers and libraries frozen from commit
**66d1789**. Do not regenerate them with current code. Asset identity is
`savage-fixture`; they contain no original assets.

The campaign contains a level-one Dwarf Soldier Fighter and a level-four Orc
Sage Fighter who selected Savage Attacker through advancement. Both wield
Longswords, have two wounds and spent Second Wind, and retain 7 sourced
Temporary HP; the Orc has one remaining Rush. The clock is 123 minutes plus
456 milliseconds. The campaign body changes only module identity.

The combat pair uses the Dwarf and a Vanguard at seed 13, before and after a
normal melee hit. The `-reaction` pair pauses the Vanguard's leave-reach movement
and records the old writer's automatic Savage Attacker opportunity hit. Choosing
Use and the higher result in the new rules must match those damage, resource,
RNG, movement and time outcomes, with two extra command revisions. Already
completed old hits are never repeated. Format 13 appends an empty decision to
old checkpoints; all previous fixture oracles include that explicit extension.

## Spell component eligibility migration

`campaign-v10-components.ogs`, `combat-v13-components.save` and
`combat-v13-components-continued.save` were written with the actual **0.6.20**
rules libraries from **46c54da**, before the Somatic eligibility fix. The new
suite's freeze path ran before changing the production rules. Do not regenerate
these bytes with the current writer.

They contain an authored level-three Cleric with Cure Wounds, Healing Word and
Blindness prepared through ordinary advancement, equipped Mace and Shield,
17/27 HP, 37 gold and a campaign clock/RNG. The prior writer incorrectly offers
Cure Wounds despite occupied hands. The continued combat file records a legal
Healing Word cast. Migration changes module identity only; the new writer
rejects Somatic casts but reproduces the prior verbal cast exactly. No original
assets are included.

## Explicit Wizard cantrip migration

`campaign-v10-poison.ogs`, `combat-v13-poison.save` and
`combat-v13-poison-continued.save` were generated by actual **0.6.21** libraries
from **8f0d05d**, before changing production rules. The `freeze()` path in
`poison_spray_tests.cpp` was run with its extra argument before the upgrade.
It refuses to run under the current writer. Do not regenerate these fixtures.

The level-three Orc Sage Wizard retains the historic Fire Bolt cantrip and Magic
Missile book entry, three wounds, equipped Wand, 37 gold, a spent slot, and
123 minutes plus 456 milliseconds of campaign time. The combat continuation
records an actual Fire Bolt cast. Campaign migration adds an absent cantrip
field and updates module identity; combat migration updates identity only.
No Poison Spray grant is invented and the continuation must remain exact.

## Explicit Cleric cantrip migration

`campaign-v11-sacred.ogs`, `combat-v13-sacred.save` and
`combat-v13-sacred-continued.save` were generated using actual **0.6.22**
libraries from commit **ab65076**, before production rules were changed.
The `freeze()` path in `sacred_flame_tests.cpp` refuses to run under the current
writer. Do not regenerate these fixtures. Asset identity is `sacred`; no
original game assets are included.

An authored level-three Orc Sage Cleric carries a Mace and starts at one HP.
An actual self-targeted Cure Wounds spends one slot and partially heals the
character. The campaign retains 37 gold, RNG 789 and 123 minutes plus 456
milliseconds before combat handoff. The combat pair records the following
Cleric turn, before and after a second actual Cure Wounds cast. Migration must
change module identity only, preserving PC11 access, wounds, resources,
equipment, grants, RNG and clocks. No Sacred Flame selection is invented.

## Action Surge compatibility

`campaign-v11-surge.ogs`, `combat-v13-surge.save` and
`combat-v13-surge-continued.save` were written using actual **0.6.23** libraries
from **2ae050f**, before production changes. The `freeze()` path in
`action_surge_tests.cpp` refuses to run under the new writer. Do not regenerate
these files. Asset identity is `surge`; no original game art is included.

The campaign contains level-two Human, level-one Dwarf and level-four Orc Sage
Fighters, with Longswords, wounds and 37 gold each. The Human spent Second Wind
in a real encounter; campaign RNG is 789 and time is 123 minutes plus 456 ms.
Migration adds only the fixed level-two Action Surge grant to the Human and Orc.
Their existing resources remain unchanged; the newly supported feature starts
available. The Dwarf gets no grant at level one. The combat files record an
actual Dash before/after pair following Second Wind. Their PC12 access, spent
resources, wounds, initiative, RNG, action state and clock remain exact apart
from module identity; no extra action is invented in the old encounter.

### Ray of Frost prior-writer combat

`combat-v13-ray-before.save` is the actual 0.6.24 writer's
`build/mac-check/poison-fixtures/known.save`, generated by
`opengold_poison_spray_tests` before Ray of Frost implementation. The fixture
was copied unchanged after the shared selector increment `bc2d152` (whose rules
were still 0.6.24). It records explicit Wizard Fire Bolt/Poison Spray knowledge.
The Ray of Frost regression requires byte-identical restoration apart from the
module version and verifies that migration invents no Ray of Frost grant.

### Sage fixed training (rules 0.6.25)

`campaign-v11-sage-before.ogs` was written by the actual 0.6.25 libraries at
`490ed19`, before the Sage grant changes. `freeze_sage()` in training_tests.cpp
records the reproducible setup and rejects execution with a newer writer. The
level-3 Sage Wizard has explicit language/cantrip choices, wounds and spent slots;
it has none of the three newly supported Sage grants. The fixture is unedited.

### Acolyte/Soldier fixed training (rules 0.6.26)

`campaign-v11-backgrounds-before.ogs` was written by the actual 0.6.26 libraries
at `0004064`, before the additional background grant changes. The test-only
`freeze_backgrounds()` setup ran with those libraries and requires that version.
It contains level-three Acolyte Cleric and Soldier Fighter characters with explicit
languages, wounds and spent spell/feat resources. Neither has the newly supported
fixed proficiencies. The fixture is unedited.

### Archery prior writer (rules 0.6.27)

`campaign-v11-archery-before.ogs` and `combat-v13-archery-before.save` were written
by the actual 0.6.27 libraries at `131942d`, before Archery changes. The test-only
`freeze()` in archery_tests.cpp ran with those libraries and requires that version.
The level-four Sage Fighter selected Defense, has three missing HP and one
remaining Second Wind; the combat fixture equips a Shortbow. Both files are
unedited, contain PC16 where applicable and have no Archery grant. Migration must
preserve their entire bodies except the module version (and campaign checksum).

### Fighter starting styles prior writer (rules 0.6.28)

`campaign-v11-styles-before.ogs` and `combat-v14-styles-before.save` were written
by the actual 0.6.28 libraries at `dda8d0c`, before starting-style changes. The
test-only `freeze_styles()` in training_tests.cpp ran against that writer and
requires its version. Four Human Sage Fighters have explicit languages: level
one, level-four Defense, level-four Archery and level-four Constitution +2.
Each has two missing HP and spent Second Wind. Both files are unedited; combat
contains PC17 profiles. Migration must preserve their bodies except module
identity and campaign checksum, without inventing a starting style.

### Great Weapon Fighting preparation (rules 0.6.29)

`campaign-v11-gwf-before.ogs` and `combat-v14-gwf-{first,second,resolved}.save`
were written by the actual 0.6.29 libraries at `88c2649`, before extracting the
damage roller. The `freeze_gwf()` setup in damage_tests.cpp requires that version.
The Human Soldier Fighter has starting Defense, level-four Archery, wounds and
spent Second Wind. Seed zero produces a critical Greatsword hit; the combat
files capture the first Savage Attacker decision, second rolled result and its
application. Files are unedited. The foundation checks exact campaign bytes and
both real command transitions, including RNG, HP and action/feat expenditure.

### All-class skill choices prior writer (rules 0.6.29)

`campaign-v11-class-skills-before.ogs` and
`combat-v14-class-skills-before.save` were written by the actual 0.6.29 libraries
at `8da4565`, before the class skill catalog changed. `freeze_class_skills()` in
training_tests.cpp requires that writer. All twelve classes have recorded
languages; Rogue has skills/Expertise and Fighter has Archery. Fighter, Cleric
and Wizard have level-four histories; everyone has wounds, with spent resources
for the supported classes. Campaign members are in reserve to fit the existing
six active-PC limit; the combat fixture includes all twelve participants.
The files are unedited. Migration preserves all data except module identity and
campaign checksum, leaving the newly supported selections pending.

### Bard instrument choices prior writer (rules 0.6.30)

`campaign-v11-bard-instruments-before.ogs` and
`combat-v14-bard-instruments-before.save` are generated by the actual 0.6.30
production libraries at `80a7d1a`, before instrument entitlement changes.
The version-guarded `freeze_bard_instruments()` in training_tests.cpp creates four
level-one Bards covering Sage, Criminal, Acolyte and Soldier. All have explicit
Performance/Persuasion/Perception and Elvish/Dwarvish, two missing HP and recorded
resource state. Combat profiles are PC19. Both files remain unedited; their
round-trip baseline permits only module identity and campaign checksum changes.

### Monk tool choice prior writer (rules 0.6.31)

`campaign-v11-monk-tools-before.ogs` and `combat-v14-monk-tools-before.save` were
written using the actual 0.6.31 libraries at `1c5f9bf`, before Monk tool changes.
`freeze_monk_tools()` is version guarded. Four level-one Monks cover all current
backgrounds with Acrobatics/Insight, Elvish/Dwarvish, two missing HP and recorded
resources. PC20 combat recipes and all fixture bytes remain unedited. Migration
checks permit only module identity/checksum changes and keep tool choices pending.

### Druid Herbalism Kit prior writer (rules 0.6.32)

`campaign-v11-druid-herbalism-before.ogs` and
`combat-v14-druid-herbalism-before.save` were generated by the actual 0.6.32
libraries at `c0457f8`, before the fixed proficiency changed. The version-guarded
`freeze_druid_herbalism()` creates four level-one Druids across all backgrounds,
with Nature/Medicine, Elvish/Dwarvish, two missing HP and recorded resources.
Combat recipes are PC21; fixture bytes remain unedited. Campaign migration adds
only the fixed Druid proficiency and identity/checksum; old combat stays exact
except module identity.

### Soldier Gaming Set prior writer (rules 0.6.33)

`campaign-v11-soldier-gaming-before.ogs` and
`combat-v14-soldier-gaming-before.save` were generated by the actual 0.6.33
libraries at `ba9673c`. The version-guarded `freeze_soldier_gaming()` covers all
12 Soldier classes with complete prior training, Savage Attacker, wounds and
resource state. Fighter/Cleric/Wizard have level-four advancement. Campaign PCs
are in reserve; combat includes all twelve. PC22 profiles and fixture bytes
remain unedited. Migration changes only identity/checksum; missing Gaming Set
choices stay pending until explicitly completed.

## Cunning Action baseline

`campaign-v11-cunning-before.ogs` and `combat-v13-cunning-before.save` were
written by actual rules 0.6.34 at `a2aed46`, before Cunning Action changes, using
`opengold_training_tests --freeze-cunning`. Four level-one Orc Rogues cover all
backgrounds and complete training. Their campaign wounds and combat Action Dash
plus Adrenaline Rush expenditure must survive migration without refunds.
The generator refuses a newer module; keep these fixtures unchanged.

## Sneak Attack baseline

`campaign-v11-sneak-before.ogs` and the three `combat-v15-sneak-*.save` files
were produced by rules 0.6.35 at `3846c21`, using
`opengold_training_tests --freeze-sneak` before live Sneak Attack changes.
They cover normally attained Rogue levels one/two, Soldier training, wounds,
wealth, spent Bonus Action Dash and an attack awaiting Savage Attacker. The
second-roll/resolved files are actual continuations, not fabricated expectations.
Keep these unchanged after the feature is introduced. See `SNEAK-ATTACK.md`.

## Unconscious enemy transit baseline

`combat-v13-unconscious-transit-before.save` and
`combat-v13-unconscious-transit-dash.save` were written by actual rules 0.6.35
at `ff297ef`, before the movement correction, with
`opengold_rules_tests --freeze-unconscious-transit`. The baseline contains a
stable Unconscious blocker, a conscious guard and an active mover. The second
file is the actual prior Dash continuation. Preserve their bytes.

## Pre-Eldritch Blast Warlock fixtures

`campaign-v11-eldritch-before.ogs`, `combat-v13-eldritch-before.save` and
`combat-v13-eldritch-continued.save` were generated by actual rules 0.6.36 at
`15fcda5`, before production changes, using `eldritch_blast_tests.cpp --freeze`.
The old Sage/Orc Warlock has missing cantrip choices, wounds, equipment and wealth;
the combat spent Dash and Adrenaline Rush before the saved End Turn continuation.
The generator rejects newer writers. Tests preserve bytes except module identity
(and campaign checksum), never synthesizing old files with the new serializer.

## Pre-Shocking Grasp fixtures

`campaign-v11-shocking-before.ogs`, `combat-v15-shocking-before.save`, and
`combat-v15-shocking-continued.save` come from actual rules 0.6.37 at `310319e`,
captured before production edits using the Shocking Grasp test's guarded freeze
entry point. The level-three Wizard knows Ray of Frost. Saved combat retains its
hit's FX2 state, spent Action/Adrenaline Rush, and continuation across both turns.
Tests compare every byte except module identity and campaign checksum, preserving
missing Shocking Grasp access rather than inventing a new selection.

## Pre-Warlock Poison Spray fixtures

`campaign-v11-warlock-poison-before.ogs`, `combat-v13-warlock-poison-before.save`,
and `combat-v13-warlock-poison-continued.save` were captured with actual rules
0.6.38 at `3b7e943`, before production edits, using
`opengold_eldritch_blast_tests --freeze --warlock-poison`. The guard rejects newer
writers. They preserve the old explicit Eldritch Blast choice, pending second
cantrip, wounds/wealth and spent Dash/Adrenaline Rush, then reproduce the next
turn's Eldritch Blast attack. Current tests compare exact bytes except module
identity and campaign checksum.

## Pre-Sorcerer cantrip fixtures

`campaign-v11-sorcerer-cantrip-before.ogs`,
`combat-v13-sorcerer-cantrip-before.save` and
`combat-v13-sorcerer-cantrip-continued.save` were captured with actual rules
0.6.39 / PC27 at `6988432`, before Sorcerer cantrip access. The freeze helper in
`sorcerer_cantrip_tests.cpp` requires that prior writer and must not be run with
current rules. The Orc/Sage Sorcerer has no selected cantrips, equipped staff,
wounds, 37 units of wealth, and campaign RNG 789. Combat starts at seed 2, spends
Dash and Adrenaline Rush, then the continuation ends the turn. Tests require
byte-identical recipes/budgets/random continuation aside from module identity
and campaign checksum; no new cantrip is inferred on loading.

## Pre-rest-activity campaign fixtures

`campaign-v11-rest-activity-before.ogs` and
`campaign-v11-rest-activity-spent.ogs` were produced by the actual `aeeb3b7`
writer (rules 0.6.40), before rest activity or format 12 changes, using
`opengold_campaign_rest_tests --freeze-rest-activity`. The guard requires the old
session allocation as well as format 11 and rules identity. The level-four
Fighter/Wizard party starts wounded with spent resources, clock 1000 minutes plus
4321 milliseconds and RNG 29. The first file retains a completed Short Rest
spending session; the second is the actual next Fighter Hit Die continuation.
Tests require exact bytes, preserving wounds, resources, tickets, time and RNG.
Do not regenerate these fixtures using the new engine.

### Natural sleep's actual prior writer

`combat-v15-sleep-before.save` and `combat-v15-sleep-continued.save` were copied
unchanged from the existing `cunning-fixtures/available.save` and `spent.save`
written at 2026-09-25 01:44:55 UTC by the pre-sleep rules 0.6.40 binary. They
record a level-two Rogue before and after Bonus Action Dash followed by ordinary
Dash. The source files existed before the sleep implementation and were checked
for their original module identity before copying. `opengold_natural_sleep_tests`
restores and continues the first, then compares with the second byte-for-byte
apart from the module identity. Do not regenerate these with the new writer.


## Held equipment and rest import

`campaign-v13-detached.ogs` and `combat-v16-ground.save` were written by
rules 0.6.42 and the Core writer from commit `381b2f8`, before adding rest-session
ground equipment. They contain only authored data. The campaign asset identity
is `detached-items`; one party member's original sword has moved to an ally and
one shield remains detached. The combat has a sleeping owner, an adjacent awake
ally, and a dropped sword/shield. Preserve these bytes; the new rest import must
not change their exact save/load continuation. `natural_sleep_tests.cpp` verifies
both fixtures. They were captured from the built previous libraries, not generated
by the new campaign 14 writer.


## Tactical Mind's actual prior writer

`campaign-v11-mind-before.ogs`, `combat-v15-mind-before.save`, and
`combat-v15-mind-continued.save` were generated before Tactical Mind changes,
using the rules/Core libraries from `36e574c` (rules 0.6.42, PC28) and
`opengold_action_surge_tests --freeze-mind-baseline`. Only the capture/test driver
was added; the gameplay libraries were unchanged. The campaign asset identity
is `mind-before`. All data is authored, with no original game archive content.

The level-two Fighter begins wounded and actually spends one Second Wind,
leaving one use. Equipment, 37 gold, campaign clock 123 minutes plus 456 ms and
campaign RNG 789 remain recorded. The second combat file is the actual result
of Action Surge followed by Dash from the first. The compatibility regression
requires byte-exact combat continuation apart from module identity and checks
campaign resources, equipment, time, RNG and canonical reload. Preserve these
bytes when the check/feature implementation changes.

SHA-256:

- `campaign-v11-mind-before.ogs`: `2c97462b62d896f8a77aebb901c38fe4655715d4b1749a880108d33c86aad179`
- `combat-v15-mind-before.save`: `44d450a76575dc2052ad946ac7cbf2c416e6e603046d25450aebf31e91944086`
- `combat-v15-mind-continued.save`: `e40bb504963583c4e1c7988e512b4f7f5f5516bec912e848ea089393116dc720`

## Chill Touch prior writer (0.6.43)

`campaign-chill-0.6.43.ogs`, `combat-chill-0.6.43.save`, and
`combat-chill-0.6.43-continued.save` were captured with the unchanged gameplay
libraries at `fba0ca3`, using the added
`opengold_chill_touch_tests --freeze-chill-baseline` driver. It requires exactly
module 0.6.43; never regenerate these bytes under a later writer. An authored
level-four Orc Wizard actually casts Chill Touch on herself. The campaign asset
identity is `chill-baseline`; the continued combat records two End Turn commands.
The regression requires byte-exact combat migration/continuation except module
identity, plus campaign persistence and expiry without invented healing.

### Champion's actual prior writer

Captured before production Champion changes with module 0.6.45 / PC30 from
runtime `ce04673` (main `2966d25`). The capture-only addition to
`action_surge_tests.cpp` runs with `--freeze-champion-baseline` and rejects a
later module identity. Do not regenerate these files with the new implementation.

The campaign contains normally attained Fighter levels 1–4 with wounds,
equipped swords, wealth and a nonzero campaign clock/RNG. The combat is an
ordinarily attained level-four Fighter with Savage Attacker: pending weapon
choice, a subsequent actual critical hit after Action Surge, and resolved
Savage damage. The original writer offers no Champion movement. All three
combat captures are format 14; filenames reflect the actual wire header.

SHA-256:

- `campaign-v11-champion-before.ogs`: `e39009d03b4a5b55293d8de804dfab41a46bfa08da1683b8ec09a4b9fd6d2cf8`
- `combat-v14-champion-before.save`: `323bb4a10d90d1896a81a966673462ed1fb050c92a310a3f8512e7a949ca91bf`
- `combat-v14-champion-critical.save`: `35f53b2dc97a1d5e6f8d688ec278601077bb310cb730821d4cd7e330f3ef745d`
- `combat-v14-champion-resolved.save`: `0752aa3333fe099516a7b2649e55c769d7bc060d3f4dbf61dc7f0b589481fdbf`

The pre-change regression passed in `/tmp/champion-baseline-tests.log` before
runtime edits. `champion_prior_writer` checks byte-exact combat continuation and
ordinary campaign migration/canonical reload. Future fixed-grant migration
expectations must name their additions rather than replacing these captures.

## Pre-Thrown-inventory writer (0.6.46)

The `*-thrown-*` files were captured with the actual **0.6.46** writer from
runtime `6324a51` / main `74dfb32`, before thrown-inventory gameplay changes.
`opengold_thrown_weapon_tests --capture-prior-writer` is guarded to that module
version; do not regenerate these frozen files with a later writer.

The ordinary level-four Soldier Fighter carries three each of all seven Thrown
weapons, equips Javelin and Shield, retains wounds, wealth 37 and a fixed campaign
clock/RNG. The combat starts with the shield on the ground, giving a real
held/ground-item ledger, and spends Second Wind before the first capture.
The captured sequence reloads the actual format-16 save, makes a critical ranged
Javelin attack, uses Savage Attacker's second roll, enters Champion free movement,
declines movement, then uses Action Surge for a second throw. This deliberately
preserves the old writer's unlimited-throw behavior for in-flight old encounters;
it is not acceptance evidence for the new inventory semantics.

Capture continuations start after an actual reload. The old format-16 codec
implicitly enables its movement-format flag on loading; a later format-18
Champion state exposes that flag. An initial unsaved-constructor continuation
therefore differed from the loaded sequence. The final captures retain the real
writer's loaded behavior without editing fixture bytes or production codecs.
Campaign migration checks compare the complete body and all item identities and
quantities; combat replay compares complete saves including RNG and budgets.

SHA-256:

- `campaign-v11-thrown-before.ogs`: `c8d5c8538342555863d0aa9dcf2feb677193773cb7ca905806b805e8c4bb7ecf`
- `combat-v16-thrown-before.save`: `284fa08d2587a831625b7bb185ea7ad496805b8edc82134f382d7de1e885e556`
- `combat-v16-thrown-choice.save`: `fdba9f4e421ddb605755c4c1248f17d82ca3a483477d4601085f07813ad21df6`
- `combat-v16-thrown-continued.save`: `54b136fde4c298a2f7a3e5925c6eef3e451c843df2a053006ada042a625f6227`
- `combat-v18-thrown-free.save`: `5f5cc8504126c476175fc0bc619927550626668737883e14efdb3bfbd738fbbe`

## Ammunition baseline, actual 0.6.47 writer

Captured before any #57 runtime changes, using the released rules/Core sources
at `dbf911d` (runtime `5d7813d`), with the new capture-only test harness.
`opengold_ammunition_tests --capture-prior-writer` requires module **0.6.47**.
Never regenerate these files with a later writer. The level-four Fighter carries
two original arrow stacks (7 and 3), five original quarrels and two daggers, and
equips a longbow. Ammunition enters through the actual purchase conversion and
retains original provenance; the old writer records it as unsupported equipment.
The daggers activate the actual format-19 physical ledger. Ammunition is present
in the campaign inventory but absent from that old ledger; old in-flight combat
did not expend it. Capture starts after a genuine reload and continues two
ranged attacks separated by Action Surge. These are compatibility fixtures,
not evidence that #57 is implemented. Tests compare full combat bytes/RNG/budgets
and campaign stack identities/quantities after loading.

SHA-256:

- `campaign-v11-ammunition-before.ogs`: `39dc9d8edcc963f9d348caf3dd011c4bfca5c774a4ea0b79fe0bab28c82ffecb`
- `combat-v19-ammunition-before.save`: `86b6047b71b79d8cf1d3d61bc79c66fd0b03efd62427de51a488486e70c8898d`
- `combat-v19-ammunition-continued.save`: `e8783194ec041a919b68c7c94bf0785a1d996e1d3d97b2aaa63d1898d3759428`

## Arcane Recovery baseline, actual 0.6.48 writer

Captured with runtime `79558fd`, before Arcane Recovery changes. The guarded
`opengold_arcane_recovery_tests --capture-prior-writer` uses ordinary Wizard
creation/advancement to levels 1–4, spends first-level slots through actual Magic
Missile commands (and a second-level slot at levels 3–4), and completes a Short
Rest with its spending ticket retained. The test content adds a 1000-HP target
to the ordinary pack so spell expenditure cannot prematurely end the encounter.
The fixture reader uses that same deterministic content extension.

The level-four combat capture reloads before saving its baseline, then advances
the turn and casts another Magic Missile. Full continuation bytes, budgets/time
and RNG are compared. These records contain no Arcane Recovery grant or use;
future migration must preserve spent slots and earned rest eligibility. Do not
regenerate with a new module version.

SHA-256:

- `campaign-arcane-level1.ogs`: `6e970542122b5672e82a41c2600fa10f2b4d0724e55d958bc889f326200df02a`
- `campaign-arcane-level2.ogs`: `6ed6d09a334b55ca619a96b68f6b43031b9a4394a02f5acf79bbae9cf9e86b5e`
- `campaign-arcane-level3.ogs`: `b293508cccc895d6aca9317e791d25269e223e3700e776be563e263a2d58e2a8`
- `campaign-arcane-level4.ogs`: `fa4585d360444045c784dde88b8c7b20dc2198afda64565425aff0b7d42f4f14`
- `combat-arcane-before.save`: `9147f2acb99105bf987c92e08c7feb9d0f0666af4174d68ea0c09d4ccab26f9f`
- `combat-arcane-continued.save`: `5b15798e4b110d3eda130982dcfe793219c2047ab8475b4f364f619c8cdcd298`


## Wizard spell-choice workflow baseline (SRD 0.6.50)

`campaign-wizard-choices-level1.ogs` through `level4.ogs` and
`combat-wizard-choices-before.save` / `combat-wizard-choices-continued.save` were
captured with the actual 0.6.50 library before spell-choice workflow changes,
using `opengold_spell_access_tests --capture-wizard-choices` on 2026-09-25.
They preserve explicit cantrips, acquired Scholar, known but unprepared book
spells, wounds, spent slots and spent Arcane Recovery. Campaign formats 11/15
and the combat continuation are old-writer output, not synthetic rewrites.
See [the frozen packet](../../docs/WIZARD-SPELL-CHOICES.md).

## Light/hand-state baseline — actual 0.6.53 writer

Generated on2026-09-26 before changing the runtime in `bf9ec73`. The only
capture additions are `tests/light_attack_baseline.h` and its training-test CLI;
`git diff bf9ec73 -- src demos/src` was empty at capture. The generator rejects
any writer other than0.6.53. It calls ordinary creation, XP advancement,
equipment, public combat commands and campaign serialization; no bytes are
edited to impersonate an older writer. The custom target is loaded by the same
stable fixture content helper on capture and verification.

The campaign holds a Fighter PC and recruited Paladin/Ranger, all level4, with
actual GWF/Archery grants, wounds, an equipped Greatsword, three carried Daggers,
a Hand Crossbow and a Shield. Combat21 captures a real critical hit, both Savage
Attacker decisions and resolved Champion movement. A second sequence fires a
Loading Hand Crossbow twice through two distinct actions using Action Surge.
It preserves unused Bonus Action, spent resources, HP and RNG. Future attacks
can acquire new Light history; the Loading continuation compares those gameplay
values independently rather than forbidding newly implemented legal actions.
Restoring the historical already-spent checkpoint must not invent new history.

Reproduce capture only with the genuine0.6.53 runtime:
`opengold_training_tests --freeze-light`. Verify with `--light-baseline` or the
full training test, which now includes these checks. Focused CTest passes after
rebuilding `opengold_training_tests`; runtime inputs are unchanged.

| File | SHA-256 |
| --- | --- |
| `campaign-v17-light-before.ogs` | `8247a44eee1c4e128ca3ceb51ac2ed1f2b3ed5587b5c5e39956804b905dc5bdb` |
| `combat-v21-light-before-attack.save` | `dee3402f18b691d664b97a687baa9b0d0b87d520b828d53865739811a9b846e7` |
| `combat-v21-light-before-first.save` | `613969267a8ed9be8e9b218d7ab981fad961b47ffd80dc77933636ff453b0a51` |
| `combat-v21-light-before-second.save` | `a21b2e6f9deb59d656e2f71275c1140dcf42e0aba2073862b3af7b8b794ac3be` |
| `combat-v21-light-before-settled.save` | `b258e60c77783ec7727d843f25d799be7a68b2b19aeeb18b4f9acb1ac44f611a` |
| `combat-v21-loading-before.save` | `b49d73576583173380549120340cdd318343cf81da260641ea3f414446e2e54d` |
| `combat-v21-loading-two-actions.save` | `e06afeed956b27bd5fbca84c0f44184cda19e9eb01559a6dfbfcb71ea8758a05` |

## Dual-hand baseline — actual0.6.54 writer

Captured from production runtime`c539347` before combat weapon-selection/Light
changes, using `opengold_training_tests --freeze-hands`; verified using
`--hands-baseline`. The only capture edits were the test harness. Includes
PC/recruited dual equipment, distinct stack units, pending Savage/RNG stages
and wounds. These are genuine writer outputs, never rewritten version tags.

| Fixture | SHA-256 |
| --- | --- |
| campaign-v17-hands-before.ogs | `9f675cd35d362127ac0b9aff92610634439f8a33f675ccfc68f3af32e9e383c1` |
| combat-v21-hands-before-attack.save | `8e9e282a8cb351f180bb7e9c3af4653390c0f0903996a76c3927bc6ccb2e58cb` |
| combat-v21-hands-before-first.save | `441629aaa3e56b2c01598bcce121be82a019722f05b707f2e85ea500e43281f7` |
| combat-v21-hands-before-second.save | `579b15007c02ce438e5194495eb7f27df278413d6193083fb575c94289d55d71` |
| combat-v21-hands-before-settled.save | `33c45dea2b30e1ec48171ff9ba2275c8de7ff689acdd3f1b3970081e770ceae5` |

## Weapon Mastery baseline — actual 0.6.55 writer

Captured2026-09-26 before any mastery runtime edits. Production writer is
`69771eb` (rules0.6.55), inherited at preflight HEAD`7f35c89`.
`tests/mastery_baseline.h` is included by the existing training target;
`--freeze-mastery` checks the exact writer version before producing files.
Do not regenerate these historical fixtures with a later writer.

The five-class campaign covers Fighter/Rogue/Paladin/Ranger at4 and Barbarian1,
three PCs/two recruited NPCs, prior choices, equipment and wounds. Combat covers
physical Light qualification and a TWF extra attack through both Savage decisions
and settlement. `--mastery-baseline` and the ordinary training suite verify exact
restoration and deterministic continuation (only module version is normalized).
No new mastery behavior is claimed.

| Fixture | SHA-256 |
| --- | --- |
| `campaign-v17-mastery-before.ogs` | `e7815e705693472598a2919cf391c1aebc8a748df0ca6f58e1b21e999948520b` |
| `combat-v22-mastery-light-first.save` | `e8edb6ac1c2d51b31781100895430018151c6af26907a2afdb07fe204e72c10c` |
| `combat-v22-mastery-light-qualified.save` | `679f703fb77b89d6605ad1a2f76b2fe4dc04b23d3e014a6158ec02d61841f5d0` |
| `combat-v22-mastery-light-second.save` | `f1602842a8d4a35a99a713d2dfce4bef3e660838e7fd606e2f6370a2e9a47a24` |
| `combat-v22-mastery-light-settled.save` | `9e5f26067519e30523607b39db1fdec55bd0224d6fffd20fe203232843a661eb` |

### Mastery acquisition before rest replacement — actual 0.6.56 writer

`campaign-v15-mastery-acquired-before.ogs` is the unchanged main-game Review
Training output `/tmp/mastery-review-main.ogs`, captured during acquisition
verification before Long Rest replacement edits. Its production writer is the
acquisition tree committed as `5b978df`; that commit's later regression changes
only adjusted tests. MainEN/ES/demoEN UI outputs were independently compared with
native results before this fixture was retained. No current replacement writer
was used to synthesize an older format.

SHA-256: `182d59edb78ae4e61d32033a8e24b13baf7177991a3324cc1f22e65deaf63431`.
`mastery_rest_checks.h` verifies exact canonical re-encoding with no invented
replacement history/window, including acquired masteries and prior advancement.

### Nick shared-budget baseline — actual0.6.56 writer

Captured before Nick production edits, with production revision `86ac760` and
rules0.6.56. `OPENGOLD_NICK_BASELINE=tests/fixtures` runs the guarded capture path
in `opengold_status_effect_tests` (`tests/nick_attack_checks.h`). Capture requires
that old writer identity and used seed1. These are complete, unmodified writer
outputs, not edited examples or re-created old formats.

- `combat-v22-nick-before.save`: a wounded Fighter2 with two physical daggers and
  actual chosen Nick mastery, before its Attack action (Nick not yet implemented).
- `combat-v22-nick-light-pending.save`: Light extra attack has spent the Bonus
  Action; the old Savage Attacker choice is unresolved.
- `combat-v22-nick-light-spent.save`: the preceding choice is skipped/resolved.
- `combat-v22-nick-other-bonus-spent.save`: Second Wind spent the Bonus Action,
  then the first Attack qualified for Light. No Light extra attack was made.

The last two states demonstrate why the old Bonus Action flag alone cannot
reconstruct whether the extra attack was used. New code preserves the entire
old current turn and enables Nick only at the first fresh turn boundary; the
old pending choice resolves to the old settled oracle byte for byte apart from
the module identity. New format23 records the shared allowance and the current
Attack action's physical qualifying weapon explicitly. No old format is dropped.

SHA256:

```
2e0ba1df72f12b7b5464953867a24a58e028828b35e9d4b2bb44aa7fc70bd3af  combat-v22-nick-before.save
c8d4893b059650dc520d33237e8f40d4dd119496babc5ad7a1d791e43ccfe7b3  combat-v22-nick-light-pending.save
3a6db3e16f7ef02af3db7422deef24ef2ccb863adc54725c63cd18fd6be4ddea  combat-v22-nick-light-spent.save
0ca778093241e1cc1a681baec16c7dea632f0ef5190dd6e3175453fe102e2cb0  combat-v22-nick-other-bonus-spent.save
```


## Optional mastery choices baseline — actual0.6.57 writer

Captured2026-09-26 from production `0bba07e` before Slow/Topple or optional
mastery choice implementation. `tests/mastery_choice_checks.h::capture` asserts
the actual writer identity and writes its unmodified `CombatSession::save()`.
Both scenarios use seed30, a Soldier Champion3 with the selected Longbow or Maul,
physical equipment and the normal content-pack Vanguard target. Each has four
states: `before` the critical attack, pending Savage `damage`, pending Champion
`move` after skipping Savage, and `settled` after declining free movement.

The status-effect target verifies exact round trips and command-by-command
continuation against all eight actual writer files. No bytes were relabeled as
historical evidence. Capture command after building that target:

```bash
OPENGOLD_MASTERY_CHOICE_BASELINE=tests/fixtures build/mac-check/opengold_status_effect_tests
```

Do not rerun this capture with a newer production writer. Build/capture/test logs
are `/tmp/mastery-choice-baseline-{build,capture,test}.log`.

```text
a59d63d32a990efe8d32db93852035f7ba17b703c5f176c44b318d7034d131d3  combat-v23-longbow-before.save
225c1ba93d948ba1ff6fb167bd9c66073124d44065281e0fcc5fa5b247a889ab  combat-v23-longbow-damage.save
2d0ba8d28b7e319b65ca7592dcad85765f2b6b0a1d83e1468e37fadbf3d4ad60  combat-v23-longbow-move.save
b1fed47245de38c7249f0d5f875e8a050357388dc7c3d410cac0d6817c78066b  combat-v23-longbow-settled.save
47b3f0eaa274cb0cf9a371511fbd88994b615c0b3b8661af3ba49a26e280f945  combat-v23-maul-before.save
bd1953e3ef2117f94ba47582c7d049bb8e0c46932f3941882c0b0e552e1a4c6e  combat-v23-maul-damage.save
ba61d92a73b7d58e26d7a4d2196652375842f68306d27ad42e02f25636205ebf  combat-v23-maul-move.save
bb56c8397ddf26eaa87f880a44ef2f0a2c066c7caa6460b252193ca538c8718c  combat-v23-maul-settled.save
```

### Actual0.6.58 Slow writer before Graze

`combat-v15-slow-0.6.58.save` was captured2026-09-26 from the actual0.6.58
`build/sprite-demo/libopengold_rules_srd5.a`, built at `a0514a9` before that build
was updated for Graze. The capture driver used the matching `rules.h` from that
commit and asserted the runtime identity was0.6.58. It created a12×8 empty board,
Vanguard1 at(1,1), Bandit99 at(3,1), scope777 and seed89. Bandit initial vitality
carried one six-second Slow sourced from1; the old module created, saved and
round-tripped the session before writing this file. This is real writer output,
not a current checkpoint with a changed version header.

SHA256: `758e1934fba8b5db82d58e2825f72cfa42d19f64fe3495cc17070b0bdd010a45`.
`graze_checks.h` requires exact continuation encoding except the expected module
identity update and verifies that Slow survives. Earlier actual0.6.57 mastery
critical-phase and0.6.56 Nick fixtures are retained unchanged.

## Pending Graze before optional hit masteries

`combat-v24-graze-pending-0.6.59.save` and
`combat-v24-zero-graze-0.6.59.save` are genuine **0.6.59** writer output from the
libraries built at `a038d91`, before the format25 writer was compiled. The first
retains a positive Graze choice from an actual Greatsword miss; the second retains
an actual miss by a Strength10 Fighter, for whom the old writer offered zero
damage. New attacks suppress futile zero-damage prompts, but migration preserves
and resolves this already-pending historical choice. No original game assets are
included. Do not regenerate these fixtures with the current writer.

SHA-256:

- positive: `b4c2cdbb824e7ed859edafbe866fee2ee851a6e7d199083fe97d2db20b29453d`
- zero: `8820cafc280de2d5c229a121d51ebcf8e64df393bd4efd4107e75383c6f552ea`

`optional_mastery_checks.h` verifies unchanged pending bodies apart from module
identity and exact continuation. Existing0.6.57 pending damage/movement fixtures
retain their original continuation; fresh attacks after loading an older settled
checkpoint may offer the newly implemented properties.

## Alert baseline — actual0.6.60 writer

Captured2026-09-26 before any Alert runtime changes, from production80ce54b
(inherited at63936c1), using `opengold_training_tests --freeze-alert`.
The generator requires exactly0.6.60. The campaign contains normally advanced
Criminal Fighter/Rogue4, actual equipment, chosen training/masteries and wounds.
Combat captures retain their original Initiative order and one End Turn
continuation; loading must never reopen Initiative choices. Campaign migration
adds only the fixed Criminal Alert grant. No fixture bytes were relabeled.

- `campaign-alert-0.6.60.ogs`: `cdeb881c090315272b68de3d52ccb847f97f01891040ce639f29d8daf3cfe9b9`
- `combat-alert-0.6.60.save`: `405a4626789d490371e7194d8e245112cac69561d1570ae503494b61d06bc8fb`
- `combat-alert-0.6.60-continued.save`: `7d2d3c6895947c003f5695426d112e367509d85dda10e48edc858e4911110ca5`
