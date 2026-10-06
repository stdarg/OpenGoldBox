# Sacred Flame and starting Cleric cantrips

Rules **0.6.23** adds a playable Cleric path for
[#203](https://github.com/stdarg/OpenGold/issues/203), using the creation and
combat controls approved in questions 15–16. The issue remains open for the
partial-cover exception and remaining component/source integration.

## Rules and controls

Authority: [SRD 5.2.1 pp. 36–37, 159, 191](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
Sacred Flame uses an Action. A visible, living
creature within 60 feet makes a Dexterity save against 8 + proficiency bonus +
Wisdom modifier. Failure deals 1d8 Radiant damage through character level 4;
success deals none. Typed defenses apply. This is a saving throw, so natural
1/20 have no automatic result, damage gains no ability modifier or critical
dice, and adjacent enemies do not impose ranged-attack Disadvantage.

The shared spell-save query now automatically fails Strength/Dexterity saves
for creatures at zero HP, without consuming a save roll. Sacred Flame damage
at zero HP causes one death-save failure, including within five feet. Existing
Dodge and untrained-armor Dexterity-save modifiers apply. Dead targets, total
cover and lack of sight are rejected; a Blinded caster cannot target even self.
Untrained armor also restricts casting.

Cleric creation uses the existing Spell Choices step after Training and before
Name. Sacred Flame has a labeled checkbox, keyboard access and selection counts.
Back retains selections; changing class clears invalid choices and hides their
checkboxes. The base entitlement is three cantrips, increasing to four at level
4. Missing catalog choices remain pending. Preset Clerics receive the supported
choice. Grants carry the source
`class:cleric:spellcasting` and acquisition level 1.

The main combat row lists Sacred Flame in the approved [Spell dropdown](CANTRIP-CONTROLS.md)
when the current Cleric knows it. Cast is disabled when casting is unavailable;
the existing keyboard action cycle remains. Selecting the spell and pressing
Cast highlights legal creatures, including allies and the caster. Clicking an
eligible ally casts without changing party selection. The standalone combat
demo retains its existing layout; both creators share the spell-choice control.

## Persistence and verification

Profile recipes validate Cleric cantrips against their sourced grants.
Loading never fills missing choices or replenishes resources. Player saving
remains restricted to camping or an inn.

- [Native tests](../tests/sacred_flame_tests.cpp) use independent level-1–4
  save/damage/RNG expectations, defenses, range, sight, hands, armor, Dodge,
  self/allied/dead/unconscious targets, atomic rejection, real PC/NPC campaign
  handoff, advancement, presets and exact replay.
- [Creation tests](../tests/cleric_cantrip_view_tests.gd) exercise keyboard
  selection, pending counts, Back, class changes and the resulting sheet.
- [Combat tests](../tests/sacred_view_tests.gd) check known/blocked/unknown
  controls, keyboard selection and an actual cast at an ally. English and Spanish
  rows are also rendered at 1120×800 and 1920×1080.

Validation passed all 37 native/tool checks and 14 Godot runtime checks, with
five native fixture prerequisites. The art-dependent Cleric and Wizard creator
scripts and Spanish localization script also passed. Both Godot extensions
built successfully. Cleric choice controls were inspected in English and Spanish
at 1120×800; combat rows were inspected in both languages at both sizes above.
Localization validation covers 734 messages. No original game art is committed.

## Remaining acceptance

The current battlefield represents open, opaque and difficult terrain, with no
half- or three-quarters-cover data. Sacred Flame's exception to those save
bonuses cannot yet be verified in a real covered encounter. **#203 stays open**;
total-cover rejection alone does not complete cover conformance.

Live speech blockers remain [#39](https://github.com/stdarg/OpenGold/issues/39).
Other grant routes remain Magic Initiate #75/#200, Pact of the Tome #161,
Thaumaturge and Blessed Warrior #140. Cleric preparation, Divine Order and the
fourth selection are in [Cleric preparation](CLERIC-PREPARATION.md); level-up
cantrip replacement waits for a second Cleric cantrip. Higher-level damage scaling
remains #176–178. The zero-HP save rule does not complete all conditions in #35.
The [spell inventory](SPELL-INVENTORY.md) therefore records this path as partial.
