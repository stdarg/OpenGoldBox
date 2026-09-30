# Complexity review — 2026-09-18

## Findings and changes

Screening covered the C++ implementation under `src/`, followed by inspection
of the combat rules, checkpoint codec, ECL dispatcher, town host, and Godot
view refresh routines. Branch counts locate candidates; they do not by
themselves distinguish difficult algorithms from long catalogs of constants.

The highest-priority changes were in `OpenGold.Rules.Srd5`:

1. **Repeated path searches.** `legal_commands()` ran Dijkstra separately for
   every destination. It now computes one reachability map per request. For
   `V` cells and at most eight edges per cell, the worst-case search work falls
   from `O(V² log V)` to `O(V log V)`, plus the linear destination scan.
2. **Three copies of movement policy.** Path planning, movement execution, and
   checkpoint validation independently handled allied transit, terrain costs,
   enemies, and diagonal walls. All now use `MovementGrid::step_cost()`.
3. **Checkpoint parsing mixed with relational validation.** One function read
   the format, decoded actors, restored state, checked overlaps, validated the
   pending path/reactions, and loaded logs. It now orchestrates named helpers,
   with parsing separated from checks that require the complete candidate.
4. **Sight traversal was difficult to audit.** Its integer crossing arithmetic
   is now isolated in `has_line_of_sight()` and explained beside the algorithm.
   It can be tested without constructing a combat session or rolling initiative.

The grid helpers live in
[`combat_grid.h`](../src/OpenGold.Rules.Srd5/src/combat_grid.h) and
[`combat_grid.cpp`](../src/OpenGold.Rules.Srd5/src/combat_grid.cpp).
They remain private implementation details of the SRD module. The public rules
interface, Godot presentation boundary, checkpoint formats 1–3, and command
ordering remain compatible. Grid snapshots and search results own their data
by value; session candidates use `std::unique_ptr`.

### Function complexity

Lizard 1.24.0 cyclomatic-complexity measurements at parent commit `7057cde` and after
this refactor (the count includes boolean branches):

| Function | Before | After |
| --- | ---: | ---: |
| `Session::restore` | 113 | 17 |
| `Session::path_to` | 28 | 1 |
| `Session::line_of_sight` | 10 | 1 |
| `Session::progress_movement` | 16 | 15 |
| `Session::legal_commands` | 42 | 42 |

These are per-function measurements: extracted logic still has branches.
`MovementGrid::reachable` measures 11, `step_cost` 11, and the standalone sight
traversal 12 (including its endpoint checks). Checkpoint helper complexity ranges
up to 24. The unchanged branch count for `legal_commands()` hides the substantial
reduction in repeated work; it still enumerates the same supported actions.

## Correctness arguments

- **Shortest paths:** every allowed edge costs either 5 or 10 feet. Dijkstra
  removes the least-cost queued cell; a route through a more expensive cell
  cannot improve that cost. Stale queue entries are ignored. A predecessor is
  recorded only for a strict cost improvement, so following predecessors
  strictly decreases cost and must terminate at the origin.
- **Budget safety:** a step is accepted only when its cost fits the remaining
  budget. Subtraction precedes addition in that check, avoiding overflow even
  with a large caller-supplied budget.
- **Destination rules:** allies can be crossed at the ground's ordinary cost, but
  no occupied cell or origin is offered as a destination. Enemies cannot be
  crossed. Corpses are omitted from occupancy; unconscious actors are retained.
  Allied occupancy adds no surcharge, including on difficult ground.
- **Deterministic ties:** equal-cost queue entries use the row-major cell
  index, and equal-cost alternatives never replace a predecessor. This retains
  the original chosen route, which matters for opportunity attacks.
- **Visibility:** boundary-crossing times are compared by integer cross
  multiplication. Each iteration crosses at least one boundary toward the
  endpoint. A corner crossing checks both touched side cells and the diagonal
  cell. The documented 64 × 64 board bound keeps the arithmetic in range.
- **Restore safety:** counts and dimensions are bounded before dependent
  allocations. Actor IDs and path/reaction indices are validated before use.
  A paused path validates only its untravelled suffix; the prefix has already
  spent movement. The owned candidate is returned only after parsing and all
  relational checks succeed. A paused mover may share an allied transit cell;
  a saved involuntary-overlap marker admits a knockdown/recovery at that cell
  until the actors separate. Other living overlap and occupied destinations
  reject. Failure destroys the candidate.

These arguments concern the extracted algorithms and their stated contracts;
they are not a formal proof of the entire game or every possible combat state.

## Verification

- [`combat_grid_tests.cpp`](../tests/combat_grid_tests.cpp) compares all 3,125
  assignments of floor/wall/difficult/ally/enemy around a 3 × 2 origin, plus all
  6,561 terrain assignments around a 3 × 3 center. Each is checked at eight
  movement budgets against an independent Bellman-Ford oracle. Returned paths
  must use legal steps, have the advertised minimum cost, and contain no cycle.
- Sight is compared against an independent segment/closed-rectangle intersection
  oracle for every endpoint pair in all 512 wall layouts on a 3 × 3 board:
  41,472 cases, also checked for symmetry.
- Boundary cases cover bad geometry, out-of-bounds queries, negative/large
  budgets, stable ties, allied terrain costs, and ownership of snapshots.
- [`rules_tests.cpp`](../tests/rules_tests.cpp) adds malformed checkpoint-section
  rejection and a reaction paused after a spent path
  prefix while the mover shares an ally's square. Restored continuation must
  spend only the suffix's movement and match the uninterrupted session.
- A local before/after experiment ran 32 seeds with 80 accepted-command states
  each, restoring after every command. All 2,560 checkpoint and legal-command
  sequence fingerprints matched. This is regression evidence for those traces,
  not exhaustive combat-state coverage.

The full `build/mac-check` build, including the macOS game package, completed.
All 23 configured native/tool/Godot tests passed with the local original-data
fixtures enabled through `OPENGOLD_GAME_DIR`.

Run the new focused checks in an existing configured build with:

```bash
cmake --build build/mac-check --target opengold_combat_grid_tests opengold_rules_tests
ctest --test-dir build/mac-check -R '^opengold_(combat_grid|rules)_tests$' --output-on-failure
```

## Other reviewed areas

| Area | Assessment |
| --- | --- |
| `por_effects.cpp::describe_effect` | Large branch count comes mainly from a fixed effect catalog. It is already a pure, directly testable lookup; its case structure makes the source IDs explicit. |
| `EclMachine::execute` | Opcode dispatch necessarily has many cases. Existing opcode-specific comments and synthetic bytecode tests make the dispatch auditable. |
| `RolfTourSession::handle_town_host` | Still combines many distinct host services. Extracting service-specific handlers is a useful next boundary, with event-continuation tests guarding each extraction. |
| `CharacterCreationView::refresh` | Still mixes visibility decisions, displayed text, and node mutation. A pure presentation-state calculation would improve isolation; layout and control behavior need preservation. |
| SRD `character_definition` and `parse_content` | Dense parsing/validation remains. Separating profile parsing, equipment application, and derived statistics is a further candidate; malformed-input and character-profile compatibility must remain covered. |
| Godot `check_run` / `party_check` | High counts primarily reflect embedded integration scenarios. Their size does not imply equivalent runtime algorithmic cost. |
