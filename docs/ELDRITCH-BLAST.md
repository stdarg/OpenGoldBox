# Level-one Warlock Eldritch Blast

Rules 0.6.37 delivers [#224](https://github.com/stdarg/OpenGold/issues/224), a
bounded child of [Pact Magic #160](https://github.com/stdarg/OpenGold/issues/160)
and [Eldritch Blast #204](https://github.com/stdarg/OpenGold/issues/204).
Authority: [SRD 5.2.1, pp. 71 and 127](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

The approved Q26 Spell Choices step exposes Eldritch Blast to level-one Warlocks.
The grant records `class:warlock:pact_magic`, level one and cantrip access. The
rules validate class, source, acquisition level, uniqueness and selected casting
spells. Rules 0.6.37 exposed one of two owed cantrips.
[Warlock Poison Spray](WARLOCK-POISON-SPRAY.md) now makes both available.
Presets select the supported cantrip in advance.
This delivers only the cantrip portion of Pact Magic, not slots or preparation.

The existing Spell dropdown and Cast button perform a Charisma-based ranged
spell attack against a living creature within 120 feet, including allies/self.
A hit deals 1d10 Force damage; criticals double dice. Shared attack modifiers,
typed defenses, line of effect and armor training apply.
The Magic action is spent; movement, Bonus Action, Reaction and resources remain.
V/S metadata is registered; general speech blocking remains #39.

Profile recipes permit the sourced cantrip. No player combat-save controls exist.

Evidence: [native rules and persistence](../tests/eldritch_blast_tests.cpp),
[creation controls](../tests/warlock_cantrip_view_tests.gd), and
[combat controls](../tests/eldritch_view_tests.gd). Native checks independently
assert Charisma rather than Intelligence, hit/miss/critical damage, Force
resistance/vulnerability/immunity, range boundaries, occupied hands, armor,
allies, unconscious targets, atomic rejection, PC/NPC campaign handoff,
Short/Long Rest save/reload and deterministic internal continuation.

Remaining: targetable objects [#225](https://github.com/stdarg/OpenGold/issues/225),
Warlock advancement and the rest of Pact Magic #160, Tome and invocations
[#161](https://github.com/stdarg/OpenGold/issues/161), full selection #37,
components #39, and higher beams #42/#176–178. The parent #204 stays open;
level-one evidence does not certify levels two through four or object targets.

Verification: all 42 native/tool checks passed, followed by the final focused
Eldritch check and all 18 headless Godot checks (nine native prerequisites).
The asset-backed creator check also passed. English/Spanish creation and combat
renders cover 1120×800 and 1920×1080. The localized Cast position follows the
dropdown's actual width; regression checks require separation and fit. Main/demo
extensions build, and 820 localization messages validate. Logs are in
`/tmp/eb-regression.log`, `/tmp/eb-final-checks.log`, `/tmp/eb-creator.log` and
`/tmp/eb-combat-render.log`.
