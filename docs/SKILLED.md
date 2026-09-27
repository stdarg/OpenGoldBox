# Skilled feat delivery packet

## Frozen next batch

- Original requirement#77 from backlog familyB01. One owner, no agents/new tasks.
- Outcome: choose Skilled through the existing supported level4 feat entitlements
  and gain three distinct skill/tool proficiencies with sourced grants and real
  check effects. Skilled is repeatable; validate each acquisition's entitlement
  and choices independently, while rejecting duplicate use of one entitlement.
- Include the complete SRD proficiency catalog. Existing training has18 skills
  and33 tool variants; add the four missing tool proficiency entries (Disguise
  Kit, Forgery Kit, Navigator's Tools, Poisoner's Kit). This is proficiency
  metadata, not tool acquisition, crafting or new exploration actions.
- Exclude Human's extra Origin-feat source#71, new class/level entitlements,
  equipment/wealth/crafting rules and other feats. Those original issues retain
  their scope; never invent a second feat entitlement to demonstrate repeatability.
- Reuse feature grants, class advancement transactions, skill/tool check queries,
  shared Training checkbox styling and existing level-up/spell pages.
- SRD owns options, counting, validation, grant outcomes and modifiers; Core
  owns the advancement transaction; main/demo render rules-owned choices.
- Requested Astra/high for sourced repeatable choices and save compatibility;
  actual configured runtime selection unverified. No settings changes/delegation.
  Escalate after two unsuccessful fixes of the same failure or outside-scope
  dependency. No speculative prerequisite or automatic issue splitting.
- Preflight observed2026-09-26 16:40:50 UTC. No implementation started.
  User subsequently requested pausing after Alert#74; goal paused before asking
  SKILLED-1. Require explicit resumption before further work.
  SKILLED-1 requires control approval before implementation. Record continuation
  timing after the answer, with the60-minute checkpoint and90-minute maximum.

## Acceptance and preservation

1. All18 skills and37 tool variants are available where not already proficient;
   choose exactly three in any mixture. Reject unknown, duplicate, already-known
   or incomplete choices atomically. Expertise remains unchanged; proficiency is
   not added twice, and combined skill/tool effects use the existing SRD query.
2. Real feat entitlements, provenance, non-stacking proficiencies, repeatable feat
   semantics and per-acquisition source validation. No background or species
   silently receives Skilled. Existing class-specific advancement choices stay.
3. Capture actual0.6.61 campaign/combat output before writer changes. Retain all
   supported saves and existing choices/resources/wounds/equipment. Old saves
   gain no invented Skilled selections. New choices and checks survive reload.
4. MainEN/ES and demoEN at1120×800/1920×1080, mouse/keyboard, three-pick validation,
   Back/Cancel, switching feat and Wizard spell-page composition. Verify actual
   UI outputs against native results, focused grants/training/advancement/save
   checks and appropriate final integrated regression.
5. Record tested code/commands/limitations in SRD-COVERAGE, commit/push and close
   #77 only when its acceptance is complete. No other issue closure is implied.

## Proposed control: SKILLED-1 (pending)

Keep the existing700×670 level-up window. When Skilled is selected, Next opens a
Skilled Training page after the current feat/class choices and before a Wizard's
existing spell-choice page. Reuse the existing scrollable page area at(24,70),
652×475. Show one labeled checkbox list with “Skill”/“Tool” prefixes and a shared
“Selected:0/3” count. Existing proficiencies are visibly unavailable. Require
exactly three new proficiencies before continuing or confirming.

Use the existing Back/Cancel/Next/Confirm button positions. Back preserves valid
selections; switching away from Skilled discards only its unconfirmed picks.
Cancel/Escape commits nothing. The final Confirm applies the entire advancement
once, including other required class/spell choices. Standard control styling,
keyboard focus/scrolling and mouse selection remain available. No new creation
screen, combat control or saving behavior is proposed.

Static page sketch:

```text
Level up — Skilled Training
Selected: 0/3
┌ Scrollable proficiency list ─────────────────────────────┐
│ [ ] Skill: Acrobatics                                    │
│ [ ] Skill: Animal Handling                               │
│ ...                                                     │
│ [ ] Tool: Alchemist's Supplies                           │
│ ...                                                     │
└─────────────────────────────────────────────────────────┘
                            Back     Cancel     Next/Confirm
```

## Sources and targeted routes

[SRD5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
[Skilled](https://www.dndbeyond.com/sources/dnd/br-2024/feats#Skilled),
[tool catalog](https://www.dndbeyond.com/sources/dnd/br-2024/equipment#Tools).
Official rules confirm three skill/tool proficiencies and repeatability.

`feature_grants.cpp` owns feat provenance and repeatability; `training.cpp`
owns the skill/tool catalog and check grants; `srd5.cpp` owns advancement/profile
and module-version validation. Main/demo `level_up_view.cpp` own the existing
700×670 page flow. `training_control.h` supplies styled checkbox controls.
Keep `rules.h` contracts generic and preserve Core's transaction boundary.
