# Arcane Recovery — #99

## Delivered outcome

Runtime/test revision `642c7a1`, SRD module 0.6.49, implements Wizard Arcane
Recovery through attained levels 1–4. After a completed Short Rest, the selected
eligible Wizard can recover expended slots using the approved dropdown and
Recover slots button. A valid nonempty choice spends the entire feature use;
Long Rest restores it. Finish/Escape without use preserves availability.

The standing SRD goal authorized this bounded batch; AR-1 approved its new shared
game/demo controls on 2026-09-25 at 19:42 UTC. One owner, #99 only, no new issues,
agents or tasks. No player combat saving. Scholar, Ritual Adept, Evoker, new spell
catalogs/book/preparation systems, other classes and levels above four remain
outside this delivery. This is not Wizard class completion or full-goal completion.

## Rules and architecture

[SRD 5.2.1, printed page 78](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf):
level-one Wizard feature; on finishing a Short Rest, choose expended slots with
combined levels at most half Wizard level rounded up, never level six or higher;
one use per Long Rest. Supported levels 1–2 can recover one level-one slot;
levels 3–4 can recover one/two level-one slots or one level-two slot. Choosing
less than the available budget still consumes the use.

The statically linked SRD library owns fixed grant provenance, eligibility,
legal opaque choice IDs, slot arithmetic, resource capacity/recharge and encoded
continuation. Core validates the completed-rest ticket and member eligibility,
then commits an owned candidate party state atomically. Core contains no Wizard
or spell-slot arithmetic. Godot displays rules-provided options and submits the
chosen ID; both applications use `rest_dialog_impl.h`.

AR-1's row appears above Result, shortening the scrollable Info area only while
shown. Non-Wizards have no row. No legal allocation or a spent use disables both
controls. Standard Godot controls preserve keyboard access; the result states
what was restored. Existing camp/inn saves retain both unused eligibility and
committed expenditure. There are no combat saving controls.

## Persistence and acceptance evidence

The PC42 profile carries the fixed grant; SRD10 records spent Arcane Recovery.
The combat checkpoint retains the use alongside ordinary or physical inventory
mode. See [saves](SAVES.md#pre-10-format-policy).

| Requirement | Evidence |
| --- | --- |
| Starting grant and attained levels 1–4 | Feature-grant tests, ordinary advancement, newly created level-three Wizard in the real UI. |
| Exact choices/costs/recharge | `arcane_recovery_tests.cpp`: each supported allocation at every level, over-budget/full-pool rejection, whole-use cost, Short/Long Rest differences. |
| Transaction safety | Stale tickets, nonmembers, invalid IDs, repeated use and sleeping eligibility reject without changing campaign bytes; HP, gear, Hit Dice, time and RNG remain unchanged. |
| Campaign/combat persistence | Canonical save/reload, ordinary level-four advancement, both combat inventory modes and exact next-cast continuation preserve expenditure. |
| Other rest/effect interactions | Qualified interrupted Long Rest benefits work; Ray of Frost still expires in SRD10 without refreshing the feature or consuming RNG. |
| Player path | Shared runtime check creates/advances a Wizard, completes rest, saves/reloads before and after recovery, uses keyboard dropdown/button, declines with Escape, rejects repeat use, and verifies non-Wizard visibility. |
| Layout/localization | Game English/Spanish and the existing English demo rendered at 1120×800 and 1920×1080; choices and result states inspected. Demo localization remains its existing English-only presentation. |

## Final verification

All build inputs were finished before their builds. Final tested revision:
`642c7a1` (native foundation `bc948e7`).

- Rebuilt all 49 native test targets and `opengoldbox_test_project` in
  `build/mac-check`; rebuilt `opengold_godot` in `build/sprite-demo`.
- `ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6`
  — **51/51 pass**, 13.29 seconds, `/tmp/arcane-integrated-native.log`.
- `ctest --test-dir build/mac-check --output-on-failure -R '^opengold_godot_' --fixture-exclude-setup godot_project`
  — **26 runtime checks pass**, 43/43 entries including setup/native prerequisites,
  38.57 seconds, `/tmp/arcane-integrated-godot.log`.
- Six graphical runs of the shared `--rest-check`, with game EN/ES and demo EN
  at both sizes, each returned success and the expected completion marker.
  Captures: `/tmp/arcane-ui-captures/{game-en,game-es,demo-en}-{1120,1920}/{70,80}.png`;
  logs: `/tmp/arcane-render-*.log`.
- `python3 tools/localization.py --check`: **902** English/Spanish messages valid.
- `git diff --check`: clean. Scope, RAII and static-library boundary reviewed.

Godot editor import returned success with window/debugger/timer diagnostics;
subsequent runtime and graphical checks passed. Existing slot-pool labels in the
Spanish details remain untranslated; new recovery labels/options/result are
translated. This pre-existing localization gap is not new scope for this batch.

## Timing and routing

Requested `gpt-6-astra` / `high` retained for rest/persistence interactions; actual
configuration not independently verified, no model change or delegation.

| Phase | Observed UTC on 2026-09-25 | Outcome |
| --- | --- | --- |
| Batch start | 18:57:23 | Frozen scope and pending AR-1; checkpoint 19:57:23, maximum 20:27:23. |
| Native verification | By 19:23:42 | 51/51 pass; exact prior-profile/grant expectations updated. Effect-timer omission caught and fixed. |
| Existing application compatibility | 19:25–19:26:56 | Both builds and previous shared rest controls pass. |
| Input dependency | Native checks complete until approval at 19:42 | Remaining UI required AR-1; no unrelated batch started. |
| Approved UI integration | 19:42:20–19:52:41 | Controls, keyboard/save checks, final regressions and six visual runs complete. |

Acceptance verification ended 55m18s after the original start, including time
awaiting approval. Exact earlier capture/implementation phase endpoints were
not recorded; no invented phase durations or token-saving claim. Keyboard test
routing needed two corrections: existing popup tests route through the root
viewport. No production control workaround or reduced acceptance was used.
One original requirement delivered; no new issues or expanded scope. Continue
the full SRD goal after delivery rather than treating this issue as completion.
