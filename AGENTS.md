# OpenGoldBox contributor guidance

- Use RAII for every owned resource in native code.
- Do not represent ownership with raw pointers. Prefer value types and standard
  RAII containers; when pointer semantics are necessary, use `std::unique_ptr`
  by default and `std::shared_ptr` only for genuine shared ownership.
- After updating this repository, commit your changes and push the current branch
  to `origin`, unless the user explicitly asks otherwise. Include only your own
  changes and complete relevant checks before committing.

## Scope and review boundaries

- Answer questions and provide suggestions without treating them as
  authorization to make changes.
- A design request authorizes a written proposal, diagrams, and static
  layout sketches. It does not authorize coding.
- Do not create prototypes, applications, scripts, or launchers for a
  design request.
- When a design is requested for review, deliver it and stop for
  comments. Wait for explicit authorization before implementation.
- Commit/push instructions govern authorized changes; they do not
  expand the scope of a task.

## Technology decisions

- Read docs/TECH.md and the relevant requirements before selecting
  an implementation approach.
- Follow OpenGoldBox's established Godot 4.x, C++20, and GDExtension
  architecture and documented language boundaries.
- Keep SRD mechanics in the statically linked `opengold_rules_srd5` library.
  Core depends on `opengold_rules` interfaces, never the SRD implementation;
  the application selects and injects the implementation. Core owns campaign
  orchestration and transactions; the rules module decides rule outcomes.
  Do not duplicate SRD calculations or decision branches in Core or UI.
- Reuse existing components, build tools, and launch conventions.
- Do not introduce another UI stack, runtime, or framework without
  explicit user approval, including for prototypes and review tools.
- Skills and available tools must serve the project's requirements.
  Their availability does not authorize a technology substitution.
- Report specific blockers instead of silently changing technologies.
- Before committing, verify scope and architecture compliance as
  well as functional correctness.

## Time and communication

- Keep responses concise and investigation proportional to the task.
- Do not perform speculative or unrelated work.
- Provide commands appropriate to the user's actual shell.
- Ask numbered questions to confirm UI layout and control behavior before
  making independent choices about them. Restart visible numbering at 1 for each
  new set of questions; retain topic-specific identifiers in the decision ledger.

## Efficient SRD work

- Ranged weapons assume unlimited ammunition (user decision 2026-09-25). Do not
  add ammunition stock requirements, selection, expenditure or recovery. Preserve
  existing saves; Thrown weapons still use physical weapon inventory. See
  `docs/AMMUNITION.md`. Reintroducing ammunition tracking requires explicit approval.

- Start with `docs/SRD-COVERAGE.md` for what is already delivered, then follow
  `docs/SRD-WORKFLOW.md`. Use `docs/SRD-REPO-MAP.md` for targeted navigation and
  `docs/SRD-DECISIONS.md` for approvals; do not reload the whole history.
- Follow `docs/SRD-MODEL-ROUTING.md` before each implementation batch and any
  authorized delegation. Record and visibly announce model/effort and reason;
  distinguish the requested model from the actual configured model. Re-read
  the recorded assignment after context resets. Escalate after two unsuccessful
  fixes of the same failure, or immediately for decisions beyond the assigned
  tier. Model routing does not authorize delegation, new tasks or scope changes.
- Keep one delivery batch active, with fixed acceptance criteria and one
  implementation owner per requirement. Batch related issues/source routes;
  preserve their full acceptance. Use the existing backlog grouping in
  `docs/SRD-BATCHING-REVIEW.md`. A checklist is the default; report newly
  discovered work separately rather than automatically creating more issues.
  This execution policy supersedes older mandatory ticket-splitting language;
  it does not supersede feature requirements or review boundaries.
- Freeze the batch's player-visible outcome, included issues, acceptance and
  exclusions before coding. Never silently expand scope. Defer discoveries
  outside that boundary; if one blocks delivery, explain the dependency and ask
  for explicit approval before implementing it. Necessity is not authorization.
  Routine implementation choices within accepted scope need no repeated approval.
- Keep at most one explicitly approved out-of-scope prerequisite active, with a
  return path. No speculative foundations, unrelated fixes or tooling projects.
- Finish edits to a build's inputs before starting it. Do not edit source,
  headers or generated inputs consumed by a running build. After a code change,
  rebuild affected targets before testing; choose targets that avoid unneeded
  packaging/export work.
- Search with `rg`, then read bounded sections. Reuse current evidence and
  snapshots; refresh affected issues when their state matters. Keep full logs
  outside conversation and return failures or summaries, not truncated dumps.
- Use `docs/SRD-COVERAGE.md` as the completion record. Handoff holds only current
  state; feature docs hold mechanics/evidence. Link records instead of copying
  delivery narratives. Record tested revision, commands and material limitations.
- Run focused checks during development; broader required checks on the final
  integrated changes. Never use stale binaries or weaken verification. Preserve
  released-save compatibility; no unapproved reduction in supported history.
- Use the compact batch card in `docs/SRD-WORKFLOW.md`; record phase timings
  and a checkpoint 60 minutes after starting, no later than 90 minutes.
  Measure delivered original requirements, not child-ticket count. Do not
  fabricate past timings or promise a speedup before measuring it.
- Batch related UI questions and reuse approved patterns within their recorded
  scope. Play `/usr/bin/afplay /System/Library/Sounds/Glass.aiff` before questions.
  Write each question visibly in the conversation; a question widget alone is
  insufficient. A decision reply or workflow maintenance does not resume a paused goal.
- New tasks, parallel agents and changes to compatibility require explicit
  authorization. Carry a compact handoff at authorized task boundaries. Do not
  change model/runtime settings as an incidental optimization.

## UI controls

- Make interactive elements visibly recognizable as controls. Buttons must look
  like buttons, and selectable rows must visibly communicate that they can be
  selected; do not present actions as indistinguishable plain text.
- Use consistent standard control styling with clear boundaries, readable
  contrast, padding, and distinct hover, pressed/selected, disabled, and keyboard
  focus states. Preserve keyboard access and clear labels.
- Apply this rule to new screens and when updating existing UI. Confirm layout
  and control behavior with numbered questions as required above.

## Combat sprite generation

- Before generating or regenerating monster combat sprites, read and follow
  `dev_tools/combat_sprite_catalog/generation/README.md` and `rules.txt` in that
  directory. Use its prompt preparation and acceptance commands for each bipedal
  monster weapon set. These rules apply even when a later request only says
  "regenerate"; do not rely on conversational memory.
- For every monster, inspect its current saved equipment and enumerate all
  weapon sets before generating art, including weapons not currently readied.
  Generate a separate ready/action pair for EVERY set. A one-handed weapon and
  shield are one unit when a shield is available; without a shield, the
  one-handed weapon is its own set. Ranged weapons and two-handed weapons are
  separate sets. For example, bow + mace + shield requires a bow pair AND a
  mace/shield pair. Ask the user if any set or equipment classification is unclear.
- Bipedal monsters face screen right in front three-quarter view with the chest
  partly visible. Melee ready poses hold the weapon UP, ready to strike; action
  poses are near the end of a swing or thrust. Projectile weapons are loaded in
  ready and have just released their projectile in action.
- Use the user-approved original pose guide and approved newly created character
  art as references. Original game artwork must not be generation references.
  A pending guide must receive explicit user approval before production use.
- File validation does not establish correct poses. Visually review both frames
  at combat size and alternating in the catalog; record approval for the exact
  image hashes. Reject and regenerate failures before installing either frame
  or clearing the monster's regeneration flag. Keep existing art until the pair
  passes. Follow the documented review for every weapon set, and verify every
  set has both images before calling a monster complete or clearing its flag.
