# Manual advancement

When a living character of any class has enough XP, a small **↑** button appears
beside the character's name in the party roster and idle exploration party list.
XP alone never changes a level. Click the arrow to preview the next level's HP,
select spells and other choices, and choose a feat or ability points when the
class gains one.
**Confirm** applies one level; **Cancel** and closing the dialog change nothing.
If another level is available, the arrow remains. Advancement is blocked during
combat and unfinished exploration events.

Single-class levels 2–15 are supported, with the SRD's XP thresholds (300 for
level 2, 900, 2,700, 6,500 and on to 165,000 for level 15). Level 15 is the
game's cap for every class
([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)). HP uses the
fixed average after first level, includes retroactive Constitution changes and preserves the existing HP deficit. Each new level adds
its HP using the previous Constitution modifier (minimum one), then applies any
new modifier increase once per attained level. Earlier minimum-one gains are
retained. For example, a Human Wizard starting with Constitution 3 has 2, 3 and
4 maximum HP at levels 1–3; a level-four +2 Constitution increase yields 9 HP.
Dwarven Toughness adds one HP per level. Existing resource
expenditure is preserved; only new capacity is added. An unconscious character
does not become conscious merely by leveling. No XP is deducted.

## Available choices

- **Ability points:** at levels 4, 8 and 12 (also 6 and 14 for a Fighter and 10
  for a Rogue), add two points to one ability or one point to
  two abilities, with a maximum score of 20. Scores, modifiers, saves and derived
  combat values are rebuilt together.
  The modifier dialog attributes these points to the level at which Ability
  Score Improvement was acquired, separately from the original background bonus.
- **Defense:** Fighter choice at those levels; +1 AC while wearing armor.
- **Savage Attacker:** choice at those levels for a character who does not already have
  it from Soldier. Once per turn on a weapon hit, choose whether to roll damage
  twice, then keep either result. Soldier grants the same decisions, including
  opportunity attacks. See [Savage Attacker](SAVAGE-ATTACKER.md).
- **Checkbox pages:** a choice of several picks (Skilled, Metamagic, Eldritch
  Invocations) has its own page after the first.
- **Replace:** a class that may swap a cantrip, spell, invocation or Metamagic
  option on gaining a level does so on the Replace page; see
  [level swaps](LEVEL-SWAPS.md).
- **Spells:** the last page. Prepared casters see their prepared spells, earlier
  ones locked (see [Cleric preparation](CLERIC-PREPARATION.md)); the Wizard's
  spellbook gains two spells per level (see [Wizard](WIZARD.md)). A spell the
  game cannot cast yet is disabled.

Each class takes the SRD's one subclass for it at level 3. Slots by level and
casting from higher slots are in [spell slots](SPELL-SLOTS.md); each class's
features by level are on its page, linked from the
[README's current status](../README.md#current-status). Multiclassing is deferred
([SCOPE-1](SRD-DECISIONS.md#scope-1-2026-09-30-level-cap-deferrals-and-rare-situations)).

Rules use the [official SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
including its current Healing Word and Savage Attacker mechanics. Existing pack
attribution applies; original game resources are loaded locally.

## Saves and verification

Campaign saves record each confirmed choice and reconstruct the resulting
sheet and Constitution history through the rules module; see
[saves](SAVES.md#pre-10-format-policy). Spent resources and death-save counters
persist. Profiles carry HP history and grants.
Campaign saves explicitly store acquired grants with stable feature/feat IDs,
source IDs, acquisition levels and named choices. Soldier's Savage Attacker is a
level-one `background:soldier` grant. A level-four selection records
`class:<id>:ability_score_improvement`, even when the chosen feat is Defense or
Savage Attacker. Ability Score Improvement records each chosen ability and amount.
A source can only spend its entitlement once; nonrepeatable feats cannot be
acquired twice. Defense requires the recorded Fighting Style feature. ASI is
repeatable under the SRD, with one entitlement per level that grants it.

Advancement adds only the newly acquired Hit Die, preserving previous dice
expenditure in the rules continuation; see [rest resources](REST-RESOURCES.md).

Campaign saves and profiles also store skill and Expertise grants.
Advancement refreshes skill totals after ability changes; training choices are
described in [training support](TRAINING.md).

Existing Second Wind, Spellcasting, Unarmored Defense, Dwarven Toughness and
Goliath speed have source records. Fighting Style records the existing Defense
prerequisite; its level-one selection and replacement are in [Fighting Styles](FIGHTER-STYLES.md).
This does not add unimplemented class features, Origin feats or Human choices.
New grant records must agree with creation and leveling history on load.

Ability-adjustment sources are also reconstructed from creation and advancement
choices. Each records its source ID, acquisition level
and ability amounts. This presentation correction introduced no profile or campaign format change.
Standalone combat checkpoints retain second-level slots,
per-turn spell/feat usage, timed effects, presentation facing, pending movement
reactions, involuntary overlap during allied transit, remaining Hit Dice, mortality recovery clocks and sourced Temporary HP.

From PowerShell:

```powershell
.\build-opengoldbox.cmd
.\demos\review-character.cmd --advancement-check
.\demos\review-character.cmd --level-up-review
```

The automated check exercises real roster and town buttons, Cancel, invalid
points, HP preview, spell/feat confirmation and disappearance of the arrow at
the level cap. It writes local captures under `user-data/level-up-*.png` and exits.
The review command opens an isolated three-character level-3 fixture with enough
XP for level 4, then leaves the controls available for manual inspection. Its
saves use a separate profile under `user-data/level-up-review-profile`.
Both game and demo advancement checks verify separate background/feat lines in
the existing modifier dialog and identical text after campaign reconstruction.

Native advancement tests cover every class, transactional rejection,
Constitution HP history (including low scores and odd/even modifier boundaries),
Defense AC, Savage Attacker damage, spell action
budgets, second-level resources and campaign/combat save reconstruction.

[Feature-grant regressions](../tests/feature_grant_tests.cpp) cover Soldier creation
across all twelve classes, separate advancement entitlements and choices,
prerequisites, duplicates, forged sources and malformed saved choices.

Rules 0.6.15 also persists Orc Adrenaline Rush uses and pending Temporary HP
replacement; see [Temporary HP](TEMPORARY-HP.md).
