# Concentration implementation

[Foundation #207](https://github.com/stdarg/OpenGold/issues/207) implements shared
value-owned transitions in [concentration.h](../src/OpenGold.Rules.Srd5/src/concentration.h).
Authority: [SRD 5.2.1 p. 179](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=179).

Each owner has at most one active application, identified by encounter scope,
caster and application ID. Beginning another effect returns the old source for
cleanup, including recasts. Validation runs before replacement. Integration must
call this at the start of a new concentration spell's casting, not after its hit
or save resolves; invalid commands must be rejected before starting the cast.

Expiry, voluntary release and incapacitation return the ended source. Damage
uses the existing Constitution saving throw machinery, including advantage and
disadvantage. DC is half damage rounded down, bounded to 10–30. Zero damage and
inactive concentration do not roll; incapacitation/death ends without a save.
The caller supplies damage after defenses and before Temporary HP absorption,
and supplies the resulting incapacitated/dead state. A natural 1 or 20 does not
supersede the saving throw total. No actor ownership or combat pointers are held.

[Tests](../tests/concentration_tests.cpp) cover source replacement, invalid-input
atomicity, separate owners/scopes, Silence's 600,000 ms duration, exact expiry
and time partitioning, DC boundaries, save outcomes, modifier cancellation, RNG
draw counts and cleanup identity. Concentration, status-effect and Ray of Frost
focused tests pass. This isolated helper does not change existing gameplay,
save formats or module version.

## Concentration subrecord

The isolated `CN1` reader/writer preserves empty state or the application scope,
ID, caster and exact remaining milliseconds. It parses into a fresh value,
rejecting unknown versions, invalid presence flags, zero identifiers/duration,
negative or overflowing numbers and malformed tokens. Tests verify canonical
bytes and identical save/RNG/expiry continuation after a round trip.

This is not yet embedded in combat or campaign saves. The enclosing reader must
validate the caster against the owner, the application against the area/effect
registry, and duration against the named spell. It must reject trailing fields
at its own record boundary. Existing save formats remain unchanged; embedding
it changes the current formats in place without migration (see the
[pre-1.0 format policy](SAVES.md#pre-10-format-policy)).

## Combat integration

Rules 0.6.69 first used this state in combat for Shield of Faith and Heroism.
Every Concentration spell in `spell_table.h` now shares it: one Concentration
spell per caster, the damage save, ending when the caster is Incapacitated or
drops to 0 Hit Points, expiry with combat time, and `CN1` embedded in each
actor of combat checkpoints. Ending Concentration ends the spell's effects on
every creature and removes its area. Concentration ends when the combat ends;
it is not kept in campaign saves. See [Paladin](PALADIN.md#buffs-and-concentration).

The [representative encounter](RULES.md#representative-encounter) shows a Web
held by Concentration freeing two creatures at once when a hit breaks it, and
a restored checkpoint continuing in step with the original session.

#38 (the lifecycle) is closed. Silence (#208) and its campaign carry-over (#209)
were closed as not planned under
[SCOPE-2](SRD-DECISIONS.md#scope-2-2026-09-30-exploration-halves-and-marginal-features),
and speech-blocking sources (#39) under
[SCOPE-1](SRD-DECISIONS.md#scope-1-2026-09-30-level-cap-deferrals-and-rare-situations);
Silence is now an ordinary spell (`silence_checks` in
[cleric_channel_tests.cpp](../tests/cleric_channel_tests.cpp)).
