# PoR script execution source audit

Audited September 5, 2026 against the local Steam DOS assets and the native
implementation. [SCRIPTS.md](SCRIPTS.md) describes the executable API and tests.
The target profile is **Pool of Radiance PC 1.3**. Facts from later Gold Box games
or the Apple II release require separate validation.

## Sources and how they were used

| Source | Use and limit |
| --- | --- |
| Stephen S. Lee, [Exhaustive Game Information v2.00](https://gamefaqs.gamespot.com/c64/578753-pool-of-radiance/faqs/73869), April 2, 2026 | Sections 12.3 and 12.4 describe PoR commands and flags; section 14's `19B5` and `1FDC` function listings provide implementation details. The cross-listed URL says C64, but the document identifies itself as the **PC 1.3** guide. Primary source for the corrections below. |
| Lee's [v2.00 announcement](https://www.gog.com/forum/forgotten_realms_collection/pool_of_radiance_detailed_faq_v200_now_available) | Identifies updated findings including CALL services, OR flags, CLOCK and SPELL; use the actual FAQ to establish behavior. |
| marainein, [ECL analysis](https://forums.goldbox.games/index.php?topic=2519.0), 2013 | Independently documents zero-based Slums dispatch, event 1's creature calls and completion flag, and event 3's fourth-table-entry selection. Used for the installed-data event/revisit regression. Its older interpretation of CLEARMONSTERS is superseded by the detailed PC research. |
| [Gold Box Explorer](https://github.com/bsimser/Gold-Box-Explorer/tree/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl), pinned revision `eac30abaa6ee66aea6f5d65ebe6d676b10015a8f` | Inspected Program.cs, Commands.cs and Memory.cs for grammar, table addressing, string references and bitwise comparison effects. This is a disassembler spanning multiple games, not an execution oracle. Its DIVIDE remainder and CLOCK assumptions must not replace the PoR-specific findings. |
| [Gold Box Companion](https://gbc.zorbus.net/) | ECL Tool and ECL-monitor provide independent inspection/live-tracing routes. Public documentation and the ECL Tool description were reviewed. No Companion runtime source or new live DOS trace was obtained; no implementation is claimed to have been tested against Companion's engine. |
| Simeon Pilgrim's [Curse of the Azure Bonds reimplementation](https://github.com/simeonpilgrim/coab) | Cross-check for tagged values, virtual-memory/string handling and separation of VM operations from game functions. It targets a later game, so its addresses, clock and division side effects are not PoR specifications. |
| [Pool of Radiance JS](https://github.com/stlhood/pool-of-radiance-js), local pinned reference | Additional context reviewed in the earlier asset audit. It targets Apple II data and is not used as authority for DOS opcode behavior. |

No reference implementation source or original game script has been copied into
the native runtime. Tests use original hand-authored bytecode, or optionally read
the user's installed assets. For the earlier asset-reference revisions and local
paths, see [asset-source-audit.md](asset-source-audit.md).

## Corrections applied

The previous interpreter used one-based menus and indexed dispatch. Both are now
zero-based. The Slums trace independently anchors this correction: event 3 goes
to the fourth destination, `0x9F13`. Event 1 reaches `0x9E1A` and sets `0x4ACA`
after combat. The new optional regression verifies its revisit behavior.

The runtime now implements condition side effects, quotient-only division,
bounded randomness, table access, C-string references, string writes, input and
parlay requests, and blank-line/clear output. It recognizes documented no-ops
and separates known engine services from undefined instructions. The API's
behavior table and tests record the precise choices, including explicit faults
for unsafe accesses instead of reproducing undefined DOS memory behavior.

Two details found during real-data verification deserve particular attention:

- Inline string SAVE can have a **tag 129 destination**, as in Slums `0x9B9F`.
  Destination roles use the encoded address; they must not read the old string
  or reject it as a numeric pointer mismatch.
- Script tables contain ordinary bytes, while scratch/persistent bindings hold
  one 16-bit value per logical address. Indexing advances by one logical cell;
  it must not double the index in a generic byte-array implementation.

## Why the six inspection failures remain visible

All seven diagnostics have a speculative path originating at an ON GOTO's
out-of-range fallthrough. Nearby GETTABLE operations identify the same bytes as
table data. The traversal does not prove selector ranges, so it also attempts to
interpret those bytes as instructions. The updated native inspector reports the
fallthrough's origin alongside the diagnostic.

| Program | ON GOTO PC | Data immediately after it | Result of speculative decoding |
| --- | --- | --- | --- |
| ECL2.DAX:9 | 9CF2 | 9D10 | Apparent GOTO to 0807. |
| ECL2.DAX:15 | 9B2E | 9B5B | Apparent operand tag 12. |
| ECL4.DAX:21 | AED8 | AEF6 | Apparent GOSUB to 0002. |
| ECL5.DAX:7 | 99C9 | 99D8 | Apparent GOTO to 0302. |
| ECL6.DAX:28 | A6BD | A6C9 | Apparent GOTO to 0002. |
| ECL8.DAX:16 | 9B7E | 9B96 | Apparent GOTO to 0504. |
| ECL8.DAX:16 | 9AEE | 9B00 | Apparent operand tag 12. |

This identifies the source of the diagnostics; it does not prove every such
fallthrough impossible for every game state. Do not remove all indexed-jump
fallthrough edges or invent opcode definitions for table bytes. A future static
analysis can refine paths using selector bounds and table contents. The runtime
already selects only the actual branch and reads tables as data.

## Compatibility questions and deliberate limits

| Topic | Current decision / next evidence needed |
| --- | --- |
| GETTABLE condition flags | Preserves flags. Settled by the [DOS probe](../dev_tools/docs/dos-probe.md#findings) (2026-10-24): after `COMPARE 1,1` or `COMPARE 2,1` and a GETTABLE, the original's `IF =` or `IF >` still held. |
| RANDOM upper endpoint | Inclusive, with argument 255 producing 0..254. Settled by the DOS probe (2026-10-24): in 3,000 rolls the original's `RANDOM 8` gave 8 350 times and never more, and `RANDOM 255` gave 254 eight times and never 255. OpenGoldBox's seedable RNG intentionally has a different sequence. |
| Conditional skip quirks | Skips one whole decoded instruction. The DOS probe read the original's skip table (2026-10-24): it skips only the opcode of VERTICAL MENU, ON GOTO, ON GOSUB and HORIZONTAL MENU, two of SETUP MONSTER's three operands and one of ADD NPC's two. None of the 1,936 IFs in the 29 original scripts is followed by one of these, so both skip the same bytes on every original script. |
| `0x1F` | Two-operand grammar, undefined handler. Execution faults; it is not a guessed command or silent success. |
| ECL CLOCK | One operand is confirmed. Original execution uses an uninitialized value; by the user's policy campaign play passes time for the operand's steps ([nomad camp](audits/nomad-camp.md)), and a VM without the campaign host faults. |
| Runtime code changes | Private script writes and subsequent fetches are implemented and isolated. Original self-modifying command sequences still need a dedicated DOS compatibility trace. |
| Mapped engine fields | Bindings are explicit research state. Character-name special handling, selected-creature views, position/wall updates, time and inventory must be connected to authoritative world objects. |
| Engine services | Opcodes have opt-in host requests. The campaign host implements them for the supported areas; [ECL-COVERAGE.md](ECL-COVERAGE.md) lists each one's variants and evidence. Reply acceptance means the registered host has performed the service. The native console registers none of these services. |
| Persistence and invocation timing | Program changes clear bound local flags and require explicit startup entry invocation. A full scheduler must handle movement-before-resolution, search-after-movement/Look, camp, load/save initialization and reentrancy. |

## Host behavior checklist from the research

Use the detailed source handlers before implementing a concrete game host. In
particular, preserve the distinctions between creature setup/loading, encounter
cleanup, combat versus shop/temple routing, and replacement of the active script
versus loading map resources. These are separate operations in the request API.

The [DOS probe](../dev_tools/docs/dos-probe.md) now records DOS events: the
script bytes the original reads and the variables it writes. The next validation
step is to compare request PCs, decoded operands, state writes and branch
choices with OpenGoldBox. Start with the existing Slums event fixture, replacing its
mock fight result with an observed result. Then cover map transitions, a text
input event, party queries and a table-driven random encounter. Record executable
version and resource hashes with each trace; findings from another release must
not silently alter this profile.
