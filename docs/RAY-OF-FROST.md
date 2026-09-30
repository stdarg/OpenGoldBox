# Ray of Frost through level 4

Rules 0.6.25 delivers the Wizard player path for [#205](https://github.com/stdarg/OpenGold/issues/205).
Authority: [SRD 5.2.1, p. 157](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=157).

## Behavior and access

An explicitly learned Ray of Frost uses the Wizard's Intelligence for a ranged
spell attack against a creature within 60 feet. It spends the Magic action,
deals 1d8 Cold damage on a hit, and reduces Speed by 10 feet until the start of
the caster's next turn. Critical hits double the weapon-independent damage dice;
Cold resistance, vulnerability and immunity use the shared damage resolver.
A hit still slows a Cold-immune creature. Misses do not apply the effect.
Somatic eligibility uses the existing occupied-hand rules; V/S metadata is recorded.

Independent applications retain caster identity, encounter scope and duration.
Their Speed penalties do not stack. Expiring one application leaves the others
active. Speed floors at zero. Movement spent remains spent when the penalty
starts or ends; ordinary Dash and Adrenaline Rush add the current reduced Speed.

The approved Spell Choices step includes Ray of Frost alongside Fire Bolt and
Poison Spray. Unfilled choices retain the existing pending flow. New preset
Wizards receive all three supported choices. The approved Spell dropdown and Cast button expose the learned spell,
including legal ally/self targeting without changing party selection. Keyboard
activation and translated labels use the established controls.

## Persistence

Profile recipes permit the new cantrip. Combat checkpoints record Dash
allowance counts. Effect state (FX8) stores the independently sourced, nonsaving
slow timers. The shared campaign recovery scheduler handles these deadlines without
rolling repeat saves. Camping expires the short duration before the ordinary save.
No player combat-saving controls were added.

## Evidence and remaining scope

- [Native tests](../tests/ray_of_frost_tests.cpp): independent damage/RNG examples
  at levels 1–4, defenses, misses/criticals, two real casters, nonstacking and
  exact expiry, movement already spent, Dash, invalid targets, occupied hands,
  deterministic checkpoint continuation, campaign handoff, camp save/reload,
  and malformed effects.
- [Combat view checks](../tests/frost_view_tests.gd): learned-only selection,
  keyboard Cast, actual ally click, localized slow display and hidden save controls.
- [Creation checks](../tests/cantrip_view_tests.gd): keyboard choice, counts,
  pending choices, Back retention and final sheet. Cleric creation also passes.
- Native regression: 39 checks pass after correcting the spell-catalog count
  assertion. All 16 Godot runtime checks and seven native fixture prerequisites
  pass. Main and demo extensions build; localization validates 744 messages.
  English/Spanish combat controls were rendered at 1120×800 and 1920×1080.

The spell remains **partial**, and #205 stays open. Speech-blocking enforcement
belongs to [components #39](https://github.com/stdarg/OpenGold/issues/39).
Additional access routes remain [Sorcerer #132](https://github.com/stdarg/OpenGold/issues/132),
[High Elf #67](https://github.com/stdarg/OpenGold/issues/67),
[Magic Initiate #75](https://github.com/stdarg/OpenGold/issues/75),
[Pact of the Tome #161](https://github.com/stdarg/OpenGold/issues/161), and
[Polar Land #157](https://github.com/stdarg/OpenGold/issues/157).
Full spell selection remains #37; scaling above level 4 remains #176–178.
See the [spell inventory](SPELL-INVENTORY.md) for the complete grant mapping.

Rules 0.6.40 adds the explicit [level-one Sorcerer source](SORCERER-CANTRIPS.md)
with Charisma casting in #228. References above to remaining Sorcerer access
now concern later levels and the rest of #132, rather than this delivered route.
