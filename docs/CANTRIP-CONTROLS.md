# Shared combat cantrip controls

Approved in question 18 as preparation for [Ray of Frost #205](https://github.com/stdarg/OpenGoldBox/issues/205).
The main game's Spell dropdown and Cast button replace the individual Fire Bolt,
Poison Spray and Sacred Flame buttons. They occupy the existing row to the right
of Adrenaline Rush; Action Surge occupies that space for eligible Fighters, who
currently have no supported cantrip grants. Future combined feature routes must
integrate both controls without overlap when those grants become available.

The dropdown lists only the current actor's known cantrips. Cast follows the
selected spell's native legal-command availability, including spent actions and
component restrictions. Knowledge remains visible when casting is unavailable.
Choosing a spell does not consume an action and cancels previous targeting;
pressing Cast highlights legal targets. A legal target click submits the same
native command used previously. Poison Spray and Sacred Flame retain their
approved ally-targeting behavior. The existing A/Space cycle also remains.

Both controls use standard Godot styling and keyboard focus. Enter/arrow keys
operate the dropdown; focused Enter/Space operates Cast without ending the turn.
Selection survives unrelated refreshes, and changing actors/knowledge removes
choices that no longer apply. Stable spell IDs are stored as item metadata;
labels are localized. No SRD costs or eligibility rules are duplicated in UI.
Rules 0.6.25 adds the sourced [Ray of Frost](RAY-OF-FROST.md) choice to this list.

Rules, grants and save formats are unchanged. No combat saving controls are
exposed. The standalone demo keeps its previous controls. Ray of Frost's further source/component integrations were completed under #205.

## Verification

Nine affected Godot checks and their six native fixture prerequisites passed.
[Poison Spray](../tests/poison_view_tests.gd) exercises known/unknown/blocked
choices, keyboard dropdown selection, Cast and ally clicks; [Sacred Flame](../tests/sacred_view_tests.gd)
also exercises A/Space casting. Component, Action Surge, Adrenaline Rush, Savage
Attacker, Grip, opportunity and general combat checks cover neighboring behavior.
English/Spanish rows were rendered at 1120×800 and 1920×1080; localization
validation covers 739 messages. The existing game extension was rebuilt.
