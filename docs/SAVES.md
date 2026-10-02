# Campaign saves

The OpenGoldBox rename preserves the original `user://` location using an explicit
custom user directory, so saves stay in the same folder.

The shared character/party/New Phlan flow supports manual named campaign saves.
The first Slums expedition also supports saving during idle exploration, including
its district map, script continuation and deferred original loot.
Use **Save game** or **Load game** on the party roster or during idle town
exploration. Select an existing slot or enter a new name. Overwriting requires
confirmation and retains a previous version; loading always asks before replacing
the current campaign. The load list also offers previous versions for recovery.

Loading returns to the party roster. **Explore New Phlan** resumes the saved town
position, facing, visited map and script state. A save made before first entering
town resumes before Rolf's tour. Unfinished character-creator drafts are not part
of a campaign save; add completed characters to the party first.

## Pre-1.0 format policy

Until 1.0, every save kind has exactly one accepted format, and it always writes
every field:

- Campaign saves (`.ogs`): header `OPENGOLD-CAMPAIGN 20`.
- The internal training-combat checkpoint: `OGCOMBAT 28`.
- Records embedded by the SRD module: character profile recipe `PC42` (an
  explicit spell ID list), vital state `SRD9`, effect state `FX8` and
  concentration `CN1`.

A save or checkpoint with an older format number, or one written under a
different rules identity (module, version or content hash; currently
`opengold.srd5` 0.6.64), is rejected with the localized message "This save was
made by an older pre-release version of OpenGoldBox and can't be loaded." A newer
or unknown campaign number reports "Unsupported campaign save version". Nothing
is migrated.

New work adds no save migrations. When a format needs to change before 1.0,
change the current format in place.

## Supported boundaries and state

Saving/loading is supported from the party roster and idle exploration in New Phlan
or the Slums.
Town controls are disabled while dialogue, input, shopping or services are pending.
Combat must finish and return to the roster first. The core also rejects saving
during combat or an unfinished town event. A completed Short Rest spending window
may be saved at the idle boundary. No autosaves, unfinished-script-request saves,
mid-combat campaign saves or original DOS saves are implemented. The standalone tour and combat research demos retain their existing
behavior; these campaign controls belong to the shared party flow.

A campaign save stores:

- Finished character drafts, appearances, levels, advancement choices, training
  selections and acquired feature/feat/training grants,
  stable member and inventory IDs,
  inventory and original item provenance, equipment, purses, NPC identities/morale,
  active/reserve membership, selected slot and character-pool candidate identities.
- HP/death state, opaque rules-owned resources, XP, claimed reward IDs, recovery
  timers, campaign minutes/millisecond remainder and service RNG state.
- Lasting effects in the rules-owned SRD9/FX8 state, including individual
  applications, source provenance, fixed DCs, remaining duration and recovery
  schedule. Encounter scope IDs distinguish reused monster IDs across fights.
- A completed Short Rest's spending ticket, eligible members and completion time,
  plus its next session ID. Reload continues after the last committed die without
  replaying the hour or recharge. Malformed/stale/inactive continuations reject.
- No rest in progress: a rest completes in one step or is interrupted, so there
  is no resumable rest activity or dropped gear to save
  ([SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)).
  The camp/inn Rest dialog opens the Save dialog during pending Hit Die choices.
  Loading retains spent resources, clears stale displayed roll messages, and
  reopens choices on returning to the town. See
  [rest controls](REST-RESOURCES.md#player-rest-controls-192).
- Current New Phlan script/resource context, private mutable ECL image, bound
  variables/flags, instruction spans, comparison flags, request counter and ECL
  RNG. The completed event is not replayed. Dialogue and visited cells persist;
  transient encounter pictures/sprites are cleared for the idle exploration view.
- The camp interruption step count, which carries over between rests, so a
  reload repeats the same [camp outcomes](RECOVERY.md#original-campaign-mappings).
  A count of 24 or more rejects the load.
- Separate visited and seen bitsets for each district. Looking ahead through the
  first-person view reveals visible cells permanently; changing maps or loading
  a save preserves that history. Invalid masks, unknown districts, or missing
  knowledge for visited cells reject the load.
- Deferred loot: each entry's reward ID and money, plus the creature records it
  came from. Encounter loot is rebuilt from those records and must match the
  saved money; script treasure has no records and saves its generated items.
  Doors forced open are not saved; a loaded district starts with its original
  locks.

The `--no-fog` launch flag only changes overhead rendering. It does not fill
either bitset, and saving while it is enabled does not reveal unexplored areas
when the save is later loaded during normal play.

Character sheets are reconstructed through the rules module by replaying validated
creation and advancement choices; see [advancement](ADVANCEMENT.md). Grant IDs,
sources, acquisition levels and choices are stored explicitly and must agree with
that history; missing, duplicate, forged or inconsistent records reject before
replacing live state. Equipment and roster state are also validated before
replacement. Loading never rolls dice, spends actions or refunds resources. See
[training support](TRAINING.md).

## File safety

Saves live in Godot's writable **user://saves** directory (normally
`%APPDATA%\Godot\app_userdata\OpenGold\saves` on Windows). UTF-8 slot names are
encoded into safe filenames, with `.ogs` and previous-version `.ogs.bak` files.
Original installations may remain read-only.

The loader checks the format version, rules module/version/content identity and
an ordered fingerprint manifest of installed DAX archives and ITEMS. A different
asset installation, unknown definition, malformed resource state or different
format or rules identity rejects explicitly. Reinstalling identical assets at a
new path is valid. FNV-1a fingerprints/checksums detect accidental changes; they
are not signatures or protection against deliberate tampering.

Writes use a temporary file, flush it to disk and verify its bytes before replacing
the destination. Windows uses `ReplaceFileW` with a retained backup, or
`MoveFileExW` for a new slot. Failed replacement leaves the existing save intact.
Truncated/corrupt loads and validation failures preserve the current campaign.
Each write uses a separate temporary filename and removes that file on failure;
files left by interrupted writes do not block later saves. The game's training
combat saves use the same storage service with their own size limit and codec.
Temporary files are ignored by the slot list. If the newest save is damaged,
explicitly choose its previous version in **Load game**.

Player save policy: save while camping or at an inn. Do not expose saving during
combat. Combat checkpoint codecs remain internal tools for deterministic testing
and continuation; their existence does not authorize an in-combat save control.

## Verification

From PowerShell:

```powershell
.\demos\build-rolf.cmd
.\demos\review-character.cmd --save-restart
```

The launcher uses an isolated profile under `user-data/save-check-profile`, writes
fixtures under `user-data/save-check`, exits the writer process, then starts a
fresh Godot reader. The writer runs the existing original-data party route and
saves after advancement, interrupted rest, cancelled temple service, temple
payment, inn rest, a rejected full-health healing request, denied repeated rest,
and subsequent combat. The full-health rejection invokes the native campaign
service directly; the other original service paths use existing Godot callbacks.
The reader compares all serialized state, checks reward claims/rest eligibility,
verifies corrupt-load rollback and exercises named-slot save/load confirmations.
The process-restart suite covers nine states, including pending Short Rest
spending and the next committed die.

`opengold_save_tests` writes actual files and covers inventory IDs, reserve members,
HP/resources, levels, money, reward/rest continuity, ECL continuation, deterministic
subsequent combat and service RNG, incompatible identities, malformed/truncated
files, backup rotation and failed Windows replacement. The existing nine native
suites remain in the build alongside this tenth suite.

Tests build saves and checkpoints with the current writer, check exact
round-trips and continuation, and check that older formats and different rules
identities are rejected. No frozen prior-writer save files are kept.

For visual checks, after the restart route:

```powershell
$env:APPDATA = Join-Path $PWD 'user-data/save-check-profile'
godot --path demos/godot --resolution 1280x900 res://scenes/character_creation.tscn -- --save-check-read --capture
```

Run the visual command from a temporary PowerShell session so its profile override
does not affect later interactive launches. Captures go to `user-data`; repeat at
1120x800. The tested Windows build uses Godot 4.7.2 and the repository's C++20
MSVC/GDExtension configuration.

Ohlo's quest is saved and reloaded at its Slums save points by
`opengold_expedition_tests` and through the game's own save controls by
`opengold_godot_ohlo_save_route`; see [saving during the quest](QUESTS.md#saving-during-the-quest).

This completes the bounded existing-flow persistence milestone, not the complete
expedition in issue #1. Issue #10 retains future class-feature/training coverage
and integration with the expedition's reward/quest state.
