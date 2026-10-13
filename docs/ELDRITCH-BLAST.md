# Eldritch Blast through level 4

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
Spells have no components
([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)).

Profile recipes permit the sourced cantrip. No player combat-save controls exist.

Evidence: [native rules and persistence](../tests/eldritch_blast_tests.cpp),
[creation controls](../tests/warlock_cantrip_view_tests.gd), and
[combat controls](../tests/eldritch_view_tests.gd). Native checks independently
assert Charisma rather than Intelligence, hit/miss/critical damage, Force
resistance/vulnerability/immunity, range boundaries, occupied hands, armor,
allies, unconscious targets, atomic rejection, PC/NPC campaign handoff,
Short/Long Rest save/reload and deterministic internal continuation.

## Levels 1 to 4

`eldritch_blast_tests` checks every level from 1 to 4 (2026-10-13). Each
Warlock is advanced with its ordinary level-up choices, so from level 2 it
casts with Agonizing Blast, which adds its Charisma modifier to a hit's damage
(not doubled on a critical). The checks cover the attack bonus, hit, miss and
critical damage, Force resistance, vulnerability and immunity, atomic
rejection, PC and NPC campaign handoff, and Short/Long Rest save and reload at
every level. One beam is fired through level 4; more beams come at level 5
([#176](https://github.com/stdarg/OpenGold/issues/176)–178). Agonizing Blast
and Repelling Blast are also checked in `opengold_warlock_tests`
([WARLOCK.md](WARLOCK.md)).

Eldritch Blast is on the Warlock's list only, so the Warlock is its one class
route, casting with Charisma. Pact of the Tome, which can take any cantrip,
is still pending ([#161](https://github.com/stdarg/OpenGold/issues/161)).

Objects are not targets: object targeting was dropped
([SCOPE-2](SRD-DECISIONS.md#scope-2-2026-09-30-exploration-halves-and-marginal-features);
[#225](https://github.com/stdarg/OpenGold/issues/225) closed as not planned).
