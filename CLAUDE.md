## Use the ./docs folder
Read ./doc/PRD.md for context and know that useful docs outline key dynamics of game systems. Look at the documents for helpful context.
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

Always run all tests after completing task. Then, commit your work and push to
origin.

## 6. Do your work in git worktrees.

Before committing and pushing to origin/main, ensure all tests pass. Always
commit your work when done and push to origin/main.

## 7. Write readable code.

- Use clear names: remaining\_hit\_points, not hp2; verbs for actions (apply\_damage), nouns for values (damage\_roll).
- Keep functions small and single-purpose. A reader should quickly see what a function promises to do.
- Prefer straightforward control flow: early returns often beat deeply nested if blocks.
- Make ownership explicit: values by default, references for non-owning required inputs, std::unique\_ptr for exclusive ownership, and avoid owning raw pointers.
- Let types communicate meaning: scoped enums, strong domain types where useful, const for immutable inputs, [[nodiscard]] on results callers must handle.
- Avoid hidden state and clever side effects. Make mutations and error handling visible at call sites.
- Use standard-library algorithms and containers when they express the operation more directly—but don’t compress logic into inscrutable one-liners.
- Keep headers lean: expose the public interface, hide implementation details, and avoid unnecessary dependencies.
- Write comments for why a choice or constraint exists, not to narrate code the reader can already see.
- Follow one consistent local style for formatting, error handling, and naming; consistency matters more than a particular style.
- Test behavior at meaningful boundaries. Clear tests double as executable examples of intended use.
- Use blank lines for readability before and after functions and methods, as well as any
  C++ block.

A useful rule: a capable teammate should be able to infer ownership, invariants, inputs, outputs, and side effects from the type signatures and a quick read of the function body.

## 8. Keep the Git Repository Tidy

- Do it a git pull from orgin / main before working.
