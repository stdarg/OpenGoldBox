# Death Knights of Krynn

The fresh earned campaign, native integration, application and package checks
pass under the same all-twelve-game request. The batch follows
Champions delivery `233b6282`; the actual fresh Champions winners and their
ordinary save are preserved in `/tmp/opengold-deliveries/233b6282/`.

## Installed-data baseline and references

Original files: `/home/edmond/GoldBox/DKK`. Reference manuals, journal and
cluebook: `/home/edmond/GoldBox/docs/DKK`. The primary reconstruction is
[Death Knights of Krynn](https://github.com/talonius/death-knights-of-krynn),
reference revision `0ae5c4a3c29ecc90c3dc7636b9609381260f98f7`, read locally at
`/tmp/opengold-death-knights-source`. It supplies format/rule facts, not copied
implementation code or redistributed original assets.

The pre-implementation census misidentifies the installation as Secret of the
Silver Blades. It finds 25 scripts, 19 maps and 64 creatures, but cannot decode
the changed scripts, creature records, tile art, item list, spell names or
title sound. BACK archives, SKYGRND and MON1WIZ remain unread. This is an
explicit failing baseline, not a playability claim:
`/tmp/opengold-dkk-census-before.log`.

Initial source facts to verify against the actual records:

- `DKK.CFG` identifies the game before the older generic archive markers.
- Creature records are 216 bytes; the 220-byte spell block is separate in
  MON1WIZ. It has 74 memorized entries, 114 known entries and four rows of eight
  slot counts. Items retain their 63-byte layout.
- Top-level RETURN ends its event. AWARD EXPERIENCE reads a word, without the
  later VGA games' multiplication by 100.
- The three robe moons have phases at 4CF9h..4CFBh and day counters at
  4CFCh..4CFEh. Their day thresholds are 8, 1 and 6; phases cycle through 0..3.
  Memorization and effect cleanup account for phase bonuses.
- Native creation starts single-class characters at 210003 XP, divided for
  multiclass characters, and supplies class-family equipment. This must not
  overwrite the actual imported Champions winners' earned state.
- Original Champions import reads its 409-byte character and embedded spell
  data, converts item types and caps age. The exact conversion is part of the
  batch. The user's later universal winner library must retain all characters
  present at victory before Continue or scripted departures.

## Reading checkpoint

On base `233b6282` plus the active reader changes, the complete installed-data
census passes: `/tmp/opengold-dkk-census-v5.log`. All 25 scripts, 19 maps, 64
creatures, 14 separate spell blocks, 96 numbered item templates, pictures,
walls, full 256-tile sheets, font, title score, music banks and executable
name/sound tables decode. Original programs are identified but never executed.

The native script dialect already has WAIT KEY, JOURNAL ENTRY, YES NO, CLEAR
AND EXIT, ACTION MENU and the three-wealth-field treasure list found in the
later VGA games, while LOAD FILES still has three operands. SETUP MONSTER has
four. Spell names use 114 slots, including empty slots, and start Bless,
Barkskin; item names have 125 components. Each wall bank uses its own complete
tile sheet. FFh denotes 256 frames. Earlier literal 255-frame tile sheets
remain supported.

Focused new-reader, Champions, archive and ECL checks pass **4/4 in 0.68
seconds** (`/tmp/opengold-dkk-reader-checks-v1.log`). Synthetic checks preserve
independent deity/robe/order fields and all eight native spell-slot levels,
reject truncated records/templates/frames, and verify backward-compatible tile
handling. These reading checks do not establish campaign playability or an
earned ending. Native rules, importing the actual Champions winners, runtime
events, every-map walk, earned ending and final game/package checks remain.

The moon bonus is a shared allowance of one or two extra mage preparations
across eligible spell levels, not an extra allowance at every level
(`camp.pas` and `effhndlr.pas`). Native phase-day thresholds 8/1/6 advance after
9/2/7 days, giving complete 36/8/28-day cycles. New games start all phases at 2.
These facts must remain in the rules module when campaign time is connected.

## First implementation checkpoint

At **2026-11-19 11:23:39 UTC** (**60.18 minutes** after the batch start), the
reading census remains complete. Isolated runtime tests cover native yes/no
flags, journal/continue/clear-and-exit behavior, top-level RETURN, byte/word
arithmetic, full-word wealth/XP rewards, numeric STORE arguments and the
three-argument LOAD FILES alias. The command variants are separated from the
VGA renderer/memory selection. Death Knights, Champions, Darkness and ECL checks
pass **4/4 in 2.19 seconds** (`/tmp/opengold-dkk-vm-checks-v2.log`).

Native base progression tables are implemented separately from Champions:
11 XP/slot progressions, class/race minima, class eligibility, alignment masks
and racial caps. New paladin choices, rounded starting XP shares, knight-order
thresholds, ranger nature/arcane rows and eighth-level spell slots have focused
checks. These plus existing creation and Champions checks pass **3/3 in 0.66
seconds** (`/tmp/opengold-dkk-progression-checks-v1.log`). Character/spell
integration, clock/moon behavior, the actual Champions-winner import, campaign
runtime, every-map walk, earned ending and final native/Godot/package checks
remain. **Zero complete player outcomes of one planned** are accepted.

The same configured model/effort and one implementation owner remain in use,
with no delegation or runtime/model changes. Recorded timings are the actual
checkpoint and test-command durations; earlier compiler phase durations were
not estimated retroactively. No original installation or released save history
was changed.

## Character and runtime checkpoint

Native character creation is connected to the separate Death Knights rules
identity, seven Krynn races, eligible classes, rounded XP shares, 500 steel,
class-family starting kits, deity gifts, robes, native attack/save tables and
full spell-slot rows. Every supported race/class creates a live combat profile.
Native spell IDs map to the existing spell mechanics by meaning, with native
range, duration, targeting, casting time and AI metadata retained. Champions'
spell numbering and earlier-game creation remain covered by regression checks.
Character checks pass **4/4 in 1.25 seconds**
(`/tmp/opengold-dkk-character-checks-v2.log`).

The three moons use rules-owned chronological character-history outcomes.
Their initial phases are 2; periods are 9/2/7 days per phase, with the first
calendar rollover 720 campaign minutes after the noon opening. Preparation
adds one or two shared mage slots across eligible spell levels, rejects
cross-level over-allocation atomically, and forgets unavailable extras when
the phase changes. A combined calendar outcome and segmented outcomes replay
to the same state. Core copies the derived phases/counters into script memory.
No released save version was removed or replaced.

The installed campaign's resources load: opening script 16 on disk 1, all
64 encounter creatures with all 14 separate spell books attached, native wall
banks with complete tile sheets and BACK/SKYGRND backdrops, and eight shops.
The final shop has 30 entries; the reconstructed source's stock array is
truncated, but the installed START.EXE contains all 69 contiguous stock bytes.
The reader obtains the lists from that executable without executing it.

ITEM0 contains 96 raw 17-byte slots. Slots 1–91 are usable definitions;
92–95 contain trailing machine-code bytes, and 96 contains screen-fill bytes.
The format reader retains every byte. The playable catalog excludes slots
whose type/name indices or enchantment cannot represent an item; it offers
91 definitions. Native kits and shops use those valid definitions.

Runtime, native creation/combat, and Champions checks pass **4/4 in 1.35
seconds** (`/tmp/opengold-dkk-runtime-checks-v4.log`, after
`/tmp/opengold-dkk-runtime-build-v4.log`). Earlier integration failures exposed
the unused template tail, a district's obsolete ITEM1 lookup, and ACTION MENU
being enabled as a host operation. These are corrected without changing the
other games' command variants. This verifies resource integration and the
focused rules cases, **not an earned campaign ending**. Native monster-effect
mapping/AI, services, actual Champions-winner import, campaign movement,
every-map walk, an earned ending, ordinary winning save and final game/package
verification remain required.

## Combat clock checkpoint

Native combat advances one campaign minute per round. The encounter receives
the campaign's starting time, and the Original Rules refresh mage moon effects
as days change during battle. Core applies the rules-owned calendar outcomes
to participating characters without processing their combat effects twice.
Every game's checkpoints use `OGORIGINALCOMBAT 30` (since 2026-11-21, with
each square's movement cost), Death Knights' with its starting time; the
reader still takes 27 and Death Knights' 28. The original charges each
square's own movement cost (its fields out of doors hold terrain costing 2
and 4); the fields carry the costs, charged while Original movement is on
(docs/ORIGINAL-RULES.md).

Creation, Original combat, Champions, Death Knights and campaign-party checks
pass **5/5 in 29.21 seconds** (`/tmp/opengold-dkk-clock-checks-v1.log`, after
`/tmp/opengold-dkk-clock-build-v2.log`). The new case crosses midnight in a real
rules session, applies its elapsed time once, and resumes a saved fight with
identical subsequent moons, commands and random continuation. This remains a
focused rules result; the actual imported campaign and earned ending are pending.

## Actual Champions-winner import checkpoint

The actual Champions winning save from `233b6282` imports all six PCs. Checks
retain their earned XP, class levels, rolled and maximum HP, current wounds,
surviving item records and all seven money denominations exactly once. The
wounded cleric remains wounded. Native age caps apply, and imported characters
receive no fresh-character steel. The ordinary party-screen save roundtrips
byte for byte; this is the pre-adventure party save, not an earned Death Knights
exploration or winning save.

Native knight petitions use Death Knights' high-level progression and lose
exactly two HP above level nine when the new order costs a level. Training
distinguishes knights, paladins and rangers. The introduction's own music bank
produces audible samples. Wilderness loading uses the installed 38-frame
WILDCOM and the 8X8D1 record 204 cursor, with its magenta background transparent.
It does not require Champions' separate RANDCOM or CURSOR archives.

After `/tmp/opengold-dkk-integration-build-v2.log`, Death Knights, Champions,
party and music-render checks pass **4/4 in 39.30 seconds**
(`/tmp/opengold-dkk-integration-checks-v2.log`). The actual winner save is passed
through `OPENGOLD_CHAMPIONS_WINNERS`. The every-map walk has started and exposed
the opening camp's incorrect script exit. That correction is being rebuilt;
the opening battle, all-map runtime walk, imported earned campaign and final
native/Godot/package checks remain outstanding.

## Every-map runtime checkpoint

All **19 installed maps** pass the runtime walk in **36.59 seconds**
(`/tmp/opengold-dkk-walk-v9.log`, built by
`/tmp/opengold-dkk-map-fixes-build-v2.log`). The survey completes 4,796 event
visits and resolves 439 battles: 312 victories and 127 defeats. It restarts
after defeats. Its freshly created fighters use the native Modify option and
native starting money and kits. This is a coverage survey, not the earned
campaign proof and not a guarantee that this party can win every encounter.

The walk identified the cross-disk SPRIT4 routing, PIC7 animation 83, Maya's
PIC5 priority, Death Knights' half-damage saving-throw flags, and the need to
end a lethal trap event before its next COMBAT. Those cases now execute without
unsupported-event errors. Player characters use the native one-square size.

Creation, Original combat, Champions, Death Knights and party checks pass
**5/5 in 33.72 seconds** (`/tmp/opengold-dkk-map-fixes-checks-v2.log`). The actual
Champions winners retain their spell preparation through a version-four import
recipe; older import recipes remain readable. Both opening rest answers reach
the battle, with the normal post-rest spell choices completed through the party
API. Native brace checks pass for all 36 changed implementation/test files,
with the final map changes checked again separately. The imported earned
campaign, authored ending, winning exploration save and final game build are
still outstanding. **Zero full player outcomes of one planned** are accepted.

## Combat checkpoint

Death Knights' monster effects now select their own mechanics: its 67/68
are one/two-level drains, 65 electrical immunity, and 93 resistance to cutting
and piercing weapons. Native powers include nightmare smoke, fire-minion
immolation/touch/cold vulnerability/fire healing, flesh- and iron-golem
responses, cold/electrical resistance, regeneration and death breath. Shared
Krynn reflections, fear, breath weapons and weapon powers reuse the existing
mechanics by meaning. The new effect bank follows all reserved Champions IDs;
the combat profile reader retains the earlier range and accepts the new bank.

`CharHlp.GetCasterLevel` also applies moon phase: a low moon subtracts one
mage level, a full moon adds one for mages above level five, and the other
phases leave it unchanged. The actual class level is preserved. Both camp
and combat use the same Death Knights helper. Combat profiles carry the
robe/phase context through their existing codec.

Checks pass **4/4 in 1.37 seconds**
(`/tmp/opengold-dkk-combat-checks-v6.log`, native library from
`/tmp/opengold-dkk-combat-build-v3.log`, final test source rebuilt in
`/tmp/opengold-dkk-combat-build-v6.log`). Actual legal cast/aim commands produce
Enlarge's full- and low-moon durations at caster levels nine/seven for the
same level-eight mage. The test accounts for the elapsed round before the
observation. A real AI smoke action then changes its victim's fixed six-point
weapon damage to four. Both live fights restore byte-for-byte, including new
effects and RNG continuation. Existing creation, combat and Champions checks
remain green. This is focused combat evidence, not an all-monster battle
walk or an earned ending. Combat/campaign clock synchronization, services,
the actual Champions-party import and the remaining campaign acceptance
checks still precede certification.

## Imported earned opening checkpoint

The six actual Champions winners from delivery `233b6282` win the native
Death Knights opening battle: four skeletal warriors and four nightmares,
with the four scripted Solamnic knights. All six PCs remain standing, with
one retained victory and zero defeats. The opening imports the source party's
actual earned experience, wounds, class levels, prepared spells, converted
readied equipment and money. It supplies no test rewards or outcomes.

The opening check passes in **20.58 seconds**
(`/tmp/opengold-dkk-earned-opening-v1.log`, build
`/tmp/opengold-dkk-earned-opening-build-v1.log`). Its ordinary campaign state
roundtrips byte-identically. The route checkpoint stands in Gargath Outpost,
map/script 36 at (6,9). This verifies the imported opening, not the full ending.
The subsequent route has legally rested in the assigned inn room and accepted
the commander's cemetery duty; that route is still being developed. The
complete earned ending and final game/package checks remain outstanding.

A fresh replay from that actual source save also completes Gargath's cemetery
and guard duties, wins both encountered battles, hears Sir Thom, and earns
the commander's release. All six PCs survive, with native quest state `4C2D=7`.
Evidence: `/tmp/opengold-dkk-earned-outpost-fresh-v1.log`, rebuilt test
`/tmp/opengold-dkk-kalaman-build-v1.log`. Earlier resumed runs were route
diagnostics; this fresh replay verifies the opening and outpost together.
No full campaign ending is claimed.

## Kalaman route checkpoint

The imported party trains all six characters normally at Gargath, rests, leaves
through the actual western gate, and walks the wilderness to Kalaman. The
arrival check passes with all six PCs standing and a byte-identical ordinary
save roundtrip (`/tmp/opengold-dkk-earned-kalaman-v3.log`). Daine and Ariela's
first audience also passes its native quest-state assertion
(`/tmp/opengold-dkk-earned-kalaman-audience-v1.log`).

The party then wins the pavilion's seven-sivak battle, receives the ransom
note at the inn, waits by ordinary ten-minute Looks, and attends the nighttime
parley. Listening and handing over the Sleepstone lead to another actual
victory against the disrupting force. All six PCs survive, and native quest
state `4C4C=3` is checked (`/tmp/opengold-dkk-earned-kalaman-parley-v4.log`).
The test's walking planner avoids the native gates that close at night and
during the kidnapping; production movement already enforces those gates.
These are earned route checkpoints, not the campaign ending. The continuous
fresh ending proof and final game/package checks remain outstanding.

## Vingaard and Sebas checkpoint

The party completes all three of the Dream Merchant's nightmare battles and
earns the native potion that ends the bad dreams. All six PCs survive; native
states `4C54=2` and `4C55=2` are checked
(`/tmp/opengold-dkk-earned-vingaard-nightmare-v1.log`). The test completes
pending spell choices after PROGRAM 9, matching the UI, then uses ordinary
camp to trigger Sir Thom's dream and its interpretation. The existing scripted
PROGRAM 9 rest grants its normal rest before that subsequent camp; its timing
is an approximation of the original scripted camp sequence.

A fresh continuous replay from the actual Champions winning save reaches
Sebas and earns the High Clerist's Tower mission: **nine victories, zero
defeats, all six PCs standing**. It crosses all nine red doors using ordinary
steps and Vingaard's native map wraps, then checks `4C57=1` and `4C68=1`.
The normal campaign save reloads byte-identically. Evidence:
`/tmp/opengold-dkk-earned-sebas-fresh-v1.log`, built by
`/tmp/opengold-dkk-sebas-build-v2.log`, base `233b6282` plus the active changes.
This verifies the first 95 route beats together. The Tower, later quests,
authored ending and final game/package checks remain outstanding.

## Tower and tunnel progress

The actual imported party reaches the High Clerist's Tower after Sebas's
warning. Its authored opening battle is won and the native NPC Durfey joins
as the seventh active member. The tower checkpoint passes with every member
alive (`/tmp/opengold-dkk-earned-tower-confirm.log`). Walking into the tunnels
retains three further victories and no defeats
(`/tmp/opengold-dkk-earned-crypt-entry-v1.log`).

The native Flare encounter also passes, with all six imported characters and
Durfey alive (`/tmp/opengold-dkk-earned-crypt-dragon-v3.log`). The player uses
ordinary camp preparation, healing spells and Resist Fire before entering the
chamber. Four actual battles are retained from the tunnel-entry checkpoint,
with no defeats; the normal exploration save reloads byte for byte. These are
resumed campaign milestones. They do not replace the required final fresh
campaign run or establish a complete Death Knights ending.

The tunnel confrontation frees Garren from his cursed sword; a party knight
survives Soth's blast while protecting him. The party then returns to Sebas
through the nine red doors and receives his Dragon Pit map. That joined route
passes with all seven members alive
(`/tmp/opengold-dkk-earned-dragon-mission-v1.log`).

The Dragon Pit milestone passes with **seven retained victories, no defeats,
and all seven members alive**
(`/tmp/opengold-dkk-earned-dragon-pit-v1.log`). The party follows the native
secret entrance, defeats the Dread Wolf, witnesses Maya's confrontation with
Karl, defeats the death dragon and takes the actual dragonlance treasure.
The altar's actual equipment and scrolls are also collected. Native quest
states `4C81h=1` and `4C7Ch=255` and the byte-equal ordinary save reload are
checked. The concluding campaign remains unverified.

The third red-door trail reaches Sebas's abandoned hideout. The party wins
the sivak ambush and finds Ariela's earring, with all seven members alive
(`/tmp/opengold-dkk-earned-sebas-abducted-v3.log`).

Victory detection requires the native council's final closing words, rather
than only `4CD0h`, which is set before Soth's final battle. Its result survives
the entrance search that starts another script invocation. An isolated check
runs the actual installed council: an ordinary tower arrival with the earlier
flag does not announce victory; the council after Dargaard does; Continue
returns to exploration. Death Knights, Champions, Treasures and ECL checks
pass **4/4 in 3.88 seconds**
(`/tmp/opengold-dkk-council-win-checks-v4.log`). This regression does not prove
an earned campaign ending.

## Final sequence checkpoint

The earned route rescues Kalaman's captives, destroys Dulcimer's lich and soul
jar, obtains the Rod of Omniscient Wisdom, and enters Dargaard through its
authored password and controls. Durfey leaves through his native story event;
his later undead encounter is defeated rather than retaining a synthetic ally.
The party frees the prisoners, witnesses Sebas's death and recovers Sturm.
Resumed milestone checks pass with all six imported PCs alive.

The final sequence passes from the earned Sturm checkpoint, through the iron
golem statues, undead Durfey, Lord Soth, Lenore's intervention and the High
Clerist's council. All six imported characters survive. The player readies
already carried armor, prepares its available spells and casts protection,
Prayer, Bless and Enlarge through ordinary actions. The combat controller avoids
fire spells that heal iron golems and prefers surviving, undrained characters.
There are no forced outcomes, rewards, healing or quest-state writes.
Evidence: `/tmp/opengold-dkk-earned-ending-v5.log`, base `233b6282` plus active
changes. A fresh complete campaign and final game/package checks remain required.

## Fresh earned campaign

The complete **331-beat** route passes in **425.62 seconds**, importing the
actual ordinary Champions winning save and playing continuously from the
opening through Soth's banishment and the High Clerist's council. There is no
route resume, beat limit, quest edit, arbitrary placement or forced outcome.
Ordinary combat/event reloads remain part of the player; discarded defeats
appear in the diagnostic phase totals, while no defeat is retained in the
accepted campaign.

All six imported PCs survive. Cleric 5 is unconscious at victory; cleric 4
casts an available Cure Light Wounds after the council, and both clerics spend
their remaining cures through ordinary exploration commands. All six are
standing when the route ends. Durfey's earlier authored departure is preserved.

Evidence: `/tmp/opengold-dkk-earned-full-v2.log`, tested binary SHA-256
`3b615e3848627f40eb36b93b373541d88b59f771ae3afbfeb10a28b85c679986`,
base `b9141da9` plus the active Death Knights changes. The upstream integration
adds documentation and session-board code without changing the tested native
game inputs.

`/tmp/opengold-dkk-winning.ogs` is an ordinary format-53 campaign save with the
actual installed DKK asset identity, rather than the route-checkpoint wrapper.
It decodes and re-encodes byte-identically with all six active winners alive.
SHA-256: `87ab1b72f94f54c16db1af3f6deb16e5a21064f0a34af1f6aceea2b444123bbf`.
Export evidence: `/tmp/opengold-dkk-winning-export-v1.log`.

## Delivery verification

On base `b9141da9` plus the final changes, native integration passes
**150/150 in 192.48 seconds**
(`/tmp/opengold-dkk-final-integration-checks.log`). Godot and package checks
pass **19/19 in 61.63 seconds**, including actual Death Knights fighter and
cleric creation, startup/layout, save/import regressions, Spanish text and the
packaged game (`/tmp/opengold-dkk-final-godot-package-checks-v2.log`).
The Linux game/export build passes
(`/tmp/opengold-dkk-linux-delivery-build-v4.log`). The test fixture's relink
uses the same `LIBRARY_PATH` as the build.

The final Death Knights map walk passes in 44.34 seconds. Curse, Secret,
Gateway, Darkness and Champions map walks also pass. The broader legacy
diagnostic batch was stopped after those checks; its unfinished Treasures walk
and old diagnostic routes are not claimed as passing
(`/tmp/opengold-dkk-final-native-checks.log`). They do not replace the separate
fresh earned Death Knights proof. Final formatting, native/Godot brace checks,
localization and whitespace checks pass.

The post-verification checkpoint is **2026-11-19 18:22:32 UTC**, an observed
batch elapsed time of **7 hours 59 minutes 4 seconds**. Individual compiler and
review phases were not separately timed. One complete player outcome of one
planned is accepted. Review preserves RAII ownership, Godot/C++20/GDExtension
boundaries, stable game IDs, released-save histories and rules ownership of
mechanics. No SRD implementation or model/runtime setting changed, and no
agents were delegated. The remaining fidelity limitations are recorded above.

The main checkout's pre-existing `docs/GAMES.md` edit is unchanged; its dirty
checkout exception prevents automatic pull/build there. Dark Queen, both Buck
Rogers games, the actual installed UA design and the automatic victory roster
remain sequential work under the original request.
