# Manual advancement

When a living Fighter, Cleric or Wizard has enough XP, a small **↑** button appears
beside the character's name in the party roster and idle exploration party list.
XP alone never changes a level. Click the arrow to preview the next level's HP,
select supported spells, and choose a feat or ability points at level 4.
**Confirm** applies one level; **Cancel** and closing the dialog change nothing.
If another level is available, the arrow remains. Advancement is blocked during
combat and unfinished exploration events.

This increment supports single-class levels 2–4, with thresholds of 300, 900 and
2,700 total XP. HP uses the fixed average after first level, includes retroactive
Constitution changes and preserves the existing HP deficit. Each new level adds
its HP using the previous Constitution modifier (minimum one), then applies any
new modifier increase once per attained level. Earlier minimum-one gains are
retained. For example, a Human Wizard starting with Constitution 3 has 2, 3 and
4 maximum HP at levels 1–3; a level-four +2 Constitution increase yields 9 HP.
Dwarven Toughness adds one HP per level. Existing resource
expenditure is preserved; only new capacity is added. An unconscious character
does not become conscious merely by leveling. No XP is deducted.

## Available choices

- **Ability points:** at level 4, add two points to one ability or one point to
  two abilities, with a maximum score of 20. Scores, modifiers, saves and derived
  combat values are rebuilt together.
  The modifier dialog attributes these points to the level at which Ability
  Score Improvement was acquired, separately from the original background bonus.
- **Defense:** level-4 Fighter choice; +1 AC while wearing armor.
- **Savage Attacker:** level-4 choice for a character who does not already have
  it from Soldier. Once per turn on a weapon hit, choose whether to roll damage
  twice, then keep either result. Soldier grants the same decisions, including
  opportunity attacks. See [Savage Attacker](SAVAGE-ATTACKER.md).
- **Cleric:** Cure Wounds and Healing Word; Blindness from level 3. At least
  one supported spell must be selected.
- **Wizard:** Magic Missile, plus Scorching Ray and Blindness from level 3. Fire Bolt remains
  available without spending a slot. At least one leveled spell must be selected.

The spell list is a curated subset, not the complete class preparation or
spellbook system. Unavailable examples (Bless, Shield, Grappler and Magic
Initiate) are disabled and explain their missing mechanics in a tooltip.

Level-one slots are 2/3/4/4 at character levels 1/2/3/4. Level-two slots are 2 at
level 3 and 3 at level 4. **Slot level** in combat switches between level-one and
level-two casts for Cure Wounds, Healing Word and Magic Missile. Scorching Ray
always spends a level-two slot. Blindness also always uses a level-two slot and
implements only the blindness option of Blindness/Deafness; see
[status effects](STATUS-EFFECTS.md). Healing Word uses the bonus action and heals
2d4 plus the casting modifier; its level-two cast heals 4d4. Cure Wounds uses
2d8/4d8. Magic Missile fires three/four darts. Scorching Ray makes three separate
ranged spell attacks for 2d6 each. Only one spell slot may be spent per turn.

Damage spells currently direct all darts/rays at one target; split targeting is
unavailable. Full class/subclass features, Action Surge, Channel Divinity,
concentration, broader spells/feats and multiclass advancement remain outside
this increment. The dialog states the class/subclass limitation.

Rules use the [official SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
including its current Healing Word and Savage Attacker mechanics. Existing pack
attribution applies; original game resources are loaded locally.

## Saves and verification

Campaign saves record each confirmed choice and reconstruct the resulting
sheet and Constitution history through the rules module; see
[saves](SAVES.md#pre-10-format-policy). Spent resources, death-save counters and
grip persist. Profiles carry HP history, grip and grants.
Campaign saves explicitly store acquired grants with stable feature/feat IDs,
source IDs, acquisition levels and named choices. Soldier's Savage Attacker is a
level-one `background:soldier` grant. A level-four selection records
`class:<id>:ability_score_improvement`, even when the chosen feat is Defense or
Savage Attacker. Ability Score Improvement records each chosen ability and amount.
A source can only spend its entitlement once; nonrepeatable feats cannot be
acquired twice. Defense requires the recorded Fighting Style feature. ASI is
repeatable under the SRD, but the current level cap provides only one entitlement.

Advancement adds only the newly acquired Hit Die, preserving previous dice
expenditure in the rules continuation; see [rest resources](REST-RESOURCES.md).

Campaign saves and profiles also store skill and Expertise grants.
Advancement refreshes skill totals after ability changes; training choices are
described in [training support](TRAINING.md).

Existing Second Wind, Spellcasting, Unarmored Defense, Dwarven Toughness and
Goliath speed have source records. Fighting Style records the existing Defense
prerequisite; its level-one selection/replacement flow remains FTR00/FTR01 work.
This does not add unimplemented class features, Origin feats or Human choices.
New grant records must agree with creation and leveling history on load.

Ability-adjustment sources are also reconstructed from creation and advancement
choices. Each records its source ID, acquisition level
and ability amounts. This presentation correction introduced no profile or campaign format change.
Standalone combat checkpoints retain second-level slots,
per-turn spell/feat usage, timed effects, presentation facing, pending movement
reactions, involuntary overlap during allied transit, weapon grip, remaining Hit Dice, mortality recovery clocks and sourced Temporary HP.

From PowerShell:

```powershell
.\demos\build-rolf.cmd
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

Native advancement tests cover all three classes, transactional rejection,
Constitution HP history (including low scores and odd/even modifier boundaries),
Defense AC, Savage Attacker damage, spell action
budgets, second-level resources and campaign/combat save reconstruction.

[Feature-grant regressions](../tests/feature_grant_tests.cpp) cover Soldier creation
across all twelve classes, separate advancement entitlements and choices,
prerequisites, duplicates, forged sources and malformed saved choices.

Rules 0.6.15 also persists Orc Adrenaline Rush uses and pending Temporary HP
replacement; see [Temporary HP](TEMPORARY-HP.md).
