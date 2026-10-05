# Replaceable combat rules

The selected baseline is **SRD 5.2.1**, with standard turn-based combat and spell
slots. Open5E supplies reference content; it is not an executable rules engine.
The first implementation is a bounded C++20 rules module and a native Godot
combat scene. It is not a complete implementation of the SRD.

The [shared party preview](PARTY.md) extends this module with created-character
recipes, equipment and persistent vitals/resources. The standalone combat modes
below retain their fixed fixtures; the character scene injects the actual party.

[Spell component eligibility](SPELL-COMPONENTS.md) blocks Somatic spells when an
equipped weapon/wand and shield occupy both hands, while retaining Verbal-only
spells. Speech-blocking sources and material components remain
separate increments.

## Run from Windows CMD

From the repository root:

```cmd
demos\build-rolf.cmd
demos\review-combat.cmd
```

The existing build helper builds both native Godot scenes and runs five test
suites. Close any running OpenGoldBox scene before rebuilding its shared DLL.
See [native build prerequisites](ROLF.md#build-boundary).

Training uses one martial fixture against an SRD Bandit and needs no original
game files. Select **Slums event** for a fixed four-person party against the
original event's four orcs. Original files come from `OPENGOLD_GAME_DIR`, falling
back to `opengold/game_directory` in `demos/godot/project.godot`:

```cmd
set "OPENGOLD_GAME_DIR=C:\Games\POOLRAD"
demos\review-combat.cmd -- --slums
```

Select a party sprite or portrait to see its movement squares when its turn is
active. The combat log explains when another character has the turn. Arrow keys
move the selected character one square when it is their turn; Shift rotates an
arrow clockwise by 45 degrees. Numpad 1/3/7/9 move southwest/southeast/
northwest/northeast. Delete/Page Down/Insert/Page Up do the same, including on
the numpad with Num Lock off. **A** cycles actions, **Space** uses an immediate
action, **Z** changes spell slot level, and
**Enter** ends your turn or continues a dialogue pause. Click a highlighted
destination or target for the selected action. Gray cells block movement and
sight; brown cells cost extra movement. Allied squares can be crossed at the
normal terrain cost, but cannot be selected as stopping points. Hostile squares
still block movement in this increment.
Each square is 5 feet, including diagonals.
An arrow or Move-mode click aimed at an adjacent enemy makes a melee attack when the selected
character has an action available. The combat log follows new text until you
scroll up to read earlier entries.
To cross allies, click a highlighted free square beyond them. Arrow keys still
request a one-square move and cannot stop on an ally.
If movement or an adjacent attack is unavailable, the combat log explains why.
End turn appears above the combat log during a party turn.
An opportunity reaction pauses combat until you choose Opportunity attack or
Decline reaction above the log; movement and End turn wait for that choice.
Hovering over a monster or NPC shows a tooltip beside the pointer with its
type, current and maximum HP, AC, and the weapon it would use at its present
distance from the party. When an encounter has no named weapon, it shows
`Unspecified` rather than inventing one.
Melee, ranged, and damaging spell attacks spend the action and any required
spell slot while preserving remaining movement and Bonus Actions. Move before
or after attacking, use Second Wind if available, then choose **End turn** or
press **Enter** to advance initiative. Pending reactions resolve before the
active character continues; enemies explicitly finish their own turns.
A dead combatant displays the original combat skull for one second.
Attacks show the combatant's original action frame for one second and play the
corresponding original melee, ranged, or spell sound from the local game files.
Combat sprites face left or right toward targets in those directions when attacking.
Combat movement plays the original footstep sound. A combatant who dies plays
the original death sound, including when an attack sound plays.
When a party member dies, their portrait row shows `DECEASED` in red. The skull
occupies their battlefield square for one second; the square is then clear.
At 0 HP, a party member is unconscious while death saves remain possible; the
portrait row shows `UNCONSCIOUS` until their state changes. Their existing
combat figure lies within one square. An unstable member rolls once on each
turn entry, including the initial initiative slot. A natural 20 restores 1 HP
and permits that turn; stabilization clears both death-save counters.
Restoring a combat checkpoint adds no roll. If no party member remains conscious,
combat ends in defeat.

The shared combat demo uses twelve dagger-wielding Kobolds with melee attacks
only, plus one Kobold leader with a short sword and a short bow. Five ring
squares are open; the sixth removed Kobold is replaced by the leader at the
top of the formation. The leader's bow uses +4 to hit, 1d6+2 damage and
80/320-foot range. Original Slums creature record 1 contains a short bow and
arrows. Record 11 has no bow and remains melee only. The dagger description
follows the regular Kobold combat art.

**Restart** resets the selected isolated encounter with seed 42. After Slums
victory, **Continue** finishes any remaining dialogue, and **Revisit event** checks
the original event's persistent flag within this session. This scene is separate
from Rolf's exploration scene. In the game, exploration starts combat itself; see
[the expedition](EXPEDITION.md).

Training **Save combat** / **Load combat** use `user-data/combat.save`, retaining
the previous save as `.bak`. Saves include pending reactions and RNG state. Older
formats and other module or content identities reject; see the
[format policy](SAVES.md#pre-10-format-policy). This demo does not save its
Slums event; the game saves the campaign, including the Slums, at idle
exploration ([saves](SAVES.md)).

## Library boundary

| Component | Responsibility |
| --- | --- |
| `OpenGold.Rules` / `opengold_rules` | Edition-independent encounter, snapshot, legal-command, outcome, and checkpoint interfaces. |
| `OpenGold.Rules.Srd5` / `opengold_rules_srd5` | SRD combat calculations, turn/resource state, dice, validation, and serialization. Links only the rules interface and the C++ standard library. |
| `OpenGold.Core::CombatDemo` | Supplies authored geometry and explicit creature conversions; pauses/resumes the original ECL host request around real combat. Depends on the interface, not SRD code. |
| `OpenGold.Godot::CombatView` | Draws state and submits offered commands. The application composes the selected module. It never rolls dice or computes damage. |

`RulesModule::create(encounter, seed)` returns an owned `CombatSession`.
`snapshot()` provides display state; `legal_commands()` provides the actions the
module currently permits. `submit()` validates actor, revision, verb, target and
destination again. Invalid or stale input changes neither state nor randomness.
`save()` returns module-owned checkpoint bytes; `restore()` validates their
identity and structure before replacing a session.

Modules are replaceable at C++ composition/build time. There is no dynamic plugin
ABI or user-facing edition selector yet. A new implementation supplies the same
interface, its content and conversion profiles. Verbs are strings owned by the
module. The current demo presents the listed actions explicitly; a replacement
introducing new verbs also needs presentation controls for them. The common
interface does not expose SRD spell slots or class internals as mutable state.

Sessions exclusively own mutable state and share immutable definitions through
`shared_ptr<const Content>`. They can outlive the module that created them.
Deterministic SplitMix64 dice and stable initiative tie ordering make a seed plus
the same accepted command sequence reproducible. Checkpoints include the RNG,
turn budgets, HP, slots, death saves and unfinished opportunity reactions.

Rules module **0.6.80** writes **OGCOMBAT 35**, including an
involuntary shared-space marker, remaining Hit Dice, recovery clocks and sourced Temporary HP.
A saved leave-reach queue retains its order, partially resolved position and
deterministic continuation. Older formats and other module or content identities
are rejected; nothing is migrated. Campaign saves do not contain paused combat queues.
The Versatile grip is not stored: it follows the other hand at attack time
([AUTO-1](SRD-DECISIONS.md#auto-1-2026-09-30-automatic-choices-with-logging)).
See [equipment](PARTY.md#versatile-grip) for the seven supported Versatile weapons.

Profiles (PC42) store the background ID and acquired feature/feat grants after the gear list. Each
records a stable rules ID, source ID, acquisition level and named choices.
The rules validate entitlements, prerequisites, duplicates and ability choices
before deriving combat effects. See [advancement](ADVANCEMENT.md).

The ledger also records skill, tool, Expertise and language sources. Its
training choices and entitlements are validated before combat. Campaign saves persist
the selected choice groups.
See [training support](TRAINING.md) for the Rogue/Criminal package and limits.

SRD11 vital continuations preserve spent Hit Dice and the existing resource/effect
state; [rest resources](REST-RESOURCES.md) describes the rules APIs and campaign
rest transactions. Campaign saves persist completed Short Rest spending
sessions. Player-facing rest controls remain #192.
Vital state also preserves death-save and Stable-recovery clocks at zero HP. See
[recovery clocks](RECOVERY-CLOCKS.md) for combat support
and the outstanding campaign scheduler.

A reaction may pause an accepted route while the mover shares an allied space.
The remaining path must still lead to a free cell within the movement budget.
An incapacitated mover stays at the interruption point; an involuntary overlap
marker keeps save/reload valid through healing or natural-20 recovery. The
marker clears when the actors separate or die; it permits no voluntary move
onto an occupied endpoint. Prone and size-dependent effects of involuntary
co-occupancy remain in the condition/creature-state increments (#35/#44).

## Implemented scope

- Individual initiative; ties resolve by entity ID. Action, Bonus Action,
  Reaction, movement, and round/turn reset.
- Grid pathfinding, difficult terrain, allied transit and blocked hostile cells,
  opaque obstacles and blocked diagonal wall corners. Movement can pause for an
  opportunity attack before leaving reach; Disengage prevents it.
- Left/right facing persists across turns and combat checkpoints as presentation
  state. Turning toward an attack or spell target provokes no reaction. A visible
  creature leaving an enemy's reach triggers an opportunity attack immediately
  before that step, provided the enemy has its Reaction. Disengage prevents it;
  a Blinded enemy cannot see the departure. If a reaction incapacitates the mover,
  the movement and remaining reaction queue stop.
- One melee or ranged attack per Attack action, ascending AC, natural 1/20,
  doubled damage dice on critical hits, Dodge and ranged disadvantage from long
  range or an adjacent visible enemy. No hidden dice in the UI or AI.
- Versatile one-/two-handed melee damage chosen automatically from the other
  hand (two-handed when it is empty), named in the damage line.
  Thrown attacks retain their base damage die.
- Dash; Second Wind; Fire Bolt; touch-range Cure Wounds; single-target Magic
  Missile. Two level-1 spell slots in the caster fixtures. Cure Wounds uses
  2d8 plus the casting ability modifier from this baseline.
- Ordinary ability saving throws and Blinded through the blindness option of
  Blindness/Deafness, including recovery saves, timed effects and persistence.
  See [status effects](STATUS-EFFECTS.md).
- HP, unconscious party members, automatic death saves, stabilization, healing,
  instant death from excess damage, enemy defeat, and party incapacitation.
- Complete training checkpoints and a basic AI that uses the same public
  commands as the player.
- [Manual advancement](ADVANCEMENT.md) supports Fighter/Cleric/Wizard levels 2–4
  after explicit confirmation. XP alone does not change the level. Supported
  feats, spells, HP and slots persist together. [Recovery](RECOVERY.md) restores
  supported resources after eligible eight-hour rests; repeated rests require
  a 16-hour wait. Stable reward IDs survive scene recreation and saves.
- Temple Cure Wounds is a 100 gp atomic service for a wounded living active
  member. Dead targets reject; no resurrection is implied.

The fixed profiles are intentionally limited to levels 1–4 and ordinary Medium-sized
ground combatants. Created campaign profiles support the documented level 1–4
subset. Standalone party profiles are authored combat fixtures, not finished
character sheets. The orc conversion is authored for this demo; its AC/HP are
not an automatic conversion of original AD&D values.

Not yet implemented (as of 2026-10-02): full class features or equipment,
additional saving-throw effects and conditions, partial cover, multiple sizes,
concentration spells (the [foundation](CONCENTRATION.md) exists), most other
spells, split-target Magic Missile/Scorching Ray, retreat, morale, or complete
campaign encounter coverage. Weapon mastery, typed damage and resistance, and
Prone are implemented; grappling was dropped
([SCOPE-2](SRD-DECISIONS.md#scope-2-2026-09-30-exploration-halves-and-marginal-features)). The shared campaign now includes named saves,
surprise initiative disadvantage, original dungeon geometry and bounded original
loot; see [the expedition adapter](EXPEDITION.md) and [campaign saves](SAVES.md).
Unconscious enemies are currently treated as defeated, and attacks against
unconscious party members are not offered. Allied transit and diagonal geometry
use the documented grid adjudication; this is not an implementation of every
SRD movement exception. Unsupported creature definitions fail explicitly.

## Original Slums adapter

This is the isolated, tested `ECL2.DAX:20` search-entry-1 profile, with `C04F=1`.
It preserves original text, the one-choice menu flow, monster/count/icon operands,
and result-dependent bytecode continuation. It acknowledges the three setup and
presentation services used here; full encounter presentation is still pending.

| Original input | Current conversion |
| --- | --- |
| `LOAD MONSTER 13, 1, 4` | `MON2CHA.DAX:13`, one original ORC, `slums-orc` rules definition, `CPIC2.DAX:4` pose 0 |
| `LOAD MONSTER 4, 3, 4` | `MON2CHA.DAX:4`, three original ORCs, same explicit rules/art mapping |
| `COMBAT`, `6DC6=99`, `6DCB=0` | Fixed four-person party, four original enemies, authored 12×9 arena |
| Victory / defeat | `6DC7=0` / `128`; `6DC8` is the actual defeated-monster count |

Bank 2 is explicit in this event profile; general script-to-resource bank
resolution is not inferred. Raw original creature records remain unchanged in
`CreatureCatalog`. Combat icons are decoded locally through the native CPIC
decoder; absent CPIC art falls back to a numbered token. Original dialogue or
art is never included in the repository.

The actual fight result resumes the waiting COMBAT request and clears its
documented scratch fields. With the current fixture and seed, victory leaves
`4ACA=255`, `4ABB=1`, and `6DC8=4`. A second visit does not start another fight.
This verifies the original event state in memory, not campaign persistence or
post-combat treasure. Unknown service contexts fail rather than being simulated.

See [manual advancement](ADVANCEMENT.md) for confirmed level-up choices, supported
feats, Healing Word, Scorching Ray and level-two upcasting.

## Validation and next increments

`build.cmd` tests the rules without Godot; `demos\build-rolf.cmd` also builds the scene.
With `OPENGOLD_GAME_DIR` set, `opengold_rules_tests` executes the installed Slums
script, fights using actual legal commands, checks result writes and revisits.
The older ECL mock remains a separate bytecode regression test.

Native tests cover command rejection/RNG atomicity, action budgets, obstacle
targeting, opportunity reactions, healing and spell resources, unconsciousness,
critical attacks, checkpoint validation and deterministic continuation. Synthetic
CPIC tests cover the format separately from installed game assets.

Godot scene checks use the actual native action-button signal and target-input
path, prevent Enter from skipping enemy turns, and finish combat automatically:

```cmd
godot --headless --path demos/godot res://scenes/combat_demo.tscn -- --combat-check
godot --headless --path demos/godot res://scenes/combat_demo.tscn -- --combat-check --slums
```

Add `--capture` to the user arguments in a graphical run (omit `--headless`) to
save a local PNG in `user-data`. These checks exit automatically.

Next increments should extend [saving throws and conditions](STATUS-EFFECTS.md)
and creature/spell coverage one tested encounter at a time. The [standalone character creator](CHARACTER-CREATION.md) now
implements the requested identity, attribute, HP, and appearance flow through
a separate optional native `CharacterRules` capability; it does not replace
combat fixture definitions. Reverse engineering remains focused on campaign interfaces,
file formats and content mapping rather than recreating the original combat rules.

Reference content and attribution are recorded in
[the versioned pack](../data/rules/srd-5.2.1/README.md). The baseline is the
[official SRD 5.2.1](https://www.dndbeyond.com/srd).

The release notes below record the format numbers each release introduced. Only
the current formats load; see the [format policy](SAVES.md#pre-10-format-policy).

Rules 0.6.13 introduces [typed damage](DAMAGE.md), creature damage/defense metadata
and Dwarf Poison resistance. PC8 requires its fixed sourced grant. Campaign and combat stay at format 10.

Rules 0.6.14 adds the [Temporary HP native foundation](TEMPORARY-HP.md), combat
format 11 and SRD6. Rules 0.6.15 adds playable Orc Adrenaline Rush and the reviewed
replacement/HP controls (#197), using combat 12, PC9 and SRD7. Campaign exploration
activation remains tracked under #34; see the linked scope document.

Rules 0.6.22 adds [Poison Spray and explicit Wizard cantrip choices](POISON-SPRAY.md),
with the approved creation and main combat controls. Campaign 11 stores choices;
PC11 validates their grants. Combat 13 and existing resource formats remain.
Full spell selection, speech blocking and other granting sources remain open.

Rules 0.6.23 adds [Sacred Flame and Cleric cantrip choices](SACRED-FLAME.md),
with PC12 grant validation and the approved creation/combat controls. Shared
spell saves now automatically fail Strength/Dexterity at zero HP. Campaign 11,
combat 13 and resource formats remain. Partial cover, speech blockers and other
grant routes keep #203 open.

Rules 0.6.24 adds [Action Surge](ACTION-SURGE.md): Fighter level-two acquisition,
a separately restricted extra action and Short/Long Rest recharge. PC13,
feature-bearing combat 14 and spent-resource SRD8 preserve the new state;
campaign 11 remains. The dedicated button is pending question 17.

Rules 0.6.37 adds [level-one Warlock Eldritch Blast](ELDRITCH-BLAST.md) through
explicit Pact Magic cantrip grants and the approved shared selection/casting
controls. PC25 validates access and uses Charisma; campaign/combat formats stay
unchanged. Object targets, later Warlock
levels, slots, invocations and general speech blockers remain separate work.

Rules 0.6.38 adds [Wizard Shocking Grasp](SHOCKING-GRASP.md) at levels 1–4.
The shared value-owned effect model records sourced Opportunity Attack
suppression; combat uses it when forming and validating movement interruptions.
Reaction budgets are unchanged. PC26 validates access, FX3 encodes the new effect,
and campaign/combat schemas stay unchanged.

Rules 0.6.39 adds [level-one Warlock Poison Spray access](WARLOCK-POISON-SPRAY.md).
PC27 validates the distinct Pact Magic source and casting mask; existing Charisma
casting, spell effects and shared Godot controls are reused. New presets fill
both available starting cantrips.

Rules 0.6.40 adds [level-one Sorcerer cantrips](SORCERER-CANTRIPS.md): explicit
source-owned Fire Bolt, Poison Spray, Ray of Frost and Shocking Grasp choices.
PC28 validates access and Charisma attacks. Existing Spell Choices/Spell/Cast
controls are reused under Q27; save schemas stay unchanged.
Full Sorcerer spellcasting and Innate Sorcery remain #132.
