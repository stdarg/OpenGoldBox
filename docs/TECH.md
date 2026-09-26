# OpenGoldBox Technical Design Document

Date: 2026-08-29
Status: Draft v0.1
Project: OpenGoldBox

## 1. Purpose

This document captures the current technical design for OpenGoldBox based on the technology decisions discussed in the "Pool of Radiance Re-Master" project chat. It complements the current PRD in `docs/PRD.md` by translating product direction into an implementable architecture.

OpenGoldBox is intended to be a modern, open-source reimplementation of the SSI Gold Box engine that:

- uses user-supplied original game files at runtime
- preserves original progression and content behavior while using the selected combat rules module
- replaces the original interface with a modern desktop UI
- starts with Pool of Radiance, but is architected to support additional Gold Box titles later

## 2. Source Decisions From The Chat

This design is based on these decisions and strong preferences from the referenced chat:

- use `OpenGoldBox` as the project identity rather than a Pool of Radiance-branded product
- build the presentation layer with `Godot 4.x`
- use `C/C++` for both engine logic and Godot-facing code
- keep the core engine independent from the renderer and UI
- do not emulate the original executable; read original data files directly
- require players to supply their own legally obtained game files
- decode and render original assets at runtime rather than distributing them
- treat original scripts and data as inputs to a new engine, not code to be copied
- support enhanced graphics through shader/filter services, with `xBR` as the leading option
- prefer permissive or file-level-copyleft dependencies and avoid incorporating GPL code into the codebase without an explicit licensing decision

One earlier exploratory answer suggested `C# + MonoGame`. Later discussion and the existing repository direction clearly settled on `Godot` as the chosen presentation stack, so this document treats `Godot + C/C++` as the active design.

## 3. Technical Goals

- deterministic, testable gameplay behavior
- strong separation between parsing, rules, campaign logic, and presentation
- minimal legal exposure from asset handling and reverse-engineering workflow
- high confidence in behavioral compatibility through automated tests
- reusable engine boundaries so Pool of Radiance is the first supported game, not a one-off implementation

## 4. Chosen Stack

### Runtime and Language

- `Godot 4.x` for desktop presentation, input, scenes, UI, shaders, and packaging
- `C/C++` as the primary implementation language across the project
- a native C/C++ toolchain for the engine, tools, and tests

### C++ Engineering Guidelines

- use `C++20` as the project language standard; do not depend on
  compiler-specific `C++ latest` behavior
- employ RAII for all owned resources so cleanup is deterministic and tied to
  object lifetime
- bias toward smart pointers when pointer ownership is required: prefer
  `std::unique_ptr` for exclusive ownership and use `std::shared_ptr` only when
  ownership is genuinely shared
- prefer value types and standard RAII containers, such as `std::vector` and
  `std::string`, over heap allocation when pointer semantics are unnecessary
- do not express ownership with raw pointers; raw pointers and references may
  be used only as non-owning views with lifetimes made clear by the API
- maintain separation of concerns between file-format decoding, engine and game
  rules, campaign-specific behavior, and Godot presentation code

Shared implementation boundaries:

- `OpenGold.Core/save_file` owns bounded file reads and verified, durable save
  replacement. Campaign serialization and the game's combat view use this service;
  neither duplicates platform file handling. Temporary files and native handles
  have scoped owners, including failed writes.
- `CombatDemo` holds a scoped campaign edit lock. A failed initial snapshot
  validation releases the lock before returning an error; completing or destroying
  combat releases it as well.
- The game's `godot_nodes.h` owns detached nodes until Godot accepts them as
  children. Returned node pointers borrow from the parent. `godot_images.h`
  centralizes conversion from engine RGBA images to Godot resources.
- The SRD module shares one deterministic dice implementation across character
  creation, combat, and temple healing, preserving saved random sequences.
- The SRD module's private `MovementGrid` centralizes movement-step legality and
  cost for planning, execution, and checkpoint validation. A single Dijkstra
  search supplies all legal destinations. Pure grid helpers and staged checkpoint
  validation are documented in the [complexity review](COMPLEXITY-REVIEW.md),
  including correctness arguments and exhaustive small-grid checks.

### Status Effects and Game Time

The SRD module owns one value-based effect model for PCs, recruited NPCs and
monsters. Base statistics remain authoritative; current effects contribute
contextual modifiers through shared sight, attack and saving-throw queries.
Each application records its own ID, scoped source, original save DC, duration
and recovery schedule. Source references are identifiers, never actor pointers.

`status_effects.h/.cpp` contains the testable lifecycle, save resolver and codec.
Combat owns participating actors' mutable effects. The campaign receives their
opaque rules continuation and updates reserve members separately. A snapshot's
elapsed-time delta is applied once, preventing duplicate recovery rolls.
Six-second rounds are partitioned across fixed initiative slots; exploration
steps also advance six seconds. Rendering, menus and wall-clock waiting do not
advance effects. Campaign time retains milliseconds within each minute, including
rest-completion precision. Effect processing orders simultaneous events by entity
and application ID so different time-update sizes preserve RNG continuation.

Rules 0.5.0 adds Blinded through the blindness option of Blindness/Deafness.
Combat checkpoint version 13 stores all effect applications, timers, presentation
facing, pending movement reactions, involuntary allied overlap, weapon grip, remaining Hit Dice, mortality recovery clocks and sourced Temporary HP and pending Savage Attacker decisions. Rules 0.6.21
migrates modules 0.6.4/0.6.5/0.6.6/0.6.7/0.6.8/0.6.9/0.6.10/0.6.11/0.6.12/0.6.13/0.6.14/0.6.15/0.6.16/0.6.17/0.6.18/0.6.19/0.6.20 format-5/6/7/8/9/10/11/12/13 checkpoints, validating then canceling
obsolete facing-only queues without changing spent resources, HP or time.
Genuine movement queues retain their saved progress; future weapon attacks use
the corrected [Heavy requirements](HEAVY-WEAPONS.md). The
[weapon catalog](WEAPON-CATALOG.md) contains all 38 SRD weapons while retaining
original campaign conversions and prices. The [armor catalog](ARMOR.md)
uses shared category definitions for equipment, AC and starting-class training;
equipped ability checks combine these penalties with character skill/tool grants.
Core only collects equipped IDs and delegates the query to the rules module. Campaign
version 9 stores the clock, encounter scopes, rules-owned effect state, grip and
acquired feature/feat grants with source IDs, acquisition levels and choices.
Version 9 also stores training selections and source grants. Existing campaign
formats 1–8 migrate, preserving missing selections as pending. Campaign version
10 adds completed Short Rest spending tickets and individual eligibility records;
formats 1–9 migrate without inventing a spending session. Core owns campaign clock advancement
and transactional commits; the rules module owns rest qualification, timing,
interruption/resumption decisions, completion benefits and resource arithmetic. Campaign
format 12 adds resumable rest activity when present, retaining format 11 for
ordinary saves; native activity drives the existing atomic camp/inn route, with
shared game/demo controls and ECL damage/encounter interruption adapters
delivered; sleeping actor behavior and final rest scheduling remain pending. See [training](TRAINING.md),
[rest resources](REST-RESOURCES.md), [recovery clocks](RECOVERY-CLOCKS.md) and [status effects](STATUS-EFFECTS.md) for
mechanics, scope, persistence and tests.

Wizard choice history conditionally uses campaign format 16. Core replays spell
edits at their attained level between advancement records; SRD validates learning,
preparation and cantrip replacement. A completed Long Rest creates a once-only
choice window for eligible members before campaign actions resume. PC34 adds
replacement provenance while retaining earlier combat recipes. See
[Wizard spell choices](WIZARD-SPELL-CHOICES.md) for the bounded catalog and
compatibility evidence. Formats 1–15 remain supported.

Rules 0.6.19 derives [Wizard spell access](SPELL-ACCESS.md) from sourced grants,
separating known cantrips and retained book entries from current preparation.
New PC10 recipes validate casting access against those grants. Campaign replay
recovers only the preset and actual advancement selections; old combat recipes
retain their recorded access. Full selection controls and source-specific free
casts remain separate work.
[The Savage Attacker decision](SAVAGE-ATTACKER.md) retains a value-based pending
hit until a legal player choice resolves damage and any interrupted movement.
Snapshots expose display values; rules validate choices and Godot owns the dialog.

Rules 0.6.21 adds [Somatic hand eligibility](SPELL-COMPONENTS.md) to the existing
spell command query. Component definitions remain in the SRD library; Core and
Godot do not duplicate hand rules or spell requirements. Existing saved equipment
and attack grips are retained. Speech-blocking sources remain separate work.

Rules 0.6.23 adds [Sacred Flame](SACRED-FLAME.md) through the shared spell-access,
saving-throw and typed-damage services. The class-driven Godot cantrip control
now supports Clerics and hides stale choices when the class changes. PC12
validates the grant/casting mask; Core remains independent of spell mechanics.

Rules 0.6.24 keeps the ordinary and Action Surge allowances in a rules-owned
`ActionBudget` value. Magic eligibility consumes only the ordinary action;
other actions spend the restricted allowance first. Existing legal-command
and recovery-pool queries expose the feature without SRD parsing in Core/UI.
See [Action Surge](ACTION-SURGE.md) for persistence and migration boundaries.

### Why This Stack

`C/C++` fits the project because it supports a portable native engine with direct integration into Godot:

- binary format parsing
- rules processing
- combat state
- scripting/event execution
- save/load behavior
- compatibility quirks
- automated testing

`Godot` is used as the shell around that engine because it gives the project:

- fast UI iteration
- scene composition
- built-in 2D rendering
- shader support
- input abstraction
- desktop export targets

## 5. High-Level Architecture

OpenGoldBox should be implemented as a small set of clearly separated modules:

```text
src/
  OpenGold.Core/
  OpenGold.Rules/
  OpenGold.Rules.Srd5/
  OpenGold.Formats/
  OpenGold.Game.PoolOfRadiance/
  OpenGold.Godot/
  OpenGold.Tools/                 optional

tests/
  OpenGold.Tests/
```

### `OpenGold.Formats`

Responsible for reading and decoding original game data:

- DAX containers
- maps
- portraits
- combat sprites
- wall and scene art
- items
- monsters
- characters
- save structures used for analysis or import
- ECL script data

This layer converts raw files into typed structures. It should not contain gameplay rules or UI logic.

### `OpenGold.Core`

Responsible for engine behavior implemented by OpenGoldBox:

- world state
- party state
- campaign-to-combat adaptation through `OpenGold.Rules`
- exploration movement and encounter setup
- inventory/equipment logic
- event dispatch
- compatibility behaviors

This layer must have no dependency on Godot. It should be runnable directly under the native test suite.

### `OpenGold.Rules` and `OpenGold.Rules.Srd5`

The C++20 rules interface provides encounter creation, immutable display
snapshots, legal commands, validated submission, outcomes and versioned
checkpoints. The SRD 5.2.1 implementation owns combat turns, movement/targeting,
attacks, spell resources, HP, dice and serialization. It depends on the interface
and standard library, not Godot, ECL or original creature file formats. The
application injects a module into the campaign adapter. See [RULES.md](RULES.md).

### `OpenGold.Game.PoolOfRadiance`

Responsible for Pool of Radiance-specific configuration and adapters:

- campaign bootstrap
- title-specific content wiring
- title-specific compatibility workarounds
- mappings between decoded data and engine concepts

This allows future support for additional Gold Box games without contaminating the generic engine.

### `OpenGold.Godot`

Responsible for presentation and interaction:

- scene composition
- rendering
- input mapping
- UI layout
- audio hooks
- animation hooks
- filter selection
- user-facing installation/import flow

This project should consume the engine rather than define it.

## 6. Data Flow

The intended runtime flow is:

```text
User selects Pool of Radiance installation
        ↓
OpenGoldBox validates required files
        ↓
OpenGold.Formats decodes DAX/ECL/maps/items/graphics
        ↓
OpenGold.Game.PoolOfRadiance maps title data into engine inputs
        ↓
OpenGold.Core simulates gameplay
        ↓
OpenGold.Godot renders state and collects player input
```

The original SSI executable never runs. OpenGoldBox interprets game data through its own code.

## 7. Reverse Engineering Approach

The project should follow a clean-room-compatible discipline even if one developer performs multiple roles:

- prefer published format documentation and black-box observation first
- inspect binary behavior only when needed to answer a specific compatibility question
- document findings as specifications, not code translations
- implement from specification, not from copied routines

Recommended documentation outputs:

- `docs/dax-format.md`
- `docs/graphics-format.md`
- `docs/map-format.md`
- `docs/item-format.md`
- `docs/monster-format.md`
- `docs/character-format.md`
- `docs/ecl-format.md`
- `docs/combat-behavior.md`

The key rule is that repository artifacts should describe file formats and observed behavior, not reproduce original source or decompiled logic.

## 8. Content and Asset Strategy

OpenGoldBox must not ship copyrighted Pool of Radiance assets or data. The application should:

- prompt the user to locate a valid game installation
- verify required files are present
- decode assets locally at runtime
- optionally cache derived textures on the user's machine

It must not:

- include extracted original sprites in the repository
- ship original maps, text, portraits, or music
- convert copyrighted assets into distributable project content

This keeps the distribution limited to OpenGoldBox-authored code, UI, shaders, and documentation.

## 9. Graphics and Rendering Design

The rendering strategy should separate original artwork from modern UI.

### Original Art Layer

This layer renders decoded game assets such as:

- combat sprites
- portraits
- map tiles
- wall textures
- scene illustrations

### Exploration Map Knowledge

Normal overhead maps show only cells the party has occupied or seen in the
first-person exploration view. Unknown cells are solid black, with no grid,
walls, or doors. Known cells stay revealed after moving or turning away. The
full/visited map toggle is removed. `--no-fog` reveals the whole overhead map
for inspection; it changes presentation only and is never saved as exploration.

`TourSnapshot` keeps distinct 256-bit `visited` and `seen` sets. An occupied
cell is always in both; merely seeing a cell does not mark it visited. The
session retains each district's history when switching maps. Restarting the
campaign clears both histories, and failed event rollback restores both.

`render_exploration_view` produces the 88×88 image and visible-cell mask together.
Each pixel records its source cell as walls are painted from far to near.
Opaque foreground pixels replace the previous owner; transparent openings
preserve the surface behind them. The featureless floor is assigned cells in
the same far/middle/near projection bands, bounded to the renderer's two cells
ahead. Sky and the horizon reveal no distant map cells; map edges do not wrap.
This uses the artwork actually shown, including directional wall differences,
rather than treating a traversable door as automatically transparent.

`RolfTourSession::observe_view` merges that mask into persistent knowledge when
the game composes its exploration image. Encounter pictures do not discover a
new sightline. An opening script must publish its starting pose before discovery
can occur. The core owns all history without depending on Godot. Campaign save
version 5 stores per-district seen masks; versions 1–4 migrate their visited
history and discover the current sightline when it is next displayed. See
[campaign saves](SAVES.md).

### Goliath Combat Sprites and Draw Order

Approved rendering decision, 2026-09-18: use the **stretched** Goliath treatment
selected in the [combat sprite demo](SPRITE-DEMO.md).

- A Goliath occupies **one square**, the lower square under its feet. Its
  position, movement, pathfinding, collision, targeting, and selection refer to
  that square. Sprite dimensions never create additional occupied cells.
- Its visible artwork is exactly **one square wide and 1.25 squares tall**.
  Scale the axes independently using the nontransparent bounds, center it
  horizontally, and align its visible feet to the bottom of the occupied square.
  This makes it extend into the bottom 25% of the square above. Ignore transparent
  source padding when fitting and anchoring each rendered pose.
- **Monsters may occupy the square above the Goliath.** The overhanging pixels
  do not reserve that square or redirect a click there to the Goliath.
- Render combat sprites **from the bottom of the screen toward the top**:
  descending battlefield Y, because Godot Y increases downward. This always
  applies to monsters. Players share the same pass so a monster in the upper
  square is drawn after the Goliath below and covers its overlapping pixels.
  Break same-row ties by ascending X and then entity ID for deterministic output.
  Sort a separate draw list; retain the rules snapshot's initiative order.
- Terrain and cell markers render before sprites; health bars render afterward.
  Panning and zooming preserve this ordering. Dead/unconscious figures retain the
  same rendering order and existing tint treatment.

The game identifies Goliaths using the character's stable `goliath` species ID.
The game and demo share `combat_sprite_layout.h` for the visible-bounds fitting.
Native layout tests verify size, padding, anchoring, zoom, and row order. Party
tests verify a monster can move into the square above a Goliath and retain that
position through save/restore. Combat input continues using logical cells.

### Modern UI Layer

This layer replaces the original interface with native presentation:

- modern fonts
- tooltips
- combat log
- party panel
- action bars
- character sheets
- inventory screens
- journal and map views

The UI should be drawn natively in Godot rather than by scaling the original text interface.

### Filter Pipeline

The chat strongly favored `xBR`-style enhancement as the first upscale path. The preferred architecture is:

```text
Decoded original image
        ↓
Graphics filter service
        ├── Original
        ├── Nearest
        ├── Scale2x
        ├── xBR
        └── optional CRT/post effects
        ↓
Godot texture output
```

Important constraints:

- do not embed GPL `xBRZ` source directly unless the repository license strategy explicitly changes to allow that
- prefer permissively licensed shader implementations or an original implementation
- treat AI-assisted upscaling as optional and post-1.0
- never ship AI-derived versions of original assets as bundled content

### Texture Cache

Derived textures may be cached locally to avoid repeated processing:

```text
user-data/
  cache/
    por/
      xbr4/
      nearest/
```

This cache is a local optimization, not a distributed asset pack.

## 10. Scripting and Event Execution

The ECL system is a critical part of the design.

OpenGoldBox should:

- decode ECL resources into typed instruction data
- execute those instructions in an OpenGoldBox-owned runtime
- let original scripts drive progression and event behavior where practical

Target flow:

```text
ECL*.DAX
   ↓
EclDecoder
   ↓
Instruction stream / IR
   ↓
OpenGoldBox ECL runtime
   ↓
Core state changes, encounters, dialogs, flags, transitions
```

This is preferable to rewriting campaign progression as hand-authored C/C++ scene logic because it keeps the engine reusable and preserves original behavior more faithfully.

## 11. Rules and Compatibility

The approved baseline (2026-09-06) is **SRD 5.2.1**, using standard turns and
spell slots. Open5E is a content source, not a rules engine. A versioned offline
snapshot and curated profiles avoid a runtime API dependency. The SRD module
replaces the earlier plan to reconstruct original AD&D attack calculations,
initiative, spell memorization and class restrictions.

Campaign adapters preserve original encounter identities/counts and script-visible
results. They explicitly select converted combat definitions without rewriting
raw `CreatureCatalog` data. Original maps, dialogue, quest flags and progression
remain reverse-engineering targets. Original tactical geometry is pending; the
first combat milestone uses an approved authored arena and fixed party.

The UI and demonstration AI submit commands offered by the selected rules
module. They do not calculate hit chances, damage or resource costs. Saves bind
to module and content identities; unknown definitions and incompatible saves
fail explicitly. Replacement currently occurs at C++ composition/build time,
not through a dynamic plugin ABI. The demo UI knows its presented action verbs.

Document unimplemented SRD mechanics and conversion choices in [RULES.md](RULES.md).
The first playable subset is not a claim of full SRD or campaign compatibility.

## 12. Testing Strategy

Testing is a first-class architectural concern.

### Automated Tests

The project should maintain a compatibility-heavy test suite covering:

- parser correctness
- rule calculations
- combat edge cases
- spell behavior
- event/runtime execution
- state serialization

Parameter-heavy rule verification is one reason a strongly typed C/C++ implementation was preferred. The project should select one consistent native testing framework early.

Native checks must remain active when `NDEBUG` is defined; use explicit failures
instead of C `assert` for test expectations. Synthetic fixtures exercise malformed
sprite dimensions, in-memory SRD content parsing, campaign encounter placement,
and checkpoint failure recovery without an installed original game.
`srd5::parse_content` owns the parsed definitions and shares validation and content
identity calculation with the filesystem loader.

The [native coverage and fuzzing guide](TESTING.md) documents synthetic art and
effect contracts, deterministic mutation checks in CTest, and optional isolated
Clang libFuzzer builds with ASan/UBSan. Accepted combat checkpoints must remain
saveable after legal commands; mutation tests verify round trips, deterministic
continuation, and rejected-command atomicity.

Game builds also register headless Godot checks with CTest. Their fixture builds
the extension, copies the pinned rules, and imports the game project without an
export or original game data. Each script must exit successfully and print its
completion marker within a timeout. See the [game test commands](../src/OpenGoldBox/README.md#tests).

Runtime screenshots belong to the Godot presentation boundary. The native
`ScreenshotService` autoload captures completed renderer frames and owns no
campaign state. Global input routes Ctrl+S to that service, including input from
dialog windows; a local request file exposes the same capture operation to tools.
Godot references and scene ownership manage images, file handles, and notification
nodes. The service continues processing while paused and bounds waits for frames.

### Behavioral Oracle Testing

The original game should be used as an observation oracle:

- construct repeatable scenarios
- record outcomes from the original game
- encode expectations as tests

High-value cases include:

- spells
- surprise and initiative
- ranged combat
- condition timing
- resting
- fleeing
- event triggers

### Differential Format Testing

For uncertain binary structures:

- create small controlled differences in original saves or game state
- diff the resulting data
- map offsets to meaning
- encode findings in parser tests

## 13. UI Design Constraints

The technical design assumes a modern mouse-first desktop UI with keyboard support.

### Display baseline

The game application in `src/OpenGoldBox/godot` uses **1920 x 1080 pixels
(Full HD, 16:9)** as its reference viewport and default requested window client
size. These dimensions exclude the operating system's title bar and borders.
The existing resizable game screens retain their 1120 x 800 minimum client size;
window managers may constrain the initial window to the available desktop area.
Fullscreen is available through Godot's `--fullscreen` launch option and uses
the display's fullscreen dimensions. The 1920 x 1080 baseline is a design target,
not a requirement to change the monitor's display mode.

The two startup screens share one static texture,
`art/OpenGoldBoxSplashBackground.png`. It is loaded once; advancing the sequence
changes only a transparent lettering texture containing the title and supporting text. The background's
pixels, placement, and sampling remain identical across both screens.
The lettering textures reproduce the reference's textured metallic gold faces,
beveled rims, shadows, and slender ivory serif supporting text. Godot displays
them with aspect-preserving `TextureRect` fitting over the shared backdrop;
no installed font is required. Both overlays have genuine alpha transparency.

The generator returned a 1672 x 941 background despite the requested Full HD
canvas. It remains unmodified. Godot applies a uniform aspect-preserving fit;
at 1920 x 1080 this leaves about one pixel of total horizontal margin. Other
aspect ratios use black letterboxing. The generated lettering layers also remain at their original 1672 x 941 size. The two 1920 x 1080 PNG screen previews in `art/` are captured from
Godot's final rendering; the runtime uses the shared backdrop and transparent lettering layers.

Startup retains the optional `--splash` sequence: engine introduction, then
Pool of Radiance introduction, then character creation. The background appears
immediately. Each lettering layer fades linearly from transparent to opaque
over 0.6 seconds, using C++ `_process` delta time. A key clears the first lettering
and starts the second fade; a further key opens character creation. Input
remains responsive during fades and held-key repeats do not advance.
Only the lettering opacity animates; the shared background remains unchanged.
Any key advances;
Escape skips the splash sequence. Default startup opens character creation.
The build copies the local shared backdrop from `art/` into the game package.
See `art/OpenGoldBoxScreens.provenance.md` for artwork sources and prompts.
The rendered startup check compares both backgrounds byte for byte with the
text hidden, in addition to verifying the selected overlay, its alpha transparency, and navigation behavior.

### Localization

The game uses Godot TranslationServer with English and Spanish gettext catalogs.
Native presentation translates complete messages with named arguments; the
C++20 rules modules emit engine-independent message templates and literal values.
Stable identifiers, saved names and campaign state do not depend on language.
Scene-authored text is translated once before native refresh methods take over.

`--reset-lang` opens a centered modal before the optional splashes, with English and
Español rows, mouse/arrow-key selection and Enter or the translated Continue button
to confirm. Highlighting a row previews the dialog text in that language while
language names retain their native spelling. Only confirmation changes the
active language and saves it in `settings.cfg` beside the executable. Later launches
use the saved value or environment override; a missing value prompts again.
Initial setup follows the system language, with English fallback.
Closing the selector follows the global graceful exit path.
Spanish splash lettering uses `.es.png` variants over the same shared background
and the title Estanque de Resplandor. Original campaign dialogue supports separate
context-keyed overrides and retains source text when untranslated.
See [LOCALIZATION.md](LOCALIZATION.md) for catalog maintenance, artwork and coverage.

`application_settings` resolves the original-game path for all game views and
save identity checks. Portable `settings.cfg` beside the executable stores the
path and language; missing/invalid values prompt before splashes, language first.
The development editor uses an ignored project-local config. `--reset-game-path`
and `--reset-lang` force selection without deleting prior values. Environment
overrides are `OPENGOLD_GAME_DIR` and `OPENGOLD_LANG`; explicit selections and
reset flags take precedence for the current run. MD5 checks against a bundled
PC 1.3 manifest produce a Quit/Continue warning on mismatches. Missing files
must be corrected. See [CONFIGURATION.md](CONFIGURATION.md) for full semantics.

### Screen structure

Primary Godot-driven screens include:

- exploration
- combat
- character sheet
- inventory
- spellbook
- journal
- onboarding/import

The UI should support both a faithful and enhanced presentation model:

- `Classic`: original mechanics and conservative visual treatment
- `Enhanced`: optional quality-of-life overlays such as movement range, spell radius, combat detail, and quick-save affordances

## 14. Platform Targets

The chosen stack is optimized for desktop releases:

- Windows first
- Linux next
- macOS where practical

Web export is not part of the primary design. That aligns with the game’s desktop-oriented workflow and keeps the initial platform scope focused.

### 14.1 Windows and macOS are both first-class for development

**The project must configure, build and test on both Windows and macOS.** This
is a requirement on the repository, not a preference: work that only builds on
the machine it was written on cannot be verified by anyone else, and a check
that cannot run is not a passed check.

Concretely:

- **Do not hard-code a developer's paths, build directory or host tools.** A
  build directory name that encodes a platform (`build/mac-check`) belongs in a
  personal note, not in a document that tells the next contributor how to
  verify. Refer to the CMake presets instead; add a preset when a platform
  genuinely needs different cache variables.
- **Do not put host-specific commands in shared instructions.** `afplay`,
  `/usr/bin/…` and `/Users/<name>/…` are macOS-only and break the Windows
  workflow silently — the command simply fails and the step gets skipped.
- **Assume nothing about auxiliary tooling.** As of 2026-09-26 the Windows
  development machine has no `python3` (the `python` on `PATH` is a Microsoft
  Store stub) and no `clang-format`. Any procedure that depends on them must
  either state the install step or degrade to "unverified", never to "passed".
  `tools/localization.py --check` is affected.
- **Keep the entry points symmetrical.** `build.cmd` is the Windows path
  (vcvars, configure, build, `ctest`, with `errorlevel` checked between steps).
  macOS and Linux need an equivalent shell script with the same contract, so
  that "run the suite" is one documented command on every platform.
- **Say which platform a check belongs to.** The Godot runtime tests and the
  rendered English/Spanish layout reviews require a Godot install; the native
  CTest suite does not. A coverage claim should record where it ran, so a
  missing toolchain shows up as a gap instead of an assumed pass.

### 14.2 Portability rules for code

- Prefer the C++20 standard library over platform APIs; use
  `std::filesystem::path` rather than string concatenation with separators.
- Never assume a path separator, a case-sensitive filesystem, or a line ending.
  Original DOS game data is upper-case and case-insensitive; the repository is
  developed on both a case-insensitive (Windows, default macOS) and a
  case-sensitive filesystem, so file lookups must not depend on the difference.
- Keep MSVC and Clang both warning-clean. They disagree on enough of C++20 that
  code compiled against only one regularly fails on the other; designated
  initialisers, aggregate initialisation and `constexpr` evaluation limits are
  the usual culprits.
- Text written for later parsing is UTF-8 without a BOM and with `\n`, on every
  platform. On Windows, PowerShell's `Set-Content` and `Out-File` do not default
  to that; pass the encoding explicitly.
- CI should build both platforms. Until it does, a change that touches the build
  system, file IO or tooling is unverified on the platform it was not written on,
  and should say so.

## 15. Dependency Policy

Dependency selection should follow the licensing posture described in the source chat:

- preferred: `MIT`, `BSD-2`, `BSD-3`, `Apache-2.0`, `MPL-2.0`
- review carefully: `LGPL`
- avoid incorporation by default: `GPL`, `AGPL`

Reference implementations may still be studied, but source incorporation must respect license boundaries.

Examples from the source chat:

- `Gold Box Explorer` is a good implementation reference because it is MIT licensed and focused on data extraction
- `Dungeon Craft` is useful as a behavioral reference, but its GPL license makes direct code reuse undesirable under the current project direction

## 16. Prototype Plan

The recommended first technical milestone is a rendering-first proof of concept:

1. validate a user-supplied Pool of Radiance installation
2. decode one portrait, one combat sprite, one wall asset, and one map asset
3. display them in Godot
4. apply filter switching between original and enhanced modes
5. mock a modern combat screen around real assets

This proves:

- file ingestion
- legal asset boundary discipline
- presentation viability
- graphics enhancement pipeline

before deeper investment in rules fidelity and full campaign support.

## 17. Risks

- exact event and rules compatibility will be harder than asset decoding
- ECL semantics may contain title-specific behavior that resists generic modeling
- license mistakes in scaler or reference-code selection could create avoidable problems
- too much early abstraction for future Gold Box titles could delay a playable Pool of Radiance milestone

## 18. Current Recommendation

The current technical recommendation is:

- `Godot 4.x` for presentation
- `C/C++` across engine and UI code
- a native C/C++ toolchain for the project baseline
- a layered architecture centered on `Formats`, `Core`, `Game.PoolOfRadiance`, and `Godot`
- user-supplied original assets loaded at runtime
- ECL-driven campaign execution
- shader-based graphics enhancement with `xBR`-style filtering
- test-driven compatibility work from the start

This gives OpenGoldBox the best balance of faithfulness, maintainability, legal caution, and room to grow into a reusable Gold Box engine.

### Equipment sprite composition and shared equipment operations

The equipment demo uses a temporary `CampaignParty` with the same `equip`,
`unequip`, rules metadata, profile validation and atomic replacement as the
main game. SRD 0.6.0 supports every ordinary reviewer weapon through explicit
original-to-SRD conversions (see `docs/PARTY.md`). Saved ordinary items that
used the old unsupported key migrate only when original provenance matches.

The equipment resolver retains the character's saved anatomy. Its shared
`icon` method composes stable torso/clothing/legs with wielding arms and gear
from the reviewed equipment pose; head composition and palette selection follow.
Runtime masks and restoration use locally decoded original records. The catalog
continues to describe complete reference poses and is never rewritten by the
demo or compositor. Missing/deleted mappings visibly fall back to unarmed.

Rules 0.6.15 also persists Orc Adrenaline Rush uses and pending Temporary HP
replacement in combat format 12 and SRD7; see [Temporary HP](TEMPORARY-HP.md).

Rules 0.6.22 adds [Poison Spray and explicit Wizard cantrip choices](POISON-SPRAY.md),
with the approved creation and main combat controls. Campaign 11 stores choices;
PC11 validates their grants. Combat 13 and existing resource formats remain.
Full spell selection, speech blocking and other granting sources remain open.

Rules 0.6.25 adds [Ray of Frost](RAY-OF-FROST.md) through sourced Wizard access,
typed attacks and the shared effect/recovery timeline. Nonstacking Speed penalties
are applied to base movement remaining and recorded Dash allowances, preserving
movement already spent. FX2 represents effects with no repeat saving throw;
PC14 and combat 15 validate the new access and movement continuation.

The concentration transition helper owns only optional value state and returns
source identities for cleanup. Combat and campaign integration remain separate;
see [concentration](CONCENTRATION.md). No new runtime or save format is introduced
by the foundation.

Rules 0.6.26 adds the Sage fixed training package. PC15 validates those grants;
prior profiles use the previous training policy. Campaign reconstruction adds
only owed fixed grants from the existing background. The shared Training display
and modifier queries expose them; see [Sage training](SAGE-TRAINING.md).

Rules 0.6.27 adds Acolyte and Soldier fixed proficiencies through the shared
training service. PC16 uses a versioned training policy; PC15 retains Sage-only
additions and older profiles retain their original catalog. Campaign replay adds
only owed fixed grants. Existing translated Training and character-sheet controls
consume the same rules data. See [background training](BACKGROUND-TRAINING.md).

Rules 0.6.28 derives Archery's attack bonus from the validated feat grant and
Ranged weapon category. PC17 carries its feature bit; old profiles and campaign
identities reject future Archery grants. The existing advancement selector reads
the available option from the rules module. Core and Godot do not implement the
bonus. See [Archery](ARCHERY.md) for available routes and remaining class work.

Rules 0.6.29 adds Fighter starting styles as a single-selection training group.
Core replaces that group's selection atomically; Godot renders the approved
standard dropdown using group metadata. Style feats remain subject to feature
validation as well as training validation. PC18 accepts their distinct level-one
source; old profile policies and identities reject it. Existing campaign choice
maps need no schema change. Old choices remain pending. See
[Fighter starting styles](FIGHTER-STYLES.md) for evidence and remaining work.

The internal damage roller owns no resources and accepts value-only dice and
an explicit die-value rule. Current combat invokes the normal rule; the Great
Weapon Fighting replacement helper is tested but awaits feat/control integration.
It never rerolls or doubles flat modifiers on critical hits. Actual pre-extraction
Savage Attacker checkpoints prove unchanged continuation. See
[Great Weapon Fighting](GREAT-WEAPON-FIGHTING.md).

Rules 0.6.30 adds the eleven missing starting class skill catalogs to the shared
training service, preserving Rogue's catalog. PC19 selects that policy; older
profiles retain their original lists. The campaign choice map is unchanged and
old missing selections remain pending. Reused checkbox callbacks are rebound to
the current class entitlement without replacing the focused control. Presets
use the existing rules-driven generator. See [class skills](CLASS-SKILLS.md).

Class skill groups share a continuity key while retaining distinct source IDs.
Core uses that metadata to carry selections across class changes, then prunes
options and counts against the new group. Rules remain responsible for lists,
counts and provenance; Core contains no SRD class-specific transfer logic.

Rules 0.6.31 / PC20 adds Bard instrument choices to the existing rules-owned
training service. Older recipes retain their original entitlement policy; campaign
choice maps need no schema change. The shared Training controls render the new
group and translated tool sources without SRD mechanics in Core or Godot. See
[Bard instruments](BARD-INSTRUMENTS.md).

Rules 0.6.32 / PC21 adds Monk starting tool choices. Tool-category eligibility
stays in SRD rules, with the existing generic Training controls and choice
continuity metadata shared by Bard and Monk. No Core class-specific behavior or
new persistence schema is introduced. See [Monk tools](MONK-TOOLS.md).

Rules 0.6.33 / PC22 adds fixed Druid Herbalism Kit proficiency. Historical
campaign ledgers validate against the pre-grant policy before reconstruction
adds the fixed entitlement. Existing combat recipes retain their original
policy and continuation. No new control or save schema is needed. See
[Druid Herbalism Kit](DRUID-HERBALISM.md).

Rules 0.6.34 / PC23 adds Soldier Gaming Set choices through the existing training
service and checkbox presentation. Tools use an explicit category enum to keep
artisan, instrument and gaming entitlements distinct. Source validation and
legacy policy handling remain rules-owned; Core and save schemas are unchanged.
See [Soldier Gaming Set](SOLDIER-GAMING.md).

Rules 0.6.35 / PC24 adds bounded Rogue level-two advancement and Cunning Action
Dash/Disengage. Rules-derived Bonus Action entitlements feed the approved combat
row; Godot does not calculate eligibility or movement. Existing combat format 15
stores Dash counts and existing action budgets; old profiles retain their original
behavior. See [Cunning Action](CUNNING-ACTION.md).

Rules 0.6.36 corrects movement through Unconscious enemies. The shared value-owned
occupancy grid distinguishes blocked enemies from passable incapacitated enemies,
with a single Difficult Terrain surcharge. Existing movement controls reuse rules
queries; formats and character grants are unchanged. Enemy-overlap continuation
is validated against module version. See [Unconscious transit](UNCONSCIOUS-TRANSIT.md).

Rules 0.6.37 adds [level-one Warlock Eldritch Blast](ELDRITCH-BLAST.md) through
explicit Pact Magic cantrip grants and the approved shared selection/casting
controls. PC25 validates access and uses Charisma; campaign/combat formats stay
unchanged. Old missing choices remain pending. Object targets, later Warlock
levels, slots, invocations and general speech blockers remain separate work.

Rules 0.6.38 adds [Wizard Shocking Grasp](SHOCKING-GRASP.md) at levels 1–4.
The shared value-owned effect model records sourced Opportunity Attack
suppression; combat uses it when forming and validating movement interruptions.
Reaction budgets are unchanged. PC26 validates access, FX3 encodes the new effect,
and existing campaign/combat schemas and old choices remain compatible.

Rules 0.6.39 adds [level-one Warlock Poison Spray access](WARLOCK-POISON-SPRAY.md).
PC27 validates the distinct Pact Magic source and casting mask; existing Charisma
casting, spell effects and shared Godot controls are reused. Old choices remain
unchanged, while new presets fill both available starting cantrips.

Rules 0.6.40 adds [level-one Sorcerer cantrips](SORCERER-CANTRIPS.md): explicit
source-owned Fire Bolt, Poison Spray, Ray of Frost and Shocking Grasp choices.
PC28 validates access and Charisma attacks. Existing Spell Choices/Spell/Cast
controls are reused under Q27; old choices and save schemas remain unchanged.
Full Sorcerer spellcasting and Innate Sorcery remain #132.


## Static SRD library boundary

`opengold_rules_srd5` is a C++20 static library. Its only project-library dependency
is `opengold_rules`, the engine-independent interface/value types. It does not
link Core, original game formats, or Godot. `opengold_core` links the interface,
not the SRD implementation. The application selects/loads SRD at composition time
and injects a `RulesModule`; presentation calls Core or the rules interfaces.

SRD rest transitions live in `OpenGold.Rules.Srd5/src/rest_activity.cpp`.
Core supplies progress and events and applies the returned continuation and
benefits transactionally. Core retains member IDs, tickets, campaign clocks and
save serialization. Rest progress value types cross the interface without Core
or Godot types. Save fields and existing SRD behavior remain unchanged. Character
preset ability priorities are queried from `CharacterRules`, not inferred from
class names in Core.

`alternate_rules_boundary` in `campaign_rest_tests.cpp` supplies intentionally
non-SRD cancellation, continuation, default activity and completion benefits. It
checks that Core follows the injected implementation. Existing SRD activity and
save fixtures verify that this separation preserves game behavior. Original game
content IDs, art mappings and authored demo parties remain campaign content;
they are not rules calculations.


Wizard Scholar uses generic advancement training groups with acquired-level
metadata. Core stores those choices in advancement history (conditional campaign
format 15) and replays isolated candidates for Review Training, preserving live
vitals. The static SRD module owns eligibility, level-two source validation and
Expertise arithmetic; PC33 validates combat provenance. Godot presents the
approved dropdown/checkbox controls without SRD decisions. See [Scholar](SCHOLAR.md).
