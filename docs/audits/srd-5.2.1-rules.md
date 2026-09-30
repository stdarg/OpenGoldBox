# SRD 5.2.1 rules audit and completion plan

Date: 2026-09-23. Audited commit:
`e5c78783ad9522b317abb0a90aa0bbf119991511` (fetched from `origin/main`).

**Required outcome: support all twelve SRD classes.** The user explicitly
confirmed this during the audit. The current Fighter/Cleric/Wizard advancement restriction
is an implementation gap, not an acceptable definition of project scope.

OpenGoldBox has a useful, deterministic rules foundation, but does not yet
implement complete SRD characters. All twelve classes can be created and enter
combat with basic attacks. Nine
remain at level 1 without their defining class features; Fighter, Cleric and
Wizard support only part of levels 1–4. Most species traits, origin feats, class
features, spell choices, equipment
and recovery mechanics are missing. Five specific errors in existing behavior
are identified below, separately from missing coverage.

This document proposes work for review. It changes no gameplay, rules, tests,
UI layout, or existing campaign data.

**Decision update, 2026-09-23:** after reviewing this audit, the user approved
preserving remaining turn resources after attacks, removing facing-based
opportunity triggers, permitting SRD movement through allies, and restoring
one-/two-handed Versatile use with the appropriate damage. These four decisions
are no longer pending. The [incremental implementation plan](../SRD-IMPLEMENTATION.md)
records them and breaks the work below into bounded deliveries. Findings and
policy tables here continue to describe the audited commit.

Implementation progress is tracked in the [coverage ledger](../SRD-COVERAGE.md)
and [GitHub issue index](https://github.com/stdarg/OpenGold/issues/186). E5 is
corrected by I01; E2 and E3 are corrected by I02; E1 is corrected by I03; E4 is
corrected by I04. The coverage gaps retain their open status in the ledger.

**Save policy update:** the save-migration steps below (E1, E4, Stage 1, Stage 2
and the verification strategy) predate the pre-1.0 hard cutoff. Saves are not
migrated before 1.0; see the [format policy](../SAVES.md#pre-10-format-policy).

## Authority, scope, and evidence

- The baseline is **SRD 5.2.1**, selected by [PRD](../PRD.md) and
  [RULES](../RULES.md), checked against the
  [official PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
  Page references below are the PDF's printed page numbers. Open5E is reference
  content, not the authority for executable mechanics or a replacement for the
  pinned edition.
- All twelve classes and all nine SRD species are included in the coverage
  assessment. The class matrix includes the subclass supplied for each class
  in this SRD; it does not silently add subclasses from other books.
- The audit compares progression with the SRD's levels 1–20. Delivering those
  levels in stages is a proposal below; the user's class requirement does not
  itself select a campaign level cap or a release deadline. A release cap must
  not be confused with complete SRD progression.
- Multiclassing is recorded as a separate missing SRD capability. Supporting
  every single class and supporting arbitrary multiclass combinations are
  distinct completion gates.
- Documentation describing a feature as unsupported explains the current state;
  it does not exempt that feature from the requested completion plan.
- Rules that documentation calls user-requested house rules are listed as
  departures for review. This audit does not independently establish continuing
  user approval of those choices or silently remove them.
- Original Pool of Radiance encounter conversions, prices and XP policies are
  distinguished from SRD mechanics. Their balance is not established by this
  audit. See the existing [first-adventure audit](issue13-first-adventure.md).

The implementation review covered the SRD character, combat and effect code;
the public character/rules interfaces; campaign advancement, rest and save
paths; relevant native tests; and the character-sheet modifier presentation.
The official progression, species, background, feat and class feature tables
were consulted, along with the six implemented spell paths and their rules.
This is a coverage and source audit, not a claim that every unimplemented spell,
monster or magic item has received an individual textual conformance test.

Evidence labels:

- **Source-confirmed:** the behavior follows directly from the cited code path.
  The proposed regression scenarios below were not added or executed as new tests.
- **Tested baseline:** existing native suites were rebuilt and run successfully.
- **Gap:** required SRD behavior is absent or intentionally restricted.
- **Policy:** a departure that needs an explicit project decision, not an
  automatic code correction.

Priority **P1** means a major obstacle to complete, playable SRD classes or
character survival/recovery. **P2** means a narrower correctness or reporting
error. These priorities describe the completion plan; they are not claims of
crashes or security vulnerabilities.

## Existing behavior worth preserving

| Area | Assessment |
| --- | --- |
| Ability generation | 4d6 dropping the lowest, unique assignment, signed ability modifiers, and background +2/+1 or +1/+1/+1 allocations match the selected SRD method. All four backgrounds use the correct three abilities. Species should not receive invented legacy ability-score bonuses. [SRD pp. 21–22, 83](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=21). |
| Initial class statistics | All twelve class Hit Dice and saving-throw proficiency pairs are represented. Level-one HP uses maximum Hit Die plus Constitution; Dwarf adds 1 HP. Numeric save totals use proficiency +2 in the implemented tier. |
| Existing progression | XP thresholds 300/900/2,700 and Cleric/Wizard slot counts 2/3/4/4 at spell level 1, then 2/3 at spell level 2 for character levels 3/4, agree with the tables. Fighter Second Wind capacity is 2/2/2/3. [SRD pp. 23, 36, 47, 77](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=23). |
| Core rolls | Attack natural 1/20 handling, ordinary saves without automatic natural 1/20 outcomes, critical damage dice, and advantage/disadvantage cancellation are implemented. Surprise correctly affects initiative rather than importing the older surprise-turn rule. |
| Healing and spell economy | Cure Wounds uses 2d8, Healing Word 2d4, and each gains two more dice per higher slot in the implemented range. Healing Word uses a Bonus Action. The one-slot-per-turn restriction is appropriate to 5.2.1. [SRD pp. 105, 121, 139](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=105). |
| Recovery baseline | The ordinary eight-hour Long Rest and sixteen-hour wait after completion before starting another match 5.2.1. Requiring at least 1 HP to start is also correct; the missing alternatives for unconscious characters are discussed below. [SRD p. 185](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=185). |
| State integrity | Native code owns gameplay; rejected commands preserve state/RNG; advancement previews are transactional; resources and choices survive saves. Retain these properties as coverage expands. |

These observations establish specific correct mechanisms, not certification of
the complete features in which they participate.

## Errors in existing behavior

### E1 — P2: Constitution increases can undercount maximum HP

**Source-confirmed.** Both `character_definition` and `advance_character`
recompute every earlier level using the current Constitution modifier and a
minimum-one clamp. See
[srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines 104 and 879.

This loses the explicit retroactive HP increase when earlier gains were already
clamped. A Human Wizard with Constitution 3 and a background allocation that
does not increase Constitution can legally reach level 3 with 4 maximum HP:
2 at creation, then 1 at each new level. Increasing Constitution to 5 at level
4 raises its modifier from -4 to -3. The SRD's advancement sequence gives:

```text
Existing maximum                         4
New level using the old modifier         +1  (minimum-one gain)
Constitution modifier increase           +4  (one per attained level)
Correct maximum                          9
Current formula                          6
```

The retroactive rule is explicit in
[SRD p. 23](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=23).
The existing Constitution test uses a normal positive modifier and misses this
case ([advancement_tests.cpp](../../tests/advancement_tests.cpp), `progression`).

**Correction:** retain/replay HP advancement history and apply Constitution
changes as their own rule. Update profile validation and save reconstruction
together; changing only the displayed sheet would make the profile reject the
correct HP total. Define how affected old saves migrate.

**Acceptance:** the example produces 9 HP; normal Constitution changes still
work; Dwarven Toughness remains additive; living HP deficits and unconscious
state are preserved; save/reload reconstructs the same result.

### E2 — P2: The first initiative slot can skip its death save

**Source-confirmed.** On encounter construction, if the first actor has 0 HP,
the session immediately calls `end_turn()`. That routine advances to the next
actor before processing death saves. An unstable character already at 0 HP who
rolls first initiative therefore misses the first turn's death save. See
[srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines 209–210 and 459–477.

The SRD requires a death save when starting a turn at 0 HP unless Stable;
see [pp. 17–18](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=17).
This is reachable when carrying an unconscious party member into another fight.

**Correction:** use one turn-entry path for the initial actor and subsequent
actors, including death saves and natural-20 recovery. Keep checkpoint restore
from replaying a turn-entry roll already represented by the checkpoint.

**Acceptance:** start an encounter with an unstable 0-HP PC first in initiative,
a conscious ally, and a conscious enemy. Exactly one death save occurs before
the next initiative slot; a natural 20 allows the revived PC's turn. Stable and
dead initial actors do not roll. Save/restore consumes no additional RNG.

### E3 — P2: Stabilization retains completed death-save counters

**Source-confirmed.** A third success sets `stable=true` without clearing
successes/failures ([srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp),
lines 468–473). The stale counters are serialized. The SRD explicitly resets
both counters upon becoming Stable
([p. 17](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=17)).

Current targeting restrictions mask much of the consequence: attacks against
unconscious characters are not offered. Nevertheless, the saved rules state is
incorrect, and stale successes would become dangerous when damage at 0 HP is
implemented.

**Correction and acceptance:** after the third success, Stable is true and both
counters are zero, including after combat handoff and reload. Subsequent damage
ends stability and adds fresh failures under the appropriate normal/critical
damage rule.

### E4 — P2: Level-up ability increases are attributed to the background

**Source-confirmed.** Advancement adds feat ability points to `next.bonuses`,
which initially holds background bonuses
([srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), line 875).
The game modifier dialog labels that entire value as the background
([character_sheet_view.cpp](../../src/OpenGoldBox/character_sheet_view.cpp),
lines 94–99); the demo does the same.

For example, a Soldier with background Strength +2 who takes Strength +2 at
level 4 sees **Soldier background (+4)**. The final score may be correct, but
the explanation falsely credits the background. This directly undermines the
requested ability to inspect racial/class/background bonuses separately.

**Correction:** expose derived adjustments with source IDs and acquisition
levels, separating creation bonuses from feats, species, items and effects.
Render their sum without reassigning provenance. Existing advancement history
provides evidence for reconstructing old characters.

**Acceptance:** show Soldier +2 and level-4 Ability Score Improvement +2 as
separate contributions; a feat increase to an ability outside the background's
allowed set must never appear as a background increase. Verify both game/demo
sheets and save/reload. No layout changes are selected by this audit.

### E5 — P2: Rogue and Monk lose owed weapon proficiency bonuses

**Source-confirmed.** `trained()` grants martial weapon proficiency only to
Barbarian, Fighter, Paladin and Ranger
([srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines 30–35).
Rogue is also proficient with martial weapons that have Finesse **or** Light;
Monk is proficient with martial weapons that have Light. See
[SRD pp. 49 and 61](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=49).

Shortsword and Scimitar are already implemented and have both properties in
the SRD. A level-one Rogue with Dexterity 16 receives a +3 Shortsword attack
bonus instead of +5; the same lost +2 affects a Monk's proficient use of those
weapons. The catalog represents Finesse but not Light
([weapons.h](../../src/OpenGold.Rules.Srd5/src/weapons.h)). Documentation calls
this a proficiency subset, but the supported weapon still produces an incorrect
class bonus.

**Correction:** derive proficiency from the weapon category/properties and
actual class/feature grants. Add the missing property rather than relying on
names for all future weapons. Keep starting proficiencies distinct from
multiclass entry and optional grants such as Divine/Primal Order.

**Acceptance:** Rogue and Monk both receive proficiency for Shortsword and
Scimitar, including equipment notes, attacks and save/reload. When Rapier is
added, Rogue receives proficiency while Monk does not receive it from its base
class grant. A Wizard without an additional grant remains untrained with these
martial weapons.

## Required coverage gaps

### G1 — P1: Nine classes lack class mechanics and advancement; none is complete

All twelve classes now enter combat. The removed combat gate is not an open
finding against this baseline. However,
[srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), line 97, rejects levels
above 1 for classes other than Fighter, Cleric and Wizard. `advancement_options`,
lines 825–827, repeats the three-class/level-four gate. Spell slots and spell
access are still supplied only to Cleric/Wizard; most other class resources and
features do not exist. Lifting the remaining level gate alone is not a fix.

All twelve share basic HP/saves, equipment and ordinary attacks. Barbarian and
Monk also receive unarmored AC formulas. A playable generic attack profile does
not establish support for the class's actual abilities.

The following matrix is the required class work inventory. The early-feature
column lists principal omissions through level 4, in addition to shared gaps
such as skills, equipment, spells and full feat choices. Higher-level entries
are landmarks for completion, not an exhaustive replacement for the SRD tables.

| Class / SRD pages | Current playable rules | Required early features and SRD subclass | Later completion landmarks |
| --- | --- | --- | --- |
| Barbarian, 28–30 | Level-1 basic attacks and unarmored AC; no advancement | Rage, Weapon Mastery, Danger Sense, Reckless Attack, Primal Knowledge; Path of the Berserker/Frenzy | Extra Attack, Fast Movement, Feral Instinct, Brutal Strike, Relentless/Persistent Rage, subclass progression, Epic Boon, Primal Champion |
| Bard, 31–35 | Level-1 basic attacks and statistics; no advancement | Bardic Inspiration, Charisma spellcasting, Expertise, Jack of All Trades; College of Lore/Bonus Proficiencies/Cutting Words | Font of Inspiration, Countercharm, Magical Secrets and Discoveries, Peerless Skill, Superior Inspiration, Epic Boon, Words of Creation |
| Cleric, 36–40 | Attacks, selected healing/Blindness, HP and slots through 4 | Three starting cantrips, full preparation, Divine Order; Channel Divinity/Divine Spark/Turn Undead; Life Domain/Disciple of Life/domain spells/Preserve Life | Sear Undead, Blessed Strikes, Divine Intervention, Blessed Healer, higher domain features, Epic Boon |
| Druid, 41–46 | Level-1 basic attacks and statistics; no advancement | Wisdom spellcasting, Druidic, Primal Order, Wild Shape, Wild Companion; Circle of the Land/spells/Land's Aid | Wild Resurgence, Elemental Fury, Natural Recovery, nature defenses, Beast Spells, Epic Boon, Archdruid |
| Fighter, 47–49 | Attacks, Second Wind, HP and selected level-4 feats through 4 | Level-1 Fighting Style, Weapon Mastery, Action Surge, Tactical Mind; Champion/Improved Critical/Remarkable Athlete | Extra Attack and later attack counts, Tactical Shift/Master, Indomitable, Studied Attacks, Champion progression, Epic Boon |
| Monk, 49–52 | Level-1 basic attacks and unarmored AC; no advancement | Martial Arts, Monk's Focus, Unarmored Movement, Uncanny Metabolism, Deflect Attacks, Slow Fall; Warrior of the Open Hand/Open Hand Technique | Extra Attack, Stunning Strike, Empowered Strikes, Evasion, higher Focus/deflection/movement, subclass progression, Epic Boon, Body and Mind |
| Paladin, 53–57 | Level-1 basic attacks and statistics; no advancement | Lay On Hands, level-1 Charisma spellcasting, Weapon Mastery, Fighting Style/Blessed Warrior, Paladin's Smite, Channel Divinity; Oath of Devotion/spells/Sacred Weapon | Extra Attack, Faithful Steed, Aura of Protection and other auras, Radiant Strikes, Restoring Touch, oath progression, Epic Boon |
| Ranger, 57–61 | Level-1 basic attacks and statistics; no advancement | Level-1 Wisdom spellcasting, Favored Enemy/Hunter's Mark, Weapon Mastery, Deft Explorer, Fighting Style/Druidic Warrior; Hunter/Hunter's Lore/Hunter's Prey | Extra Attack, Roving, Expertise, Tireless, higher Hunter's Mark benefits, Nature's Veil, Hunter progression, Epic Boon, Foe Slayer |
| Rogue, 61–64 | Level-1 basic attacks and statistics; no advancement | Expertise, Sneak Attack, Thieves' Cant, Weapon Mastery, Cunning Action, Steady Aim; Thief/Fast Hands/Second-Story Work | Cunning Strike, Uncanny Dodge, Evasion, Reliable Talent, additional feat at 10, Thief progression, Epic Boon, Stroke of Luck |
| Sorcerer, 64–70 | Level-1 basic attacks and statistics; no advancement | Charisma spellcasting, Innate Sorcery, Font of Magic, Metamagic; Draconic Sorcery/Draconic Resilience/Draconic Spells | Sorcerous Restoration, Sorcery Incarnate, affinity/flight/companion, Epic Boon, Arcane Apotheosis |
| Warlock, 70–76 | Level-1 basic attacks and statistics; no advancement | Eldritch Invocations, Pact Magic, Magical Cunning; Fiend Patron/Dark One's Blessing/Fiend Spells | Invocation prerequisites and upgrades, Contact Patron, Mystic Arcanum, patron progression, Epic Boon, Eldritch Master |
| Wizard, 77–82 | Fire Bolt and selected leveled spells, HP and slots through 4 | Full cantrip/preparation/spellbook choices, Ritual Adept, Arcane Recovery, Scholar; Evoker/Evocation Savant/Potent Cantrip | Memorize Spell, Sculpt Spells, Empowered Evocation, Overchannel, Spell Mastery, Epic Boon, Signature Spells |

**Completion gate:** each class must be usable through ordinary character
creation, equipment, encounters, advancement, recovery and save/reload. For a
released level band, every acquired class/subclass feature must work or be an
explicitly approved adaptation; a selectable class label is not support.

### G2 — P1: Species and background benefits are mostly missing

Species mechanics currently affect Dwarf HP and Goliath speed. Other traits are
described as unsupported in
[character_rules.cpp](../../src/OpenGold.Rules.Srd5/src/character_rules.cpp),
lines 140–161. The draft has no lineage, ancestry, size or species spellcasting
ability choice ([character_rules.h](../../src/OpenGold.Rules/include/opengold/character_rules.h)).

| Species | Implemented mechanical distinction | Required missing traits/choices |
| --- | --- | --- |
| Dragonborn | Default 30-foot speed | Dragon ancestry, typed resistance, Breath Weapon shapes/DC/uses/scaling, Darkvision; Draconic Flight at 5 |
| Dwarf | Dwarven Toughness, +1 HP per level | 120-foot Darkvision, Poison resistance, advantage against Poisoned, Stonecunning uses and Tremorsense |
| Elf | Default 30-foot speed | Drow/High/Wood lineage, lineage spells and casting ability, Wood Elf speed, Fey Ancestry, Keen Senses, Trance, Darkvision |
| Gnome | Default 30-foot speed | Small size, Gnomish Cunning on Intelligence/Wisdom/Charisma saves, Forest/Rock lineage, spells/devices, Darkvision |
| Goliath | 35-foot speed | Giant Ancestry selection and its limited-use ability, Powerful Build; Large Form at 5 |
| Halfling | Default 30-foot speed | Small size, Brave, Luck reroll, Nimbleness, Naturally Stealthy |
| Human | Default 30-foot speed | Size choice, Resourceful/Heroic Inspiration, Skillful, additional Origin feat |
| Orc | Default 30-foot speed | Adrenaline Rush with Temporary HP and recovery, Relentless Endurance, 120-foot Darkvision |
| Tiefling | Default 30-foot speed | Size choice, Fiendish Legacy/resistance/spells/casting ability, Otherworldly Presence, Darkvision |

Authority: [SRD pp. 83–86](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=83).
Do not import older-edition racial stat increases, 25-foot Dwarf/Halfling speed,
or a magic-only restriction on the Gnome's mental-save advantage.

Background ability allocations work, but the rest of the package is incomplete:

| Background | Origin feat owed | Other missing grants |
| --- | --- | --- |
| Acolyte | Magic Initiate (Cleric) | Insight, Religion, Calligrapher's Supplies, equipment/50 GP choice |
| Criminal | Alert | Sleight of Hand, Stealth, Thieves' Tools, equipment/50 GP choice |
| Sage | Magic Initiate (Wizard) | Arcana, History, Calligrapher's Supplies, equipment/50 GP choice |
| Soldier | Savage Attacker is applied in combat profiles | Athletics, Intimidation, Gaming Set choice, equipment/50 GP choice; explicit feat provenance on the sheet |

Skill/tool proficiency choices, overlapping grants, languages, Expertise and
Passive Perception also need durable representation. They affect class and
species benefits even outside combat. Broad "other features unsupported"
messages do not make these bonuses optional.

### G3 — P1: Feat acquisition is conflated with the level-four choice

`CharacterSheet::feats` contains advancement feats; `character_profile` expects
exactly one at level 4 and zero before it. Soldier's feat is inferred separately
from its background. Human origin choices and class-granted Fighting Styles
cannot be expressed consistently. See
[srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines 825–845 and 945–953.

- Fighters are owed a Fighting Style at **level 1**. Making Defense available
  only as a level-4 choice withholds that entitlement for three levels and makes
  the only available way to obtain it consume a later feat choice.
- Taking another eligible Fighting Style at level 4 can itself be legal. The
  error is not Defense's category; the missing level-one grant is the gap.
- Paladin/Ranger Fighting Style eligibility must come from their actual feature
  at level 2, not a hard-coded `class == Fighter` check.
- Implement all four SRD Origin feats (Alert, Magic Initiate, Savage Attacker,
  Skilled), both General feats (Ability Score Improvement, Grappler), all four
  Fighting Styles, and the SRD Epic Boons when their level band is supported.
- Preserve feat category, prerequisite, source, selections and repeatability.
  Magic Initiate can repeat for different class spell lists; Skilled and Ability
  Score Improvement can repeat. Savage Attacker and Defense cannot simply stack.
- Savage Attacker's decisions are delivered in
  [FT03 #76](https://github.com/stdarg/OpenGold/issues/76): an eligible hit pauses
  for use/skip, then the player can retain either damage result. Opportunity
  attacks preserve their spent Reaction and pending movement across both
  decisions. AI uses the higher result; player choice remains explicit. See
  [scope, grants and persistence](../SAVAGE-ATTACKER.md). Human feat selection,
  unfinished class entitlements and the other feats remain separate gaps.

Authority: [SRD pp. 47–48, 87–88](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=87).

### G4 — P1: Spell access, preparation and learning remain incomplete

F07a [#199](https://github.com/stdarg/OpenGold/issues/199) now records sourced
Wizard cantrip/book knowledge independently of preparation and retains old book
entries across advancement. Its entitlement counts expose unfilled choices;
only the existing Fire Bolt/Magic Missile preset and two level-two spell
implementations are supported. See [scope and persistence](../SPELL-ACCESS.md).
Full cantrip/book selection, class-specific replacement policies, free casts,
copying and Long Rest preparation controls remain unfinished. Cleric and other
class preparation policies are separate increments; G4 remains open.

Concrete target examples:

- Cleric and Wizard start with **three cantrips and four prepared leveled
  spells**; through levels 1–4 their preparation counts are 4/5/6/7 and their
  cantrip counts 3/3/3/4. These are 5.2.1 table values, not older
  level-plus-casting-modifier formulas.
- A Wizard's book starts with six level-one spells and gains two eligible spells
  on each Wizard level. Preparing fewer spells does not erase the book. Include
  copying costs/time, replacement books, ritual access and Arcane Recovery.
- Always-prepared subclass/species/feat spells must not consume the class's
  ordinary preparation allowance. Each grant can have its own casting ability
  and a free-use resource separate from spell slots.
- Bard, Sorcerer and Warlock replacements differ from Cleric/Druid/Wizard rest
  preparation. Paladin/Ranger can replace one prepared spell per Long Rest.
  Implement their individual class rules, not one universal editor.
- Paladin and Ranger already have Spellcasting at level **1** in this SRD.
  Warlock uses its own Pact Magic slot progression and Short Rest recharge;
  Mystic Arcanum is a separate later resource.

Authority: class sections in the matrix, particularly
[SRD pp. 36–37, 54, 58, 71–72, 77–78](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=77).

### G5 — P1: Only six spell paths exist, with substantial shared gaps

The module offers Fire Bolt, Cure Wounds, Magic Missile, Healing Word,
Scorching Ray and the blindness half of Blindness/Deafness. See
[srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), `legal_commands` and
`submit`, and [ADVANCEMENT](../ADVANCEMENT.md).

| Implemented path | Verified core | Remaining scope |
| --- | --- | --- |
| Fire Bolt | 120 feet, ranged spell attack, 1d10 in levels 1–4 | Typed Fire damage, object/ignition targeting, character-level upgrades at 5/11/17, components |
| Cure Wounds | Touch, Action, 2d8/4d8 + casting modifier | Components/hand availability, other class/feat access, higher slots and contextual healing bonuses |
| Magic Missile | Sight, 120 feet, three/four automatic darts | Split targeting, typed Force damage, Shield interaction, higher slots, components; record simultaneous-dart damage handling as a conformance case |
| Healing Word | Sight, 60 feet, Bonus Action, 2d4/4d4 + modifier | Verbal restriction, other granting classes/features, higher slots and contextual bonuses |
| Scorching Ray | 120 feet, three separate 2d6 attacks using a level-2 slot | Split targets and remaining-ray choices, typed Fire damage, higher-slot ray counts, components |
| Blindness/Deafness | 120 feet, sight, Constitution DC, end-turn recovery, up to one minute, no concentration | Deafness choice, voluntary dismissal, higher-slot additional targets, Verbal restriction; verify overlapping-spell precedence as more effects are added |

Spell references: [Fire Bolt p. 132](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=132),
[Cure Wounds p. 121](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=121),
[Magic Missile p. 146](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=146),
[Healing Word p. 139](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=139),
[Scorching Ray p. 159](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=159),
[Blindness/Deafness p. 113](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=113).

Required shared systems include:

- Concentration: one ongoing concentration effect per caster, damage saves,
  replacement, incapacitation and voluntary release.
- Spell reactions and trigger windows, including Shield/Counterspell and class
  reactions; track one-slot-per-**turn**, including casts on another actor's turn.
- Free casts and rituals, separate from slot expenditure; general V/S/M
  requirements, foci, consumed/costly materials, occupied hands and silence.
- Areas and multiple targets; creature/object eligibility; clear path versus
  sight; half/three-quarters cover; range and saving-throw effects.
- Typed damage, resistance/vulnerability/immunity, Temporary HP, ongoing effects,
  summons/companions, auras, shapechanging, dispelling and condition removal.
- Noncombat spell effects tied to exploration, inventory, time and campaign
  interactions. A spell is not complete merely because its combat button works.

The current command has one target and one destination, and `Definition` has
only level-one/two slots, one casting bonus and a small spell mask. Those are
explicit capacity limits, not just missing content entries.

### G6 — P1: Progression stops at 4 and cannot represent multiclass characters

XP tables, profile validation, feat counts, spell slots and campaign loading
all enforce the current ceiling. Proficiency +2 is embedded in attacks, saves
and casting calculations. It is correct through level 4, not beyond it.
Relevant sources: [srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp),
lines 95–97, 106–124, 820–829, 877, 947;
[campaign_save.cpp](../../src/OpenGold.Core/src/campaign_save.cpp), line 71;
[character_rules.h](../../src/OpenGold.Rules/include/opengold/character_rules.h).

Add the full advancement tables, class-specific feat schedules, subclass
choices/progression, resource maxima and recovery, spell access, Hit Dice and
level-dependent traits. Test every class at 4→5: proficiency becomes +3,
cantrips may scale, martial classes acquire their own attack features, and
spell access changes. Do not give all classes Extra Attack or full-caster slots.

`target_classes` explicitly records future intentions, not acquired levels.
Multiclass support requires actual class levels/history, total-level XP and
proficiency, starting versus multiclass proficiencies, per-class spell access,
separate casting abilities and Hit Dice pools. Combined Spellcasting uses the
5.2.1 Paladin/Ranger round-up rule; Pact Magic stays distinct. Extra Attack and
alternative AC formulas must not stack incorrectly.
Authority: [SRD pp. 23–26](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=23).

### G7 — P1: Recovery lacks Short Rests and class/species recharge policies

Only a global Long Rest policy and `recover()` exist. Hit Die size/count can be
displayed, but there is no spendable Hit Dice pool. See
[rules.h](../../src/OpenGold.Rules/include/opengold/rules.h), `RestPolicy` and
`RulesModule`; [srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines
897 and 919–926; [campaign_party.cpp](../../src/OpenGold.Core/src/campaign_party.cpp),
`rest`; [RECOVERY](../RECOVERY.md).

Implement one-hour Short Rests with chosen Hit Dice spending; full Hit Dice
recovery on Long Rest under 5.2.1; partial versus full feature recharge;
Arcane Recovery; Pact Magic; Monk Focus; Rage and other class resources;
Elf Trance; exhaustion recovery; interrupted-rest progress and resumption.
The group policy must represent individual eligibility/duration while still
advancing the shared campaign clock correctly. Removing an ineligible member
from the active group is not a substitute for that rules model.

### G8 — P1: The unconscious/death lifecycle stops at combat boundaries

Combat rolls death saves, but `Module::elapse()` advances only timed blindness
effects. An unstable survivor outside combat never makes another death save,
and a Stable survivor never regains 1 HP after 1d4 hours. A group rest cannot
rescue that character because correctly starting it requires at least 1 HP.
Healing remains available, but should not be the only route.

The module also lacks Help/Medicine stabilization and a full implementation of
damage while at 0 HP, while declaring a party defeat as soon as no party member
is conscious. The latter is a campaign/game-over policy rather than necessarily
the death of every character. See [srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp),
`damage`, `update_outcome`, `end_turn`, `elapse`; and
[SRD pp. 17–18](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=17).

**Completion gate:** stabilization, timed natural recovery, damage at 0 HP,
healing, death saves and condition state continue consistently between combat
and exploration and through saves. Confirm the intended all-unconscious
game-over policy separately; do not silently change it as part of a rules fix.

### G9 — P1: Equipment support prevents ordinary class builds

The current pack includes all 38 SRD weapon definitions plus a plain wand focus. The
campaign converts 47 original weapon/focus types. Real ranged and thrown attack
commands, weapon reach, hand counts, shield compatibility and atomic weapon
replacement now exist. Those improvements are not open gaps.

[EQ02 / #54](../WEAPON-CATALOG.md) completes the source weapon table, including
fixed Blowgun damage, while retaining original campaign conversions.
[EQ01 / #53](../ARMOR.md) supplies all twelve armor suits and Shield, with
category-based AC, starting-class training, Strength/Stealth penalties and
equipped ability-check queries. Donning/removal and shield Utilize actions
remain #198; unsupported original armor conversions remain in the equipment
queue. Starting class/background packages and spellbooks are not
awarded. A held plain wand does not establish component/focus enforcement.

Weapon properties remain incomplete. [EQ03 / #55](../HEAVY-WEAPONS.md) now
applies Heavy Disadvantage below Strength 13 (melee category) or Dexterity 13
(ranged category) to all nine Heavy weapons. Lance remains two-handed until
mounted companion play (#173) enables its catalogued exception. Ammunition is deliberately unlimited under the
[approved #57 exception](../AMMUNITION.md). [Thrown inventory](../THROWN-WEAPONS.md)
is implemented; Light extra attacks
and dual wielding are absent, and weapon mastery remains unavailable. I08 / #27
now supports one- and two-handed Versatile grips, alternate melee damage and
shield compatibility.

Complete the remaining properties (excluding the approved unlimited-ammunition exception),
Light weapon choices, action-time hand requirements,
weapon mastery, equipment interactions and proficient use from all actual grants. Extend
original-item conversion separately without treating unconverted magic/curses
as ordinary gear. Damage types and effects must flow through the shared rules
resolver. See [weapons.h](../../src/OpenGold.Rules.Srd5/src/weapons.h),
[srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), `trained` and
`character_definition`; [PARTY](../PARTY.md);
[SRD pp. 89–94](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=89).

### G10 — P1: Shared combat state cannot yet carry the full bonuses/effects

The current structured effect model implements Blinded. Unconscious/dead and
Dodge are separately represented. Other SRD conditions, contextual checks,
size-dependent space/reach, alternate movement, lighting/senses, hiding and surprise detection,
grappling/shoving, auras and ongoing modifiers are incomplete or absent.
This blocks traits such as Gnomish Cunning, Goliath ancestry, Dwarf resistance,
Rogue Sneak Attack, Monk strikes, Bardic Inspiration and many spells.

Extend the existing native effect/query model with explicit context and
source-tracked contributions. Preserve base statistics, nonstacking rules and
deterministic timing. Do not implement these bonuses as presentation-only
adjustments. In particular, advantage on a conditional save is not a permanent
numeric addition to all six saving throws.

Evidence: [status_effects.h](../../src/OpenGold.Rules.Srd5/src/status_effects.h),
[status_effects.cpp](../../src/OpenGold.Rules.Srd5/src/status_effects.cpp),
[combat_grid.cpp](../../src/OpenGold.Rules.Srd5/src/combat_grid.cpp),
[STATUS-EFFECTS](../STATUS-EFFECTS.md).

### G11 — P2: Documentation and tests can overstate or obscure support

- [PARTY](../PARTY.md) still contains an automatic level-two advancement claim
  and rules-module/checkpoint versions 0.3.0/2, despite manual advancement and
  the implemented 0.6.0/5 versions. ADVANCEMENT and RECOVERY also retain 0.5.0
  module claims. Its broad origin-feats-unsupported statement also
  obscures the implemented Soldier Savage Attacker exception.
- [CHARACTER-CREATION](../CHARACTER-CREATION.md) retains stale persistence and
  integration statements. Creation modifier text says other background
  features are unimplemented even when Soldier's combat benefit is applied.
- Existing tests verify basic all-class combat, but also the three-class
  advancement ceiling and narrow spell/feat choices as intended behavior. Passing those tests cannot establish the user's
  all-class requirement.
- `supported_features()` advertises broad labels such as `manual_advancement`
  without a class/level/variant coverage description. Consumers need a precise
  capability contract as implementation grows.

Maintain a single coverage ledger linking SRD section, feature ID, supported
classes/levels/options, implementation, tests, save version and any approved
adaptation. Documentation should derive its support claims from that inventory.

### G12 — P1: Turn, facing and movement policies depart from the SRD

These behaviors are documented application policies. Their SRD differences
need explicit disposition before the expanded class action model is designed;
they are not silently treated as authorization to reverse earlier decisions.

- **Attacks end the turn automatically.** Melee, ranged, Fire Bolt, Magic Missile
  and Scorching Ray advance initiative after resolving pending reactions, even
  if movement or a Bonus Action remains. A Fighter cannot attack and then use
  Second Wind, or attack and then move. This also obstructs later Extra Attack,
  Action Surge and Monk/Rogue action sequences. The SRD permits movement before
  or after actions and flexible Bonus Action timing unless its feature specifies
  a time. See [srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines
  506–507 and 566–575; [SRD pp. 10 and 14](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=14).
- **Turning in place provokes an opportunity attack.** Attacking toward the
  opposite horizontal side queues reactions from enemies on the side left
  behind, without the attacker moving out of reach. Those reactions resolve
  after the original attack. Standard SRD opportunity attacks instead trigger
  when a visible creature leaves reach and interrupt just before it leaves.
  See [srd5.cpp](../../src/OpenGold.Rules.Srd5/src/srd5.cpp), lines 509–522 and
  566–568; [SRD p. 15](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=15).
- **All occupied spaces block transit.** `MovementGrid::step_cost()` rejects
  every occupied destination, including an ally's space. The SRD permits moving
  through allies, incapacitated creatures, Tiny creatures and creatures at
  least two sizes apart. An ally's space does not itself impose Difficult
  Terrain in 5.2.1; another applicable terrain effect still can. Ending movement
  in an occupied space remains restricted. See
  [combat_grid.cpp](../../src/OpenGold.Rules.Srd5/src/combat_grid.cpp), lines
  81–90; [SRD p. 14](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=14).

**Completion gate:** record whether each policy is retained as a house rule or
replaced by SRD behavior. Implement the resulting decision in Stage 2, with
separate expectations for every retained departure. For SRD behavior, test
attack → movement, attack → eligible Bonus Action, multiple attacks within one
action, allied transit, and turning without movement. Preserve the existing
correct leave-reach reaction timing and checkpoint continuation. Confirm any
resulting UI control changes before selecting a layout or interaction.

## Departures requiring a project decision

These are disclosed separately from E1–E5. No policy is changed by this report.

| Existing policy | SRD relationship | Proposed treatment |
| --- | --- | --- |
| Starting-class primary scores must meet the multiclass minimum of 13 | Not a level-one SRD restriction; documented as requested in CHARACTER-CREATION | Explicitly retain as a named house rule or remove after review. Do not call it an SRD requirement. |
| Unlimited complete rerolls | Custom generation policy, documented as requested | Keep it distinguished from the specified 4d6 method and from any future standard-array/point-buy choices. |
| Manual Confirm to apply a level; fixed-average HP | Confirmation is an application workflow; fixed values are a listed SRD method | Preserve unless changed deliberately. Fix E1 without requiring random HP rolls. |
| Level-up adds new HP/resource capacity while preserving expenditure and does not wake an unconscious character | Explicit existing application policy | Retain through this plan unless separately revised; do not use leveling as an implicit rest. |
| Attacks automatically end the turn | Prevents legal remaining movement, Bonus Actions and future attack/action sequences | Resolve G12 explicitly; an SRD flow needs to preserve remaining legal choices. Confirm resulting controls before changing them. |
| Turning to attack provokes a reaction without leaving reach | Additional facing-based trigger absent from standard SRD opportunity attacks | Retain as a named house rule or restore the SRD trigger after review. |
| Allied occupied squares block transit | SRD allows passing through allies without occupancy itself imposing Difficult Terrain | Retain as a named movement restriction or implement the SRD transit rules. |
| Battleaxe, Spear, Quarterstaff and Trident always require two hands but retain one-handed damage | Explicit PARTY/weapons.h policy; SRD Versatile permits one or two hands, increasing melee damage from d8 to d10 for Battleaxe/Trident and d6 to d8 for Spear/Quarterstaff when used with two hands | Review the forced hand count and damage together; do not assume it is an accidental conversion. |
| Universal preview 250 GP; original prices and authored XP/conversions | Campaign policies, not SRD starting packages or encounter-balance certification | Separate preview/campaign grants from class/background entitlements. Decide how normal campaign creation uses SRD starting equipment. |
| Immediate all-unconscious party defeat; allies-only healing and enemies-only attacks; several automatic choice optimizations | Application restrictions on otherwise broader SRD outcomes/choices | Record individually and review as the relevant mechanics are completed. |

## Plan to close the findings

The sequence below is a proposal for implementation after review. It preserves
the established **Godot 4.x / C++20 / GDExtension** architecture. SRD rule
evaluation remains in `OpenGold.Rules.Srd5`; the edition-independent interface supplies
validated choices/commands; Core owns campaign integration and persistence;
Godot presents the supplied options. Use value ownership/RAII throughout.

### Stage 1 — Repair existing errors and establish the acceptance baseline

**Closes E1–E5, resolves the G12 policy decision, and starts G11.**

1. Add the five targeted regressions described above, then correct HP history,
   initial turn/death-save entry, stability counters, modifier provenance and
   Rogue/Monk weapon proficiency.
2. Define migration behavior for affected saved HP and bonuses. Preserve spent
   resources, player choices and deterministic continuations.
3. Record all twelve required classes and their SRD subclass in the coverage
   ledger. Mark every current omission visibly as a gap, not as completion.
4. Reconcile stale support/version claims and list policy decisions separately.
   Resolve G12's turn, facing and transit policies and the related hand/damage
   policy before implementing their effects on the shared model.

**Exit:** all five regression cases pass through normal APIs and save/load;
existing native suites remain green; the audit's supported baseline is precise.

### Stage 2 — Expand the shared character and action model

**Shared foundation for the class and rules gaps.**

Deliver this stage in small increments, each exercised by a real feature from
the class/origin inventory. Extend the existing models and build/test tools;
do not introduce a new runtime or a speculative general-purpose rules engine.

- Persist stable class/subclass/species/lineage IDs, acquired features and
  choices, source-specific ability/proficiency grants, level history and HP/Hit
  Dice history. Distinguish class-granted, background, species and level-choice
  feats without inventing extra grants.
- Add resource pools with explicit maxima, usage and recharge events. Support
  Short Rest, Long Rest, turn, round and feature-specific recovery.
- Represent spell grants, cantrips, books, prepared lists, casting abilities,
  ordinary/Pact slots and free casts separately. Use class-specific tables.
- Extend commands for feature choices, multiple targets, reaction triggers and
  attack sequences. Invalid/stale choices must still be atomic.
- Apply the reviewed G12 decisions to turn completion, facing reactions and
  occupied-space transit. Preserve remaining actions/movement wherever the
  selected rules permit them, including interactions with class features.
- Extend the existing effect model for typed damage, Temporary HP, conditions,
  concentration, contextual bonuses and movement/senses/size. Include the
  campaign death/recovery lifecycle here.
- Add versioned save migration and bounded validation as each new state type
  lands; use owned state/IDs, not actor ownership pointers.

**Exit:** representative tests prove each shared mechanism, including conflicting
effects, simultaneous events, reserve members and checkpoint continuation. A
higher-level profile is not made available just by removing its level guard.

### Stage 3 — Complete creation, origins and equipment for all classes

**Closes the level-one portions of G2/G3/G4/G9.**

Implement all background packages, species choices/traits, class proficiencies,
skills/tools/languages, origin and Fighting Style feats, starting equipment and
spell selections. Complete ordinary weapon/armor access and full modifier-source
explanations. Apply the reviewed campaign/house-rule policies explicitly.

**Exit:** ordinary creation can produce a mechanically valid member of each of
the twelve classes and each of the nine species; required choices persist and
produce actual effects. Tests include Human extra Origin feat, Criminal Alert,
Acolyte/Sage Magic Initiate, Gnome mental saves, Halfling Luck, Orc endurance,
lineage spell grants, and Fighter level-one Fighting Style.

Before adding or changing UI flows, ask the user numbered questions confirming
layout and control behavior, as required by AGENTS.md. This plan selects no
layout. Use existing native scenes and recognizable, keyboard-accessible controls.

### Stage 4 — Complete levels 1–4 across all twelve classes

**First proposed all-class playable milestone; closes the early G1 matrix.**

Implement these work packages on the shared mechanisms, with none treated as
optional class support:

| Package | Required work | Key acceptance examples |
| --- | --- | --- |
| Fighter, Barbarian, Rogue, Monk | Attack action budgets, mastery, martial resources, skills, mobility, relevant reactions; all four SRD subclasses | Fighter Action Surge adds a non-Magic action; Champion crits on 19; Rage modifies eligible damage/resistance; Rogue Sneak Attack qualifies correctly and is once per turn; Monk's attacks use its Martial Arts rules and Focus recovers correctly |
| Cleric and Wizard | Full low-tier spell lists and preparation/book lifecycle, Channel Divinity/orders, recovery, Scholar, Life Domain and Evoker | Life Domain adds 2 + slot level healing under its actual timing rule; Channel Divinity has separate uses; Wizard books retain unprepared spells; Arcane Recovery restores eligible slots once per Long Rest |
| Bard and Sorcerer | Their complete low-tier spells, Inspiration/Expertise and Innate Sorcery/Sorcery Points/Metamagic, Lore and Draconic subclasses | Inspiration and Cutting Words apply in the right windows; point/slot conversion and Metamagic costs are atomic; subclass bonuses/spells use the revised SRD values |
| Paladin and Ranger | Level-one half-caster progression, mastery/styles, healing/marks/smites, Channel Divinity, Devotion and Hunter | Both cast at level 1; Paladin's Lay On Hands uses a Bonus Action and a pool; Divine Smite uses the 5.2.1 spell trigger/economy; Ranger free Hunter's Mark casts still obey concentration |
| Druid and Warlock | Wild Shape/companions and Pact Magic/invocations, full low-tier lists, Land and Fiend subclasses | Wild Shape uses the revised form/Temporary HP/resource rules, not a copied 2014 model; companions have controlled lifetimes; Pact slots recharge on Short Rest and Magical Cunning has its own limit |

Cover all available SRD spells/features in this tier, including exploration use.
For open-ended effects that require an authored computer-game adaptation,
prepare a concrete proposal for review instead of silently disabling or
substituting the spell. Include split-target casts and early reactions such as
Shield in the normal command flow.

**Exit:** every class passes creation → equipment → combat → reward → confirmed
level-up → rest → next combat → save/reload through level 4. All subclass grants
at 3 and feat/cantrip/resource changes at 4 are exercised. Run mixed-class
parties so cross-character buffs, reactions and recovery are verified.

### Stage 5 — Extend every class in complete level bands

**Closes the single-class portions of G5/G6 beyond the current ceiling.**

Deliver 5–10, then 11–16, then 17–20 across all twelve classes, each with the
features and complete SRD spell access for that band. In particular:

- At 5: XP 6,500, proficiency +3, class-appropriate Extra Attack, cantrip upgrades,
  new spell levels, species level-5 traits and class-specific resource changes.
- At 9/13/17: proficiency changes must reach every derived attack, save, skill,
  DC and proficiency-scaled resource; validate intermediate class tables too.
- Respect extra feat levels for Fighter and Rogue, higher martial attack counts,
  invocations and Mystic Arcanum, subclass schedules and long-duration effects.
- At 19/20: implement the SRD Epic Boons and class/subclass capstones. Use their
  actual ability caps rather than a universal score-20 validation rule.

**Exit for each band:** every class passes all gained-level table assertions,
feature behavior and save/reload checks; the released cap and remaining gaps
are explicit. Do not raise the cap for only three classes and label the work
complete. The campaign may impose a reviewed lower cap while the rules library
continues to have a separately defined target.

### Stage 6 — Multiclassing and remaining campaign adaptations

Complete G6's multiclass rules as a separate milestone: prerequisites across
all acquired classes, level accounting, limited entry proficiencies, mixed Hit
Dice, spell preparation versus slot access, Pact Magic interaction, and
nonstacking AC/Extra Attack. Establish source-based examples before coding.

Complete campaign-facing uses of skills, noncombat spells, companions,
resurrection, movement and time as required by the coverage ledger. Explicitly
resolve open-ended spell behavior and remaining conversion policies. This work
must not be hidden behind the assertion that combat-only effects cover the SRD.

**Exit:** all included combinations and adaptations have documented semantics,
conformance examples and persistence tests; any intentionally excluded feature
has a reviewed scope decision. Single-class completion can be reported
separately without implying multiclass completion.

## Verification strategy and checks performed

Use independent expectations from the pinned SRD, not tests that merely repeat
the implementation's tables. Add a feature/source/test link for every acquired
bonus and resource. Test level boundaries, negative modifiers, capped ability
scores, duplicate grants, prerequisites, failed choices, rest boundaries,
conditional saves and resource interactions.

For each rule increment:

1. Native unit/conformance tests verify arithmetic, legal actions and state
   transitions with deterministic seeds.
2. Campaign tests verify advancement, inventory, recovery, reserve status,
   combat handoff and save migrations without resetting resources.
3. Existing Godot acceptance routes verify the real controls expose the legal
   choices and display the same totals and provenance as the rules module.
4. Representative normal parties exercise original campaign integration;
   authored fixtures remain useful tests, not proof of ordinary class support.

For this audit, the following existing targets were rebuilt and all **8/8
native suites passed** in the existing macOS native build. Commands use Bash:

```bash
cmake --build build/mac-check --parallel 4 --target \
  opengold_character_tests opengold_advancement_tests opengold_rules_tests \
  opengold_party_tests opengold_status_effect_tests opengold_content_tests \
  opengold_save_tests opengold_combat_grid_tests

ctest --test-dir build/mac-check --output-on-failure \
  -R '^opengold_(character|advancement|rules|party|status_effect|content|save|combat_grid)_tests$'
```

No new regression tests or gameplay changes were made. `OPENGOLD_GAME_DIR` was
unset, so optional installed-game paths were not exercised by this run. No new
Godot acceptance or graphical playthrough was performed. E1's numeric example
was independently calculated; E1–E5 otherwise rest on source-path inspection,
not a claim that the proposed regression scenarios already ran.

The next implementation gate is review of the plan and explicit policy choices,
followed by Stage 1. Existing contributor instructions require implementation
authorization after design review and numbered confirmation of UI layout/control
behavior before selecting new flows.

## Sources and attribution

- [Official SRD overview](https://www.dndbeyond.com/srd) and
  [SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
- Project requirements: [TECH](../TECH.md), [PRD](../PRD.md),
  [RULES](../RULES.md), [CHARACTER-CREATION](../CHARACTER-CREATION.md),
  [ADVANCEMENT](../ADVANCEMENT.md), [PARTY](../PARTY.md),
  [RECOVERY](../RECOVERY.md), [STATUS-EFFECTS](../STATUS-EFFECTS.md).
- The SRD-derived material in this report uses the attribution in
  [NOTICE](../../data/rules/srd-5.2.1/NOTICE.md), under CC BY 4.0. The report
  summarizes rules and identifies OpenGoldBox's implementation differences.
