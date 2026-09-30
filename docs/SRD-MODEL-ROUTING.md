# SRD model routing policy

Adopted at the user's request. Applies to the SRD goal and survives task changes
through AGENTS.md, the workflow batch card and the handoff. This is an execution
rule, not authorization to resume a paused goal, expand scope, create tasks or
spawn agents. Existing authorization boundaries still apply.

## Required selection before implementation

At every batch start, after a context reset, and before any authorized delegation:

1. Read this policy and the current handoff; check existing approvals rather than
   reconstructing them from conversational memory.
2. Classify the work using the table. Record the exact requested model, reasoning
   effort, reason and escalation conditions in the batch card.
3. Report the selection visibly before implementation. Distinguish the intended
   model from the model actually configured. Never claim a switch occurred just
   because the instructions name a model.
4. Check that the model and effort are supported by the execution mechanism.
   If unavailable or unchangeable, report the mismatch and obtain a user choice
   before substituting another model for that assignment.

| Work | Model | Initial reasoning effort |
| --- | --- | --- |
| Bounded repetitive changes following an existing example: catalog entries, straightforward grants, documentation | `gpt-6-luna` | `low` |
| Normal implementation batches using established rules interfaces and approved controls | `gpt-6-sol` | `medium` |
| Complex rule interactions, save format changes, architecture, or difficult escalations | `gpt-6-astra` | `high` |

These are starting assignments to evaluate against actual delivery, not a claim
of guaranteed savings. Classify by the hardest required decision, not issue title
or patch size. A seemingly small spell can require Astra if its interactions or
persistence are unresolved. Do not reduce acceptance or testing for a smaller model.

The coordinator cannot change its own running model by writing a prompt or editing
this file. Use the user's model selection for the current task, or an explicitly
authorized delegation mechanism with a supported model override. Do not build
new orchestration tools or change global/project model settings for this policy.

## Work packet and ownership

Keep one implementation owner. Start sequentially; parallel agents and new tasks
still require explicit authorization. This policy does not grant that permission.
For an authorized handoff/delegation, send a compact packet containing:

- Exact outcome, issue IDs, acceptance criteria and exclusions.
- Applicable AGENTS.md instructions and links to workflow/routing policy.
- Relevant SRD references, existing approvals and unresolved decisions.
- Relevant files and one established implementation example.
- Required focused checks and final regression scope.
- Selected model/effort, elapsed time, checkpoint and escalation conditions.

Provide relevant source excerpts when necessary; do not forward the entire
conversation or ask the worker to rediscover the design. Require the worker to
honor scope, static-library boundaries, pause status and visible/audible questions.
The coordinator retains responsibility for acceptance, integration and delivery.

## Escalation and review

Escalate after two unsuccessful fixes of the same failure, or immediately when
an unresolved architectural decision, rules interaction or save format change exceeds the
assigned tier. Move Luna to Sol for ordinary implementation difficulty; use Astra
for complex decisions or persistent failures. Preserve the patch, reproduction,
failed attempts, logs and remaining acceptance. Do not restart investigation.
Record and announce the new assignment. If no authorized mechanism can execute
the escalation, report the blocker and request the necessary user action.

A model escalation does not approve new scope. Out-of-scope dependencies still
require explicit user approval before implementation. Do not reset the batch's
60-minute checkpoint (90-minute maximum) on escalation.

Use stronger-model review selectively for consequential rules interactions,
save format changes and architecture. Review the diff and evidence; do not repeat a full
repository audit. Routine data changes need no automatic second-model review.

## Measurement and continuity

At delivery/checkpoint record actual model(s), effort, completed player outcomes,
elapsed time, repeated fixes and review overhead. Include tokens/cost only when
available; otherwise mark unavailable. Evaluate total cost per accepted feature,
including failed attempts and review, rather than per-token price alone.

Retain the routing decision and escalation history in the current handoff until
delivery. After context reset, resume from that record; do not silently fall back
to a default model. Update these assignments only with user authorization or an
explicitly reported, already-authorized escalation. No unmeasured speedup claims.
