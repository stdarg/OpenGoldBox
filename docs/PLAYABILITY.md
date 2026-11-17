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
| Pool of Radiance | Original-rules opening/combat/training and SRD whole-map ending tests | Original-rules whole campaign; remove whole-map level grants and relocations |
| Curse of the Azure Bonds | Route reaches the ending and wins its fights | Replace injected import experience/gold and relocations with verified gameplay/import |
| Secret of the Silver Blades | Fresh complete route reaches the actual ending with every fight played, including Oswulf, the Dreadlord and the soul gem; zero forced wins | Replace progression grants and relocations with verified gameplay/import |
| Gateway to the Savage Frontier | Route reaches the ending and wins its fights | Replace experience/gold/equipment/time grants and relocations |
| Pools of Darkness | Assisted route reaches the ending; final protective spells verified in actual combat; palace wandering/rest hang reproduced and fixed | Remaining forced fights, including the finale; progression assistance and relocations |
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

## Reproduction and evidence

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

Darkness's finale still defeats the replay party with forced wins disabled.
The actual initial effects include Resist Cold on all six members, Mind Blank
on the three casters, Haste and a globe on the magic-user. An experimental
Meteor Swarm aiming heuristic also lost and was removed. These losses establish
an unresolved replay/playability gap, not that the native game cannot be won.
No combat balance change or invented victory was made to conceal the result.

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
