# The Dark Queen of Krynn

Active implementation under the existing all-twelve-game request, following
Death Knights delivery `baf0bc01`. No complete Dark Queen player outcome is
accepted yet.

## Fixed delivery batch

The installed campaign must load and play with its Original Rules, import the
actual earned Death Knights winners, and reach its authored ending. Acceptance
includes complete installed-data census; native creation, classes, gods,
robes, knights, progression, spells and moons; combat, camps, services and
vault; every-map runtime walk; fresh earned campaign; ordinary byte-identical
winning-save roundtrip; and relevant native, Godot and Linux package checks.

Reuse the existing Godot 4.x/C++20/GDExtension architecture, library readers,
Original Rules and campaign orchestration. Append stable game ID 9 without
renumbering released games. Preserve released saves and keep mechanics in
the rules module. No new UI stack, framework, SRD implementation, model/runtime
change is part of this batch. Parallel work was explicitly authorized on
2026-11-20; the coordinator retains Native implementation ownership. No forced outcomes, artificial
rewards/healing, quest writes, arbitrary placements or original-data edits
are allowed in earned-campaign proof.

Both Buck Rogers games and UA retain separate acceptance records within the
all-twelve-game request. UA's winning-save correction is delivered in
`ed0be5e8`; Countdown and Matrix workers continue while Native verification
proceeds. The user explicitly requested parallel agents and approximately
80 percent CPU utilization on 2026-11-20. Coordinate useful builds and tests
across the sixteen logical cores, with roughly twelve workers available and
headroom for the desktop; monitor memory and actual load before new launches.
The universal library of every winning PC and permanent NPC follows
all twelve wins, with snapshots captured at victory before Continue/departures.

## Integrated control checkpoint (2026-11-20)

Native knights now use the original combat-start leadership rule. Living,
non-NPC knights attempt to control living NPC allies with current Intelligence
above three, using their order and current Charisma with the source's strict
roll comparison. Unled NPCs remain automatic; successful control uses the
existing manual and Quick controls. A restored combat keeps that result
without rerolling. The existing combat format 29 and profile 15 are unchanged.

The integrated Native control, Matrix paid-shop prefix, native Camp and save
checks pass **22/22**, with **12/12 application/fixture checks** and a Linux
package smoke pass. Logs are
`/tmp/opengold-integrated-led-caloris-focused-checks-v1.log`,
`/tmp/opengold-integrated-led-caloris-godot-checks-v1.log` and
`/tmp/opengold-integrated-led-caloris-package-smoke-v1.log`.
These checks do not establish a complete earned Dark Queen ending; the Font
and subsequent campaign ending remain unverified.

## Baseline and references

Original files: `/home/edmond/GoldBox/DQK`; manuals, cluebook and journal:
`/home/edmond/GoldBox/docs/DQK`. The installation has three disk directories,
GLB/TLB resources, `DQK.EXE`, `ITEMS.DAT`, `ITEM.DAT` and `VAULTA.DAT`.

Primary reconstruction:
[The Dark Queen of Krynn](https://github.com/talonius/dark-queen-of-krynn),
reference revision `8a25186357a8f4fb70732c506459b6f154cc96c6`, read locally at
`/tmp/opengold-dark-queen-source`. It supplies format and behavior facts;
its implementation and original assets are not copied or distributed.

The actual source party is
`/tmp/opengold-deliveries/baf0bc01/opengold-dkk-winning.ogs`: six surviving,
active imported PCs, with their earned experience, wounds, spells and items.
Durfey remains an inactive roster entry after his native departure and is
not a winner to import as an active companion.

Start observed: **2026-11-19 18:28:11 UTC**. The mandatory sixty-minute
checkpoint is due **19:28:11**, no later than **19:58:11**. One implementation
owner continues with the same configured model/effort; exact identifiers are
not exposed. The model-routing policy applies to SRD, which is excluded.
The main checkout retains its user's existing `docs/GAMES.md` edit and is
left untouched under the dirty-checkout exception.

The pre-implementation census on `baf0bc01` reports **no Gold Box game found**
and exits 2 (`/tmp/opengold-dqk-census-before.log`). The installation's
`DQK.EXE` is 449,664 bytes, SHA-256
`6b0a9ac5c873997087f1f3f626334a71092fe7c8d1e92ce7f72869a59931f837`.
Its hash differs from the reconstruction's stated reference, so executable
offsets and behavior require validation against the installed data.

## Initial verified format facts

The unpacked HLIB indexes name **47 ECL scripts, 20 GEO areas and 98 MONCHA
creatures**. Existing Unlimited Adventures container readers apply. The native
character is 414 bytes, followed by effect/item counts, five-byte effects and
17-byte carried items; each creature is PackBits-packed behind a big-endian
expanded length. Its spells include 137 memorization bytes, 128 known-spell
bits and four rows of nine slots. Native level slot one is a knight, replacing
the unused druid class slot; the common profile retains knights in slot seven.

GEO has an eight-byte header and four square planes. Maps are rectangular and
can reach 39 squares along one dimension, with at most 784 squares in total.
The first reader phase retains the whole area separately; connecting these
dimensions to campaign movement/rendering remains required. `ITEM.DAT` has
254 records of 17 bytes, while `ITEMS.DAT` has 128 sixteen-byte type records.
These facts are reading progress, not an earned campaign or a playability claim.

## Reading checkpoint

The complete installed-data census passes on `baf0bc01` plus the active
changes (`/tmp/opengold-dqk-census-v3.log`). All 47 scripts are walked through
their reachable instructions, all 20 rectangular areas and 98 creatures decode,
and the item definitions, nested art libraries, font, speaker/VOC sounds and XMI
music decode. Census row counts include each library's index.

GLB scripts start directly at address `8000h`, without a DAX record's two-byte
prefix. The shared ECL memory descriptor records that difference; existing
games keep their two-byte prefix. An isolated headerless STORE/EXIT check
verifies that self-modification reaches the correct final byte and remains
private to its machine. Existing UA and ECL regressions also pass:
**3/3 in 0.10 seconds** (`/tmp/opengold-dqk-reader-checks-v3.log`).

Native creature loading honors the effect/item counts and retains ignored
trailing bytes. Most are alignment padding; Sahuagin Lord has eighteen unused
bytes. They remain provenance and do not become equipment. Truncated payloads
and counts exceeding the expanded record remain rejected. Native brace checks
pass (`/tmp/opengold-dqk-reader-braces-v2.log`). Character rules, campaign
geometry/runtime, actual winning-party import, earned ending and final game
checks remain; zero complete player outcomes are accepted.

## Sixty-minute checkpoint

Observed **2026-11-19 19:28 UTC**, about sixty minutes after the recorded
start. Installed-data readers and the rectangular-map foundation are complete;
**zero complete Dark Queen player outcomes** are accepted. The focused geometry,
Death Knights, tour, UA session and released-save checks pass **5/5 in 2.28
seconds** (`/tmp/opengold-dqk-geometry-checks-v2.log`). This includes decoding
and re-encoding the actual released Death Knights winning save byte for byte.
Games 0–8 retain their 256-bit map-knowledge fields; appended game 9 uses the
784-square capacity without reducing released-save support.

The installed executable exactly matches thirteen complete numeric rule tables
from the reconstruction: experience, race limits and classes, starting ages,
class minima and alignments, spell-table selectors, spell slots, saving throws,
spell lists, racial caps and attack scores. The complete 141-row spell facts
also match, with 137 named/native active rows and four empty tails. Verification
records are `/tmp/opengold-dqk-rule-table-verification.json` and the local fact
extraction scripts. No reconstructed C procedures or original assets are copied.
The new rules implementation is not yet built or accepted. Campaign integration,
actual import, earned ending, Godot and package checks remain in this fixed batch.

Phase observations: the reading checkpoint passed before geometry; geometry
verification ended at approximately 19:23 UTC. More precise earlier phase
boundaries were not recorded and are not estimated retroactively. There is no
new scope, parallel work, model change or completion claim at this checkpoint.

## Character-rule checkpoint

The initial native rule/creation checks pass **4/4 in 1.68 seconds**, including
Champions, Death Knights and released saves
(`/tmp/opengold-dqk-rules-checks-v3.log`). All **66 native race/class choices**
create live one-square profiles with the game's exact starting XP shares and
200 steel. Tests cover the half-elf knight cap, Rose advancement cap, progression
through forty, robe-dependent XP, native full spell-slot rows, high Intelligence
and Wisdom requirements, ninth-level spells, separate deity gifts, high-level
knight HP limits and fixed gains, and distinct ranger caster levels.

The installed native character structure and the script's virtual character
record are different layouts. Campaign memory integration must preserve that
virtual interface; successfully decoding native records alone does not do it.
The game's script memory is `0001h..0400h` (world), `0401h..0800h` (virtual
character), and `8000h..AEE2h` (script), confirmed in `gb_zone`. Its fresh game
loads ECL 4 and begins at (7,13), unlike the earlier Krynn opening ECL 16.
These facts are required for the next integration phase; no earned victory or
complete playability is claimed by this character checkpoint.

## Native asset checkpoint

The native asset and regression checks pass **6/6 in 2.79 seconds**
(`/tmp/opengold-dqk-assets-checks-v3.log`). This covers all 98 creatures with
counted equipment in the shared catalog and rules content, 128 item types,
all **155** item words (including the adjacent Krynn extensions), and every
wall set in both installed wall libraries. The map's even flag bits are
passages; its odd bits also choose backdrops and must not become lock codes.

Library decorations can extend beyond a plain wall's nominal rectangle.
The shared wall renderer now retains each piece's anchor offset and full
composited extent. A separate pixel test checks that both the base wall and
a decoration extending left of it appear at their intended coordinates.
Existing games retain zero offsets. Focused brace checks pass for the changed
composer, renderer and tests (`/tmp/opengold-dqk-assets-braces-v3.log`).

These are native resource and shared-renderer checks. Campaign integration,
faithful import, actual ending, full runtime map walks and Godot/package
validation remain required before any Dark Queen playability claim.

## Opening and actual-party import checkpoint

The actual opening ECL 4 reaches its authored Palanthas destination menu
and renders the native 88-square-pixel view without script diagnostics.
The installed loader reads all 47 scripts, 20 maps, native pictures,
sprites and backdrops. PIC animation frames combine the base glyph with
each transparent overlay at its own origin. The native knight promotion
check also verifies two required level losses, each costing two rolled HP.

The actual six active Death Knights winners import from delivery `baf0bc01`
with their exact earned ability scores, levels, XP, age, rolled/max/current
HP, surviving items and purse. The inactive Durfey is excluded. Native
item types, name words, spell bytes and equipment effects are converted;
ammunition is omitted under the existing unlimited-ammunition policy.
Native spells are remapped while constructing the destination character
and its remaining-spell state. No fresh-character money is added. The
ordinary native party-screen save decodes and re-encodes byte for byte
under its actual installed asset identity. The focused Dark Queen, Death
Knights and released-save checks pass **3/3 in 1.60 seconds**
(`/tmp/opengold-dqk-import-checks-v3.log`).

This verifies the opening and imported party/save, not combat or a campaign
win. Battlefields, native effect behavior, services, runtime map walks,
the earned ending and Godot/package verification remain in the same batch.

## Equipment, staging and authored-opening checkpoint

The native item loader assigns the file's 254 records to IDs **1–254**, as
`item_data_read` does. Its first record is arrows; ID 80 is the +2 arrows
and ID 91 the +2 quarrels. Both are omitted from starting stock under the
existing ammunition policy. ID 86 is forty physical thrown darts, retained
with that quantity. All **66** valid race/class choices receive their actual
starting equipment. The 595-word starting-item table, the complete 140-byte
scroll-kind table and the three shop tables match the installed executable;
shop inventories and random scrolls use those native facts.

Native bows fire without ammunition inventory; hoopaks remain in hand
when fired, while other thrown weapons retain their physical quantities.
Actual imported remaining spell copies match the converted Death Knights
combat profile without replenishing spent copies. The six focused suites
pass **6/6 in 3.41 seconds** (`/tmp/opengold-dqk-ammunition-checks-v1.log`).

An isolated staging fixture uses an authored opposing monster/icon pair.
It verifies the native dungeon's complete battlefield, tiles and two combat
poses, and all **32** native outdoor terrain styles with rivers. The terrain
flag table and complete native combat-tile table match the executable; all
38 ordinary outdoor movement costs match the shared library battlefield
implementation. Native LOAD FILES preserves its combat set when passed 255;
that set is included in native-only save/rollback state. Earlier games'
save bytes remain unchanged. The focused Dark Queen, Death Knights, UA
session, tour and save suites pass **5/5 in 3.01 seconds**
(`/tmp/opengold-dqk-wilderness-checks-v1.log`). This is staging validation,
not an earned battle or campaign victory.

The actual six imported winners meet Laurana through PALACE and leave
Palanthas through LEAVE, reaching Caergoth without diagnostics
(`/tmp/opengold-dqk-caergoth-checks-v2.log`). The installed creatures carry
**103** distinct native effects; native effect behavior, services, full map
walks, the earned ending and Godot/package checks still remain.

## Native ability and ending-command checkpoint

Observed **2026-11-19 22:06 UTC**. Focused Dark Queen, Original combat,
Death Knights, ECL and released-save checks pass **5/5 in 2.57 seconds**
(`/tmp/opengold-dqk-ending-checks-v1.log`). New combat checks cover native
cold vulnerability, fire resistance and its rounding, ordinary adjacent weapon
immunity, exactly eight HP of lightning growth, venom, enchanted paralysis,
anemone poison/acid, fireshadow and Eye of the Deep rays, and exactly three
sea-dragon steam breaths. The gaze defense adds twenty to its save and still
fails on a natural one; it is not replaced with immunity. The fire-minion
handler is registered but absent from native trigger groups and remains dormant.

PROGRAM 1 now reaches the campaign victory host. An explicitly isolated command
fixture checks the victory flag and unchanged winning vitals. It does **not**
prove an earned win, and the native closing animation remains outstanding.
Mummy rot, remaining monster abilities, full runtime walks, actual battles and
the earned ending are still active work. No complete Dark Queen player outcome
has been accepted or delivered.

## Native runtime and route checkpoint

Observed 2026-11-19 22:55 UTC. Underwater context, native ability outcomes,
time effects, creation, Death Knights and save regressions pass 5/5 in 2.54
seconds (/tmp/opengold-dqk-underwater-checks-v3.log). This includes one-third
movement and its native exemption, restricted fire spells, underwater
Lightning Bolt and Ice Storm, anti-magic routing, and byte-identical native
combat checkpoint 29 with its starting calendar. Released game checkpoints
27/28 remain supported. Green-fire road ticks preserve HP, profile bytes and
RNG across one 31-minute update versus 310 six-second updates.

All 98 installed creatures construct actual Original Rules combat sessions
with their equipment and effects (/tmp/opengold-dqk-creature-audit-v1.log).
That audit is construction evidence, not 98 earned encounter victories.

The existing campaign route runner now imports the six actual Death Knights
winners and reaches Caergoth through the normal opening. Its earned walking
uses actual rectangular map dimensions. The next route attempt restores the
party with actual rest and healing spells, then walks to the first authored
encounter. Its unsupported-effect guard exposed native equipment effect 200
before combat could begin. The active correction separates native C7/C8
type/immunity flags from the earlier Krynn layout and connects dragonlance,
disruption and related weapon-quarry behavior. This correction is not yet
accepted by the next rebuilt run.

Player portraits and customizable icons already use the application's shared
Pool of Radiance CharacterArt through settings::game_path, as other campaigns
do. Native GEN.TLB supplies the original frame, not player portraits; no new
portrait decoder is required for that frame. Native finale animation,
campaign services, full-map runtime verification, the earned ending and
Godot/Linux delivery checks remain. No complete Dark Queen player outcome
has been accepted.


## Earned first battle and cave arrival

Observed 2026-11-19 23:19 UTC. The actual Death Knights winners complete
Caergoth's authored dragon ambush, receive its experience, recover with
ordinary rest and healing spells, recruit Captain Daenor and reach GEO/ECL
10 at (11,27). No test health, experience, items, quest writes or placements
are used. The campaign saves and reloads byte for byte. This is a cave-entry
checkpoint, not the Dark Queen ending. Evidence:
`/tmp/opengold-dqk-earned-caves-v3.log`.

The battle exposed two corrected integration errors: loot now uses the
native global creature table, and COMBAT accepts native win/flight flags.
Recruitment uses MONCHA.GLB; FIND NPC supplies text and an output address.
Native new-script setup clears its fifty map locals and rest counters while
retaining persistent campaign state. Focused Dark Queen, interpreter,
Death Knights and save checks pass 4/4 in 1.58 seconds:
`/tmp/opengold-dqk-npc-contract-checks-v2.log`. Earlier battle/creation/combat
regressions passed 6/6 in 2.38 seconds:
`/tmp/opengold-dqk-result-checks-v1.log`.


## Cave completion and underwater arrival

Observed 2026-11-19 23:36 UTC. A fresh actual-winner import clears the
Draconian Caves, sees Crysia escape, leaves on the Silver Shark and reaches
Naulidis (GEO/ECL 14, 11,17). Eight encounters are won with zero party
defeats. All six PCs are alive at full HP after normal spell recovery;
Captain Daenor leaves through the authored ship sequence. The ordinary
campaign roundtrips byte for byte. No campaign ending is claimed.
Evidence: `/tmp/opengold-dqk-earned-sea-elves-v5.log`.

The route walks back to the cave entrance between major encounters for the
cluebook's safe rests. Recovery uses actual memorized healing and revival
spells; the runner now wakes unconscious healers first. Native dungeon
fields are selected directly, without constructing the legacy 16-square
field first. Their wall-free passages remain open even when a passage bit
is zero. Staging checks exercise native positions beyond row and column
15. Native PROGRAM 9 resumes the script after its normal rest unless the
separate wilderness-view flag at 24h is exactly one; this preserves the
voyage after the offered ship rest. Focused native, Death Knights and save
checks pass 3/3 in 1.75 seconds:
`/tmp/opengold-dqk-scripted-camp-checks-v1.log`.

The next route checkpoint reaches Lyzian's introduction and takes/readies
the actual Ring of Free Action on knight 1. The initial assertion used
123h=2, which belongs to Daenor's later rescue, rather than the introduction's
9Eh bit 1. That verification assertion is being corrected to the authored
introduction flag; the playthrough itself reports no runtime diagnostic.

## Native rewards and Daenor checkpoint

Observed **2026-11-20 00:37 UTC**. A fresh route from the actual Death Knights
winners completes **42 beats, 13 won battles and zero party defeats**, rescues
Daenor from Lyzian's bedroom, returns him to a safe hall and receives the
Crylokon and dragon-egg assignment (`123h = 3`). All six PCs are alive and at
full HP at the checkpoint. This is not the campaign ending. Evidence:
`/tmp/opengold-dqk-earned-captain-v1.log` and its ordinary campaign checkpoint
`/tmp/opengold-dqk-earned-captain.sav`.

The route searches actual guardrooms for Irenia and escorts the children into
safety before expecting another aid assignment. Normal safe-hall rests do not
invoke an after-camp encounter: the native camp invokes that script entry only
when interrupted. No rest-trigger workaround or quest-state write is used.

Native scripted XP awards are hundreds, independently of the combat no-XP
flag. Combat eligibility excludes unconscious and petrified members before
post-combat recovery; steel treasure contributes two XP per coin. Native
battle hit/movement modifiers, no-item/no-XP flags, nonlethal recovery and
post-combat flag cleanup are connected. The focused Original combat, Death
Knights, Dark Queen, training and released-save checks pass **5/5 in 2.55
seconds** (`/tmp/opengold-dqk-reward-context-checks-v4.log`). The isolated reward
fixtures do not count as earned battles. Earlier partial routes preceded these
reward corrections; the fresh 42-beat run supersedes their reward evidence.

Vault implementation and its new native-only save extension are undergoing
verification. Celanost, the remaining campaign, native closing presentation,
every-map runtime walks and final Godot/Linux verification remain required.

## Vault, closing display and Celanost checkpoint

The native vault, closing display, Death Knights, training and save checks pass
**5/5 in 2.09 seconds** (`/tmp/opengold-dqk-vault-ending-checks-v4.log`). Vault
money and full equipment survive a byte-identical roundtrip; withdrawals
restore the actual property. A later unsupported instruction rolls back both
the party and the vault, preventing duplication. No original vault file is
written. Only Dark Queen writes format 54; released games keep their existing
format-53 output and supported history. Earlier native checkpoints load with
an empty vault; writing a nonempty vault into format 53 is refused.

All three `FINAL.TLB` records compose into seventy timed closing screens in
native order 1, 3, 2. Representative frames were visually inspected in
`/tmp/opengold-dqk-film-review.png`. This checks the display and does not mean
the campaign has been won. The existing startup menu and earlier-game dialog
now include Dark Queen and its Death Knights import; final Godot build and
flow checks remain pending.

The authentic Celanost route reaches the air chamber using the Crylokon at the
well, clears the fiery halls and mage, rests in cleared rooms, and recovers
all twelve eggs with six living PCs at full HP. Evidence is the resumed
`/tmp/opengold-dqk-earned-eggs-v2.log` and checkpoint
`/tmp/opengold-dqk-earned-eggs.sav` (52 beats). Prompt-specific route answers
prevent an incidental turn or damaged door from spending a breathing-shell
answer. This is a quest checkpoint, not the complete fresh winning proof.

## Taladas and Luminari route checkpoint

The resumed route returns all twelve eggs, survives Talhook's palace ambush,
attends Lyzian's ceremony, meets Ezra, trains all six PCs through Hizden's
militia, repels the town raid, and departs normally. The original script's
training exit returns the party outside the hall; the route respects that
transition. Evidence: `/tmp/opengold-dqk-earned-taladas-v3.log` and
`/tmp/opengold-dqk-earned-hizden-v2.log`, with trusted partial saves through
beat 63. No victory is claimed.

Native wilderness movement now respects a script's blocked step and its own
transfer between northern and southern views. The renderer selects picture
240 or 241 from the script state and uses the actual 8-by-8 party marker from
`FRAME.TLB`. Focused native, Death Knights and save checks pass **3/3 in 1.93
seconds** (`/tmp/opengold-dqk-wilderness-checks-v1.log`). Installed travel from
Hizden to Luminari passes (`/tmp/opengold-dqk-earned-luminari-v2.log`, beat 65).

The party then survives the authored pit fall and its deep-level battle,
heals using its own spells, and climbs into the first deep with all six PCs
alive at full HP. The accepted checkpoint is beat 67, at (5,1) in GEO/ECL 22:
`/tmp/opengold-dqk-earned-luminari-deep-v1.log` records the battle and climb;
its extra, erroneous attempt to revisit the mound was removed, and
`/tmp/opengold-dqk-earned-luminari-deep-v2.log` verifies the corrected endpoint.
These resumed checks do not substitute for the required complete fresh
campaign proof. Remaining campaign progression, the scripted drain service,
every-map runtime walks, and final Godot/Linux checks remain unfinished.

## Native services and lighthouse checkpoint

The native, Death Knights and released-save checks pass **3/3 in 1.47 seconds**
(`/tmp/opengold-dqk-native-drain-checks-v5.log`). This includes Native PROGRAM 3:
the VM dispatches the request, Original rules choose the drained class and
rolled HP/XP losses, and Core commits ordinary character history and RNG
atomically. Tests cover preserved wounds, midpoint XP, class tie selection,
last-level death and byte-identical save replay. Earlier-game behavior remains
covered by the neighboring checks.

Native heading writes normalize modulo four immediately, matching the
original virtual-memory setter. Luminari's teleporters intentionally use
out-of-range intermediate headings; rejecting those writes previously rolled
back a legitimate step. A focused script verifies that the next instruction
reads both wrapped negative and positive headings correctly.

The actual party waits through the lighthouse's authored earthquake and
teleporter shutdown, rests in the cleared first deep, enters the portal,
meets Fastillion and receives both keys and 50,000 XP per PC. The audience is
recorded at beat 71 in `/tmp/opengold-dqk-earned-kristophan-v1.log`; its saved
checkpoint is `/tmp/opengold-dqk-kristophan-stage-71.sav`. All six PCs remain
alive at full HP. The same run exposes a departure failure after beat 74:
the native engine must run pre-move once between script entry and arrival
search. That missing entry call is being corrected and tested against the
installed lighthouse/wilderness scripts. The campaign has not been won.

## Kristophan and Tombs checkpoint

The lighthouse departure correction passes the installed-data departure test:
script initialization runs main, pre-move and arrival in order. The first
ordinary wilderness step leaves Luminari. Native, released-save and Death
Knights checks pass **3/3 in 1.47 seconds**
(`/tmp/opengold-dqk-native-entry-checks-v1.log`). The earned party reaches
Kristophan at beat 76, trains at its guild, recruits the actual Selias record,
survives the dragon raid and enters the Tombs at beat 80. All seven members
are alive at full HP. The accepted entrance checkpoint is
`/tmp/opengold-dqk-earned-tombs.sav`; it has not yet run Tombs quest flags.

Native DIVIDE also writes its remainder to cell `29h`. Omitting that write
made the actual Tombs bit-flag helpers confuse adjacent flags: the vampire
warning could mark its battle complete, and the gold door could mark the
king's tomb searched. A regression invokes the installed ECL43 flag-setting
and querying subroutines with their unchanged mask table, proving that flags
`28h` and `29h` remain independent. Native, ECL, Death Knights and released-save
checks pass **4/4 in 1.52 seconds**
(`/tmp/opengold-dqk-remainder-checks-v2.log`). Earlier Tombs suffix saves made
before this correction are rejected as evidence; the clean entrance is the
replay boundary.

After the correction the actual party opens the gold-key door, follows the
upper teleporter, wins the required coffin encounter and searches the king's
tomb for the first Oracle piece. All seven survive, with several levels
drained. This is partial campaign progress, not a victory. The current
clerics cannot yet prepare seventh-level Restoration, and the mage does not
know Haste; the test route must not assume either spell is available.

The vampire encounter exposed a separate native combat gap: its Turn Undead
uses the extended rank table, a try per undead LOAD MONSTER, cell `0Fh`'s
penalty and the native deity bonus. The active correction reuses the existing
extended turning implementation, preserving the rule boundary and existing
combat checkpoint format. Focused verification and the earned vampire fight
are still pending.

The extended-turning correction now passes **4/4 native combat and neighboring
checks in 1.29 seconds** (`/tmp/opengold-dqk-turning-host-checks-v1.log`). The
native host test loads seven real undead copies in two LOAD MONSTER calls and
checks that they grant two attempts and retain the authored penalty. Combat
tests cover repeated attempts, immediate departure when turned, failure
falling through to weaker undead, the deity's two-level bonus, and ordinary
combat-checkpoint roundtrip. Native FIGHTRECORD2 retains deity, knight order
and robe; FIGHTRECORD1 remains readable, and earlier games continue writing it.

The test player now uses legal Dispel Magic on held companions and can target
native delayed fireballs explicitly. Its reload scoring also treats drained
levels as a cost, as the Death Knights route already does. These are player
choices in the earned-route harness, not changed monster statistics or forced
wins. Haste is absent from this imported mage's robe-specific learning list;
the route instead learns Delayed Blast Fireball at the guild and memorizes
three copies of the already-known Enlarge for the knights. The vampire fight
and remaining Oracle pieces are not yet accepted.

## Earned vampire fight and recovery

The actual vampire encounter is won at route beat 95, with the second Oracle
piece recovered (`105h = 2`). The accepted fight is recorded in
`/tmp/opengold-dqk-vampire-aimed-v1.log` and
`/tmp/opengold-dqk-oracle-victory-95.sav`. Selias survives; all six PCs die.
This is an individual encounter win, not a campaign victory. The route returns
to Kristophan and uses normal temple services before continuing.

That return exposed a real map-transition blocker: Native LOAD FILES loads
Kristophan's smaller map before the script replaces the Tombs coordinates.
Publishing the old square immediately rejected the legal transition. Native
now publishes the settled square at redraw or event completion. The installed
Kristophan entry is tested from the Tombs script and its actual old square;
the Native, Death Knights and save checks pass **3/3 in 0.65 seconds**
(`/tmp/opengold-dqk-smaller-map-checks-after-v3.log`).

The People’s Temple also lacked Native's original Restoration service. Native
now receives that existing service at its original zero fee. An isolated
native temple fixture and neighboring save/Death Knights checks pass **3/3 in
0.73 seconds** (`/tmp/opengold-dqk-temple-restoration-checks-v1.log`). The earned
party physically reaches the actual temple and chooses Raise Dead for each
PC, Restoration for drained levels, and Heal followed by Cure Light Wounds.
All seven are alive, fully healed and at their previous levels at beat 98:
`/tmp/opengold-dqk-peoples-temple-v3.log` and
`/tmp/opengold-dqk-oracle-recovered.sav`. The PCs retain the consequences of
being raised; their maximum HP are 117, 120, 105, 89, 77 and 59, with Selias
at 89. No test healing, placements, quest writes or forced victories are used.
The remaining Oracle pieces and complete ending are still pending.

## Citadel checkpoint (2026-11-20)

The earned route has completed the Oracle, interrupted Trandamere's ritual,
recovered the Hawkbluff book, liberated Bai'or, freed Firebreath, returned
Tremor's iron scale and reached Aldinanachru. The false king's two battles
are won; six imported PCs, Grunschka and Captain Daenor are alive. This is
still a partial route, not an accepted campaign win.

Quiet citadel rooms exposed a native camp incompatibility: interval zero
disables encounter checks even when chance is 255. Native now follows that
behavior and refuses Rest only for interval one with chance at least 100.
The new host regression fails before the correction. Native, Death Knights
and save checks then pass **3/3 in 1.98 seconds**
(`/tmp/opengold-dqk-citadel-camp-checks-v1.log`); the actual earned route
refreshes its spent spells in the quiet workshop. The real king, Tower,
Abyss, final fresh run, all-map walk and application checks remain pending.

## Preparation, persistence and runtime survey checkpoint

Native Speed, Frost Giant Strength and Invisibility potions now use their actual
powers outside combat, consume the carried item, and retain their timed effects.
Speed cancels Slow or applies Haste, charging its year once before combat. Native
Fire Shield offers its original hot/cold choice in camp and preserves both its
protection and retaliating effect until they expire. Focused checks include
actual effect expiry and unchanged released Death Knights save roundtrips.

The Royal Mages' two-battle chain has been won with all eight party members
surviving, using earned supplies and ordinary combat commands. The actual
checkpoint is `/tmp/opengold-dqk-prepared-potions-royal-258.sav`; its log is
`/tmp/opengold-dqk-prepared-potions-royal-v1.log`. This resumed checkpoint is
progress evidence, not the required final fresh campaign proof.

Native Mirror Image retains its packed image count and caster level. Dispel uses
all five native caster-level bits. Magical blasts honor numeric resistance, and
Delayed Blast Fireball and Meteor Swarm retain their damage-die counts for fire
protection. Enchanted Auraks use their native eight-d8 final-form bolts and
sixteen-d8 explosions; a naturally expired frenzy does not gain the slain form's
bolt attack. These corrections preserve the earlier games' paths.

Native Fire Shield retaliation now stops the disabled attacker's remaining
blows after the triggering hit. Its regression fails before the correction.
The actual early-cave White Mage scroll also exposed an Original save-history
ordering defect: a later rest could memorize a scribed spell before reload
replayed the scroll that taught it. Original scroll grants are now available
before validating same-level spell choices, while their saved journal order
and existing formats remain intact. A regression scribes, memorizes, reloads
and resaves byte for byte; the actual native cave checkpoint now reloads too.

The eight focused creation, combat, game, Krynn, scroll and save checks pass
in **2.66 seconds** (`/tmp/opengold-dqk-scribed-replay-green-checks-v1.log`).
The Linux extension rebuild and four Godot checks pass; the full package is
still pending. The improved earned route is being replayed with the actual
copied Globe and a learned Fire Shield. The Font battle and complete campaign
win remain unverified; resumed checkpoints do not satisfy the final fresh-run
acceptance.

Long campaigns exposed an inconsistent save bound: ordinary daily Krynn calendar
entries can exceed one thousand history records. The reader now uses the
existing 65,536-entry collection bound and 16 MiB save bound instead. A regression
advances the ordinary clock for 1,100 days and reloads/resaves byte for byte.

The full rectangular-map runtime survey completes without unsupported events or
stalled conversations (`/tmp/opengold-dqk-native-all-map-walk-v5.log`): 12,405 idle
stops, eight shop visits, and actual diagnostic battles. It enters the nineteen
authored runtime maps, exercises both Luminari/Fastillion scripts on their shared
GEO, and covers each script's own stair table. The twentieth GEO, 24, has no
matching ECL or discovered LOAD FILES reference and is parsed by the census;
no runtime script is invented for it. The survey resets after diagnostic losses
and deliberately places characters for coverage. It is separate from earned
campaign proof.

The last five-suite checkpoint passed 5/5 in 4.11 seconds
(`/tmp/opengold-dqk-native-recovery-checks-v1.log`). Further native spell checks
are active. The Font, Abyss, complete authored ending, final fresh replay,
ordinary winning-save proof and final application/package checks remain pending.
No complete Dark Queen win is claimed.

## Current verification (2026-11-20, 12:18 UTC)

Dark Queen still has no accepted campaign win. The Font and the complete
authored ending remain open. The existing Easy setting also failed with the
first two test-player tactics; application difficulty defaults were unchanged.

Native Aurak final forms now discard their earlier energy-bolt power, as both
native transformation paths do. A regression first reproduced the retained
power. Their targeted energy bolts also respect packed mirror images. The
image test isolates distant, immobile attackers so ordinary melee cannot satisfy
it. Current focused combat/Krynn/save checks pass 5/5 in 3.97 seconds; the Linux
extension builds and Godot checks pass 4/4 in 19.95 seconds. Logs are
`/tmp/opengold-dqk-aurak-isolated-bolts-checks-v1.log` and
`/tmp/opengold-dqk-aurak-transition-godot-checks-v2.log`.

The active-power assumption in this earlier checkpoint is superseded by the
installed-executable activation audit below. The Native mirror-bolt test was
asserting an untriggered behavior; general Mirror Image coverage remains.

An ordinary Normal replay with paid sling purchases cleared the Royal Mages,
the king rescue and the tower through beat 332. All six PCs and Grunschka are
alive at full HP in `/tmp/opengold-dqk-four-slings-current-332.sav`. The Font
remains unwon; ranged support alone did too little damage. Its next test-player
tactic restores the fighters' melee weapons, prioritizes Sivaks, and permits
multiple actual Stinking Cloud casts against foes not already disabled. These
are legal player choices, not changes to the encounter or its rewards.

The primary native AI, combat menu and camp all honor the same global
`STATE_MAGIC_DISABLED` flag. The Abyss narrative's reference to mages does not
establish a missing class-specific suppression mechanic.

## Defensive scroll and aid-route checkpoint (2026-11-20, 14:43 UTC)

The earned cave store now supplies its actual White Mage scroll. The mage
casts Read Magic, marks Globe of Invulnerability for copying, and learns it
at the next ordinary rest. Preparing a newly copied spell exposed a save
replay ordering defect: Original scroll grants now apply before spell choices
at the same level, while history retains its original order. The regression
failed before correction, then actual checkpoint 14 reloaded successfully.
No save format or supported-history change was needed.

Native lethal Fire Shield retaliation also stops the killed attacker's
remaining attack forms after the triggering blow. Focused creation, combat,
Krynn, save and SRD-scroll checks pass 8/8 in 2.66 seconds; the updated Linux
extension builds and Godot checks pass 4/4 in 19.98 seconds. Evidence:
`/tmp/opengold-dqk-scribed-replay-green-checks-v1.log` and
`/tmp/opengold-dqk-scribed-replay-godot-checks-v1.log`.

Parallel script analysis identified why the new route missed Naulidis's
captain assignment: the earlier spear mission prevented requesting the hinge
errand, and stationary Look inside a safe hall could not request another one.
After delivering the spears, the route leaves the hall facing east, collects
hinges from the actual workshop, delivers them and walks to the other hall.
The fifth distinct aid mission and audience now complete through normal
commands. A resumed check reaches beat 42 with Daenor rescued, the Crylokon
received, `123h = 3`, and all six PCs alive at full HP:
`/tmp/opengold-dqk-aid-hall-exit-captain-v1.log`. This is a partial checkpoint.

The subsequent walk exposes a separate failure accepting an actually won
sea-snake/anemone battle. That failure is under diagnosis; the new defensive
preparation has not yet reached the Font. Neither the Font nor the complete
campaign has been won.

## Installed Aurak activation audit (2026-11-20)

The installed executable's resident `fx_trigger_group` dispatch was checked
case by case against the reconstruction: none of its 24 groups contains raw
151. The AI group 14 omits it, and combat-start group 24 contains only raw100.
The native handler is registered but has no direct activation call. Installed
Auraks 77/91 carry it at strength255, duration0, no removal callback and no
usable equipment that invokes it. The source `fx_add` stores the effect
without activating it. Evidence is retained in
`/tmp/opengold-native-fx-installed-group-facts.txt` and
`/tmp/opengold-native-aurak-audit-facts.txt`.

OpenGoldBox was starting the earlier Krynn counter and actively firing that
power in Native. Two Native-only dispatch guards preserve the inherent
metadata without activating its bolt/breath handler. A regression first
reproduced the incorrect255-to3 initialization, then passed with Native passive
and the earlier Krynn power still active. Damage, saving throws and Native
spellcasting are unchanged. This is activation fidelity evidence, not an earned
Font or complete campaign win.

### Sword petition save replay

An imported Crown knight can visit level 13, advance to 15, and accept the
ordinary Sword petition back to 13. Later divine preparation belongs after
that chronological petition. The reader now defers that nonempty preparation
while the earlier visit cannot prepare spells, then applies the existing rules
validation after the petition. No save fields, format changes, removed history
or SRD rule changes are involved. Invalid and unconsumed choices still fail.

The focused save regression reproduces the failure before the correction and
passes afterward. Previously failing real route saves at beats 277 and 332
also independently reload and resave byte for byte, preserving both actual
Sword promotions, HP, XP and spell choices. Evidence is in
`/tmp/opengold-dqk-sword-petition-audit-{277,332}-replay-green-v1.txt`.
The prepared Font party has not yet won that battle or the campaign.

### Movement checkpoint compatibility (2026-11-23)

Native now writes Original combat format 31, combining its time/underwater
state with main's per-square movement costs. The other games retain format
30. Native's costless format 29 remains readable with its original field
order, alongside earlier formats 27/28 and main's 30. The compatibility tests
exercise an actual legacy continuation, nonuniform movement costs and refused
malformed cost vectors; verification is pending the rebuilt integration.

## Final Aurak forms and single-attempt route (2026-11-23)

Four Native combat mismatches found by the source audit are corrected, each
with a regression that failed before the fix. Neither final Aurak shape
(raw 210/213, both carrying 165) strikes a creature leaving it
(`cbaction.c`). The shape the frenzy's timer makes, without the slain form's
zap, ends its AI turn at once and still explodes after three rounds
(`aiturn.c`). The explosion's stun spares the madness, frenzy and explosion
forms (1413, 1432, 1460), not the zap (1466). The frenzy's immolation burns
only the other side, as fire (`fx_immolate`). Champions and Death Knights keep
their own behavior. No checkpoint format changes.

The early caves now answer Crysia's Dispel with cleric 4, who has it
prepared; the White mage had none, so the fireball struck before fight 12.

The accepted proof plays each fight once: `OPENGOLD_ROUTE_SINGLE_ATTEMPT=1`,
Easy, the fixed fight dice, and no reload of a fight or event. Under those
rules the old route lost at fight 12. A beat may now name the tactic the
player picks for its fights (`RouteBeat::fight_tactic`); tactic 3 is
tactic 2 with fighters striking enemy spellcasters first. The route also
rests before the Luminari pit. With those choices a fresh run from the
imported Death Knights winners wins 147 fights in a row, through Bai'or and
the Gnome capital, in about two minutes.

It then loses the disguised court beneath the Gnome King (map 58 at 1,9,
beat 242). Eight members at full strength face two Dark Wizards and
enchanted Auraks and Bozaks. Their first-round Fireball (68 damage), Delayed
Blast Fireball, Disintegrate, Cone of Cold and Flesh to Stone kill everyone
under tactics 0, 2 and 3. The old route allowed 240 reseeded reloads here,
480 at the palace ritual and 2400 for the vampires. The Font of the Dark Queen
is still ahead. **No earned Native ending exists yet.**
`OPENGOLD_ROUTE_BEAT_TACTICS=N:T,...` sets a beat's tactic when planning.

## Route past the Font (2026-11-24)

The certifying standard is now the same as the other accepted endings: a lost
fight replays from the save before it with fresh dice, up to the route's
existing tries and event reloads. A fresh run from the imported Death Knights
winners on Easy, with Original movement, wins through beat 333. That includes
the Gnome court and the Font of the Dark Queen; the Font was won on its tenth
of twenty event reloads, and the dead were raised after it.

The beats after the Font had never been played and needed corrections:

- After the Font, fire creatures break every camp on its lava floor, so the
  route no longer rests there.
- The gnome windship rises from the shaft north of 17,10. Stepping into it
  plays the authored rescue of Crysia, whose spell choice is part of the
  scene. The old destination, 17,8, is reached only through that shaft.
- The windship sets the party down on a ledge at 0,24. Its way east crosses
  two shaft edges, which the party jumps when the prompt offers.
- Arriving at the Sanctum (5,25) plays both waves and the fall into the
  Abyss, so the separate step beat that followed is gone.
- Raistlin's cell is entered from 13,2. The party rests to full first, which
  the Abyss allows.

**Current blocker: Raistlin's torturers (map 66 at 15,2, beat 344).** Spells
fail in the room, and the fight cannot be avoided: approaching, waiting and
fighting all lead to it. The enemies include five enchanted Bozaks, whose Fire
Shields return each blow as twice the damage, as untyped magic (raw 112,
`fx_adjacent_double_zap`). There are also enchanted Sivaks whose deaths cast
the spell table's spell 0x7c on their neighbours (`fx_queue_meteor_explosion`),
plus a death dragon, amphi dragons and eyes of the deep. Both behaviors match
the reconstructed source. The full-strength party loses in rounds 4 to 6. All
20 event reloads lost with the default tactics, and tactic 4, which avoids
shielded targets, won none of 1518 scored tries. The authored ending is not
reached yet.
