# Test fixtures

These files are independent reference data. They contain no original game
assets and no saves.

`weapons-srd-5.2.1.tsv` is a separate, source-verified table of all 38 weapons
from SRD 5.2.1 p.91. Tests compare every definition/property, including exact
quarter-pound/copper units, without reading expected values from implementation.

`armor-srd-5.2.1.tsv` is an independent transcription of the SRD 5.2.1 p. 92
table. Its thirteen rows cover twelve suits and Shield. Times are seconds;
Shield's zero times denote a Utilize action, not a free equipment change.

`spell-offer-baseline.txt` records the spell offers each class sees. The spell
table test writes it when it is missing and otherwise compares against it.

## No save fixtures

Before 1.0 there is no save compatibility: every save kind has exactly one
accepted format (see `docs/SAVES.md`), and older saves are rejected with
a player-facing message instead of being migrated. Frozen prior-writer saves
therefore have nothing left to verify and were removed. Tests build campaigns
and combat checkpoints with the current writer and check that they round-trip,
and check that an older format is rejected. Save files that Godot checks
byte-compare are written by the native tests into the build directory.
