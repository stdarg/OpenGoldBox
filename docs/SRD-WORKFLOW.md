# SRD implementation workflow

Use [the handoff](SRD-HANDOFF.md) as the entry point, [the issue index](https://github.com/stdarg/OpenGold/issues/186)
as the backlog, and [coverage](SRD-COVERAGE.md) as completion evidence. Keep the
[full implementation plan](SRD-IMPLEMENTATION.md), all twelve classes and later
level/multiclass milestones intact. Efficiency changes execution, not acceptance.

## One delivery batch cycle

1. Read the short handoff, check branch/worktree state, then inspect the active
   issues and relevant comments. Use the repository map to locate code. Read
   TECH and relevant requirements once per context; re-read only changed or
   uncertain sections. Full backlog snapshots are for planning/reconciliation,
   not every implementation turn.
2. Fill the handoff card before coding: named outcome, original issues, immutable
   acceptance rows, source/level matrix, real dependencies, decisions and checks.
   Select a bounded package from the batching review, not an entire work family.
   Each requirement has one implementation owner; related tickets share evidence.
   Freeze included outcomes and exclusions. Prefer several features using an
   existing rules mechanism and approved controls over a broad infrastructure
   project. Selecting a batch does not authorize expanding its requirements.
3. Resolve related UI/policy questions together using the decision register.
   Existing approval is reusable only within its recorded scope. Keep dependent
   work pending when an answer is required; do useful independent work inside
   the batch. A new dependency outside accepted scope requires explicit approval
   before implementation; otherwise defer it. At most one approved prerequisite
   may be active with a return path. Do not ask again for routine choices or
   controls already covered by a recorded approval.
4. Plan persistence across the batch. Distinguish schema changes, semantic
   changes and data additions. Reuse valid records; version when rejection or
   continuation requires it. Capture actual prior-writer evidence before changing
   the writer. Preserve every supported released format and its required tests.
5. Implement a real player path, including all named grants/routes. No speculative
   helper or abstraction without an immediate consumer. A separate state machine
   may warrant a reviewable substep, not automatically another GitHub issue.
6. Finish the current edits before building; never modify inputs of a running
   build. Run focused checks as implementation evolves. Run required broader checks on
   the final integrated tree. Repeat after relevant changes/failures or unresolved
   risk, not routinely. Build before testing; record the tested revision/tree and
   commands. Save full logs locally and return concise results/failure excerpts.
7. Review every acceptance row, architecture, scope and diff. Record delivery
   once in SRD-COVERAGE with links to durable tests/feature evidence; feature docs
   explain behavior, issue updates link to it, and handoff retains only next state.
   Commit/push owned work, then close only issues whose full acceptance is proven.
   Administrative consolidation is not functional completion.
8. At the review checkpoint report elapsed effort, delivered original requirements,
   player paths, blockers and new scope. Do not silently reset an unsuccessful
   trial. A checkpoint is a report boundary, not permission to skip checks, kill
   a live verification process or independently change goal status.

This batch policy supersedes old one-issue and mandatory-child-ticket execution
instructions in planning documents and issue boilerplate. Exact rule requirements,
new UI approvals, distinct state-machine review and all-class scope remain binding.
Do not mass-edit historical issues or close consolidation candidates just to
reduce counts. When an issue enters a batch, reconcile its execution note with
this policy without removing acceptance criteria.

## Scope gate and compact batch card

Fill this in the handoff before the next authorized implementation batch. It is
an execution record, not a requirement to re-approve already authorized work.

- **Authorization / status:** user instruction; active or paused.
- **Player outcome / issues:** bounded behavior and existing issue IDs.
- **Acceptance / exclusions:** fixed requirements; explicit deferred boundaries.
- **Reuse:** existing rules mechanism, components and approval IDs.
- **Model routing:** requested and actual model/effort, reason, execution mechanism,
  escalation conditions and failed-attempt count. Apply
  [the routing policy](SRD-MODEL-ROUTING.md) and announce the selection before work.
- **Verification:** focused checks and one appropriate final regression pass.
- **Timing:** observed start, checkpoint (60 minutes; maximum 90), phase times.
- **Delivery:** completed player requirements / planned requirements; commits,
  issue closures, elapsed time, remaining work and blockers.

Record discoveries as short deferred notes in this card, not new implementation
or automatic GitHub issues. If an out-of-scope discovery blocks delivery, explain
what is blocked, the smallest additional work and its cost/uncertainty. Obtain
explicit approval before that work; continue independent in-scope work meanwhile.
Do not turn a blocked batch into unrelated work or reset its clock.

At the checkpoint, report the delivered result or the concrete reason for an
overrun and remaining work. Distinguish playable delivery from infrastructure
and administrative issue changes. Do not invent token or time savings. The next
batch after resumption is a measured trial of these rules; its checkpoint must
compare actual delivery and elapsed time with the fixed card. A checkpoint does
not automatically pause a goal or authorize scope expansion.

## Read, build and token discipline

- Use the map, `rg -n` and bounded `sed` ranges rather than whole large files.
  If output truncates, narrow the next query instead of repeating the same dump.
- Cache a backlog snapshot for the planning session. Refresh only affected issue
  bodies/comments at batch start, before closure, or when new evidence arrives.
- Batch independent reads; keep dependent edits, checks and approvals sequential.
- Avoid rewriting unchanged files: timestamp churn can trigger unnecessary builds.
- Do not reconfigure unchanged CMake or regenerate bindings without a cause.
  Adding a real target can require reconfiguration; measure that cost rather
  than avoiding a necessary test. Never build concurrently in the same directory.
- Build only the required targets; do not trigger app packaging/export for a
  rules-only change unless that packaging is part of acceptance.
- Poll a known live handle with useful waits; do not restart because observation
  timed out. Record handles in handoff before yielding long-running work.
- Optimize a build bottleneck only after timings identify repeated cost and a
  bounded fix is likely to repay its own effort. No new tooling project by default.

## Effort and checkpoint record

Use UTC clock/tool timestamps. Set a delivery checkpoint 60 minutes after starting
a batch; it must occur no later than 90 minutes unless the user sets another limit. Exclude user-paused
intervals; label unknown timing rather than reconstructing it from memory.
Record coarse phase transitions, not per-command bookkeeping:

| Phase | Start/end UTC | Wall time | Evidence / outcome |
| --- | --- | --- | --- |
| Investigation/decisions | observed timestamps | measured or unknown | requirements and blocker |
| Implementation | observed timestamps | measured | changed paths |
| Build | observed timestamps | measured | log and configuration cause |
| Verification | observed timestamps | measured | tests, revision, results |
| Delivery | observed timestamps | measured | commit/push/issues |

Parallel phase durations overlap: report elapsed time separately and do not sum
it as unique effort. Report token deltas only if the runtime supplies comparable
start/end usage; otherwise mark unavailable. Never infer token savings from fewer
commits. At checkpoints compare accepted requirements delivered, original issue
closures, WIP, added scope and build/test time with the batch's baseline.

## Verification selection

These examples use the `default` CMake preset, whose build directory is `build`
on every platform. Run `build.cmd` (Windows) or `./build.sh` (macOS, Linux) for
the whole suite; the targeted commands below are for development loops.
Build changed test targets before running them; `ctest` alone can run stale binaries.
Discover names with `ctest --test-dir build -N` when needed.

| Change | Minimum relevant evidence |
| --- | --- |
| Documentation/configuration | Diff/links and configuration parsing; no game rebuild. |
| Local rule or grant | Feature test plus affected grant/access/advancement tests. |
| Combat budgets, effects or movement | Feature plus rules/grid/effects tests as affected; shared changes also get native regression suite. |
| Serialization or campaign handoff | Feature, save and campaign continuation tests; real prior-writer fixtures and native regression suite. |
| Controls or presentation | Affected Godot runtime tests and neighboring shared input/layout checks; rendered English/Spanish at 1120×800 and 1920×1080. |

Example focused build and test (replace names for the selected issue):

```bash
cmake --build build --target opengold_action_surge_tests -j6
ctest --test-dir build --output-on-failure -R '^opengold_action_surge_tests$'
```

For game changes, prepare once after the last native/scene/localization changes:

```bash
cmake --build build --target opengoldbox_test_project -j6
ctest --test-dir build --output-on-failure -R '^opengold_godot_surge$' --fixture-exclude-setup godot_project
```

Fixture exclusion is valid only after successful preparation of the current
changes. CTest still runs the selected test's native fixture prerequisites.
Run Godot tests serially: internal test checkpoints share a user-data path.
Do not build concurrently in the same build directory. Avoid reconfiguring an
unchanged CMake setup; it can regenerate the Godot bindings unnecessarily.
Use `python3 tools/localization.py --check` after message changes.

For shared rules/persistence changes, build the affected native targets and run:

```bash
ctest --test-dir build --output-on-failure -E '^opengold_godot_' -j6
```

See [testing](TESTING.md) for new-machine setup, sanitizers and fuzzing; use those
when warranted by the change, not as a repeated gate for documentation or UI text.

## Reasoning and fresh tasks

The [model routing policy](SRD-MODEL-ROUTING.md) governs assignment: Luna/low
for bounded repetition, Sol/medium for normal batches, Astra/high for complex
rules, migrations, architecture and escalations. Record actual execution settings;
repository prose cannot switch a running model. This policy takes precedence over
using the project default below as the intended effort for every task.

`.codex/config.toml` selects `low` for routine project work without changing the
model or global preferences. Use `high` for rule interactions, state machines,
save migrations and difficult debugging. Explicit task settings can override
the project default. Existing tasks may retain their selected setting; wording
in a prompt does not dynamically switch reasoning effort.

In the desktop app, select effort when starting the task. For a CLI task:

```bash
codex -C /Users/edmond/src/OpenGold -c 'model_reasoning_effort="high"'
```

This follows the [official configuration precedence and override guidance](https://learn.chatgpt.com/docs/config-file/config-advanced).
Do not change models or enable parallel agents as an incidental optimization.

At a completed major milestone, use a fresh task when user-authorized. Pass only
the next issue/milestone, repository, handoff link, constraints and done criteria;
do not fork the full conversation. Preserve the overarching goal and its status.
Only one task should own implementation at a time, and pausing must not launch
replacement work. A fresh task does not inherit an active goal automatically.

Suggested starting message after explicit resumption:

> Read AGENTS.md and docs/SRD-HANDOFF.md. Implement the named next milestone using
> docs/SRD-WORKFLOW.md, preserving the full SRD scope and recorded approvals.
> Verify current repository/issue state, complete the issue's acceptance checks,
> commit and push owned changes, and update the handoff and coverage ledger.
