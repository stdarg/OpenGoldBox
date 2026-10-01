# Maps, wall artwork, and movement

See [ECL decoding and execution design](SCRIPTS.md) for the proposed script
interpreter, map-event dispatch, and gameplay integration.

OpenGoldBox can load Pool of Radiance's exploration maps and wall artwork from the
original game installation. GEO map decoding and a top-down inspection demo are
implemented. The native [Rolf tour](ROLF.md) now assembles and displays the
original Phlan wall artwork, ordinary-door traversal and the bounded
[New Phlan location scheduler](PHLAN.md). A
standalone ECL interpreter is also available; general area resource selection
and full map-script integration remain future work.

This document describes the implementation and planned extensions. Format details below come from
the pinned Gold Box Explorer reference implementation and must be validated
against our DOS release before being treated as gameplay rules. Combat maps and
overland travel are outside this initial exploration-map scope.

## Run the map demo

From the repository root, run `build.cmd` followed by `demos\review-maps.cmd`, or:

```powershell
godot --path demos/godot res://scenes/map_inspector.tscn
```

The demo uses `OPENGOLD_GAME_DIR`, falling back to `opengold/game_directory` in
`demos/godot/project.godot`. Point it at the directory containing the GEO DAX files.
The inspected DOS installation loads **29 maps**, each with 1026 decoded bytes.

- Select a location in the dropdown, or use Previous / Next. Location names
  appear alongside archive/record IDs and at the top of the details panel.
  Hover over the selector or map summary to see the reference-name provenance.
- Click a cell or use arrow keys to move the inspection cursor. This is not
  collision-checked player movement.
- Toggle wall/door drawing or event markers. Blue lines represent nonzero wall
  IDs; gold segments represent nonzero door codes. Each cell's edges are inset,
  preserving directional differences rather than merging neighboring values.
- Purple markers and the clickable event list show cells with a nonzero event
  byte. Numbers are the low seven bits; `*` marks the high bit, including `0*`.
- Selected-cell details expose all four wall and door codes, the complete event
  byte, and its separated fields. The map summary counts opposing-edge differences.

Markers identify potential script-trigger locations from GEO data. They do not
prove which ECL handler runs, or that every event fires when stepped on. Event
zero and cells without markers may still participate in area-wide script logic.
The demo explicitly labels ECL handlers unresolved and does not execute scripts.
The high bit is identified as an indoor flag by the additional format reference
below; Pool of Radiance runtime verification and exact event dispatch rules remain
outstanding. The demo currently includes indoor-only cells (`0*`) in its marker
list and nonzero-byte count, so those counts overstate potential event locations.

## Reusable native loader

The demo's location labels are maintained separately in
[`por_map_names.gd`](../godot/scripts/por_map_names.gd), using the pinned Gold Box
Explorer Pool of Radiance name table and archive assignments from the inspected
installation. They are reference labels, not names decoded from GEO or verified
by running the original game. Full archive/record keys prevent names from leaking
into unfamiliar banks. Records 25 and 27 retain `Unknown Lair`, record 26 retains
`Unknown Zone`, and unmapped keys display `Unknown area` with their source ID.

[`geo_map.h`](../src/OpenGold.Formats/include/opengold/geo_map.h) provides
`GeoMap`, `MapCell`, and `decode_geo_map(span)` in `opengold::por`.
[`map_catalog.h`](../src/OpenGold.Core/include/opengold/map_catalog.h) provides
the owning `MapCatalog` loader, linked through `opengold_core`.

```cpp
#include <opengold/map_catalog.h>

void inspect_maps(const std::filesystem::path& game_directory)
{
    using namespace opengold::por;
    const auto catalog = MapCatalog::load(game_directory);
    for (const auto& [id, map] : catalog.all()) {
        const auto& cell = map.at(0, 0);
        // id.archive and id.record identify the source; walls/doors are N,E,S,W.
        const auto event = cell.event_number();
        const auto high_bit = cell.event_high_bit();
    }
    const auto found = catalog.find({"GEO3.DAX", 0});
    // found borrows from catalog; an ordinary GeoMap copy owns its entire data.
}
```

The loader discovers `GEO.DAX` and `GEO<number>.DAX` case-insensitively and uses
canonical uppercase filenames in IDs. It validates DAX archives using the
existing decoder, requires at least 1026 bytes per map, retains all original
record bytes, and checks coordinates in `at()`. Missing maps, ambiguous archive
filenames, malformed archives, and truncated records throw `MapError`. A failed
load never returns a partial catalog. Owned data uses standard RAII containers.

`opengold_maps GAME_DIRECTORY` exports this catalog as versioned JSON to stdout;
errors go to stderr with a nonzero exit code. The Godot scene invokes the native
tool from `build/` at startup and renders its output. This development bridge
keeps a single map decoder before GDExtension integration; exported builds will
need native bindings or an explicitly packaged loader. No original game data is
written into the project by the demo.

## Validation

`build.cmd` runs native tests covering map nibble/bit ordering, first and last
cells, coordinate bounds, event flags, raw-byte preservation, truncation, archive
identity, case normalization, duplicate record rejection, and atomic failure.

```powershell
godot_console --headless --path demos/godot --script ../../tests/map_inspector_tests.gd
```

The inspector tests use a synthetic map for event lists and cell selection, and
browse all installed maps when a game installation is available. To capture the
rendered scene for visual inspection, omit `--headless` and append `-- --capture`;
the screenshot is saved under ignored `user-data/map-inspector.png`.

## Wall artwork

The Phlan profile is implemented in C++ and used by `demos\review-rolf.cmd`. See
[the fifteen appearance IDs, resources and verification](phlan-wall-art.md).
The pipeline below describes the reusable format boundary and the remaining
general area-selection work.

Three sources cooperate to produce an area's first-person walls:

| Source | Role |
| --- | --- |
| `8X8D*.DAX` | Small image tiles used to construct wall pieces. |
| `WALLDEF*.DAX` | Layouts that assemble tiles into walls at different viewing angles and distances. |
| `ECL*.DAX` | Scripts that load maps and select the wall sets used by an area. |

The loading pipeline should decode the DAX records, decode the 8x8 tiles, resolve
the tile bank for each wall set, assemble the wall view pieces, and upload those
images as Godot textures. Preserve archive names and record IDs for inspection.

The reference wall decoder divides definitions into 156-byte wall slices, each
describing ten view pieces. It also handles shared tiles and wall sets that need
multiple tile records. Its shared-tile and record-selection conventions are
validation leads, not rules to apply blindly to every game or archive.

These assets are already drawn in perspective. They can support an original-style
first-person renderer that composites the appropriate pieces based on party
position, facing, and nearby geometry. They are not ready-made seamless textures
for arbitrary 3D wall meshes.

Resolve map and wall-set selection through decoded ECL instructions and their
resource context. The reference viewer scans byte patterns for some of these
relationships; OpenGoldBox should not use that shortcut, since operands and embedded
data can resemble instructions.

## Map geometry

The reference GEO decoder reads each decompressed map record as a 16x16 grid.
For cell index `i = y * 16 + x`, it uses these byte offsets:

| Offset | Interpretation in the reference |
| --- | --- |
| `2 + i` | North wall ID in the high nibble; east wall ID in the low nibble. |
| `258 + i` | South wall ID in the high nibble; west wall ID in the low nibble. |
| `514 + i` | Event byte. |
| `770 + i` | Door information: two bits per direction, ordered north, east, south, west from least significant bits. |

This layout accesses bytes through offset 1025. A decoder must validate bounds,
preserve the first two bytes and any unexplained data, and avoid assuming that
this is the complete format for every record. The reference uses the low seven
event bits when collecting event numbers. Additional evidence identifies the
remaining bit as an indoor flag, as described below.

### High bit: evidence for an indoor flag

The [ImHex Gold Box GEO format definition](https://github.com/WerWolv/ImHex-Patterns/blob/master/patterns/GoldBox/GB_GEO.hexpat#L1433-L1435)
defines the cell's `EVENT` byte as a seven-bit `EventID` followed by a one-bit
`IsInterior` flag. Its annotation describes the flag as controlling whether the
cell is indoors or outdoors, affecting floor/ceiling and combat tile selection.
The format definition explicitly includes Pool of Radiance among its supported
games. This source was inspected September 5, 2026; the linked branch can change.

This supports the following interpretation of the current demo labels:

| Label | Raw byte | Interpretation supported by ImHex |
| --- | --- | --- |
| `9` | `0x09` | Event ID 9, outdoors. |
| `9*` | `0x89` | Event ID 9, indoors. |
| `0*` | `0x80` | Indoors with event ID zero; the flag alone does not identify a script trigger. |

This is format-reference evidence, not yet an execution-verified finding for our
Pool of Radiance DOS release. It provides stronger support for an indoor flag
than for either an event-enabled flag or a record that the event has already
triggered. Do not use the high bit as event activation/completion state without
runtime evidence.

The current native API preserves this bit as `event_high_bit()` and the demo
still renders it as `*`. A future UI update should display indoor/outdoor status
separately and base numbered event markers/counts on the low seven bits, while
retaining access to the raw byte. Event ID zero must still be evaluated in the
context of area-wide ECL logic rather than assumed to disable all scripting.

### Door codes

The reference names nonzero directional door values as ordinary door (`1`),
locked door (`2`), and wizard-locked door (`3`). Those labels do not establish the
full interaction rules. Wall identifiers, door bits, and script state must be
validated together. Exploration answers code 2 with the original "Locked."
menu; see [QUESTS.md](QUESTS.md#locked-doors).

## Scripts and map events: findings

The original scripts are ECL bytecode stored in `ECL*.DAX`. GEO supplies map
geometry and per-cell event data; the active ECL program supplies the logic that
interprets that data. An event number is not an ECL record ID, filename, or direct
instruction address. Map and script identities must retain archive/bank context.

Repeated event numbers identify the same low-seven-bit value within a map, but
do not guarantee identical outcomes: dispatch can depend on coordinates, facing,
party state, variables, and branches. Exact dispatch rules still need validation.
Neither event zero nor the indoor high bit should be used as a universal test for
whether scripts run. The high bit is not established as activation/completion
state, and finishing an event must not automatically clear it.

### Format knowledge already used by OpenGoldBox

The existing [PoR ECL inspector](../godot/scripts/por_ecl_decoder.gd) removes a
two-byte record prefix and interprets code at virtual origin `0x9900`. These
virtual addresses are numeric offsets in the script model, never native pointers.
The prefix's exact length convention still needs validation before a runtime
loader enforces it.

The program begins with five jump instructions. The reference decoder identifies
the following entry roles, in order:

| Slot (zero-based) | Reference interpretation |
| --- | --- |
| 0 | Movement command, before movement resolution in the PC 1.3 main-loop research. |
| 1 | Search location, after movement resolution or Look. |
| 2 | Pre-camp check. |
| 3 | Camp interruption. |
| 4 | Initial entry after script load or saved-game load. |

These are separate entry points into a program, not a list of map-cell event
handlers. Timing is documented in the PC main-loop research listed in the
[script source audit](script-source-audit.md); our scheduler still needs to
reproduce that ordering, including blocked moves and transitions.

The inspector decodes typed operands, including constants, address-bearing
values, inline packed six-bit text, and string references. It follows reachable
instructions through branches, subroutines, indexed dispatch, and conditional
instruction skips. Menus and indexed branches can have variable operand lists.
Embedded data can resemble opcodes, so raw byte-pattern searches are not a
reliable way to associate maps with scripts.

Pool-specific differences matter. Our inspection work found that `ECL CLOCK`
(`0x34`) takes one operand in the local data: the inspected reference's
two-operand interpretation misaligns the `ECL7.DAX:17` routine at virtual address
`0x9D37`. This establishes a decoding correction, not the clock instruction's
complete execution semantics. See [NPC art findings](npc-art-identification.md).

### Commands that connect scripts to maps and encounters

| Command | Finding and integration implication |
| --- | --- |
| `LOAD FILES` (`0x21`) | Provides resource/map-loading references. Resolve decoded operands and active resource context instead of assuming an ECL record matches a GEO record. |
| `LOAD PIECES` (`0x37`) | Selects wall sets used to render an area; map geometry alone does not select all artwork. |
| `NEW ECL` (`0x20`) | Changes the active script. State lifetime, entry selection, and transition timing need runtime verification. |
| `PRINT`, `PRINTCLEAR`, menus, `PICTURE` | Supply dialogue, choices, and presentation requests. Input must be returned to the suspended script. |
| `LOAD MONSTER` (`0x0B`) | Has distinct creature, count, and combat-icon operands. Resolve the creature bank before using `CreatureFactory`; keep artwork identity separate. |
| `SETUP MONSTER` (`0x0C`) | Selects encounter presentation separately from combat creature loading. |
| `COMBAT` (`0x24`) and `ADD NPC` (`0x36`) | Connect scripts to encounter resolution and recruitment; decoding these commands does not yet implement those behaviors. |
| `CALL` (`0x2D`) | Requires explicit OpenGoldBox implementations of referenced engine services. Never execute a stored DOS address as native code. |

To identify an area's script, follow decoded map-loading instructions and inspect
their control flow, dialogue, and transitions. These can corroborate reference
area names and help investigate unknown maps. A static reference is a candidate
association until its bank context and execution path are resolved.

### Current capability and execution plan

OpenGoldBox has a native ECL loader and resumable interpreter for control flow,
explicitly bound variables, arithmetic, table data, strings, input and menus,
alongside the existing inspector and asset-evidence analysis. Engine services
have opt-in host requests; their concrete gameplay implementations and mapped
world state remain outstanding. The map demo's event list still does not execute
handlers. Static paths must not be presented as observed gameplay.

The native engine separates shared `EclProgram` data, a resumable `EclMachine`
with private writable script bytes, and typed presentation/input/engine requests.
The machine owns its PC, conditions, variables, RNG and return stack. Replies
validate request identity and output writes before continuing; a script
replacement finishes the old invocation. Instruction budgets allow a future
Godot integration to remain responsive. Missing host capabilities and unresolved
commands produce contextual diagnostics.

The external research corrected zero-based menus and indexed jumps, bitwise
condition flags, and `PRINT RETURN` fallthrough. For example, Slums event 1
dispatches to `0x9E1A`; after combat its script sets `0x4ACA` to 255. The optional
installed-data test now runs that event with a mock combat host and verifies that
a second visit does not repeat the encounter. This flag is distinct from the
map's raw event byte and its high bit.

The six programs with static decoding warnings have speculative indexed-jump
fallthrough paths into embedded tables. The runtime now reads those tables with
GETTABLE; the inspector retains possible fallthroughs and identifies their source.
See the [audit](script-source-audit.md) for exact addresses and unresolved details.

Map-event dispatch will construct a context from the map, position, facing, and
cell data, then invoke the verified entry point in the active program. Movement
ordering, blocked moves, repeat visits, one-time events, persistent variables,
and map transitions remain explicit verification tasks. The first target is one
original location event with text, a choice, a persistent state change, and
correct revisit behavior. See [SCRIPTS.md](SCRIPTS.md) for architecture, ownership,
save/resume contracts, implementation stages, and acceptance criteria.

## Movement model

Movement should model crossing an edge between neighboring cells. A single
`walkable` flag on a cell is insufficient: one side can be open while another
has a wall or door.

Keep decoded map data separate from mutable gameplay state. The decoded map
should retain the four directional wall IDs, door fields, event bytes, and source
identity. Runtime state should hold the party's position and facing, door changes,
scripted geometry changes, and explored cells.

A proposed `try_move(direction)` operation should:

1. Determine the destination and edge being crossed.
2. Resolve that edge's validated wall, door, and current script-state rules.
3. Return whether movement is allowed, blocked, requires a door interaction, or
   invokes a transition.
4. Update position and dispatch applicable events in the validated game order.

Do not infer collision from image pixels or assume every nonzero wall ID is
impassable. Likewise, do not assume maps wrap at their boundaries or that a map
exit is always blocked. Scripted transitions need explicit handling. Inspect
opposing edge values on neighboring cells and report disagreements while the
original game's directional behavior is being established.

The same geometry can drive a top-down inspector and a player map. Keep explored
state separate so a gameplay map can reveal only discovered areas while the
inspector shows the complete decoded layout.

## Implementation stages

1. **Map inspector (implemented):** GEO decoding in `OpenGold.Formats` uses the
   existing `decode_dax_archive` infrastructure. The Godot demo shows selectable
   maps, walls, doors, event markers, an inspection cursor, and source IDs.
2. **Movement:** add exploration state and edge-based movement rules to
   `OpenGold.Core`. Validate open passages and blocked edges, then door behavior,
   ECL transitions, and scripted changes.
3. **First-person artwork:** add 8X8D decoding and WALLDEF assembly, resolve the
   selected wall sets, and composite the appropriate view pieces in Godot.

Use value types and standard RAII containers for native data and owned resources.
Keep format decoding independent of Godot so it can be tested without launching
the presentation layer.

The map inspector is the first milestone because it makes geometry and movement
easy to verify before perspective rendering is added. Validation should cover
malformed and truncated records, directional bit extraction, known map layouts,
wall-piece assembly, and observed movement in the original DOS game. Door
interactions, event timing, secret passages, and boundary transitions remain
explicit gameplay validation gaps.

## References

Gold Box Explorer revision `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` is available
locally under `user-data/reference-gbe`:

- [GEO record decoding](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoDaxFile.cs)
- [Directional wall and door fields](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoWallRecord.cs)
- [Wall definition and tile assembly](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/Dax/DaxWallDefFile.cs)
- [ECL entry table and command registration](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Program.cs)
- [ECL command descriptions](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Commands.cs)

See also the [asset source audit](asset-source-audit.md) and the existing
[graphics format notes](graphics-format.md). The reference viewer establishes a
useful decoding path; it is not a complete movement-engine specification.
