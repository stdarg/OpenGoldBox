# Installed-game playability verification

## Acceptance and scope

The user authorized fixing all discovered Linux test and playability errors,
including pre-existing errors. The completion objective covers all twelve Gold
Box games installed in `/home/edmond/GoldBox`. The seven currently supported
games are listed below; the three Krynn and two Buck Rogers games still need
implementation. Arbitrary third-party Unlimited Adventures designs are not
certified by one generated test design. Winning-character availability in the
next game follows certification of every game, as requested by the user.

An unassisted completion requires an actual winning event through player
commands, earned experience/equipment or a verified import, physically valid
movement and working saves. A forced victory, injected reward, quest flag or
arbitrary relocation is diagnostic assistance. Passing an assisted route is
not an unassisted completion certificate. Existing architecture, save history,
UI behavior and the current configured model are retained; no delegation.

## Current evidence

| Game | Verified | Remaining completion gap |
|---|---|---|
| Pool of Radiance | Fresh Original Rules party starts at zero XP, earns its training and equipment, defeats Tyranthraxus and reaches the ending; SRD map audits remain separate | Optional quests, other party compositions and difficulty settings are not certified by this route |
| Curse of the Azure Bonds | Fresh Original Rules party earns all three artifacts, equipment, paid advancement and recovery, physically reaches Myth Drannor, defeats Tyranthraxus and shows the actual ending | Optional quests, other party compositions and difficulty settings are not certified by this route |
| Secret of the Silver Blades | Fresh complete route reaches the actual ending with every fight played, including Oswulf, the Dreadlord and the soul gem; zero forced wins | Replace progression grants and relocations with verified gameplay/import |
| Gateway to the Savage Frontier | Route reaches the ending and wins its fights | Replace experience/gold/equipment/time grants and relocations |
| Pools of Darkness | Fresh whole route and detailed finale replay defeat Gothmenes, reach the ending and restore all six members; zero forced results | Progression assistance and relocations remain |
| Treasures of the Savage Frontier | Route reaches the ending and wins its fights | Replace starting experience/money and relocations |
| Unlimited Adventures | Generated design walks, wins all fights, saves at the stairs, reloads and reaches its actual winning event | Other designs require their own verification |

The six exported commercial campaigns previously passed headless and rendered
window launches under an isolated `cage` compositor at `e3b2ae9c`; the installed
data suite also exercises gameplay, saves, imports and the UA UI. Launching and
assisted route completion do not close the gaps above.

## Corrections

- Original-rules camp omitted Minor Globe of Invulnerability, Globe of
  Invulnerability and Mass Invisibility. Both globes were also discarded
  between casting and combat. The regression checks actual action availability,
  self/party targets, consumption and effects in the next combat.
- Darkness palace script 65 clears random index 9Eh instead of used-group mask
  A9h after exhausting its eight wandering groups, looping forever at 8B6Bh.
  A guarded operand override clears A9h without modifying original bytes or the
  save format. The regression exercises four full selection cycles; it failed
  before the correction. See [ORIGINAL-RULES.md](ORIGINAL-RULES.md).
- The route player now preserves explicitly requested late-game preparation,
  matches full spell names, uses actual memorized spell counts, finishes pending
  rest/learning choices, retains earlier Darkness preparation, and avoids
  wasting every Heal on the spell's intentional last 1d4 damage.
- Secret uses earned training, temple recovery, camp wards, caster targeting,
  range/visibility checks and whole-event reloads to win its former forced
  battles. A real ending with surviving party members counts as a victory;
  casualties do not invalidate the game's winning event.
- The UA winning route now proves campaign save/reload and asserts that every
  combat was won. No rewards, victory flags or encounter outcomes are injected
  into that generated-design route.
- Pool's script-provided treasure now earns Original Rules experience, including
  treasure accompanying real fights. Combined awards preserve division and
  prime-score rounding, rollback and reward claims; SRD rewards are unchanged.

## Previous integrated verification (2026-11-16)

Tested working tree is based on `e3b2ae9c`, with the changes accompanying this
record. Both environment variables are required:

```bash
OPENGOLD_GOLDBOX_DIR=/home/edmond/GoldBox \
OPENGOLD_GAME_DIR=/home/edmond/GoldBox/POOLRAD \
JOBS=8 ./build.sh --test-all
```

The final integrated build/test run passed **336/336 checks, zero failures
and zero skips**, including 171 Godot checks, in 1,211.51 seconds. Secret's
complete route passed in 626.13 seconds and Darkness's assisted route in
1,194.74 seconds. After rebuilding, the nine demo checks, two DOS checks
(469.96 seconds) and 21 Python tests also passed. Localization validates
4,143 messages. Commands and earlier baseline results are recorded in
[TESTING.md](TESTING.md).

Local investigation logs (temporary files, not distributed):

- `/tmp/opengold-secret-final-whole.log`: fresh Secret combat replay, exit 0,
  ending at beat 2276; zero forced-win messages. Its original progression and
  movement assistance remains. Checkpoints share `/tmp/opengold-secret-final-whole-`.
- `/tmp/opengold-playability-rerun-routes.log`: both corrected full routes pass.
- `/tmp/opengold-palace-loop-before.log`: reproduced palace selector regression.
- `/tmp/opengold-palace-loop-fix-tests.log`: corrected selector and focused checks.
- `/tmp/opengold-playability-delivery-suite.log`: final integrated build/test run.
- `/tmp/opengold-playability-demo-tests.log`,
  `/tmp/opengold-playability-dos-tests.log`,
  `/tmp/opengold-playability-python.log`: additional passing suites.

Darkness's finale gap from this run is addressed by the 2026-11-17 work below.
These earlier results remain historical evidence, not the current validation.

## Timing checkpoints

The first captured timestamp was 2026-11-16 18:40 PST; earlier setup was not timed.

| Local time | Elapsed | State |
|---|---:|---|
| 19:38 | 58 min | Secret preparation and Darkness finale investigation; no certified unassisted commercial completions |
| 20:38 | 118 min | UA save-to-win passed; protective-spell fixes tested; Secret suffix passed; palace hang reproduced |
| 21:42 | 182 min | Fresh Secret whole route passed with all fights played; palace selector fixed; all 336 checks green across full run and corrected reruns |
| 22:04 | 204 min | Final integrated run passed all 336 checks with no skips; corrections ready for delivery |

No commercial campaign yet has the full unassisted certificate defined above.
Token/cost measurements are unavailable. This record preserves the remaining
scope rather than treating green assisted routes as completion of that scope.


## 2026-11-17: Darkness defenses and finale

The user approved Darkness's remaining forced battles, followed by one
unassisted Original Rules Pool of Radiance campaign. This first batch fixes
proven defects and wins the required finale through player commands; it does
not certify the route's existing progression grants or relocations.

Three verified corrections, detailed in [ORIGINAL-RULES.md](ORIGINAL-RULES.md)
and [darkness.md](games/darkness.md):

- The actual dragon-breath protection scroll can be used before combat,
  is consumed and protects its user for twelve minutes, including across
  campaign/combat transitions. Both 256-color games have lifecycle regressions.
- Darkness Fire Shield returns twice an adjacent weapon hit's damage as magic,
  including Bane minions' innate shield. The cast spell grants retaliation
  rather than electricity immunity. Earlier games' mappings are unchanged.
- The recognized DOS 1.1 ending recovery loop starts at character zero instead
  of Xoham's leftover monster count. Real dead campaign vitals, including the
  first and last members, are now tested; the old fixture's display-only
  casualties did not actually exercise recovery. Original files and save
  serialization are unchanged.

The replay takes real +4 weapon upgrades and missile stacks from treasure,
keeps its late preparation, restores its answers when reloading an event,
heals during combat, avoids attacking retaliating shields in melee and uses
unambiguous inventory row indices. It reserves Haste copies for Xoham and
reads the paladin's actual protection scroll from Inventory during the AGREE
prompt, before Gothmenes bars magic. Camp casting remains unavailable between
chained fights; no new UI behavior is introduced.

The ordinary route now uses this verified manual plan and rejects forced-win
beats. Its final-battle check also rejects a living Gothmenes: the original
fifteen-round ceasefire can otherwise trigger the ending without defeating
him. That original combat rule is preserved; the earlier ceasefire completion
was withdrawn as boss-defeat evidence.

**Actual defeat evidence:** `/tmp/opengold-darkness-final-suffix.log` (exit 0),
resumed from the last safe preparation checkpoint. The log shows weapon damage
putting Gothmenes down, his death from wounds, all three encounters won, the
actual ending and all six party members restored to maximum Hit Points.
There are zero forced-win messages and zero ceasefires in that replay. An
earlier run's exit-status failure was an unrelated audit demanding Bane's
earlier music event from a late checkpoint; that audit now applies to whole
routes and reports its diagnostic before returning. Full-route verification
still requires the music event.

**Final validation:** all 336 checks pass across the complete run and the
corrected Secret rerun, with no skips, including all 171 Godot checks. The
complete run passed 335/336 in 1,328.04 seconds; its sole failure exposed four
obsolete forced-win markers in Secret's castle passages. After removing them,
the rebuilt Secret whole route passes again (exit 0), with the actual ending
and no forced results. Darkness's fresh whole route passes in 1,307.62 seconds,
including its earlier music event and strict Gothmenes-defeat check.

The four focused Original Rules/Darkness checks pass in 2.05 seconds. The
rebuilt nine demo/catalog checks pass in 9.91 seconds; the earlier two DOS
checks pass (508.30 seconds together with that run's demos), and 21 Python
tests pass. Localization validates all 4,143 messages. Formatting passes;
clang-tidy also passes for the Godot view when given `build/linux` rather than
the hook's older non-Godot `build` configuration. Tested tree is based on
`742f7840`, with the changes accompanying this record.

Reproduction uses both data variables shown above. Focused evidence:

- `/tmp/opengold-fireshield-regression-before.log`,
  `/tmp/opengold-fireshield-focused-after.log`.
- `/tmp/opengold-darkness-recovery-ctest-before.log`,
  `/tmp/opengold-darkness-recovery-focused-after.log`.
- `/tmp/opengold-darkness-secret-regression-recheck.log`.
- `/tmp/opengold-darkness-integrated-ctest.log` (intermediate 334/336 result).
- `/tmp/opengold-darkness-final-ctest.log` (335/336 before marker correction),
  `/tmp/opengold-darkness-final-secret-route.log` (corrected whole-route pass).
- `/tmp/opengold-darkness-final-whole-route.log` (fresh Darkness route),
  `/tmp/opengold-darkness-final-suffix.log` (detailed final-battle proof).

Observed start: 07:40 PST on 2026-11-17. Checkpoints were recorded at
2026-11-17 08:40 PST / 16:40 UTC, 2026-11-17 09:40 PST / 17:40 UTC,
2026-11-17 10:43 PST and 2026-11-17 11:40 PST / 19:40 UTC. At the latest
checkpoint the real finale and corrected Secret route had passed; the fresh
Darkness route and one serial UI check remained in the integrated run.
The work retained the configured model/effort;
exact runtime identifiers and token/cost figures were unavailable. No
model/runtime change or delegation was made. The Pool of Radiance phase has
not started implementation at that checkpoint; the next batch follows.

## 2026-11-17: unassisted Original Rules Pool of Radiance

- **Authorization:** the approved follow-up; one implementation owner.
- **Outcome:** one newly created party reaches the actual winning event using
  the existing game commands. Creation's legal Modify controls are allowed;
  experience starts at zero and money comes from the creation rolls.
- **Acceptance:** actual purchases and treasure; earned experience and paid
  training; valid movement; real camp/rest and recovery; campaign save/reload;
  real combat victories and Tyranthraxus defeated. No injected levels, money,
  equipment, healing, quest flags, relocations or combat outcomes.
- **Reuse:** the installed-data expedition player and existing Original Rules,
  campaign, inventory, camp, training and save interfaces. No UI layout or
  control changes, new runtime, save-format change or reduced compatibility.
  Existing assisted audit routes retain their separate purpose.
- **Model:** current configured model/effort retained; exact runtime identifiers
  are unavailable. This Original Rules batch is outside the SRD-specific model
  assignments. No delegation or runtime change.
- **Checks:** rebuild affected targets before focused runs; complete the fresh
  winning route and relevant native/campaign regressions on the final tree.
  Retain full logs and checkpoints locally, report material limits honestly.
- **Timing:** observed start 2026-11-17 11:45 PST / 19:45 UTC; the required
  60-minute checkpoint and subsequent observations are recorded below.

At 12:07 PST / 20:07 UTC, the new campaign exposed a genuine progression
defect: script-provided Pool treasures granted money/items but no experience.
The focused regression fails before the Core correction and passes after it,
including prime-score bonuses, rollback, guarded revisits and legitimate repeat
payouts. See [Original Rules](ORIGINAL-RULES.md) for the pinned behavioral
reference. Evidence: `/tmp/opengold-earned-treasure-before.log` and
`/tmp/opengold-earned-treasure-after.log`. The unassisted completion remains in
progress; no winning-campaign claim is made yet.

At 12:42 PST / 20:42 UTC, the earned route has created and equipped its party,
searched the Slums, defeated Norris, freed Sokal through Ferran's dialogue,
returned by boat, collected council rewards, trained and completed the Podol
auction. Checkpoints are real campaign saves under
`/tmp/opengold-earned-campaign`; Normal-difficulty whole-stage reloads use the
game's reseeding service. The Cadorna route currently loses members to undead
level drain; the nomad defense reward is the next legitimate source of training
funds. The 60-minute checkpoint is due at 12:45 PST. The final campaign win,
combined combat/script-treasure XP rounding review, broader regression checks,
commit/push and main-checkout rebuild remain outstanding.

**60-minute checkpoint, observed 12:45 PST / 20:45 UTC:** the campaign-ending
requirement remains undelivered. The progression defect is reproduced and its
focused correction passes; the earned opening, Norris, Sokal and auction are
verified. Nomad defense currently fails; one observed problem is the reused
audit's recovery strategy trying to replenish spells by camping during active battle waves.
The next run spends only available cure spells between those waves. No new
goal, model change, delegation or additional compatibility scope is introduced.

**Checkpoint observed 2026-11-17 13:46 PST:** the unassisted ending is still pending.
Actual earned routes have completed Cadorna, the Temple of Bane and Kovel
Mansion; the stronger fresh cohort currently stops at the temple horde under
its stricter casualty policy. Investigation is confined to legal player tactics
and progression. Scripted-treasure experience and the new combined battle/loot
rounding regression pass. Broad final checks, delivery and the main rebuild
remain outstanding. No delegation, runtime change or compatibility change.

**Fresh campaign completed, observed 2026-11-17 14:56 PST:**
`/tmp/opengold-earned-final-route.log` exits successfully after the real ending.
Six newly created humans begin with zero experience and rolled creation gold;
the original's legal Modify controls maximize their starting scores and HP.
The route physically visits the Slums, Kuto's Well, Sokal, Podol's auction,
Cadorna's textile house, Kovel Mansion, the Temple of Bane and Valjevo Castle.
Quest/combat rewards, actual mirror duels, paid guild training, purchases,
lodging, cure spells and real saves supply all progression and recovery.
It uses Normal-difficulty whole-stage reloads (21 temple and 32 castle retries
in this run), and finishes with surviving members. No levels, wealth, gear,
healing, quest flags, locations or combat outcomes are injected.

The final battle must actually defeat `MON5CHA.DAX:66`, then reach winning
variable `4AFD = 255` and return to New Phlan. A ceasefire with the dragon still
alive fails. `/tmp/opengold-earned-funded-study-v3.log` independently completes
the earned suffix from a real earlier checkpoint. This establishes one winning
campaign, rather than every optional quest or an unassisted certificate for
the other commercial games.

**Final validation, observed 14:58 PST:** all **337 Linux checks pass across
the complete run and the corrected fresh-campaign rerun, with zero skips**.
The complete build/test run passed 336/337 in 1,411.65 seconds; its sole failure
was the new route spending its training money too early. The corrected,
formatted and rebuilt Original Rules checks both pass in 84.11 seconds; the
registered fresh campaign takes 83.96 seconds. The full run's Darkness route
passes in 1,388.32 seconds. Production code was unchanged during the route-plan
correction. Nine rebuilt demo/catalog checks pass in 10.84 seconds, both DOS
checks in 813.07 seconds, and 21 Python tests in 2.300 seconds. Localization
validates 4,143 messages. Scope, architecture, ownership and formatting checks
pass; no UI, save-format or SRD mechanics changes were made.

Tested tree is based on `334099c8`, with the accompanying changes. Reproduction
uses the installed-data variables above and the focused command in
[TESTING.md](TESTING.md#earned-original-rules-campaign-2026-11-17). Full logs:

- `/tmp/opengold-earned-final-build-and-tests.log` (complete run).
- `/tmp/opengold-earned-final-focused-tests.log` (corrected registered route
  and treasure/rounding/rollback regressions).
- `/tmp/opengold-earned-final-route.log` (independent fresh campaign).
- `/tmp/opengold-earned-tools-tests.log`, `/tmp/opengold-earned-dos-tests.log`,
  `/tmp/opengold-earned-python-tests.log`, `/tmp/opengold-earned-localization.log`.

Investigation and implementation were interleaved from 11:45 through 14:56;
the broad build began at approximately 14:25 and its checks finished at 14:54.
Final route verification finished at 14:58. These are observed phase boundaries,
not separate CPU-time measurements. Commit/push and the required main-checkout
rebuild follow verification.


## Active all-games delivery (2026-11-18)

- Authorization: user requests every Gold Box game playable and winnable,
  followed by availability of every winning character for the next game.
  Includes all twelve installations; supersedes the earlier seven-game scope
  and Krynn's former wait for a separate instruction. No delegation.
- First batch: Curse earned progression through Tilverton's sewer lairs.
  Acceptance: no injected rewards, healing, flags or combat results; paid
  training and normal camp recovery; actual combat wins. This diagnostic route
  retains assisted opening setup and cannot certify a campaign win. Sewer
  travel is being replaced with real steps, including scripted folded corridors.
- Reuse: original-rules campaign, combat and route player. No SRD changes,
  save-format changes, new controls or runtime changes in this batch.
- Model: current configured Codex model retained, no switch requested or made;
  SRD assignments do not apply to this original-rules batch.
- Verification: rebuild native route target, execute earned sewer route against
  installed data, then relevant native regressions before delivery.
- Timing: investigation observed 2026-11-18 03:44 UTC; checkpoint due 04:44 UTC,
  no later than 05:14 UTC. Earlier local edits were present at task start.
- Remaining: all assisted commercial routes, Krynn, Buck Rogers, UA design
  coverage, then automatic winning-character availability.


Observed 2026-11-18 04:00 UTC: the earned sewer segment passes seven real fights,
with zero losses and all six members alive at 30,912 XP. Both clerics train to
level 6. Creation money pays for real shop purchases; real steps traverse the
sewers and their folded edge passages. Earlier failures exposed test-route
assistance (relocation), an unintended checkpoint surrender, and an avoidable
attack on the bargaining otyughs. No production-rule change was needed.
See [Curse](games/curse.md#earned-sewer-progression-2026-11-18). Final registered
regression verification and delivery remain pending. The full all-games goal
and subsequent winning-character availability remain incomplete.


Final focused verification, observed 2026-11-18 04:06 UTC: **5/5 checks pass,
zero skips**, in 329.39 seconds against the working tree based on `37f6cc45`.
The target was rebuilt before the run. Command (with both installed-data
variables set as above):

```bash
cmake --build build/linux --target opengold_gold_box_games_tests -j 8
ctest --test-dir build/linux -R '^(opengold_game_census|opengold_gold_box_games_tests|opengold_curse_sewers|opengold_curse_route|opengold_original_creation_tests)$' --output-on-failure
```

The full assisted Curse regression passes in 298.19 seconds; earned sewer
progression passes in 30.44 seconds. Creation, format checks and the seven-game
census pass too. Log: `/tmp/opengold-curse-sewers-regression.log`. Scope,
architecture, RAII and diff checks pass; no production code, UI, save schema or
SRD mechanics changed. Investigation, test implementation and focused builds
were interleaved from 03:44 to 04:00 UTC; final checks ran from approximately
04:01 to 04:06. The brief user pause was not separately timed. Completion is
one earned progression segment, not a new winning commercial campaign.

Next bounded outcome: replace the assisted Tilverton opening in this segment
with real movement from the inn through town and the guild to the sewers.
Retain paid equipment, earned progression and all current passing checks.
Pre-existing edits in `docs/GAMES.md` are outside this checkpoint's commit.


Delivery: `b54db7ba` pushed to `origin/main`; the affected target rebuilds in
the main checkout and the registered sewer check passes there again (30.35s).
The main pull was skipped because pre-existing `docs/GAMES.md` edits remain.

### Active next batch: physical Tilverton opening

- Start observed 2026-11-18 04:07 UTC; checkpoint due 05:07 UTC.
- Outcome: reach the sewer progression segment from the actual inn start,
  through town and the guild, without test relocation or direct area entry.
- Acceptance: retain paid starting equipment, legal character creation, original
  script decisions and earned sewer progression. No production-rule, save-format,
  UI or SRD change unless a reproduced in-scope defect requires it.
- Owner/model: same configured Codex session; no delegation or model switch.
- Checks: incremental opening diagnostics, then rebuilt sewer regression and
  relevant existing original-rules checks on final inputs.


Observed 2026-11-18 04:17 UTC: fresh physical opening and earned sewer route
passes with eight fights won, no losses, all six members alive at 31,876 XP,
and both clerics trained to level 6. Navigation now uses the actual service
doorways, accepts the carriage surrender and follows script-owned transfers
through the guild exit. No engine defect was established or production rule
changed. Final guarded regression and save-checkpoint verification follow.


Final opening verification, observed 2026-11-18 04:18 UTC: **5/5 checks pass,
zero skips**, in 35.33 seconds on the guarded rebuilt tree based on `b54db7ba`.
Creation, Gold Box format checks, earned opening/sewers (34.42s), census and
campaign save tests pass. Log: `/tmp/opengold-curse-opening-regression.log`.
The same installed-data environment was used, with
`OPENGOLD_ROUTE_SAVE=/tmp/opengold-curse-earned-opening.sav`; the real checkpoint
contains twelve completed beats and campaign format 53. The full assisted
Curse route passed in the prior checkpoint; its behavior is unchanged here.

No test relocation, direct area entry, extra rewards or forced combat results
remain in this opening/sewer segment. Scope, architecture, RAII and diff checks
pass. Investigation and implementation ran from 04:07 to 04:17, followed by
final verification ending 04:18. This completes the physical-opening batch;
it does not certify the rest of Curse or any additional whole game.


Delivery: `f816a093` pushed to `origin/main`; the affected target rebuilds in
main and the opening/sewer check passes again in 34.38s. Pre-existing GAMES
edits still prevent the requested pull and remain untouched.

### Active next batch: earned first bond

- Start observed 2026-11-18 04:20 UTC; checkpoint due 05:20 UTC.
- Outcome: take the fresh opening party through the actual sewer exit and
  Fire Knives' hideout, break the first bond and reach the wilderness menu.
- Acceptance: actual movement and transfers, earned rewards and gear, legal
  camps/training and real combat wins; first-bond flag set by its event, no
  dead player character. No direct entry, relocation, reward or outcome grants.
- Reuse/model: existing earned route, original rules and configured Codex;
  no delegation, model/runtime change, new UI or save-format change.
- Checks: resume diagnostic from the legitimate opening save, then fresh
  registered route and affected creation/format/save/census regressions.


Observed 2026-11-18 04:24 UTC: fresh first-bond route passes twelve real fights
with zero losses and all six PCs alive at 35,912 XP, after the actual first-bond
event and wilderness transfer. The wizard is unconscious, not dead. A resumed
hideout diagnostic also reaches that event; it reports only its four suffix
fights and therefore does not satisfy the fresh route's minimum fight count.
No production-rule fix was needed. Registered final checks follow after rebuild.


Final first-bond verification, observed 2026-11-18 04:27 UTC: **6/6 checks
pass, zero skips**, in 118.41s on the rebuilt tree based on `f816a093`.
Creation, Gold Box format checks, earned opening/sewers (34.36s), fresh first
bond (83.15s), seven-game census and campaign save checks pass. Command:

```bash
OPENGOLD_GOLDBOX_DIR=/home/edmond/GoldBox OPENGOLD_GAME_DIR=/home/edmond/GoldBox/POOLRAD ctest --test-dir build/linux -R '^(opengold_original_creation_tests|opengold_gold_box_games_tests|opengold_curse_sewers|opengold_curse_first_bond|opengold_game_census|opengold_save_tests)$' --output-on-failure
```

Log: `/tmp/opengold-curse-first-bond-regression.log`; fresh independent trace:
`/tmp/opengold-curse-first-bond-fresh.log`. Scope, architecture, ownership and
diff checks pass; no production code, UI, save format or SRD rules changed.
Investigation/implementation ran 04:20–04:25; registered verification ended
04:27. One requested player milestone is delivered: the first bond earned
from new characters. Curse's ending and the all-games objective remain open.


Delivery: `0cd46cbd` pushed to `origin/main`; affected target rebuilt there,
and the registered fresh first-bond check passes again (83.53s). Main's
pre-existing GAMES edits remain untouched, so no pull was attempted.

### Active next batch: earned Hap progression

- Start observed 2026-11-18 04:30 UTC; checkpoint due 05:30 UTC.
- Outcome: continue the earned party through actual wilderness journeys to
  Hap, defeat four patrols and the barn encounter, retaining all PCs alive.
- Acceptance: no rewards, healing, flags, positions or outcomes injected;
  real camps, cleric cures, legal travel and actual combat wins. Training is
  paid if earned eligibility allows it. Dracandros is a later batch.
- Discovery: optional town dungeons are gated until Hap, Yulash or Zhentil
  Keep is completed, so they cannot fund pre-Hap advancement (cluebook and
  ECL1:80's conditional Search Area menu).
- Reuse/model: existing route, original rules, verified opening save and current
  configured Codex; no delegation, model switch, UI or save-format change.
- Checks: suffix diagnostics, fresh registered route, neighboring earned
  route, creation/format/save/census checks; no stale binary testing.


Checkpoint correction, observed 2026-11-18 04:32 UTC: the first-bond endpoint
at Journey does not export a route save because `can_leave` is false. The
claimed fourteen-beat checkpoint was not written. The verified twelve-beat
opening checkpoint exists and successfully replays the hideout; Hap diagnostics
resume there instead. The first-bond gameplay/flag verification remains valid.


Observed 2026-11-18 04:40 UTC: the Hap suffix passes six fights with no losses,
all six PCs alive at 53,073 XP and both the four-patrol and barn-clear conditions
satisfied. Drow weapons are actual collected/readied loot. The real barn is
(3,13), while the assisted route's (8,4) lies across a scripted wilderness exit.
Verified saved files: twelve-beat opening, twenty-seven-beat Hap arrival and
twenty-nine-beat cleared barn, all campaign format 53. Fresh final checks follow.


Overall checkpoint observed 2026-11-18 04:45 UTC (about one hour after the
03:44 start; brief user pause not separately measured): zero additional whole
commercial campaigns certified complete. Curse's earned opening/first bond
is pushed and passes from new characters; the earned Hap suffix passes, with
fresh Hap and neighboring checks running. All twelve games plus subsequent
winning-character availability remain the objective; Krynn and Buck Rogers
are still unimplemented. No delegation, new runtime, UI or save-format change.
This is a progress checkpoint, not a pause or a completion claim.


Final Hap verification observed 2026-11-18 04:47 UTC: **7/7 checks pass,
zero skips**, in 227.14s on the formatted rebuilt tree based on `0cd46cbd`.
Fresh Hap wins twenty-one real fights with no loss, all six PCs at full HP and
53,073 XP; its registered check takes 108.68s. Earned opening/sewers (34.40s),
first bond (83.16s), creation, formats, seven-game census and saves also pass.

```bash
OPENGOLD_GOLDBOX_DIR=/home/edmond/GoldBox OPENGOLD_GAME_DIR=/home/edmond/GoldBox/POOLRAD ctest --test-dir build/linux -R '^(opengold_original_creation_tests|opengold_gold_box_games_tests|opengold_curse_sewers|opengold_curse_first_bond|opengold_curse_hap|opengold_game_census|opengold_save_tests)$' --output-on-failure
```

Log: `/tmp/opengold-curse-hap-regression.log`; suffix trace:
`/tmp/opengold-curse-real-hap-barn.log`. The staged C++ passed the repository's
format/braces hook before the final rebuild and tests. Scope, architecture,
RAII and diff checks pass. No production code, UI, save format or SRD changes.
Investigation/implementation ran 04:30–04:43, final checks 04:43–04:47.
One earned Hap outcome is delivered; no additional entire game is certified.
The next outcome is paid advancement and the path through the caves/tower to
the second bond, retaining earned progression and actual travel.


Delivery: `f1e9f645` pushed to `origin/main`; the affected target rebuilds and
the fresh Hap check passes there again in 108.43s. Pre-existing GAMES edits
remain untouched and prevent the requested main pull.

### Verified: paid advancement and second bond

- Start observed 2026-11-18 04:52 UTC; checkpoint due 05:52 UTC.
- Outcome: train the earned Hap party through actual halls, return to the
  caves/tower, release the second bond and defeat Dracandros for the Helm.
- Acceptance: six surviving PCs; paid eligible advancement, legal player paths
  and original events; tower flags `4C59 = 1` and `4C60 = 1` after victory.
  No injected rewards, healing, positions, flags or results. Further bonds and
  ending are later batches; no new UI, save format or SRD changes.
- Reuse/model: existing original-rules services, earned route and verified
  twenty-nine-beat Hap save; same configured Codex, no delegation or switch.
- Checks: suffix from the real save, fresh registered route and affected earned
  routes, creation/format/save/census regressions before commit/push.


Observed 2026-11-18 05:45 UTC: paid Shadowdale advancement and the real cave/
roof route pass from the earned prefix, with all six PCs at level 6. Training
must precede the barn: leaving Hap destroys Drow weapons, so the final patrol
and barn replace them after returning. Actual troll loot supplies the mage's
bracers. The stair descent exposed two unsupported Curse DAMAGE forms:
selected `90h/84h` (explosive runes) and whole-party `D0h/00h` (the next trap).
The first has a failing synthetic reproduction, then a passing focused fix;
the real second trap independently reproduces the remaining failure. Curse's
reconstruction (`coab`, revision `9dc46f1`, `engine/ovr003.cs` CMD_Damage)
confirms that bit `10h` keeps full damage even on a save, unlike the later
256-color engines. Core decodes these forms through existing rules hazards;
no damage arithmetic moves into Core. Whole-party coverage is rebuilding.
The second-bond acceptance and 05:52 checkpoint remain unchanged.


60-minute batch checkpoint, observed 2026-11-18 05:52 UTC: the paid advancement,
caves, roof and complete four-stair descent are reached legally with six
surviving level-6 PCs. Both discovered DAMAGE blockers now pass synthetic
coverage and their real tower events. Dracandros is not yet defeated: the
unprepared and first warded boss attempts lose. Final preparation now takes
place at the courtyard door and replenishes planned spells after interruptions;
its Haste casts successfully, but the boss still defeats this party. The
cluebook recommends fourth-level Ice Storm or breaking the wizard's sight;
legal tactics/advancement remain under investigation. No whole additional game
is certified, and winning-character carryover has not begun. Same configured
Codex, no delegation, UI, SRD or save-format changes. The acceptance remains
six survivors plus Helm/Dracandros flags, with no injected progression/results.


Observed 2026-11-18 05:59 UTC: a legal manual-tactic replay defeats Dracandros
and gains his actual loot, with three PC deaths. The six-alive endpoint remains
required. The route now continues through the existing secret passage and
Temple of Sune rather than treating those deaths as permanent exclusions.
It separately counts actual battle defeats and fights with casualties; the
previous `fights_lost` counter conflated them. No actual defeat is accepted.
The first exit attempt is defeated by remaining wanderers before reaching the
temple; resting at the cleared courtyard before travel is rebuilding. Boss
flags and a wounded/three-dead roster are verified in the actual sixty-one-beat
save `/tmp/opengold-curse-temple-route-61.sav`; this is not a passing endpoint.


Observed 2026-11-18 06:06 UTC: the recovery suffix passes from the actual
sixty-two-beat return save. The general store at (2,5) west identifies the old
bracers for 200 gp and buys them for 6,750 gp. Sune then receives 5,500 gp for
each of three Raise Dead services. All six PCs finish alive, the two boss/Helm
flags pass, and the actual sixty-four-beat format-53 save exists at
`/tmp/opengold-curse-second-bond-all-six.sav`. This closes the suffix's recovery
gap without reducing the six-alive acceptance. Fresh complete verification,
format checks, affected regressions and the game rebuild follow before delivery.


Observed 2026-11-18 06:21 UTC: the formatted rebuilt tree based on `f1e9f645`
passes **24/24 focused checks, zero skips**, in 379.45s. Fresh second-bond
progression wins 47 real fights, with no battle defeat and one fight costing
three lives; paid Sune services return all six PCs alive. Its registered check
takes 264.43s. First bond (120.20s), Hap (151.56s), sewers (52.97s), the existing
assisted ending route, damage/creation/rules, save/rollback/camp, shop/sale,
temple/training/census and Godot Curse startup checks pass. The game is exported
through `./build.sh`; no C++/CMake inputs changed after the final rebuild.

The full repository suite is being covered without repeating the 24 completed
checks: 317 remaining tests run against those same binaries. The exact name
sets are in `/tmp/opengold-curse-suite-coverage.json`; focused build/test output
is `/tmp/opengold-curse-second-bond-integrated.log`, full focused traces are
`/tmp/opengold-curse-second-bond-focused-ctest.log`, and the remaining run is
`/tmp/opengold-curse-second-bond-remaining-suite.log`. A separate read-only
inventory finds 24 DAMAGE calls in all Curse scripts, using 13 constant forms;
all are accepted by the current dispatcher. This is no claim of an additional
whole-game completion or of winning-character carryover.


Final integrated verification observed 2026-11-18 06:37 UTC: **341/341 unique
registered checks pass, zero failures and zero skips**, including 171 Godot
checks. The remaining 317 complete in 1,253.65s; Darkness's ending route takes
1,237.97s, Treasures' map walk 1,064.10s, and the final serial Slums UI check
8.73s. The name-set comparison against `ctest --show-only=json-v1` proves the
focused 24 plus remaining 317 cover every registered check. Remaining full
traces are saved in `/tmp/opengold-curse-second-bond-remaining-ctest.log`.

The tested working tree is based on `f1e9f645`, with this commit's C++/CMake
changes; only verification documentation changed after rebuilding. Scope,
architecture, RAII, save compatibility and staged diff checks pass. The
repository's LLVM-23 format/braces hook passes. The export completes; its
nonfatal Godot editor window/debugger diagnostics do not reproduce in the
packaged-game smoke check or the 171 runtime checks.

To repeat the complete build and registered coverage in one invocation:

```bash
OPENGOLD_GOLDBOX_DIR=/home/edmond/GoldBox OPENGOLD_GAME_DIR=/home/edmond/GoldBox/POOLRAD JOBS=8 ./build.sh --test-all
```

The actual run first used `./build.sh --test=original_rules_tests,original_creation_tests,original_modify_tests,original_combat_tests,gold_box_games_tests,curse_route,curse_sewers,curse_first_bond,curse_hap,curse_second_bond,game_census,ecl_tests,kutos_well_tests,combat_rollback_tests,campaign_rest_tests,campaign_recovery_tests,original_campaign_tests,save_tests,shop_sale_tests,shop_data_tests,temple_tests,training_hall_tests,godot_default_party_curse_original`,
then `ctest --test-dir build/linux -j8 --output-on-failure --exclude-regex`
with the anchored union of the 24 passed names in the coverage JSON above.
Both used the installed-data environment shown above and the same binaries.

Observed phases: investigation/implementation 04:52–06:06, formatting/review
06:06–06:07, integrated build/focused checks 06:07–06:15, remaining suite
06:15–06:37 UTC. The 05:52 checkpoint was recorded on time. Same configured
Codex, no model/effort override or delegation; cost/token data unavailable.
One earned second-bond outcome and its two engine blockers are delivered;
no additional whole game is certified. Next is earned advancement and Moander,
with all twelve games and subsequent winning-character availability still open.


Delivery: `bfd1b1e3` pushed to `origin/main`; main matches origin. The
pre-existing GAMES edits remain untouched, so the main pull is skipped under
the dirty-checkout guard. The required main game rebuild and all four selected
checks pass again (damage/Gold Box units, Godot preparation/Curse startup and
packaged Linux smoke) in 19.89s. Log:
`/tmp/opengold-curse-second-bond-main-recheck.log`. The session claim is released.

### Earned advancement and Moander (delivered, 2026-11-18)

- Start observed 2026-11-18 06:45 UTC; checkpoint due 07:45 UTC.
- Outcome: advance eligible earned characters through paid halls, travel into
  Yulash, recruit the actual companions, enter the Pit of Moander and win the
  Gauntlet through the original battles and player-accessible paths.
- Acceptance: Gauntlet `4C5B = 1` and Moander victory `4C2D = 1`; all six PCs
  retained in the party, at least one PC standing and zero actual battle
  defeats. Casualties are reported, with paid recovery where needed/affordable;
  the six-alive second-bond check remains unchanged. No injected XP, money,
  healing, positions, flags or results. Further bonds/ending remain later work.
- Reuse/model: existing earned checkpoint at beat 64, original-rules hall,
  travel, companions and combat; same configured Codex, no model/effort override
  or delegation. No UI, save format, runtime or SRD changes planned.
- Checks: real earned suffix, fresh registered milestone and affected earned
  neighbors/format/save/companion regressions before commit/push. Current shared
  engine baseline is the 341-check verified `bfd1b1e3` delivery above.

Observed 07:37 UTC: the earned suffix reaches Yulash, obtains the real Wand of
Defoliation, enters the Pit and recruits Alias and Dragonbait. The reachable
stair is `(7,14)` on the western half of map 17; script 18 retains that map
and uses its eastern half. Its altar is event 12 at `(11,3)/(12,3)`.
Two player-facing blockers are corrected in the working tree: healing cast
while a script menu retains its character record is no longer overwritten by
the next LOAD CHARACTER, and valid lower-floor saves (map 17/script 18) reload.
The existing format-53 earned beat-95 checkpoint now decodes; the independent
installed-data save regression passes (`/tmp/opengold-curse-pit-save-unit4.log`,
0.46s). The healing regression was proved red and then green. Save format and
rules calculations are unchanged. The route still needs a winning prepared
Mogion/Moander chain; no third bond or whole additional game is certified yet.

60-minute checkpoint, observed 07:45 UTC: same owner/configured Codex, no
model/effort override or delegation; cost/token data unavailable. The two
engine regressions pass, including actual lower-floor save restoration. The
earned suffix obtains the wand and companions, rests in room `(13,6)` rather
than the guaranteed-attack altar corridor, and casts real Bless, Enlarge,
Prayer and Haste. Its saved prepared party at beat 99 is valid. Targeted
Fireball/holds improve the Mogion battle, but the retained party still loses
to the pseudopods. Acceptance remains open; no milestone is counted yet.
Failed diagnostic routes and build logs remain in `/tmp/opengold-curse-*`;
the current prepared checkpoint is `/tmp/opengold-curse-room-prepared-99.sav`.
The next bounded step is the surviving party's legal Moander tactics, followed
by fresh-route and integrated checks; further bonds and carryover remain later.

Observed 08:27 UTC: the fresh registered third-bond route passes in 364.64s:
56 retained victories, zero battle defeats, two fights with casualties, all
six PCs retained, Gauntlet and Moander flags earned. The mage stands at 6/41
HP, Dragonbait at 50/50; the paladins/ranger/clerics need recovery. The tested
Core and game build pass 341 of 342 broad checks, including all 170 checks
named `opengold_godot_*` and the separate packaged Linux smoke. No checks skip.
Earlier summaries reported 171 Godot checks; current registration proves
170 named Godot checks plus the separate package smoke.
Full trace: `/tmp/opengold-curse-moander-integrated-ctest.log`; results and
registration names agree in `/tmp/opengold-curse-moander-suite-coverage.json`
and `/tmp/opengold-curse-moander-registered.json`.

The sole broad failure is the assisted Curse ending route: new Mogion tactics
changed its later party and it lost in Dexam's caves. Those tactics are now
restricted to earned third-bond progression, preserving the older route's
choices. This last correction changes only the test player; Core/game inputs
remain the broadly tested ones. Format/braces pass, the affected native target
is rebuilt, all six shorter Curse/Gold Box checks are rerunning, and a second
fresh earned third-bond run will retain its actual checkpoint for continuation.
Logs: `/tmp/opengold-curse-moander-corrected-curse.log` and
`/tmp/opengold-curse-moander-corrected-earned.log`. Acceptance is still open
until these final affected checks pass; this is not a whole-game certificate.

Final verification observed 08:34 UTC: the corrected native target passes all
six selected checks in 397.75s, including the assisted ending route (397.74s),
the six-alive second bond (325.16s), Hap, first bond, sewers and Gold Box units.
The independent second fresh third-bond replay also passes: 56 fights, zero
defeats, two fights with casualties, actual Gauntlet and Moander flags, all six
PCs retained. It reproduces the registered run's endpoint: mage 6/41 and
Dragonbait 50/50 standing; paladin 2 and ranger dead; paladin 1, both clerics
and Alias unconscious. It saves the actual format-53 earned party at beat 101
to `/tmp/opengold-curse-third-bond-earned.sav` and
`/tmp/opengold-curse-third-final-101.sav`. Earlier legal checkpoints are retained
under that prefix, including the side room and prepared altar doorway.

Every one of the 342 registered checks is covered by the broad run and final
affected replays, with no remaining failure or skip. The final test-player-only
scoping correction leaves the broadly tested Core, game and other-game paths
unchanged; third-bond behavior is independently replayed from a fresh party.
`/tmp/opengold-curse-moander-final-coverage.json` records that comparison.
The full game is rebuilt and exported, LLVM-23 format/braces and staged diff
checks pass, and scope/architecture/RAII/save-history review passes. The tested
tree is based on `bfd1b1e3` plus this delivery's changes. The user's pre-existing
`docs/GAMES.md` edits remain unstaged and untouched.

Observed phases: investigation/implementation 06:45–07:53, format/review
07:53–07:54, full game build 07:54–07:59, broad checks through 08:24
(1506.10s), test-player scoping/rebuild 08:24–08:26, final affected checks
08:26–08:34. The 07:45 checkpoint was recorded on time. Same configured Codex,
no model/effort override or delegation; cost/token data unavailable. One earned
Moander milestone and two engine blockers are delivered; zero additional whole
games are certified. Next: earned Pit escape/recovery, then the remaining
bonds/ending; all twelve-game certification and subsequent winning-character
availability remain open.

Delivery: `96a47c98` is pushed to `origin/main`; main and origin match. The
main game is rebuilt/exported after the push, and all four selected checks
pass in 20.46s (Gold Box units, Godot preparation, Curse Original startup and
packaged Linux smoke). Log: `/tmp/opengold-curse-moander-main-recheck.log`.
The main pull is skipped because the user's pre-existing GAMES edit remains
dirty, as required by the checkout guard. That file retains its original
`d5bf49897b2dddf631b07a2afae4692bd8193c6f` content hash. The claim is released.

### Active next batch: earned Pit escape and recovery

- Start observed 2026-11-18 08:39 UTC; checkpoint due 09:39 UTC.
- Outcome: recover the earned Moander party through actual companion/clerical
  healing, walk out of both Pit levels, win the real exit encounters, travel
  to Voonlar and pay its temple/hall for recovery and eligible advancement,
  then return to visited Yulash for an idle, reloadable continuation checkpoint
  (the pending wilderness city menu cannot be saved). The original invasion
  closes Yulash after the Pit escape and redirects entry to the visited upper
  Pit entrance; that actual entry is the continuation checkpoint.
- Acceptance: Gauntlet/Moander victory retained; all six PCs alive after paid
  recovery, at least one PC standing, Alias/Dragonbait leave through their
  original departure script; mage and both clerics reach at least level 8
  through paid eligible training; zero retained battle defeats. No injected
  XP, money, healing, flags, positions or combat outcomes. Further bonds and
  the ending remain subsequent work.
- Reuse/model: actual beat-101 earned checkpoint from the verified fresh run;
  existing camp actions, travel, temple, hall and physical walking. Same
  configured Codex, no model/effort override or delegation. Original Rules work;
  no SRD, new UI, runtime or save-format changes planned.
- Checks: earned recovery suffix, fresh registered recovery milestone and
  affected Curse neighbors before commit/push; baseline is the 342-check
  passing coverage and post-push main smoke recorded above.

Observed 09:06 UTC: the resumed earned route physically clears the upper Pit
and leaves with zero retained battle defeats. Actual Enlarge, Bless, Prayer
and Haste preparation, short steps with Search off, and rests in cleared rooms
allow the weakened party to pass the slug/mound rooms and final guards.
Alias and Dragonbait depart through the original script. Voonlar's shop pays
18,750 gp for the actually collected Ring of Wizardry, funding both 5,500 gp
resurrections and eligible 1,000 gp hall training. The first suffix replay
raises both fallen PCs and trains the mage and ranger to level 8; it correctly
refuses the unconscious clerics. Paid Cure Light Wounds is now selected for
living unconscious casualties before requesting their hall training. Final
suffix and fresh-route verification remain open. No whole-game win is claimed.

Final verification observed 09:17 UTC: the fresh registered recovery milestone
passes in 431.64s, with 59 retained victories, zero battle defeats and three
fights with casualties. All six PCs are alive and standing after actual paid
recovery. The mage, both clerics and ranger train to level 8; the paladins
remain level 7. Original exit-guard, departure and reentry state transitions
are checked, and neither companion remains in the active party. The earned
format-53 checkpoint is `/tmp/opengold-curse-moander-recovery-earned.sav`
(133 completed beats, upper Pit entrance at `0,12`); an independent load
passes the same endpoint acceptance. The fresh beat-101 save is byte-identical
to the prior verified Moander checkpoint. No injected progression is used.

Gold Box units, the existing assisted Curse ending and the fresh third-bond
regression also pass on the final formatted native target. The third-bond
neighbor retains 56 victories, zero defeats and two casualty fights. Current
registration has 343 checks; this delivery adds one recovery check and leaves
production Core/rules/game inputs unchanged from `96a47c98`'s broad coverage.
Logs: `/tmp/opengold-curse-moander-recovery-fresh.log`,
`/tmp/opengold-curse-moander-recovery-assisted.log`,
`/tmp/opengold-curse-moander-recovery-third-neighbor.log`,
`/tmp/opengold-curse-moander-recovery-unit.log` and
`/tmp/opengold-curse-moander-recovery-earned-reload.log`.

Observed phases: investigation/route implementation 08:39–09:07, final
format/braces and native rebuild 09:07–09:08, suffix/reload and fresh/neighbor
checks 09:08–09:17. Final scope, architecture, RAII and compatibility review
passes; no production API, UI, runtime, rules or save-history changes. Only
the test route/player, CTest registration and verification docs change. Same
configured Codex, no effort/model override or delegation; cost/token data
unavailable. The 60-minute checkpoint is not due yet. Push and required main
game rebuild/smoke remain before delivery. Zero additional whole campaigns
are certified; next are the remaining Curse bonds and ending, then the other
game gaps, with winning-character availability still following all games.

Delivery observed 09:20 UTC: `3778786d` is pushed to `origin/main`, with main
and origin matching. The main game is rebuilt/exported and all four installed
checks pass in 19.83s: Gold Box units, Godot preparation, Curse Original startup
and packaged Linux smoke. The first post-push command omitted the installed
environment and skipped startup; the corrected command explicitly sets both
Gold Box paths and has no skips. Log:
`/tmp/opengold-curse-moander-recovery-main-installed-recheck.log`.
The main pull is skipped under the dirty-checkout guard; the pre-existing
GAMES edit remains untouched at hash
`d5bf49897b2dddf631b07a2afae4692bd8193c6f`. The recovery claim is released.

### Active next batch: earned Zhentil Keep and Dexam

- Start observed 2026-11-18 09:20 UTC; checkpoint due 10:20 UTC.
- Authorization: the resumed all-twelve-games task remains active; no questions
  or delegation. Outcome: physically leave the earned recovery checkpoint,
  travel to Zhentil Keep, follow Olive and Dimswart through the Shrine of Bane,
  enter the Caves of the Beholder, defeat Dexam and earn the Amulet, then leave
  through the actual exit and recover living casualties with actual camps.
- Acceptance: retain six PCs and the three earned artifacts; actual Dexam and
  Amulet progression, wilderness exit reached and a reloadable continuation
  saved in a previously visited district; at least one living PC standing and
  zero retained battle defeats. Dead PCs may require the next paid recovery;
  no injected XP, money, healing, flags, positions or combat results. Final
  Myth Drannor/ending and further games follow this batch.
- Reuse/model: existing physical walker, original scripts, camp preparation,
  temples/halls and battle player; actual 133-beat earned recovery save. Same
  configured Codex, no requested switch, effort override or delegation. This
  remains Original Rules work outside SRD model assignments; no UI/runtime or
  save-history changes planned.
- Checks: focused earned suffix, fresh registered milestone, affected Curse
  neighbors, save reload and required post-push installed-data game smoke.
  Baseline is `3778786d`'s verified recovery plus the existing broader coverage.

Observed 09:27 UTC: the earned party exits the Pit, heals through real
wilderness camps, travels by trail to Zhentil Keep and follows Olive into
the Shrine. Her entry is `(7,14)`; the reachable Dimswart cell is `(7,15)` and
the nearby hooded-woman event is `(4,10)`. The assisted route's instances at
`(1,7)` and `(2,3)` are in a disconnected part of the shared geometry.
The actual earned Shrine save at beat 138 also reproduces a production
reload failure: `Saved district and script disagree`. Script 33 uses NEW ECL
and LOAD PIECES without loading a new map, retaining city map 32. Its shared
map metadata is now recorded alongside the existing lower-Pit case, with an
installed-data round-trip regression for both. This is a loader metadata
repair within the required playable/savable route; no format/history change.
The unmodified actual Shrine save will be reloaded after the affected build.

Observed 09:35 UTC: the metadata repair reloads the unmodified earned Shrine
save, and the shared-map unit regression passes both Pit and Shrine round
trips. The first cave approach then failed after seven consecutive fights:
it tried to reach `(4,12)` before resting, but the physical path crosses
Dexam's `(2,11)` lair before that point. The corrected player prepares in
Dimswart's quiet cell, leaves the altar's guaranteed-interruption rectangle,
rests/prepares at `(7,5)` and the cleared ogre room `(9,5)`, then prepares at
`(2,10)` immediately north of Dexam. Actual Enlarge on the three warriors,
Bless, Prayer and Haste carry through the short approaches with Search off.

The corrected suffix passes from the earned Dimswart save: 11 real victories,
zero defeats and zero casualty fights. Dexam's actual Amulet event runs,
followed by the cave exit, wilderness recovery and return to the visited Pit.
All six PCs are alive and at full HP at beat 169, with the three artifacts
retained. The actual diagnostic checkpoint is
`/tmp/opengold-curse-fourth-prepared-diagnostic.sav`; fresh complete milestone
and integrated verification remain open. No new whole-game win is claimed.
LLVM-23 format/braces passes; the full game is rebuilt/exported and the final
installed-data integrated checks are running under
`/tmp/opengold-curse-dexam-integrated.log`.

Final verification observed 09:46 UTC: all 17 selected integrated checks pass
in 618.28s, with no failures or skips. They cover every registered Curse walk,
ending and earned milestone, Gold Box units (including both shared-map saves),
Original campaign and save units, saves by game, Curse Original/SRD startup,
Godot preparation and packaged Linux smoke. The new fresh registered fourth
bond passes in 498.38s: 70 victories, zero defeats, three casualty fights from
the earlier campaign, all six PCs alive at full HP, all three artifacts retained.
All 169 beats use earned progression and physical movement. The paladins end
at levels 7, the ranger/clerics/mage at 8; further eligible training is pending.

The separate actual earned suffix checkpoint, now preserved as
`/tmp/opengold-curse-fourth-bond-earned.sav`, independently reloads and passes
final endpoint acceptance on the final formatted native target. It is an
earned continuation from the previously saved Dimswart route, not an encoding
of the fresh CTest run. Both end at the upper Pit's `(0,11)` with full HP and
the same artifact states; their earned XP differs slightly. Fresh trace:
`/tmp/opengold-curse-dexam-fresh-final.log`; complete integrated trace:
`/tmp/opengold-curse-dexam-integrated-ctest.log`; reload:
`/tmp/opengold-curse-fourth-final-reload.log`.

Current registration has 344 checks; this is a 17-check affected regression
pass, not a claim of a new full 344-check run. The production repair only
records the valid Curse map-32/script-33 pairing. Scope, architecture, RAII,
save-history and staged-diff review pass; LLVM-23 format/braces passes. The
tested tree is based on `3778786d` plus this delivery. The user's GAMES edit
remains untouched at its recorded hash.

Observed phases: investigation/implementation 09:20–09:34 (reachable Shrine
events, actual save failure, then corrected cave preparation), final
format/build 09:34–09:35, integrated checks through the 09:46 observation.
The 60-minute checkpoint is not yet due. Same configured Codex, no model/effort
override or delegation; cost/token data unavailable. One earned fourth-bond
milestone and one save blocker are verified; zero additional whole games are
certified. Commit/push and required main rebuild/checks remain before delivery.
Next is Curse's earned Myth Drannor/ending, followed by other game gaps and,
after every game is winnable, winning-character availability.

Delivery observed 09:49 UTC: `47cb08a0` is pushed to `origin/main`; main and
origin match. The main game is rebuilt/exported and all four post-push
installed-data checks pass in 19.76s (Gold Box units, Godot preparation,
Curse Original startup and packaged Linux smoke), with no skips. Log:
`/tmp/opengold-curse-dexam-main-recheck.log`. The dirty-main guard again skips
the pull; the pre-existing GAMES edit stays untouched. The Dexam claim is released.

### Active next batch: earned Curse campaign ending

- Start observed 2026-11-18 09:49 UTC; checkpoint due 10:49 UTC.
- Authorization: resumed all-twelve-games task remains active; no questions or
  delegation. Outcome: take the earned three-artifact party through actual
  paid eligible training and preparation, the Standing Stone, Burial Glen,
  Myth Drannor's ruins and temple, defeat Tyranthraxus and reach the actual
  campaign ending through physical movement and original scripts.
- Acceptance: a fresh Original Rules whole-campaign route earns its rewards,
  equipment and advancement, retains six PCs, wins Tyranthraxus's actual
  battle and shows the game's real winning event with at least one living PC
  standing and zero retained battle defeats. No injected XP, money, healing,
  time, flags, positions or outcomes. A continuation from the actual earned
  beat-169 save is diagnostic; fresh completion is required for certification.
  Optional quests and other-party/difficulty coverage remain outside this
  certificate. Other games and automatic winning-character availability follow.
- Reuse/model: existing original camp/shop/hall actions, expedition player,
  manual ending tactics and physical walker; actual earned fourth-bond save.
  Same configured Codex, no requested switch, model/effort override or agents.
  Original Rules work outside SRD model assignments; no new UI/runtime or
  save-format/history changes planned.
- Checks: earned ending suffix, fresh registered whole campaign, affected Curse
  neighbors and relevant save/startup/package checks; final review and
  commit/push followed by the required main rebuild/installed-data smoke.
  Baseline is `47cb08a0`'s 17-check integrated pass and four post-push checks.

Observed 09:58 UTC: the ending route's first diagnostic stopped while choosing
the fireball wand. Its new Identify helper had copied an inventory span rather
than its values; the first paid identification replaced that borrowed view.
The helper now takes an owned value snapshot before any shop mutation, and
the real paid services identify both Ice Storm and Fireballs. No production
inventory change was needed. The replay readies the actually identified
fireball wand and pays for mage level 9 and both paladins' level 8 training.
It is continuing toward the Standing Stone and Myth Drannor. Fresh ending
verification remains open; no new whole-game certificate is counted yet.

Observed 10:03 UTC: the earned continuation reaches Tyranthraxus and the actual
winning event in 12 retained victories, zero battle defeats and one casualty
fight. Four PCs stand at the end; paladin 2 and cleric 5 are dead, with all six
still in the party. Actual winner save:
`/tmp/opengold-curse-ending-identified-187.sav` (also the diagnostic final save).
The normal loader decodes it and reencodes it byte-identically to
`/tmp/opengold-curse-winning-roundtrip.sav`. The diagnostic route validator
returns false on that terminal-only resume because no new win notification is
played; the persisted party/map/dialogue are intact. This is a save round-trip
probe, not a new campaign play or a failing registered check.

The whole-campaign certificate still requires the fresh registered ending run.
LLVM-23 format/braces passes and the final game is rebuilt/exported. Seven
affected integrated checks are running under
`/tmp/opengold-curse-earned-ending-integrated.log`, including the new fresh
ending, the existing assisted ending, the prior fourth bond, Gold Box units,
Godot preparation/Curse Original startup and package smoke. No production,
rules, UI or save-format changes have been needed in this batch.

Final verification observed 10:12 UTC: the fresh registered whole campaign
passes in 545.33s. All 187 beats use creation's actual starting resources,
earned rewards/equipment, paid eligible training and real movement. The
party wins 84 retained battles with zero defeats and four casualty fights,
defeats Tyranthraxus and sees the actual winning event. All six PCs remain;
paladin 1 (7/108), ranger (25/98), cleric 4 (67/67) and mage (52/52) stand.
Paladin 2 and cleric 5 are dead. Warriors/clerics are level 8 and mage level 9.
This certifies the named fresh Original Rules party through Curse's actual
ending; it does not certify every optional quest, party or difficulty.

All seven affected integrated checks pass in 545.35s with no skips: the new
fresh campaign, existing assisted ending, fresh fourth bond, Gold Box units,
Godot preparation/Curse Original startup and packaged Linux smoke. Complete
trace: `/tmp/opengold-curse-earned-ending-integrated-ctest.log`; fresh campaign:
`/tmp/opengold-curse-earned-ending-fresh-final.log`. The separately retained
earned suffix winner save at beat 187 round-trips byte-identically, as recorded
above; it is not an encoding of the fresh CTest run. Registration now has
345 checks; this delivery's final regression pass selects seven, with the
previous batch's 17-check integrated and post-push coverage still recorded.

Observed phases: route/diagnostic work 09:49–10:00 (including correction of
the new Identify helper's borrowed view), format/braces/final build
10:00–10:02, integrated checks through the 10:12 observation. Review passes
scope, architecture, RAII, compatibility and staged-diff checks. The tested
tree is based on `47cb08a0` plus this delivery; production Core, rules, UI and
save format/history are unchanged. The user's GAMES edit remains untouched.
Same configured Codex, no effort/model override or delegation; cost/token data
unavailable. The 60-minute checkpoint is not due. Commit/push and required
main rebuild/checks remain before delivery.

One additional whole commercial campaign is verified: Pool and Curse now have
unassisted completion evidence. Other commercial routes, Krynn/Buck Rogers
implementation and arbitrary UA designs remain as scoped above. Automatic
availability of every winning party member follows all-game winnability.
Next is Secret's actual starting party and earned progression.
