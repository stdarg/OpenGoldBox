# Poison Spray and starting Wizard cantrips

[#206](https://github.com/stdarg/OpenGoldBox/issues/206) supplies the native
prerequisite for [#202](https://github.com/stdarg/OpenGoldBox/issues/202) and
[#37](https://github.com/stdarg/OpenGoldBox/issues/37). Rules **0.6.22** also adds
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

## Routes through level 4

Poison Spray is complete through character level 4 for every route the game
has (2026-10-13, [#202](https://github.com/stdarg/OpenGoldBox/issues/202)). Each
casts with its own ability:

| Route | Ability | Evidence |
| --- | --- | --- |
| Wizard, levels 1–4 | Intelligence | [poison_spray_tests.cpp](../tests/poison_spray_tests.cpp) |
| Sorcerer | Charisma | [sorcerer_cantrip_tests.cpp](../tests/sorcerer_cantrip_tests.cpp) |
| Warlock | Charisma | [warlock_poison_checks.h](../tests/warlock_poison_checks.h) |
| Druid, including Magician's extra cantrip | Wisdom | [druid_tests.cpp](../tests/druid_tests.cpp) |
| Ranger's Druidic Warrior, level 2 | Wisdom | [ranger_choices_checks.h](../tests/ranger_choices_checks.h) |
| Magic Initiate (Druid or Wizard list) | The feat's ability | [MAGIC-INITIATE.md](MAGIC-INITIATE.md) |

Later routes: an Abyssal Tiefling
([#73](https://github.com/stdarg/OpenGoldBox/issues/73)) knows Poison Spray, and
a High Elf ([#67](https://github.com/stdarg/OpenGoldBox/issues/67)) or a Pact of
the Tome ([#161](https://github.com/stdarg/OpenGoldBox/issues/161)) can choose it.

Spells have no components
([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)), so no
speech or hand blocker applies. Damage above level 4 follows
[cantrip damage by level](SPELL-SLOTS.md#cantrip-damage-by-level).
