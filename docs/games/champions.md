# Champions of Krynn

The fresh Original Rules campaign reaches the authored ending with all six
PCs alive: 123 kept battles, zero defeats. Integrated native, Godot and Linux
package checks are complete. The frozen acceptance and checkpoints are in
[PLAYABILITY.md](../PLAYABILITY.md#active-batch-champions-of-krynn).

## Original files and source facts

The installed data is `~/GoldBox/game_files/COK`; manuals/journal/cluebook are
under `~/GoldBox/docs/COK`, and the reconstructed source is
`~/GoldBox/original_src/champions-of-krynn`
([GAMES.md](../GAMES.md#where-the-developers-copies-live)). Primary reconstruction:
[Champions of Krynn](https://github.com/talonius/champions-of-krynn). It has no
license; facts inform OpenGoldBox's own implementation, with no copied code.

- `KRYNN.CFG` identifies the game. Stable enum value 7 is appended; values
  0..6 and their released saves keep their meanings.
- Three ECL archives contain 22 scripts; three GEO archives contain 15 maps.
  `GameLoop` starts script 36, with the party at (7,13) facing north. The
  script selects its actual initial map.
- Script zones start at 4B00h, 7C00h and 7A00h, as in Curse/Secret/Gateway.
  `EclAwardExperience` (41h) reads a whole word, without the VGA games' x100.
- Monsters are 409 bytes, items 63 and effects 9. The record's race, class,
  knight order, deity and robe are separate fields. Spell slots have a fourth
  deity-gift row. NPC/raw/import/save metadata must retain these distinctions.
- Races are Silvanesti/Qualinesti elves, half-elves, mountain/hill dwarves,
  kender and humans; class slot 7 is a Solamnic knight, with Crown/Sword/Rose.
  Existing Original rules slots must keep their old meanings in other games.
- `PIC1.DAX` repeats ID 34 with identical bytes; `SPRIT2.DAX` repeats ID 14
  with different payloads. `DaxIo.LoadDax` chooses the first matching header.
  Archive reading must preserve that behavior and validate the actual payloads
  rather than changing the original files.

The initial existing-reader baseline misidentified COK as Curse. The new reader
passes the complete installed-data census; this is still not a playability or
winning certificate. Evidence: `/tmp/opengold-cok-census-v2.log` and
`/tmp/opengold-cok-audio-checks-v3.log` (Champions and existing late-sound checks,
2/2 in 0.43 seconds), on base `2f20d568` plus the active changes.

### Audio facts

`SOUND.DAX` record 1 contains the title score. Its 125-byte header maps nine
AdLib voices to MIDI channels. Events use running status, program changes and
note on/off messages, each followed by a single-byte tick delay. FC 80 loops;
FC 81 ends. The timer divisor is 21845, about 54.6 ticks/second; the installed
score lasts 7860 ticks, about 143.90 seconds. Native playback keeps all nine
voices through the existing FM renderer. `MIDI.DAX` records 1..5 are hardware
drivers, recognized as programs and never executed; record 3's twenty valid
instrument programs supply the AdLib instruments. START.EXE also retains the
earlier PC-speaker sound driver, with 21 sound-table entries. Item names have
123 parts, and CURSOR.DAX is an ordinary 8-by-8 EGA image.

### Creation and service facts under implementation

The original's `raceClass`, `raceLimits`, `startAge`, `classProgression` and
`classAlignMask` are game-specific. No evil alignment is offered. Dwarven
clerics serve Reorx. The other clerics choose one of seven gods; the last
three gods also permit Neutral Good, in addition to the neutral alignments.
The robe follows a mage's alignment. `ClassSuborderIndex` selects separate
good/neutral cleric, Crown/Sword/Rose knight and White/Red mage progressions.
Champions' `TrainingMenu` takes no money. Its coin counts remain in original
order (silver, copper, bronze, platinum, steel), with values 1,5,10,25,50;
prices are in steel. Trading bronze for platinum must use exact multiples,
since the adjacent denominations do not have an integral exchange ratio.

The original racial-adjustment loop used its completed loop counter and
applied no adjustments. As in the existing Original Rules creation, this
obvious bug is corrected: elves +1 Dexterity/-1 Constitution; dwarves +1
Constitution/-1 Charisma; half-elves +2 Dexterity; kender +2 Dexterity/-1
Strength. The original starting-money loop likewise used its completed class
counter; its resulting unaveraged dice total plus 20% is retained here.

### Script and combat protocol checks

Champions' SPELL opcode locates a prepared copy and its caster without casting
it. Throtl asks for Find Traps (22), offers a choice, and clears the selected
copy through its 7C20h spell window only after CAST SPELL. Repeated queries,
declining, duplicate copies, selected-caster spending and the no-match sentinel
are covered by synthetic ECL checks independent of the earned campaign.
Evidence: `/tmp/opengold-cok-spell-query-checks-v3.log` (Champions/ECL, 2/2).

Throtl and its temple share GEO32 but use scripts 32 and 33. The temple's actual
altar is event 10 at (12,7); defeating it sets 4C61h to FFh. Catacomb script 34
continues six corridors across the map edges, changing position before the
ordinary collision check; these authored transfers are followed with normal
movement commands in the earned route.

Currency presentation follows the original silver/copper/bronze/platinum/steel
slots, with prices and the party's purchasing power shown in steel. The existing
localization generator retains the earlier catalogs and supplies the new
English and Spanish labels, including deity choices and knight petitions.

Champions' own combat powers use separate canonical effects. Auraks have three
blinding breaths plus energy bolts; Maya has one cold cone. The death knight's
single 15d6 fireball, occasional innate dispel and one-use gate do not require a
spellbook. Native REMOVED ghasts wait off the battlefield for the gate, without
resurrecting slain creatures. These states retain the existing combat-save
representation. Quick cannot re-equip a weapon trapped in a Baaz's stone body.

Native weapon powers distinguish dragons, reptiles and unliving creatures using
Champions' record flags. Dragon Slayer adds two to hit and triples one d12 before
adding four and Strength, matching the native distribution. A dragonlance deals
its wielder's current HP against dragons. Disruption removes undead with a
positive turning level and doubles damage to other unliving targets. Dedicated
synthetic checks cover damage, native-item grants, reflection, acid-pool entry
and expiry, waiting ghasts, and byte-identical combat-save roundtrips. Synthetic
combat fixtures are separate from the earned campaign proof.

## Earned ending evidence

The integrated fresh replay starts with three human Knights, two Mishakal
clerics and one White mage, using ordinary creation/Modify, native starting
money and equipment, paid shops, movement, preparation, rests and training.
Backup melee weapons are bought at the opening armoury, as the cluebook
recommends for Baaz. Legal targeted Sleep protects the early party; later
preparation and wards use actual memorized copies. No XP, money, healing,
quest flags, victories or arbitrary positions are granted by the route.

The route rescues Caramon, clears Throtl's altar and catacombs, follows the
Sanction temples, defeats Sir Lebaum, enters and crashes the flying citadel,
enters Kernen and Myrtani's stronghold, defeats every Aurak form of Myrtani
and the three red dragons, and observes the native honors/Soth ending and
4C3Ah winning flag. Myrtani must be dead, including the final explosion.
Both authored citadel camp offers remain usable until the player exits camp.

Tested source: base `2879fa06` plus this batch's changes, including the user's
Treasures starting-kit fix and the corrected Pools/Champions map marker.
`opengold_champions_earned_ending` passed in **475.39 seconds**, with **123
kept battles, zero defeats and all six alive**. Evidence:
`/tmp/opengold-cok-fresh-ending-integrated-v1.log`. A prior independent fresh
run also passed (123/0, 483.38 seconds); checkpoint continuations are not used
as the fresh certificate.

The ending party consists of three level-6 Knights (177315 XP each), two
level-7 clerics (165601 XP each) and a level-7 White mage (169095 XP).
Tasslehoff is narrative assistance in this ending, not a recruited roster
member. After observing victory, the harness presses Continue to reach a
normal savable boundary. The ordinary format-53 winning save decodes and
re-encodes byte for byte. A standalone `.ogs` with the actual installed asset
identity passes the same check; its six characters retain their actual HP,
XP, inventory and campaign continuation. Stable game IDs and all prior save
versions remain supported.

Since 2026-11-24 the earned ending banks its party at the win as the game does
(`OPENGOLD_WINNERS_SAVE`, [SAVES.md](../SAVES.md#the-winning-party)), in
`champions-earned-winners.ogs`, which CTest hands to Death Knights' earned
opening and ending ([death-knights.md](death-knights.md)).

## Final verification

The full Linux build/export is recorded in
`/tmp/opengold-cok-linux-integrated-build-v1.log`. The installed-data regression
selection ran 339 checks in 2174.53 seconds: 337 passed, including Champions'
complete census, seven commercial map walks, previous route checks, Godot creation,
services, save/restart flows and Linux package smoke. Two older expectations
still rejected duplicate archive IDs and omitted Champions from the startup
list. Both were corrected; the rebuilt creature check and startup check passed
by name, with Godot preparation, **3/3 in 13.39 seconds**. There were no
gameplay changes after the integrated build or earned ending replay.
Logs: `/tmp/opengold-cok-linux-integrated-checks-v1.log` and
`/tmp/opengold-cok-linux-integrated-focused-final.log`.

The final every-map walk completed 4700 event executions in 79.56 seconds.
Its synthetic-party fights (122 victories, 845 defeats) are runtime coverage,
not an earned-win certificate. The independent fresh campaign above supplies
that certificate. Earlier games' long earned subroutes were excluded from the
routine regression selection; their prior delivered proofs were not replaced.
Localization checks cover all 4221 complete English/Spanish messages, matching
placeholders and BBCode. SRD mechanics are unchanged. Original installations
were only read; no save version or supported history was removed.

The commit hook subsequently required layout and brace cleanup, plus equivalent
explicit aggregate initialization in four synthetic fixtures for clang-tidy.
The final Linux rebuild/export passed, followed by **13/13 focused checks in
23.67 seconds**, covering Original creation/combat, Champions, Darkness,
archives, ECL, saves, Godot creation/startup and Linux package smoke. The actual
ending was replayed from an earned pre-finale checkpoint: two victories, zero
defeats, all six alive and a compatible save. That continuation checks the final
binary and does not replace the fresh 123/0 certificate. Evidence:
`/tmp/opengold-cok-linux-delivery-build-v1.log`,
`/tmp/opengold-cok-poststyle-focused.log` and
`/tmp/opengold-cok-poststyle-ending.log`.
