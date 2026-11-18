# Installed-game playability verification

## Acceptance and scope

The user authorized fixing all discovered Linux test and playability errors,
including pre-existing errors. Scope is the seven supported games using the
installations in `/home/edmond/GoldBox`. Krynn and Buck Rogers are not supported
yet; arbitrary third-party Unlimited Adventures designs are not certified by
one generated test design.

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
| Curse of the Azure Bonds | Assisted ending route wins its fights; fresh earned opening, first bond and Hap now verified | Replace injected import experience/gold and relocations with verified gameplay/import |
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
