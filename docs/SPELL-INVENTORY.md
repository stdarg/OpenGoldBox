# SRD spell inventory through character level 4

Tracking issue: [#165](https://github.com/stdarg/OpenGold/issues/165). Inventory
baseline: rules **0.6.21**, commit **1e89c1c**. This is a work inventory, not a
claim that the listed spells are implemented. Current delivery adds the
[0.6.22 Wizard Poison Spray path](POISON-SPRAY.md) and
[0.6.23 Cleric Sacred Flame path](SACRED-FLAME.md), and
[0.6.25 Wizard Ray of Frost path](RAY-OF-FROST.md), and
[0.6.37 level-one Warlock Eldritch Blast path](ELDRITCH-BLAST.md), and
[0.6.38 Wizard Shocking Grasp path](SHOCKING-GRASP.md) below. Rules 0.6.39 adds
the [level-one Warlock Poison Spray route](WARLOCK-POISON-SPRAY.md) to an existing
partial spell; the distinct-spell count is unchanged. Rules 0.6.40 adds
[level-one Sorcerer access](SORCERER-CANTRIPS.md) to Fire Bolt, Poison Spray, Ray of
Frost and Shocking Grasp. Rules 0.6.44 adds the [Chill Touch class paths](CHILL-TOUCH.md),
bringing the count to twelve partial spells and 127 missing. On 2026-09-30,
decision [DM-1](SRD-DECISIONS.md#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells)
removed 33 spells that need a DM to adjudicate (see
[Removed: needs a DM](#removed-needs-a-dm)), leaving twelve partial and 94 missing.
Rules 0.6.64 implemented Inflict Wounds. On 2026-10-04, decision
[DM-3](SRD-DECISIONS.md#dm-3-2026-10-04-spells-a-crpg-cannot-use) removed 17 more
spells that need a DM or have nothing to act on in this game, leaving **89 spells:
twelve partial, one implemented and 76 missing**.

## Scope and counting

The milestone requires **89 distinct spells: 16 cantrips, 41 level-one spells
and 32 level-two spells** (106 before DM-3 removed 17, 139 before DM-1 removed 33). It covers all twelve SRD classes, their twelve SRD
subclasses, all nine species and their lineages, the four SRD backgrounds, and
eligible SRD feats through character level 4. The baseline has **six partial
playable paths and 133 missing spells**. With 0.6.22 this becomes **seven partial
playable paths and 132 missing spells**. With 0.6.23 there are **eight partial
playable paths and 131 missing spells**. With 0.6.25 there are **nine partial
playable paths and 130 missing spells**. With 0.6.37 there are **ten partial
playable paths and 129 missing spells**. With 0.6.38 there are **eleven partial
playable paths and 128 missing spells**. With 0.6.44 there are **twelve partial
playable paths and 127 missing spells**. After DM-1 there are **twelve partial
playable paths and 94 missing spells**; no complete spell conformance is claimed.

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
Each spell name below links to its description page. Class-list membership is
recorded separately from additional grant routes: being in a list does not mean
a particular character knows or has prepared the spell. Cantrip choice counts,
spellbooks, preparation, casting ability, acquisition level, free casts and
recharge must remain source-specific.

- Bard, Cleric, Druid, Sorcerer, Warlock and Wizard can access spell levels 0–2
  by class level 4. Paladin and Ranger normal spellcasting reaches only level 1
  in this milestone; their optional styles also grant cantrips.
- Barbarian/Berserker, Fighter/Champion, Monk/Open Hand and Rogue/Thief have no
  intrinsic spell list in this tier. Their species and feat routes still apply.
- Life, Land, Devotion, Draconic and Fiend grants are included at level 3.
  Evoker adds choices from its Wizard list. Lore's additional cross-list spells
  arrive at level 6; Hunter grants no extra spell list here.
- Species level-5 grants, invocations requiring Warlock level 5+, higher-level
  subclasses and Magical Secrets do not add spells to this milestone. Sorcerer
  slot conversion cannot create a level-3 slot before Sorcerer level 5.
  Metamagic still needs its applicable effect/upcasting interactions checked.
- No spell above level 2 is granted by these routes through character level 4.
  Unique level-two Paladin/Ranger spells unavailable through another included
  route (for example Find Steed and Shining Smite) belong to later milestones.
- Scrolls, magic items, monster spellcasting and arbitrary campaign rewards can
  introduce other spells independently of character level. Those content and
  item inventories are separate; this is the class/species/background/feat
  entitlement inventory required by [#165](https://github.com/stdarg/OpenGold/issues/165). Multiclassing remains [#179](https://github.com/stdarg/OpenGold/issues/179)–184.

### Normal class-list cross-check

Counts below reproduce the printed class tables within the level-four limit,
less the spells DM-1 and DM-3 removed; they are not a character's selection allowance.
Description-only additions marked † in the inventory add Phantasmal Force to
Bard, Sorcerer and Wizard, and Mind Spike to Sorcerer. Thus the complete
inventories are 30 Bard, 43 Sorcerer and 47 Wizard entries after DM-3 (which also
removes Phantasmal Force); the other counts are
unchanged. See the source cautions below.

| Class | Cantrips | Level 1 | Level 2 | SRD list pages | Access integration |
| --- | ---: | ---: | ---: | --- | --- |
| Bard (B) | 3 | 15 | 12 | 33–34 | [#126](https://github.com/stdarg/OpenGold/issues/126) |
| Cleric (C) | 3 | 11 | 9 | 38–39 | [#91](https://github.com/stdarg/OpenGold/issues/91) |
| Druid (D) | 6 | 12 | 12 | 44 | [#152](https://github.com/stdarg/OpenGold/issues/152) |
| Paladin (P) | 0 | 10 | 0 | 55–56 | [#138](https://github.com/stdarg/OpenGold/issues/138) |
| Ranger (R) | 0 | 8 | 0 | 60 | [#145](https://github.com/stdarg/OpenGold/issues/145) |
| Sorcerer (S) | 8 | 16 | 18 | 67–68 | [#132](https://github.com/stdarg/OpenGold/issues/132) |
| Warlock (K) | 4 | 8 | 7 | 75 | [#160](https://github.com/stdarg/OpenGold/issues/160) |
| Wizard (W) | 7 | 20 | 20 | 79–80 | [#97](https://github.com/stdarg/OpenGold/issues/97), [#37](https://github.com/stdarg/OpenGold/issues/37) |
| Barbarian, Fighter, Monk, Rogue | 0 | 0 | 0 | 28–30, 47–53, 61–64 | Species/feat sources below |

## Additional grant routes

A route tag in a spell row means that spell is eligible for that route; it is
not automatically granted when a feature offers a choice. The normal class
list column intentionally does not absorb subclass or species additions.

| Tag | Availability / grant | Authority (SRD pages) | Integration issue |
| --- | --- | --- | --- |
| MI | Magic Initiate: two cantrips and one level-one spell from the same chosen Cleric, Druid or Wizard list; selected ability, one free leveled cast per Long Rest, slot casting and level-up replacement | 87 | [#75](https://github.com/stdarg/OpenGold/issues/75), [#200](https://github.com/stdarg/OpenGold/issues/200) |
| Tome | Pact of the Tome: three cantrips and two level-one Ritual-tagged spells from any class list, with the feature's preparation and book restrictions; ritual casting itself is removed by DM-1, so they use slots | 74 | [#161](https://github.com/stdarg/OpenGold/issues/161), [#160](https://github.com/stdarg/OpenGold/issues/160) |
| Thaum | Cleric's Thaumaturge Divine Order: one extra Cleric cantrip at level 1 | 37 | [#91](https://github.com/stdarg/OpenGold/issues/91) |
| Magician | Druid's Magician Primal Order: one extra Druid cantrip at level 1 | 42 | [#152](https://github.com/stdarg/OpenGold/issues/152) |
| Blessed | Paladin's Blessed Warrior option: two Cleric cantrips at level 2 (delivered; Sacred Flame so far) | 54 | [#140](https://github.com/stdarg/OpenGold/issues/140) |
| DruidicWarrior | Ranger's Druidic Warrior option: two Druid cantrips at level 2 (delivered; Poison Spray so far) | 59 | [#147](https://github.com/stdarg/OpenGold/issues/147) |
| Smite | Paladin level 2 always prepares Divine Smite and gains its own free cast | 54 | [#141](https://github.com/stdarg/OpenGold/issues/141), [#200](https://github.com/stdarg/OpenGold/issues/200) |
| Favored | Ranger level 1 always prepares Hunter's Mark and gains limited free casts (delivered) | 57–58 | [#146](https://github.com/stdarg/OpenGold/issues/146), [#200](https://github.com/stdarg/OpenGold/issues/200) |
| Life | Cleric level 3: Aid, Bless, Cure Wounds, Lesser Restoration (delivered) | 40 | [#94](https://github.com/stdarg/OpenGold/issues/94) |
| Arid | Land Druid level 3: Blur, Burning Hands, Fire Bolt | 46 | [#157](https://github.com/stdarg/OpenGold/issues/157) |
| Polar | Land Druid level 3: Fog Cloud, Hold Person, Ray of Frost | 46 | [#157](https://github.com/stdarg/OpenGold/issues/157) |
| Temperate | Land Druid level 3: Misty Step, Shocking Grasp, Sleep | 46 | [#157](https://github.com/stdarg/OpenGold/issues/157) |
| Tropical | Land Druid level 3: Acid Splash, Ray of Sickness, Web | 46 | [#157](https://github.com/stdarg/OpenGold/issues/157) |
| Devotion | Paladin level 3: Protection from Evil and Good, Shield of Faith (delivered, always prepared) | 56 | [#142](https://github.com/stdarg/OpenGold/issues/142) |
| Draconic | Sorcerer level 3: Chromatic Orb, Command, Dragon's Breath (Alter Self removed by DM-3) | 70 | [#135](https://github.com/stdarg/OpenGold/issues/135) |
| Fiend | Warlock level 3: Burning Hands, Command, Scorching Ray (Suggestion removed by DM-3) | 76 | [#163](https://github.com/stdarg/OpenGold/issues/163) |
| Evoker | Level-3 Evocation Savant adds Wizard Evocation spellbook choices no higher than level 2; acquisition must follow the feature rather than automatic preparation | 82 | [#101](https://github.com/stdarg/OpenGold/issues/101) |
| Shadows | Armor of Shadows: Mage Armor on self without a slot | 72 | [#161](https://github.com/stdarg/OpenGold/issues/161) |
| Vigor | Fiendish Vigor at Warlock level 2: False Life on self without a slot, maximizing its die | 73 | [#161](https://github.com/stdarg/OpenGold/issues/161) |
| Drow | Dancing Lights at character level 1; Faerie Fire at 3 | 84–85 | [#67](https://github.com/stdarg/OpenGold/issues/67) |
| High | High Elf's Prestidigitation is removed by DM-1; it starts with a Wizard cantrip instead, replaceable at a Long Rest | 84–85 | [#67](https://github.com/stdarg/OpenGold/issues/67) |
| HighFixed | High Elf gains Detect Magic at character level 3 | 84–85 | [#67](https://github.com/stdarg/OpenGold/issues/67) |
| Wood | Longstrider at character level 3 (level-1 Druidcraft removed by DM-1) | 84–85 | [#67](https://github.com/stdarg/OpenGold/issues/67) |
| Abyssal | Poison Spray at character level 1; Ray of Sickness at 3 | 86 | [#73](https://github.com/stdarg/OpenGold/issues/73) |
| Chthonic | Chill Touch at character level 1; False Life at 3 | 86 | [#73](https://github.com/stdarg/OpenGold/issues/73) |
| Infernal | Fire Bolt at character level 1; Hellish Rebuke at 3 | 86 | [#73](https://github.com/stdarg/OpenGold/issues/73) |

Acolyte grants Magic Initiate (Cleric) and Sage grants Magic Initiate (Wizard);
Criminal and Soldier do not grant spells. Human's chosen Origin feat can grant
Magic Initiate, as can Warlock's level-2 Lessons of the First Ones. Eligible
level-four feat choices can also grant it. Its repeatability uses different
spell lists, not unrestricted duplication of free uses. See [#61](https://github.com/stdarg/OpenGold/issues/61), [#63](https://github.com/stdarg/OpenGold/issues/63), [#71](https://github.com/stdarg/OpenGold/issues/71), [#75](https://github.com/stdarg/OpenGold/issues/75),
[#82](https://github.com/stdarg/OpenGold/issues/82) and [#161](https://github.com/stdarg/OpenGold/issues/161). Dragonborn, Dwarf, Goliath, Halfling and Orc add no spell names in
this tier; their supernatural abilities are not spells merely because they
produce similar effects. Forest and Rock Gnome lineages and every Tiefling legacy's
Thaumaturgy granted only spells DM-1 removed, so those grants are gone; the
Druid's Druidic Speak with Animals, Wild Companion, Pact of the Chain, Mask of Many
Faces, Misty Visions and the Wizard's Ritual Adept are removed with them.
Elf/Tiefling leveled grants retain their own free use
and selected casting ability; [#200](https://github.com/stdarg/OpenGold/issues/200) must preserve those sources independently.

## Delivery and evidence

Every spell needs source-correct targets, components, casting time, resource
costs, range, duration, concentration, effects, defenses, upcasting where
available, grant access and ordinary playable controls. Relevant exploration
uses and camp/inn save continuation are part of completion. No reviewed
adaptation has been approved for any missing spell in this inventory.

Every row depends on the applicable access work ([#36](https://github.com/stdarg/OpenGold/issues/36) and the route issues
above) and component rules ([#39](https://github.com/stdarg/OpenGold/issues/39); [#40](https://github.com/stdarg/OpenGold/issues/40) wherever Materials apply). The Dependencies
column highlights further capabilities, not an exhaustive substitute for the
spell description. [#174](https://github.com/stdarg/OpenGold/issues/174) includes ritual timing/exploration; [#35](https://github.com/stdarg/OpenGold/issues/35) covers needed
conditions/effect semantics; [#43](https://github.com/stdarg/OpenGold/issues/43) covers area targeting. These are capability
trackers: depend on the relevant bounded child, not completion of every spell
in the tracker. Avoid circular dependencies by choosing one representative
spell to prove each new capability.

**Partial** rows link to the existing code and tests below. **Missing** rows have
no spell implementation/conformance test yet. A **queue** entry remains assigned
to [#165](https://github.com/stdarg/OpenGold/issues/165) for decomposition, not a runnable implementation batch. Create its
named batch checklist under the existing owner before coding: at most 3–5
straightforward spells using verified
mechanics, or one complex spell with explicit prerequisites. Separate tickets
are optional when they represent genuinely independent deliverables; see
[workflow](SRD-WORKFLOW.md). The first four
new single-spell children are [#202](https://github.com/stdarg/OpenGold/issues/202)–205. Existing [#41](https://github.com/stdarg/OpenGold/issues/41), [#141](https://github.com/stdarg/OpenGold/issues/141), [#146](https://github.com/stdarg/OpenGold/issues/146) and [#166](https://github.com/stdarg/OpenGold/issues/166)–171
are reused rather than duplicated. Closing any one effect issue does not
complete its still-pending class/species/feat integrations.

| Partial spell | Implementation | Existing evidence | Remaining spell issue |
| --- | --- | --- | --- |
| Shocking Grasp | [scope](SHOCKING-GRASP.md), [rules](../src/OpenGold.Rules.Srd5/src/srd5.cpp) | [native](../tests/shocking_grasp_tests.cpp), [creator](../tests/shocking_cantrip_view_tests.gd), [combat UI](../tests/shocking_view_tests.gd) | [#223](https://github.com/stdarg/OpenGold/issues/223): remaining granting routes and speech blockers |
| Eldritch Blast | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp), [scope](ELDRITCH-BLAST.md) | [native](../tests/eldritch_blast_tests.cpp), [creator](../tests/warlock_cantrip_view_tests.gd), [combat UI](../tests/eldritch_view_tests.gd) | [#204](https://github.com/stdarg/OpenGold/issues/204): objects, levels 2–4, Tome, speech blocking |
| Sacred Flame | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp), [scope](SACRED-FLAME.md) | [native](../tests/sacred_flame_tests.cpp), [creator](../tests/cleric_cantrip_view_tests.gd), [combat UI](../tests/sacred_view_tests.gd) | [#203](https://github.com/stdarg/OpenGold/issues/203): partial-cover exception, speech blocking and remaining sources |
| Poison Spray | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp), [scope](POISON-SPRAY.md) | [native](../tests/poison_spray_tests.cpp), [creator](../tests/cantrip_view_tests.gd), [combat UI](../tests/poison_view_tests.gd) | [#202](https://github.com/stdarg/OpenGold/issues/202): speech blocking and remaining source integrations; native prerequisite #206 |
| Fire Bolt | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) | [rules tests](../tests/rules_tests.cpp), [typed damage](../tests/damage_tests.cpp), [components](../tests/spell_component_tests.cpp) | [#166](https://github.com/stdarg/OpenGold/issues/166): object/ignition use, grants and full conformance |
| Cure Wounds | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) | [rules tests](../tests/rules_tests.cpp), [components](../tests/spell_component_tests.cpp) | [#167](https://github.com/stdarg/OpenGold/issues/167): all sources, contextual healing and complete casting restrictions |
| Magic Missile | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) | [rules tests](../tests/rules_tests.cpp), [typed damage](../tests/damage_tests.cpp), [components](../tests/spell_component_tests.cpp) | [#168](https://github.com/stdarg/OpenGold/issues/168), [#42](https://github.com/stdarg/OpenGold/issues/42), [#41](https://github.com/stdarg/OpenGold/issues/41): split targets, simultaneous damage/roll policy and Shield |
| Healing Word | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) | [rules tests](../tests/rules_tests.cpp), [components](../tests/spell_component_tests.cpp) | [#169](https://github.com/stdarg/OpenGold/issues/169): all sources, contextual healing and complete casting restrictions |
| Scorching Ray | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) | [rules tests](../tests/rules_tests.cpp), [typed damage](../tests/damage_tests.cpp), [components](../tests/spell_component_tests.cpp) | [#170](https://github.com/stdarg/OpenGold/issues/170), [#42](https://github.com/stdarg/OpenGold/issues/42): split targets and complete casting restrictions |
| Blindness/Deafness | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp), [effects](../src/OpenGold.Rules.Srd5/src/status_effects.cpp) | [status tests](../tests/status_effect_tests.cpp), [components](../tests/spell_component_tests.cpp) | [#171](https://github.com/stdarg/OpenGold/issues/171): Deafness choice, dismissal, overlapping effects, Verbal blocking and future higher-slot targets |

Higher character levels and slots remain [#176](https://github.com/stdarg/OpenGold/issues/176)–178. Current Somatic restrictions
are covered by [#201](https://github.com/stdarg/OpenGold/issues/201); speech-blocking sources remain [#39](https://github.com/stdarg/OpenGold/issues/39). These existing tests
are evidence of partial behavior, not certification of the full spell.

## Cantrips

| Spell / SRD page | Normal lists | Additional routes | Further dependencies | Status / work |
| --- | --- | --- | --- | --- |
| [Acid Splash](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=107) (p. 107) | S, W | MI, Tome, High, Tropical | [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Chill Touch](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=115) (p. 115) | S, K, W | MI, Tome, High, Chthonic | [#35](https://github.com/stdarg/OpenGold/issues/35) | Partial: Wizard 1–4, Sorcerer/Warlock 1; creature casting, healing prevention and Q40 earned recovery verified in [Chill Touch](CHILL-TOUCH.md). MI/Tome/species, objects and higher class progression remain open under [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Eldritch Blast](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=127) (p. 127) | K | Tome | Objects [#225](https://github.com/stdarg/OpenGold/issues/225) | Partial level-one Warlock creatures; [#204](https://github.com/stdarg/OpenGold/issues/204) |
| [Fire Bolt](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=132) (p. 132) | S, W | MI, Tome, High, Arid, Infernal | — | Partial; [#166](https://github.com/stdarg/OpenGold/issues/166) |
| [Poison Spray](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=153) (p. 153) | D, S, K, W | MI, Tome, High, Magician, DruidicWarrior, Abyssal | — | Partial Wizard levels 1–4 and Warlock level 1; [#202](https://github.com/stdarg/OpenGold/issues/202) |
| [Produce Flame](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=156) (p. 156) | D | MI, Tome, Magician, DruidicWarrior | [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Ray of Frost](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=157) (p. 157) | S, W | MI, Tome, High, Polar | [#35](https://github.com/stdarg/OpenGold/issues/35) | Partial: [Wizard selection, Cold attack and timed slow](RAY-OF-FROST.md); other sources and verbal blockers remain. [#205](https://github.com/stdarg/OpenGold/issues/205) |
| [Resistance](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=158) (p. 158) | C, D | MI, Tome, Thaum, Blessed, Magician, DruidicWarrior | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Sacred Flame](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=159) (p. 159) | C | MI, Tome, Thaum, Blessed | Partial-cover model; [#39](https://github.com/stdarg/OpenGold/issues/39) | Partial Cleric path; [#203](https://github.com/stdarg/OpenGold/issues/203) |
| [Shillelagh](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=162) (p. 162) | D | MI, Tome, Magician, DruidicWarrior | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Shocking Grasp](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=162) (p. 162) | S, W | MI, Tome, High, Temperate | [#39](https://github.com/stdarg/OpenGold/issues/39) | Partial Wizard path; [#223](https://github.com/stdarg/OpenGold/issues/223) |
| [Sorcerous Burst](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=163) (p. 163) | S | Tome | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Spare the Dying](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=163) (p. 163) | C, D | MI, Tome, Thaum, Blessed, Magician, DruidicWarrior | [#32](https://github.com/stdarg/OpenGold/issues/32) | Implemented for Clerics (and Druid-list routes); [Cleric](CLERIC.md#guiding-bolt-bane-and-spare-the-dying) |
| [Starry Wisp](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=165) (p. 165) | B, D | MI, Tome, Magician, DruidicWarrior | [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [True Strike](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=171) (p. 171) | B, S, K, W | MI, Tome, High | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Vicious Mockery](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=171) (p. 171) | B | Tome | [#35](https://github.com/stdarg/OpenGold/issues/35) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |

## Level-one spells

| Spell / SRD page | Normal lists | Additional routes | Further dependencies | Status / work |
| --- | --- | --- | --- | --- |
| [Bane](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=112) (p. 112) | B, C, K | MI | [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Clerics; [Cleric](CLERIC.md#guiding-bolt-bane-and-spare-the-dying) |
| [Bless](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=113) (p. 113) | C, P | MI, Life | [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Clerics and Paladins; [Paladin](PALADIN.md#buffs-and-concentration) |
| [Burning Hands](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=114) (p. 114) | S, W | MI, Evoker, Arid, Fiend | [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Charm Person](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=115) (p. 115) | B, D, S, K, W | MI | [#35](https://github.com/stdarg/OpenGold/issues/35) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Chromatic Orb](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=115) (p. 115) | S, W | MI, Evoker, Draconic | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Color Spray](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=116) (p. 116) | B, S, W | MI | [#35](https://github.com/stdarg/OpenGold/issues/35), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Command](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=116) (p. 116) | B, C, P | MI, Draconic, Fiend | [#35](https://github.com/stdarg/OpenGold/issues/35) | Implemented for Clerics and Paladins, without Drop; [Paladin](PALADIN.md#buffs-and-concentration) |
| [Cure Wounds](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=121) (p. 121) | B, C, D, P, R | MI, Life | — | Partial; [#167](https://github.com/stdarg/OpenGold/issues/167) |
| [Detect Magic](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=123) (p. 123) | B, C, D, P, R, S, K, W | MI, Tome, HighFixed | [#38](https://github.com/stdarg/OpenGold/issues/38), [#174](https://github.com/stdarg/OpenGold/issues/174) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Dissonant Whispers](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=124) (p. 124) | B | — | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Divine Favor](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=125) (p. 125) | P | — | — | Implemented for Paladins; [Paladin](PALADIN.md#buffs-and-concentration) |
| [Divine Smite](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=125) (p. 125) | P | Smite | — | Implemented for Paladins; [Paladin](PALADIN.md#smites) |
| [Ensnaring Strike](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=128) (p. 128) | R | — | [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Rangers; [Ranger](RANGER.md#ensnaring-strike-and-restrained) |
| [Entangle](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=128) (p. 128) | D, R | MI | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Implemented for Rangers; [Ranger](RANGER.md#entangle-and-aiming-area-spells) |
| [Expeditious Retreat](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=129) (p. 129) | S, K, W | MI | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Faerie Fire](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=129) (p. 129) | B, D | MI, Drow | [#38](https://github.com/stdarg/OpenGold/issues/38), [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [False Life](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=129) (p. 129) | S, W | MI, Vigor, Chthonic | [#34](https://github.com/stdarg/OpenGold/issues/34) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Fog Cloud](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=133) (p. 133) | D, R, S, W | MI, Polar | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43), [#46](https://github.com/stdarg/OpenGold/issues/46) | Implemented for Rangers; [Ranger](RANGER.md#fog-cloud) |
| [Goodberry](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=137) (p. 137) | D, R | MI | [#174](https://github.com/stdarg/OpenGold/issues/174) | Implemented for Rangers as a camp heal (CLASS-6); [Ranger](RANGER.md#goodberry) |
| [Grease](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=137) (p. 137) | S, W | MI | [#35](https://github.com/stdarg/OpenGold/issues/35) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Guiding Bolt](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=138) (p. 138) | C | MI | — | Implemented for Clerics; [Cleric](CLERIC.md#guiding-bolt-bane-and-spare-the-dying) |
| [Healing Word](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=139) (p. 139) | B, C, D | MI | — | Partial; [#169](https://github.com/stdarg/OpenGold/issues/169) |
| [Hellish Rebuke](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=140) (p. 140) | K | Infernal | [#41](https://github.com/stdarg/OpenGold/issues/41) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Heroism](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=140) (p. 140) | B, P | — | [#34](https://github.com/stdarg/OpenGold/issues/34), [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Paladins; [Paladin](PALADIN.md#buffs-and-concentration) |
| [Hex](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=140) (p. 140) | K | — | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Hideous Laughter](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=140) (p. 140) | B, K, W | MI | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Hunter’s Mark](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=141) (p. 141) | R | Favored | [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Rangers; [Ranger](RANGER.md#favored-enemy-and-hunters-mark) |
| [Ice Knife](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=141) (p. 141) | D, S, W | MI | [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Identify](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=142) (p. 142) | B, W | MI, Tome | [#174](https://github.com/stdarg/OpenGold/issues/174) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Inflict Wounds](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=143) (p. 143) | C | MI | — | Implemented; [Inflict Wounds](INFLICT-WOUNDS.md) |
| [Longstrider](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=145) (p. 145) | B, D, R, W | MI, Wood | [#174](https://github.com/stdarg/OpenGold/issues/174) | Implemented for Rangers; [Ranger](RANGER.md#longstrider) |
| [Mage Armor](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=145) (p. 145) | S, W | MI, Shadows | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Magic Missile](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=146) (p. 146) | S, W | MI, Evoker | [#42](https://github.com/stdarg/OpenGold/issues/42) | Partial; [#168](https://github.com/stdarg/OpenGold/issues/168) |
| [Protection from Evil and Good](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=157) (p. 157) | C, D, P, K, W | MI, Devotion | [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Clerics and Paladins, no holy water (CLASS-3); [Paladin](PALADIN.md#buffs-and-concentration) |
| [Ray of Sickness](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=158) (p. 158) | S, W | MI, Tropical, Abyssal | [#35](https://github.com/stdarg/OpenGold/issues/35) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Sanctuary](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=159) (p. 159) | C | MI | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Searing Smite](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=160) (p. 160) | P | — | — | Implemented for Paladins; [Paladin](PALADIN.md#smites) |
| [Shield](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=161) (p. 161) | S, W | MI | [#41](https://github.com/stdarg/OpenGold/issues/41) | Missing; [#41](https://github.com/stdarg/OpenGold/issues/41) |
| [Shield of Faith](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=162) (p. 162) | C, P | MI, Devotion | [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Paladins and Clerics; [Paladin](PALADIN.md#buffs-and-concentration) |
| [Sleep](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=163) (p. 163) | B, S, W | MI, Temperate | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Thunderwave](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=169) (p. 169) | B, D, S, W | MI, Evoker | [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |

## Level-two spells

| Spell / SRD page | Normal lists | Additional routes | Further dependencies | Status / work |
| --- | --- | --- | --- | --- |
| [Acid Arrow](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=107) (p. 107) | W | Evoker | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Aid](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=107) (p. 107) | B, C, D | Life | — | Implemented for Clerics; [Cleric](CLERIC.md#aid) |
| [Barkskin](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=112) (p. 112) | D | — | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Blindness/Deafness](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=113) (p. 113) | B, C, S, W | — | [#35](https://github.com/stdarg/OpenGold/issues/35) | Partial; [#171](https://github.com/stdarg/OpenGold/issues/171) |
| [Blur](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=114) (p. 114) | S, W | Arid | [#38](https://github.com/stdarg/OpenGold/issues/38), [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Darkness](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=122) (p. 122) | S, K, W | Evoker | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43), [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Dragon’s Breath](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=126) (p. 126) | S, W | Draconic | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Enlarge/Reduce](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=127) (p. 127) | B, D, S, W | — | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38), [#44](https://github.com/stdarg/OpenGold/issues/44) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Flame Blade](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=132) (p. 132) | D, S | — | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Flaming Sphere](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=132) (p. 132) | D, S, W | Evoker | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Gust of Wind](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=138) (p. 138) | D, S, W | Evoker | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Heat Metal](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=139) (p. 139) | B, D | — | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Hold Person](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=141) (p. 141) | B, C, D, S, K, W | Polar | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38) | Implemented for Clerics; [Cleric](CLERIC.md#hold-person-and-paralyzed) |
| [Invisibility](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=143) (p. 143) | B, S, K, W | — | [#38](https://github.com/stdarg/OpenGold/issues/38), [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Knock](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=143) (p. 143) | B, S, W | — | [#174](https://github.com/stdarg/OpenGold/issues/174) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Lesser Restoration](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=144) (p. 144) | B, C, D | Life | [#35](https://github.com/stdarg/OpenGold/issues/35) | Implemented for Clerics (Blinded only so far); [Cleric](CLERIC.md#life-domain-level-3) |
| [Magic Weapon](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=146) (p. 146) | S, W | — | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Mind Spike](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=149) (p. 149) | S†, K, W | — | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Mirror Image](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=150) (p. 150) | B, S, K, W | — | [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Misty Step](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=150) (p. 150) | S, K, W | Temperate | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Moonbeam](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=150) (p. 150) | D | — | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Prayer of Healing](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=154) (p. 154) | C | — | [#30](https://github.com/stdarg/OpenGold/issues/30), [#174](https://github.com/stdarg/OpenGold/issues/174) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Protection from Poison](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=157) (p. 157) | C, D | — | [#35](https://github.com/stdarg/OpenGold/issues/35) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Ray of Enfeeblement](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=157) (p. 157) | K, W | — | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Scorching Ray](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=159) (p. 159) | S, W | Evoker, Fiend | [#42](https://github.com/stdarg/OpenGold/issues/42) | Partial; [#170](https://github.com/stdarg/OpenGold/issues/170) |
| [See Invisibility](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=160) (p. 160) | B, S, W | — | [#46](https://github.com/stdarg/OpenGold/issues/46) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Shatter](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=161) (p. 161) | B, S, W | Evoker | [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Silence](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=162) (p. 162) | B, C | — | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43), [#174](https://github.com/stdarg/OpenGold/issues/174) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Spike Growth](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=164) (p. 164) | D | — | [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Spiritual Weapon](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=165) (p. 165) | C | — | [#38](https://github.com/stdarg/OpenGold/issues/38) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Warding Bond](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=173) (p. 173) | C | — | — | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |
| [Web](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=174) (p. 174) | S, W | Tropical | [#35](https://github.com/stdarg/OpenGold/issues/35), [#38](https://github.com/stdarg/OpenGold/issues/38), [#43](https://github.com/stdarg/OpenGold/issues/43) | Missing; queue [#165](https://github.com/stdarg/OpenGold/issues/165) |

## Source cautions and verification

The class lists and spell descriptions must be checked together. Phantasmal
Force's description (p. 151) names Bard, Sorcerer and Wizard, but the spell is
missing from all three printed class tables. Mind Spike's description (p. 149)
names Sorcerer in addition to Warlock and Wizard, but the Sorcerer table omits
it. For this inventory, the explicit description grants are retained and marked
† rather than silently excluding them. These add one spell to the 138-name
printed-table union and four class memberships. This is a documented source
interpretation, not a gameplay adaptation or an implemented grant.

Flaming Sphere is labeled Evocation in the Druid/Sorcerer/Wizard lists but
Conjuration in its description (p. 132). This inventory does not silently
resolve that school discrepancy; its implementation child must record a
source-backed decision before school-dependent eligibility is coded.

Inventory verification compared all eight eligible printed class lists with
all level-zero, one and two spell-description headers. That independent pass
found the two class-membership omissions above. All 141 low-level spell
descriptions are accounted for: 106 in this milestone (89 after DM-3), 33 removed by DM-1 and
Find Steed/Shining Smite deferred until Paladin level 5. Every named additional grant was checked
against the inventory; none adds another name. Every spell has a description
page and every partial spell has code/test evidence. Existing live component definitions independently
confirm the six partial spell IDs. Repository links and issue assignments were
checked; no runtime code or save schema is changed by this inventory.

[#165](https://github.com/stdarg/OpenGold/issues/165) remains open until decomposition, implementations, source-route integration,
independent tests and any explicitly reviewed adaptations satisfy its acceptance.
The inventory alone does not close that tracker or the all-class milestone.

## Removed: needs a DM

Decision [DM-1](SRD-DECISIONS.md#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells)
(2026-09-30) removed these spells as an intentional SRD exception. Their effects
are narrative, social or exploration-only and need a DM to adjudicate; this
scripted campaign has none. Ritual casting (including Wizard Ritual Adept) and
the invocations and features that exist only to cast them (Mask of Many Faces,
Misty Visions, Pact of the Chain, Wild Companion, Druidic's Speak with Animals)
are removed too. Spells with a combat, lighting, trap, item or encumbrance
effect stay.

- Cantrips (9): Druidcraft, Elementalism, Guidance, Mage Hand, Mending, Message,
  Minor Illusion, Prestidigitation, Thaumaturgy.
- Level one (12): Alarm, Comprehend Languages, Create or Destroy Water, Detect
  Evil and Good, Detect Poison and Disease, Disguise Self, Find Familiar,
  Illusory Script, Purify Food and Drink, Silent Image, Speak with Animals,
  Unseen Servant.
- Level two (12): Animal Messenger, Arcane Lock, Arcanist’s Magic Aura, Augury,
  Detect Thoughts, Enhance Ability, Gentle Repose, Locate Animals or Plants,
  Locate Object, Magic Mouth, Rope Trick, Zone of Truth.

## Removed: nothing to act on in this game

Decision [DM-3](SRD-DECISIONS.md#dm-3-2026-10-04-spells-a-crpg-cannot-use)
(2026-10-04) removed 17 more spells: they need a DM, or depend on falling,
climbing, jumping, stealth, weight, light and darkness, traps or open-ended social
effects, none of which this game models. The Otherworldly Leap invocation, which
only casts Jump, is removed with them. Darkness and Fog Cloud stay because they
obscure areas in combat.

- Cantrips (2): Dancing Lights, Light.
- Level one (4): Animal Friendship, Feather Fall, Floating Disk, Jump.
- Level two (11): Alter Self, Calm Emotions, Continual Flame, Darkvision,
  Enthrall, Find Traps, Levitate, Pass without Trace, Phantasmal Force, Spider
  Climb, Suggestion.

## Attribution

This work includes material from the System Reference Document 5.2.1 (“SRD 5.2.1”) by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.
