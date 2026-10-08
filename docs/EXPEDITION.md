# First Slums expedition

The shared campaign can leave New Phlan through the western gate, enter the
original Slums map, resolve supported roaming encounters and the four-orc paper
encounter, and return through the same gate. Script-requested combat opens
automatically. Victory resumes the waiting script and exploration; defeat opens
the saved-game reload / Exit to OS screen.

The arena uses the verified 50 by 25 dungeon geometry and original DUNGCOM tiles
described in [combat-geometry.md](combat-geometry.md). Party and enemy placement
is an authored formation inside a shared reachable component of that geometry.
Placement never removes obstacles or omits requested enemies. It is not a claim
to reproduce the original game's formation algorithm.

## Encounter conversion

MON2CHA identities, counts and requested CPIC2 art come from the original script.
The following bounded SRD-compatible profiles are authored conversions, not
complete official monster stat blocks or translations of AD&D statistics:

| Original record | Profile | XP per creature |
| --- | --- | --- |
| 0 | Kobold | 25 |
| 1, 11 | Kobold leader | 50 |
| 2 | Goblin guard | 50 |
| 3, 12 | Goblin leader | 100 |
| 4, 13 | Orc | 75 |
| 5, 14, 15 | Orc leader | 150 |
| 63 | Bugbear | 200 |

The four-orc event retains its approved 300 XP reward and stable completion
identity. Existing campaign semantics award that amount to each living active
member. Roaming encounters use a distinct persisted reward identity each time.
Original groups can be large and dangerous to a new party; they are scaled back
to the party's XP budget as the [encounter size](SRD-DECISIONS.md) decision
describes, the four-orc search included.

**Decision (2026-09-30, issue #14): AD&D-equivalent encounter sizing.** Roaming
group sizes come from the original PARTY STRENGTH, which is fed what an
equivalent AD&D character would have (class THAC0 by level, worn armor AC,
current HP, spellcaster levels) instead of converted SRD bonuses; see
[party strength](PARTY.md). Six level-two fighters in chain and shield now meet
eight goblins and four leaders where the SRD-bonus conversion produced sixteen
and four. Group counts start from the original script and are then scaled back to the XP
budget.

When the script starts combat, the approached monster's animated close-up
fills the 3D view, with "Press any key to fight.", until any key or Continue is
pressed, then combat opens. See
[close-up animations](graphics-format.md#close-up-animations).

The pre-combat menu offers Fight, Wait, Flee and Advance with the script's
response table; once the monsters are adjacent, Parley replaces Advance. The
party's leader parleys ([LEADER-1](SRD-DECISIONS.md#leader-1-2026-10-08-a-party-leader-speaks-for-the-party)),
and a meeting that ends without a fight says so. A successful Flee lets the
script move the party, as the original's wild flight does. A parley that goes well
enough reaches the Slums script's other outcomes, each tested: a toll (Pay
enough and the monsters take half the party's coins, too little and they
attack; Surrender costs every coin and moves the party; Run escapes when the
party is as fast as the monsters) or an order to leave (Leave ends the meeting,
Stay or Fight starts one). The best outcome, the leader's advice, needs a
reaction total over 95: Charisma 19 or more with an Abusive parley. Pre-combat escape is separate from retreat during combat, which
remains deferred. Supported modern profiles have no original party surprise
modifiers. Surprise rolls share the checkpointed ECL random stream; the resulting
surprise uses initiative disadvantage in the selected SRD module. Original
movement thresholds use the explicit conversion 30 modern feet = 12 original
movement units for the menu's escape check.

Coin denominations and carried items are taken from the defeated original
records. The four-orc group carries 96 silver and one magic item from record 13.
The item's original bytes and provenance are retained; no unsupported magic
effect is invented. Script suppression of monster item drops is honored. Full
purses defer collection without losing loot; deferred rewards survive saving.

This does not make every Slums quest, creature, robbery or parley outcome
supported. Unsupported paths retain explicit diagnostics and event rollback.
Combat is prepared before switching screens. If the rules, encounter, or art
cannot initialize, exploration keeps its view and rolls the event back with a
diagnostic. Acknowledge the message to continue or return to the party. The current
combat rules accept all twelve created classes with basic combat actions.
Additional class abilities are available only for Fighter, Cleric, and Wizard.
A rejected training fight keeps the party screen
open so the party can be edited and retried.

Leaving the Slums through a west doorway enters Kuto's Well (`ECL8:29`, maps
`GEO8:29` and `GEO8:32`). Its fights, camping, the catacombs' arrow volleys,
Norris the Gray's band (fought or surrendered to), his treasure and the hideout
are supported; finds that seem to need the original Search mode are not, see the
[Kuto's Well audit](audits/kutos-well.md).

`tests/playtest_kutos_well.gd` play-tests the area in the game window. The
expedition test writes its two campaign fixtures when asked: beside the well
(`OPENGOLD_KUTO_WELL_FIXTURE`) and beside Norris's hall
(`OPENGOLD_KUTO_NORRIS_FIXTURE`). The script loads each through Load game,
climbs down into the arrow volley, meets Norris and starts his fight, and
saves a screenshot at each step with a dialogue report. It is not a CTest
check; run it with a window and a scratch `HOME`:

```sh
OPENGOLD_KUTO_WELL_FIXTURE=/tmp/kuto/well.ogs OPENGOLD_KUTO_NORRIS_FIXTURE=/tmp/kuto/norris.ogs \
OPENGOLD_GAME_DIR=/path/to/POOLRAD build/opengold_expedition_tests
HOME=/tmp/kuto/home OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --path src/OpenGoldBox/godot \
    --script tests/playtest_kutos_well.gd -- --kuto-well=/tmp/kuto/well.ogs \
    --kuto-norris=/tmp/kuto/norris.ogs --playtest-out=/tmp/kuto/out
```

`tests/playtest_slums.gd` play-tests the Slums' set encounters the same way.
The expedition test writes its saves when `OPENGOLD_SLUMS_FIXTURES` names a
folder: beside the arguing hobgoblins (met coming south from (0, 1)), beside
the monster leaders and south of the trolls' room. The script walks into each
fight and plays every party turn like a person: it cycles actions with A,
clicks the enemies, otherwise walks toward the nearest one with the arrow keys,
and ends the turn. Its report lists each distinct prompt and error with a
screenshot of its first sight:

```sh
OPENGOLD_SLUMS_FIXTURES=/tmp/slums/fixtures OPENGOLD_GAME_DIR=/path/to/POOLRAD build/opengold_expedition_tests
HOME=/tmp/slums/home OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --path src/OpenGoldBox/godot \
    --script tests/playtest_slums.gd -- --slums-fixtures=/tmp/slums/fixtures \
    --playtest-out=/tmp/slums/out
```

Locked doors offer the original Bash, Pick (with a Rogue) and Exit, and Ohlo's potion delivery is
supported from commission to reward; see [QUESTS.md](QUESTS.md). The installed
test plays it end to end.

Camping in the Slums follows the original script's interruption profile:
plain streets are checked for wandering monsters, and special-event cells are
safe; see [camp interruptions](RECOVERY.md#original-campaign-mappings). The
installed test camps on a Slums street until monsters attack, wins the fight
and rests again, in both the original process and a fresh process after a
reload. Other district transitions are outside this first expedition. The
first-room victory and required route encounters are the acceptance target.

## Review and validation

The [normal level-one party audit](audits/issue13-first-adventure.md) records a
successful four-orc outing, earned advancement, return travel and fresh-process
reload at `0ab857d`. The inn's platinum price then blocked recovery; scripts
now [make change](PHLAN.md#coin-payments) from the payer's gold and silver.
The audit includes the party, tactics, resources and linked follow-up issues.

With `OPENGOLD_GAME_DIR` set, `opengold_expedition_tests` plays the whole loop
in one session: six created fighters buy and equip original arms, leave by the
west gate, flee roaming groups, defeat the four orcs (300 XP each, 96 silver),
return, decline the inn, are refused for a payer worth less than 1 pp, then pay
10 gp for 1 pp and take a long rest. It saves (replacing an earlier save, which
is kept as the backup), reloads in a fresh process, checks the rest timer, and
revisits the orcs without a second fight or reward. The fresh process's result
must equal the same continuation without a reload.

From PowerShell, build with `.\demos\build-rolf.cmd`. Run
`.\demos\review-character.cmd --expedition-check` for the visible automated trip and
`user-data/slums-battlefield.png` capture. The fixture is six equipped level-four
fighters with explicitly confirmed Defense; it does not demonstrate that six
new level-one characters can defeat
every original roaming group. Normal play uses `.\demos\review-character.cmd`.

Native tests cover original geometry, host dice, string-copy semantics, surprise
initiative, large-arena checkpoints, coin overflow and duplicate loot. The
installed-data route also checks the four-orc flag, original silver award and
return to New Phlan. Campaign saves retain district, visited maps, script state,
RNG and uncollected loot; version-one saves and the preceding additive rules
content pack remain accepted.

References: original installed ECL2/GEO2/MON2CHA/CPIC2/DUNGCOM resources;
[Pool of Radiance technical FAQ](https://gamefaqs.gamespot.com/pc/564785-pool-of-radiance/faqs/73869)
for the encounter host protocol;
[SRD combat rules](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.pdf)
for initiative disadvantage on surprise.
