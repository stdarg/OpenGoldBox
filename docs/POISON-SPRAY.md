# Poison Spray and starting Wizard cantrips

[#206](https://github.com/stdarg/OpenGold/issues/206) supplies the native
prerequisite for [#202](https://github.com/stdarg/OpenGold/issues/202) and
[#37](https://github.com/stdarg/OpenGold/issues/37). Rules **0.6.22** also adds
the creation and main-game casting controls approved in questions 12–14.
This is a playable Wizard path, not completion of every source or component rule.

## Rules and player controls

Authority: [SRD 5.2.1 p. 153](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=153).
Poison Spray uses an Action to make a ranged spell
attack against a creature within 30 feet. A hit deals 1d12 Poison damage through
character level 4. It uses the casting ability and proficiency, doubles damage
dice on a critical hit, applies typed defenses, and does not impose Poisoned or
request a Constitution save. A living ally, the caster, or an unconscious creature
is eligible; dead creatures and blocked paths are rejected. Existing
untrained-armor, action, sight and adjacent-hostile attack rules apply.

Attacks against a creature at zero HP now include the Unconscious/Prone attack
modifiers: Advantage, canceled by Prone's Disadvantage beyond five feet; a hit
within five feet is critical even when its natural roll is below 20. The actual
roll remains in the log. This does not complete the broader condition lifecycle
in #35, including remaining Prone after regaining consciousness.

Wizard creation has a Spell Choices step after Training and before Name.
Labeled, keyboard-accessible Fire Bolt and Poison Spray checkboxes show the
selection count and pending entitlement. Back retains selections. Newly created
characters start with explicit choices; missing catalog choices stay pending.
Preset Wizards receive both available cantrips. Magic Missile remains the existing spellbook preset;
full book/preparation editors and filling pending choices remain #37.

The main combat row uses the approved [Spell dropdown and Cast button](CANTRIP-CONTROLS.md)
below End Turn/Grip, to the right of Dash/Adrenaline Rush. It lists only known
cantrips; Cast is disabled when the selected spell is unavailable. Rules snapshots
report knowledge separately from current casting eligibility.
Poison Spray participates in the existing keyboard action cycle. In its targeting
mode, clicking a legal ally casts at that ally rather than changing party selection.
No player combat-save controls are added. The old standalone combat demo retains
its existing controls; the shared creation step is available in both creators.

## Persistence and verification

Campaign saves store the creation cantrip choices. Profile recipes validate
explicit cantrip grants against their listed spells.
Loading never replenishes resources or automatically learns another cantrip.
Player saving remains limited to camping or an inn.

- [Native tests](../tests/poison_spray_tests.cpp): independent level-1–4 hit,
  miss and critical rolls, RNG consumption, defenses, range, path, casting hands,
  armor, ally/self/dead/unconscious targets, failed-command atomicity, real PC/NPC
  campaign handoff, advancement and exact campaign/internal-combat continuation.
- [Creation UI tests](../tests/cantrip_view_tests.gd): real Wizard creation,
  keyboard choices, counts, Back navigation and sheet output.
- [Combat UI tests](../tests/poison_view_tests.gd): minimum-window placement,
  enabled/disabled controls, keyboard action cycle and actual ally targeting.
- [Localization UI tests](../tests/localization_tests.gd): Spanish creation
  choices and existing translated source/pending-count explanations.

The delivery passed all 36 native/tool checks and all 13 Godot checks (plus
four native fixture prerequisites). The art-dependent creator and Spanish
localization scripts also passed with the user's local assets. English and
Spanish combat rows were rendered and inspected at 1120×800 and 1920×1080;
the English creator was checked at minimum size and Spanish at 1920×1080.
Both native Godot extensions built successfully. No original game art is added
to the repository.

## Open integration work

Poison Spray remains partial in [the full spell inventory](SPELL-INVENTORY.md).
Live speech-blocking sources remain #39. Druid #152, Sorcerer #132, Warlock #160,
Magic Initiate #75/#200, Pact of the Tome #161, Elf #67, Druidic Warrior #147 and
Abyssal Tiefling #73 must supply their own grants/abilities and applicable choices.
Magician access is part of #152. Higher-level scaling remains #176–178.
Parents #202, #37 and #165 stay open until their remaining acceptance is met.

Rules 0.6.39 also delivers the [level-one Warlock source](WARLOCK-POISON-SPRAY.md)
with Charisma attacks and explicit selection. Later
Warlock levels remain #160; parent #202 still tracks other grant integrations.

Rules 0.6.40 adds the explicit [level-one Sorcerer source](SORCERER-CANTRIPS.md)
with Charisma casting in #228. References above to remaining Sorcerer access
now concern later levels and the rest of #132, rather than this delivered route.
