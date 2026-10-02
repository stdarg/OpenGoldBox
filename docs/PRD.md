# Product Requirements Document: OpenGoldBox

**Document:** PRD.md  
**Project:** OpenGoldBox
**Status:** Draft / Living Document  
**Primary Platform:** Desktop PC  
**Target OS:** Windows, Linux, macOS  
**Engine:** Godot 4.x  
**Core Language:** C/C++  
**Godot Integration:** GDExtension  
**License for Original OpenGoldBox Code:** MPL-2.0

---

## 1. Product Summary

OpenGoldBox is an open-source modernization and reimplementation of the 1988 SSI *Pool of Radiance* game experience.

The project aims to preserve the campaign, content structure, maps, encounters, and feel of the original game while replacing its DOS-era executable and interface with a modern, maintainable implementation.

**Combat decision, 2026-09-06:** use SRD 5.2.1, with standard turn-based rules and
spell slots, behind a replaceable C++ rules-library interface. Open5E supplies
reference content. This supersedes earlier requirements for original AD&D combat
calculations, class restrictions and spell memorization. Preserve original
campaign/script behavior and encounter identities/counts through explicit
conversion adapters. The first milestone uses an approved fixed party, curated
monsters and authored test arena; see [implementation scope](RULES.md).

OpenGoldBox is **not** intended to redistribute copyrighted SSI, TSR, Wizards of the Coast, or other third-party game assets. Users must provide their own legally obtained copy of the original game files. OpenGoldBox will read and interpret those files at runtime.

The project should ultimately allow a player who owns the original game to launch OpenGoldBox, point it at the original game data, and play a faithful version of *Pool of Radiance* through a modern interface.

---

## 2. Vision

> Rebuild the original *Pool of Radiance* experience faithfully, while making it pleasant to run, understand, mod, maintain, and play on modern computers.

The project should feel like the original game and received a careful restoration rather than a redesign.

The player should recognize the original game immediately.

Modernization should improve presentation and usability without silently changing the underlying game.

---

## 3. Goals

### 3.1 Primary Goals

1. Reimplement the original game's executable behavior without using the original executable code.
2. Reuse original game data and art supplied by the user.
3. Preserve original campaign behavior while implementing the selected SRD 5.2.1 combat rules through a replaceable library.
4. Replace the DOS user interface with a modern desktop UI.
5. Support modern resolutions, windowed mode, fullscreen mode, scaling, and input.
6. Keep the core game implementation portable and testable.
7. Make the project easy for outside contributors to understand and extend.
8. Provide a clean path for reverse engineering additional file formats and game systems.
9. Allow visual improvements such as optional pixel-art upscaling without destroying the original aesthetic.
10. Build a reusable foundation that could potentially support other SSI Gold Box titles later.

### 3.2 Secondary Goals

- Faster loading and saving.
- Multiple save slots.
- Better inventory management.
- Improved character sheets.
- Modern tooltips and contextual explanations.
- Configurable keyboard shortcuts.
- Mouse-driven interaction where appropriate.
- Accessibility improvements.
- Optional quality-of-life features that can be disabled for strict-faithful play.

---

## 4. Non-Goals

The initial project is **not** intended to:

1. Create a new Dungeons & Dragons game.
2. Reverse engineer exact original AD&D combat calculations for the initial rules module.
3. Rewrite the campaign.
4. Replace original encounters with newly authored encounters.
5. Convert the game to real-time combat.
6. Turn the game into a 3D title.
7. Redistribute copyrighted original game data.
8. Ship the original DOS executable.
9. Require DOSBox.
10. Reproduce bugs merely because they existed in the original, unless a bug materially affects compatibility or expected game behavior.
11. Support every Gold Box game in the first release.
12. Introduce multiplayer in the initial product.
13. Build a general-purpose RPG engine before *Pool of Radiance* itself is playable.

---

## 5. Guiding Principles

### 5.1 Fidelity First

When choosing between:

- modern convenience,
- implementation elegance, and
- fidelity to original campaign behavior and the selected combat rules,

the default priority is:

1. Correct game behavior
2. Compatibility with original data
3. Maintainable implementation
4. Modern convenience

Quality-of-life changes should normally be optional.

### 5.2 Modern Shell, Original Game

The game engine should reproduce original campaign state transitions. The
selected rules module controls combat mechanics and character combat state.

Godot should provide the modern shell:

- windows,
- panels,
- menus,
- input,
- rendering,
- sound playback,
- animation,
- scaling,
- accessibility,
- and platform integration.

### 5.3 Data-Driven Where Possible

Original game data should be decoded into structured internal representations rather than hard-coded into the engine.

Example:

```text
Original SSI file
      ↓
Binary parser
      ↓
Validated OpenGoldBox data structures
      ↓
Game engine
      ↓
Godot presentation
```

### 5.4 Reversible Modernization

Visual or usability enhancements should ideally be switchable.

Examples:

- original nearest-neighbor rendering,
- xBRZ-style upscaling,
- enhanced fonts,
- modern inventory layout,
- combat animation speed,
- tooltip detail,
- original keyboard navigation.

---

## 6. Target Users

### 6.1 Primary User

A player who:

- owns *Pool of Radiance*,
- enjoys classic computer RPGs,
- wants faithful gameplay,
- dislikes running the original game through DOS-era interfaces,
- and wants modern resolution, controls, and usability.

### 6.2 Secondary Users

- Gold Box enthusiasts
- retro-computing enthusiasts
- preservationists
- reverse engineers
- mod developers
- open-source contributors
- Dungeons & Dragons historians

---

## 7. Technology Stack

### 7.1 Core Technology

**Game Engine / UI:** Godot 4.x

**Core implementation:** C/C++

**Godot interface:** GDExtension

No C# runtime or .NET dependency is required.

### 7.2 Why C/C++

C/C++ is appropriate because the project requires:

- binary file parsing,
- explicit control over data layout,
- efficient decoding,
- portability,
- integration with existing reverse-engineering tools and libraries,
- potential reuse outside Godot,
- and straightforward native interoperability through GDExtension.

The game simulation should not depend tightly on Godot classes.

### 7.3 Architectural Boundary

The preferred architecture is:

```text
+---------------------------------------------------+
|                    Godot UI                       |
|                                                   |
| Menus | Windows | Map | Combat UI | Input | Audio |
+-------------------------+-------------------------+
                          |
                     GDExtension
                          |
+-------------------------v-------------------------+
|             OpenGoldBox C/C++ Core                 |
|                                                   |
| Game State | Combat | Rules | Characters | Maps   |
| Events | Inventory | Saving | Data Interpretation |
+-------------------------+-------------------------+
                          |
+-------------------------v-------------------------+
|              Original Game Data Layer             |
|                                                   |
| File Parsers | Decoders | Resource Loaders        |
+---------------------------------------------------+
```

This separation makes the core testable without running the Godot renderer.

---

## 8. Repository Structure

A suggested initial layout:

```text
opengold/
├── README.md
├── PRD.md
├── LICENSE
├── CONTRIBUTING.md
├── CMakeLists.txt
│
├── docs/
│   ├── architecture/
│   ├── reverse-engineering/
│   ├── file-formats/
│   └── legal/
│
├── core/
│   ├── include/
│   ├── src/
│   └── tests/
│
├── formats/
│   ├── include/
│   ├── src/
│   └── tests/
│
├── demos/godot/
│   ├── project.godot
│   ├── scenes/
│   ├── scripts/
│   ├── themes/
│   ├── assets/
│   └── ui/
│
├── gdextension/
│   ├── include/
│   └── src/
│
├── tools/
│   ├── inspectors/
│   ├── converters/
│   └── diagnostics/
│
└── testdata/
    └── synthetic/
```

Original copyrighted game files must not be committed to the repository.

---

## 9. Original Game Data

### 9.1 User-Owned Assets

OpenGoldBox should require the player to supply the original *Pool of Radiance* installation files.

The application should provide an initial setup flow that:

1. asks the user to locate the original game directory,
2. validates expected files,
3. identifies the detected release/version where possible,
4. records the path,
5. tests that required resources are readable,
6. reports missing or unexpected files clearly.

### 9.2 Asset Policy

The OpenGoldBox repository must not contain:

- original SSI art,
- original maps,
- original text,
- music copied from the game,
- fonts extracted from the game,
- original executable code,
- proprietary logos,
- or substantial copyrighted game data.

Small byte sequences used solely for tests should preferably be synthetic.

### 9.3 Resource Access

The original files should remain authoritative whenever practical.

OpenGoldBox may:

- parse them at runtime,
- cache derived representations,
- or generate local indexes.

It should not require permanent conversion of the user's game installation.

---

## 10. Clean-Room Reverse Engineering

Reverse engineering must focus on **behavior and file formats**, not copying original source code.

### 10.1 Allowed Research Outputs

Project documentation may describe:

- binary file layouts,
- offsets,
- compression schemes,
- palette formats,
- sprite formats,
- map structures,
- encounter structures,
- save formats,
- rule behavior,
- algorithms inferred from observable behavior,
- and compatibility findings.

### 10.2 Preferred Process

For each subsystem:

1. Observe original behavior.
2. Record inputs and outputs.
3. Inspect original data files where legally permitted.
4. Document the discovered format or behavior.
5. Create synthetic tests.
6. Implement equivalent behavior independently.
7. Compare OpenGoldBox output against the original game.
8. Record discrepancies.

### 10.3 Documentation Standard

Reverse-engineering discoveries should be committed as documentation before, or alongside, implementation.

Example:

```text
docs/file-formats/character-record.md
docs/file-formats/wall-graphics.md
docs/file-formats/map-format.md
docs/reverse-engineering/combat-initiative.md
```

---

## 11. Major Functional Systems

The final game will need implementations for the following systems.

### 11.1 Application Shell

- startup
- configuration
- game-data location
- resolution
- fullscreen/windowed mode
- audio controls
- scaling options
- accessibility options
- save management

### 11.2 Character Creation

Use the selected rules module for character generation. The first standalone
demo uses SRD 5.2.1, 4d6 dropping the lowest die, freely assigned scores,
unlimited full-set rerolls, and maximum starting HP. It creates one character
and displays the character sheet; pool and party management follow separately.
See [the current creation flow](CHARACTER-CREATION.md). Character generation
ultimately includes:

- race
- class
- alignment
- sex where represented by the original game
- ability scores
- hit points
- starting equipment
- starting money
- name
- portrait/icon selection where applicable

Modern UI may make these choices clearer without changing the rules.

### 11.3 Character Management

- character sheet
- level
- XP
- armor class
- THAC0 / attack information
- saving throws
- memorized spells
- known spells
- conditions
- money
- equipment
- encumbrance
- class-specific abilities

### 11.4 Party Management

- add character
- remove character
- reorder party
- inspect party
- rest
- camp
- save
- load

### 11.5 Exploration

Implement:

- first-person dungeon/city traversal
- directional movement
- map transitions
- doors
- walls
- stairs
- zones
- scripted locations
- encounters
- searching
- environmental messages

### 11.6 Overland Travel

Where required by the original game:

- wilderness map
- travel destinations
- encounters
- movement costs
- transitions between overland and local maps

### 11.7 Combat

Combat must follow the selected rules module, initially SRD 5.2.1. Original
encounter composition and script-visible outcomes are preserved by campaign
adapters; original AD&D numerical statistics require explicit conversion.

Required features include:

- combat map loading
- initiative
- turn order
- movement
- targeting
- melee attacks
- ranged attacks
- spellcasting
- area effects
- saving throws
- damage
- death
- unconscious states where applicable
- morale
- enemy AI
- victory
- retreat/flee behavior
- treasure
- XP awards

Modern UI may display calculations more clearly.

Example:

```text
Long Sword
Attack: +4
Target AC: 15
Roll needed: 11+
```

Such explanations should be optional if they alter the original presentation significantly.

### 11.8 Magic

Implement:

- spell books
- prepared spells and standard spell slots under the selected rules module
- cleric spells
- wizard spells
- spell levels
- casting restrictions
- targeting
- durations
- saving throws
- area-of-effect rules
- rest-based recovery

### 11.9 Inventory

Modern inventory management should retain original mechanics while reducing friction.

Potential interface:

- character inventory panes
- drag-and-drop where safe
- equip/unequip
- item details
- quantity
- weight
- value
- magical status when known
- usable-by restrictions

### 11.10 Economy

- money denominations
- stores
- buying
- selling
- identification
- training fees
- temple/service fees

### 11.11 NPCs

Support:

- temporary party members
- scripted NPC behavior
- joining/leaving
- dialogue
- combat behavior
- equipment restrictions
- event triggers

### 11.12 Journal and Quest State

Preserve original campaign state and event logic.

Modern UI may optionally provide:

- searchable journal,
- known quest list,
- location notes,
- event history.

Care must be taken not to reveal information the original game intentionally concealed.

---

## 12. Rendering

### 12.1 Original Graphics

The renderer must support displaying original graphics accurately.

Initial priority:

1. Decode original image format.
2. Decode palette data.
3. Render image accurately.
4. Scale cleanly.
5. Integrate into Godot textures.

### 12.2 Pixel-Art Scaling

The player should eventually have multiple rendering options:

- nearest-neighbor
- integer scaling
- xBRZ or comparable edge-aware pixel-art scaler
- optional CRT-style treatment
- original aspect-ratio correction

The default should preserve the character of the source art.

No scaler should be required for gameplay.

### 12.3 UI Scaling

The game uses **1920 x 1080 (Full HD, 16:9)** as its reference layout and
default requested window size. Startup splash artwork targets the same resolution
and preserves its aspect ratio when fitted to other display sizes.

The UI must be usable at:

- 1080p
- 1440p
- 4K

The design should avoid assuming a fixed DOS-era resolution.

---

## 13. User Interface

### 13.1 Design Direction

The UI should evoke the structure of the Gold Box interface without copying its constraints.

Desired feeling:

- dark fantasy
- readable
- compact
- information-rich
- keyboard-friendly
- mouse-friendly

Avoid making the game resemble a generic modern mobile RPG.

### 13.2 Main Game Layout

A possible layout:

```text
+----------------------------------------------------------+
| Party Summary                                            |
+-----------------------------+----------------------------+
|                             |                            |
|       Game View             | Character / Context Panel  |
|                             |                            |
|                             |                            |
+-----------------------------+----------------------------+
| Messages / Combat Log / Commands                         |
+----------------------------------------------------------+
```

The exact layout may evolve through prototypes.

### 13.3 Keyboard Support

The game should be fully playable by keyboard.

Where possible, preserve familiar original shortcuts while also providing modern bindings.

### 13.4 Mouse Support

Mouse support should include:

- menus
- inventory
- targeting
- character inspection
- buttons
- map navigation where appropriate

### 13.5 Controller Support

Controller support is desirable but not required for the first playable milestone.

---

## 14. Save Games

### 14.1 OpenGoldBox Save Format

OpenGoldBox may use its own versioned save format.

The format should include enough information to restore:

- party
- characters
- inventory
- world state
- map
- position
- event flags
- quests
- combat state if mid-combat saving is permitted
- configuration tied to the campaign

### 14.2 Original Save Compatibility

Importing original *Pool of Radiance* saves is highly desirable.

Writing original-compatible saves is lower priority and should only be attempted if reliable.

### 14.3 Save Safety

Use defensive save techniques:

```text
write temporary file
→ validate
→ flush
→ atomically rename
```

Where possible, avoid destroying the player's only valid save.

---

## 15. Audio

Initial requirements:

- original sound support where feasible
- volume controls
- mute
- modern audio device handling

Future optional enhancements may include:

- replacement sound packs
- MIDI improvements
- community audio packs

Any replacement assets must be legally distributable.

---

## 16. Modding

Modding is not required for the first release, but architecture should avoid making it impossible.

Potential long-term mod support:

- replacement graphics
- UI themes
- sound packs
- text fixes
- rules variants
- maps
- encounters

Original campaign behavior should remain separately selectable.

---

## 17. Diagnostics and Developer Tools

Reverse engineering will be much easier if OpenGoldBox includes small inspection tools.

Useful tools include:

### 17.1 Resource Inspector

Displays:

- file name
- offsets
- record structure
- palette
- sprites
- decoded metadata

### 17.2 Map Inspector

Displays decoded maps independently of gameplay.

### 17.3 Character Inspector

Reads original character/save files and prints structured information.

### 17.4 Event Inspector

Displays event/script records in human-readable form.

### 17.5 Comparison Harness

Where practical:

```text
original input
      ↓
expected observations

OpenGoldBox input
      ↓
actual observations

      ↓

difference report
```

These tools are first-class project infrastructure, not disposable experiments.

---

## 18. Testing Strategy

### 18.1 Unit Tests

C/C++ unit tests should cover:

- binary parsing
- compression
- palette conversion
- combat calculations
- dice
- XP
- character statistics
- saving throws
- spell calculations
- item restrictions
- serialization

### 18.2 Golden Tests

For decoded resources:

```text
binary fixture
→ decoder
→ normalized representation
→ expected result
```

### 18.3 Synthetic Test Data

Use synthetic fixtures whenever copyrighted content is unnecessary.

### 18.4 Compatibility Tests

When users provide game files locally, optional developer tests may compare decoded results against known hashes or metadata without committing copyrighted data.

---

## 19. Error Handling

OpenGoldBox should fail loudly and usefully when data cannot be interpreted.

Bad:

```text
Error 42
```

Good:

```text
Unable to decode wall graphic record 17 in GEO1.DAX.

Expected decompressed size: 3072 bytes
Actual size: 3018 bytes

This game release may use an unsupported resource format.
```

Reverse-engineering projects live or die by good diagnostics.

---

## 20. Security

OpenGoldBox processes externally supplied binary files.

All parsers must treat game data as untrusted input.

Requirements:

- bounds checking
- integer overflow protection
- validated offsets
- validated record sizes
- no unchecked pointer arithmetic
- no fixed-buffer assumptions
- fuzz testing for important decoders where practical

C/C++ parsers must prioritize correctness over cleverness.

---

## 21. Performance

The original game targeted hardware many orders of magnitude slower than modern PCs.

Performance requirements are therefore modest, but architecture should avoid unnecessary overhead.

Target behavior:

- near-instant UI response
- near-instant map transitions
- fast startup after initial indexing
- negligible combat input latency
- low idle CPU usage

Frame rate is not a meaningful simulation constraint.

---

## 22. Licensing

### 22.1 OpenGoldBox Source Code

Original OpenGoldBox source code should be licensed under:

**Mozilla Public License 2.0 (MPL-2.0)**

This provides:

- open source distribution,
- commercial use,
- modification,
- patent protection,
- and file-level copyleft.

### 22.2 Third-Party Libraries

Dependencies must have licenses compatible with MPL-2.0 distribution.

Each dependency should be documented.

### 22.3 Original Game Rights

The OpenGoldBox license does not grant rights to:

- *Pool of Radiance*
- Dungeons & Dragons
- Forgotten Realms
- SSI artwork
- TSR/Wizards content
- original game music
- original text

The README should make this distinction explicit.

---

## 23. Project Naming

**OpenGoldBox** is the current working project name.

The name reflects the project's relationship to the Gold Box lineage without claiming ownership of the original trademarks.

Before a broad public release, the name should be checked for trademark and project-name conflicts.

---

## 24. Contributor Model

The project is intended to welcome outside contributors.

Contribution areas should include:

- reverse engineering
- C/C++ implementation
- Godot UI
- testing
- documentation
- platform support
- accessibility
- asset decoding
- tooling

### 24.1 Contribution Requirement

Reverse-engineering discoveries should be documented well enough for another contributor to reproduce the conclusion.

Avoid unexplained code such as:

```cpp
value = data[offset + 17] ^ 0x80;
```

Prefer:

```cpp
// Byte 17 contains the signed morale modifier.
// SSI stores the signed value using the high bit as the sign.
value = decode_morale_modifier(data[offset + 17]);
```

---

## 25. Initial Development Strategy

> **Historical plan.** Sections 25–26 record the plan the project started from.
> They are not the current checklist; see the
> [README's current status](../README.md#current-status) for what is playable now.

Do **not** begin by implementing the entire game.

The first phase should answer the question:

> Can OpenGoldBox reliably interpret original game resources and present them through Godot?

### Milestone 0: Repository Bootstrap

Deliver:

- Git repository
- MPL-2.0 license
- PRD
- README
- CMake build
- Godot project
- GDExtension bridge
- trivial C/C++ test
- CI build

### Milestone 1: First Original Graphic

Deliver:

1. Locate one known graphic resource.
2. Document the binary format needed to read it.
3. Decode it in C/C++.
4. Decode its palette.
5. expose the resulting pixel data through GDExtension.
6. Display it correctly in a Godot window.
7. Support nearest-neighbor integer scaling.

**Success criterion:**

> OpenGoldBox renders one original *Pool of Radiance* image correctly from the user's game files.

This is the project's first vertical slice.

### Milestone 2: Resource Browser

Build a developer resource browser capable of:

- opening the game data directory,
- listing known resource archives,
- enumerating records,
- rendering supported graphics,
- displaying metadata,
- exporting diagnostics.

### Milestone 3: Map Rendering

Deliver:

- map data decoding
- wall/terrain rendering
- player facing
- map movement
- collision
- transitions

No encounters are required yet.

### Milestone 4: Party and Character Data

Deliver:

- character decoding
- party display
- character sheet
- inventory display
- core AD&D statistics

### Milestone 5: Exploration Loop

Deliver:

```text
load party
→ enter map
→ move
→ trigger event
→ display text
→ change game state
→ save
```

### Milestone 6: Combat Prototype

Deliver one representative combat encounter with:

- combat map
- player units
- enemy units
- initiative
- movement
- melee
- damage
- death
- victory

### Milestone 7: Full Campaign Systems

Incrementally implement:

- spells
- ranged combat
- shops
- temples
- training
- NPCs
- quests
- wilderness
- special events
- campaign completion

### Milestone 8: Compatibility and Polish

- original save import
- graphics scaling options
- accessibility
- controller support
- installers
- crash diagnostics
- platform packaging

---

## 26. First Engineering Task

The recommended first real engineering task is deliberately small:

> **Decode and display one original *Pool of Radiance* graphic in Godot using C/C++.**

This task exercises nearly every foundational architectural decision:

- locating user-owned data,
- understanding a file format,
- binary parsing in C/C++,
- test infrastructure,
- palette conversion,
- CMake,
- GDExtension,
- Godot rendering,
- documentation,
- and legal separation of code from assets.

It provides a visible result without requiring speculative architecture for the entire RPG.

---

## 27. Definition of Minimum Playable Product

The minimum playable product is achieved when a player can:

1. install OpenGoldBox,
2. select a valid original *Pool of Radiance* installation,
3. create or load a party,
4. enter the campaign,
5. explore maps,
6. interact with campaign events,
7. enter combat,
8. complete combat,
9. manage inventory and characters,
10. save and restore progress,
11. progress through the complete original campaign,
12. reach the original ending.

The interface does not need every planned quality-of-life feature at this stage.

The game must, however, be completable.

---

## 28. Definition of Version 1.0

OpenGoldBox 1.0 should meet the following bar:

### Gameplay

- Complete *Pool of Radiance* campaign is playable.
- Major combat rules behave consistently with the selected SRD baseline; campaign state transitions remain compatible with original scripts.
- Major encounters function.
- Character advancement works.
- Spells are implemented.
- Shops, temples, training, and NPCs work.
- Saving/loading is reliable.

### Compatibility

- Supports at least one well-documented original PC release.
- Handles common legitimate distribution variants where practical.
- Provides useful diagnostics for unsupported variants.

### Interface

- Modern windowed UI
- fullscreen support
- mouse support
- keyboard support
- scalable fonts/UI
- modern save selection
- functional inventory UI
- clear character sheets

### Rendering

- faithful original rendering
- integer scaling
- optional enhanced pixel scaling

### Distribution

- Windows release
- Linux release
- macOS release if maintainable
- installation instructions
- original-data setup instructions

### Development

- automated builds
- automated tests
- documented architecture
- documented known file formats
- contributor guide

---

## 29. Future Possibilities

These are explicitly outside the initial scope but should influence architectural flexibility.

### Other Gold Box Games

Potential future support could include:

- *Curse of the Azure Bonds*
- *Secret of the Silver Blades*
- *Pools of Darkness*
- *Champions of Krynn*
- *Death Knights of Krynn*
- *The Dark Queen of Krynn*
- other related SSI titles

The project should **not** prematurely generalize for these games.

Instead:

> Make *Pool of Radiance* work cleanly first, then extract reusable abstractions from proven similarities.

### Enhanced Content

Possible later projects:

- community replacement art
- high-resolution UI themes
- remastered sound
- additional languages and full original-campaign translations
- accessibility packs
- optional rule explanations
- community campaigns

These should remain separate from strict compatibility mode.

The game interface already supports English and Spanish. `--reset-lang` must offer
English and Español in a centered dialog before optional splash screens, support
mouse, arrow keys and Enter, and save the confirmed selection for later launches.
Highlighting a language immediately translates the dialog into that language;
language names remain in their native form and previewing does not save a choice.
Spanish uses Estanque de Resplandor on the game splash. Original campaign text
falls back to its source wording until separate translation overrides exist.
See [LOCALIZATION.md](LOCALIZATION.md) for current coverage and workflow.

Persist the original game-folder path and language beside the executable. On
first run request the language and then the folder; otherwise prompt only for missing
or invalid settings. Provide `--reset-game-path` and `--reset-lang` (replacing
`--lang`), preserving previous values until confirmation. Support environment
overrides for both values. Compare consumed original assets against reference
MD5 fingerprints and offer Quit to OS or Continue when they differ. Missing
required files must be corrected before proceeding. See
[CONFIGURATION.md](CONFIGURATION.md).

---

## 30. Open Questions

The following items require further investigation:

1. Which *Pool of Radiance* release should be the reference implementation?
2. Which archive/resource formats are required for the first graphic?
3. How much of the game is stored in DAX-style resource archives?
4. How are map event scripts encoded?
5. How closely can original save files be imported?
6. Which behaviors are data-driven versus executable-driven?
7. Which pixel-art scaler should be used for the optional enhanced mode?
8. Should the modern UI be resizable panel-by-panel or use predefined layouts?
9. How should compatibility quirks and original bugs be categorized?
10. Which original keyboard shortcuts should be preserved by default?
11. Should strict compatibility mode intentionally reproduce certain original bugs?
12. What is the minimum macOS support burden the project is willing to maintain?

---

## 31. Decision Log

| Decision | Choice |
|---|---|
| Engine | Godot 4.x |
| Native language | C/C++ |
| Godot/native integration | GDExtension |
| C# | Not used |
| Project model | Open source |
| Code license | MPL-2.0 |
| Original assets | User supplied |
| Original executable | Not distributed or required at runtime |
| Reverse engineering | Behavioral / clean-room oriented |
| Primary goal | Faithful *Pool of Radiance* reimplementation |
| UI philosophy | Modern interface, original gameplay |
| Graphics | Original assets with optional modern scaling |
| First engineering milestone | Decode and display one original graphic |
| Premature multi-game engine | Avoid |
| Long-term Gold Box support | Possible after *Pool of Radiance* |

---

## 32. Product Principle in One Sentence

**OpenGoldBox is an open-source C/C++ and Godot reimplementation of SSI's 1988 *Pool of Radiance* that uses player-supplied original game assets to preserve the original gameplay while providing a modern, portable interface.**
