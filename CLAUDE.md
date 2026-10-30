## Use the ./docs folder
Read ./docs/PRD.md for context and know that useful docs outline key dynamics of game systems. Look at the documents for helpful context.
Do not blindly read all the documents, just search for relevant documents by name or keywords in the contents.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it
work") require constant clarification.

## 5. Verify Your Work Through Tests

**Do not assume your code works, verify it through tests.**

While working on a task, after each change run only the tests that are directly related to the change; we do not have the time to run every test after every single change.

Always run the full test suite when you complete a task I ask of you, and before pushing the work to main. Then commit your work and push to origin. Save the date when all tests are run for future reference.

Documentation-only changes need no tests.

## 6. Do your work in git worktrees

Before pushing to origin/main, run the full test suite and ensure all tests pass. Always
commit your work when done and push to origin/main.

## 7. Write readable code

- Use clear names: remaining\_hit\_points, not hp2; verbs for actions (apply\_damage), nouns for values (damage\_roll).
- Keep functions small and single-purpose. A reader should quickly see what a function promises to do.
- Prefer straightforward control flow: early returns often beat deeply nested if blocks.
- Make ownership explicit: values by default, references for non-owning required inputs, std::unique\_ptr for exclusive ownership, and avoid owning raw pointers.
- Let types communicate meaning: scoped enums, strong domain types where useful, const for immutable inputs, [[nodiscard]] on results callers must handle.
- Avoid hidden state and clever side effects. Make mutations and error handling visible at call sites.
- Use standard-library algorithms and containers when they express the operation more directly—but don’t compress logic into inscrutable one-liners.
- Keep headers lean: expose the public interface, hide implementation details, and avoid unnecessary dependencies.
- Write comments for why a choice or constraint exists, not to narrate code the reader can already see.
- Test behavior at meaningful boundaries. Clear tests double as executable examples of intended use.
- Use blank lines for readability before and after functions and methods, as well as any
  C++ block.

A useful rule: a capable teammate should be able to infer ownership, invariants, inputs, outputs, and side effects from the type signatures and a quick read of the function body.

Always adhere to the [C++ Core Guidelines by Stroustrup and Sutter](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#main). Good style promotes readability and correctness.

Always adhere to the rules laid out in Scott Meyers' Effective Modern C++.

## 8. Keep the Git Repository Tidy

- Do a git pull from origin / main before working.

## 9. Coordinate with other sessions

Several Claude and Codex sessions work on this repository at once, each in its
own git worktree. `tools/session_board.py` keeps a board of what each one is
doing, in `.sessions/` at the main checkout's root (ignored by git, shared by
every worktree).

- Before starting a task, claim it from your worktree: the task, its issue, the
  areas it touches (files, folders or systems) and the shared numbers it will
  raise (campaign format, rules version, combat format). Pass your ListAgents
  name so others can message you:
  `tools/session_board.py claim --name opengold-08 --task "Wand traps" --issue 4 --areas "src/OpenGold.Core/src/phlan_session.cpp, Kovel Mansion" --versions "campaign format"`
- Update the claim with `update` when the work spreads to new areas, and claim
  a shared number before raising it.
- An overlap is a warning, not a block: message the other session (SendMessage)
  or ask the user before working in the same area.
- Release the claim with `release` once the work is pushed.
- The project's hooks show the board at session start and whenever another
  session's claim changes; `tools/session_board.py list` shows it any time.
