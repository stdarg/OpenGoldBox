# Character creation

The standalone C++20/Godot demo creates one level-one character and shows the
character sheet. It uses SRD 5.2.1 and complete OpenGoldBox portraits and the original game's combat art.

Run from Windows CMD at the repository root:

```cmd
demos\build-rolf.cmd
demos\review-character.cmd
```

The shared extension uses the existing [build prerequisites](ROLF.md). The
launcher opens `demos/godot/scenes/character_creation.tscn`. Art loads from the
`OPENGOLD_GAME_DIR` environment variable, falling back to
`opengold/game_directory` in `demos/godot/project.godot`.

## Creation flow

1. **Race & Gender**: select race (the nine species included in SRD 5.2.1)
   from the two-column list and gender from the dropdown below it. Gender does
   not alter stats or restrict other choices. Both selections persist when returning.
2. Select alignment.
3. The **Dice Rolls** area starts with six empty boxes to the right of the
   abilities. Roll attributes to fill those boxes with totals only. The native
   character retains the underlying dice. Ability boxes remain empty until you drag
   results into them. Assigned results leave their original boxes empty.
   Dragging between filled ability boxes swaps their results. Dropping an
   unassigned roll onto a filled ability returns the displaced result to the
   right. All six abilities must be assigned before continuing. **Reroll all six**
   replaces the complete set and empties ability boxes; attempts are unlimited.
   Choose one of the four SRD backgrounds here, then allocate its attribute
   bonuses (+2/+1 to different allowed abilities, or +1 to all three).
   Assigned ability boxes include those bonuses and update immediately when
   either selector changes, even before all six rolls are assigned. Empty boxes
   remain empty; the unassigned dice retain their original totals.
   Each applied adjustment appears beside its ability (for example,
   `Soldier (+2)`). Only the ability score is colored: yellow above
   its original roll, red below it, and normal when unchanged. Dice boxes have
   room for a two-digit total and padding, with a drag hint beside **Dice Rolls**.
4. Select one starting class from the twelve SRD classes. Classes whose primary
   abilities do not meet the SRD multiclass prerequisite of 13 are disabled.
   Fighter requires Strength or Dexterity; Monk and Ranger require Dexterity
   and Wisdom; Paladin requires Strength and Charisma. Applying these minimums
   to the initial class is the requested OpenGoldBox house rule, not an SRD
   level-one restriction. Background bonuses count toward eligibility.
   Return to Attributes to change assignments or bonuses if needed.
5. Complete **Training**: fixed grants appear above scrollable labeled checkbox
   groups for class skills and, for Rogue, Expertise (plus Fighting Style and
   Weapon Mastery where the class has them). There are no tool or language
   choices ([DM-1](SRD-DECISIONS.md#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells)). Counts show progress and Next requires every supported choice.
   Back preserves choices; class/background edits clear only invalid choices and
   their dependencies. The full first package and limits are in [TRAINING.md](TRAINING.md).
6. Wizards and Clerics choose supported cantrips in **Spell Choices**, after
   Training. Wizard Fire Bolt/Poison Spray and Cleric Sacred Flame have labeled
   checkboxes, counts and keyboard access.
   Back preserves selections; unfilled SRD choices remain explicitly pending.
   Class changes clear invalid choices and hide their controls. Presets receive
   their supported class cantrips. See
   [Wizard spell choices](POISON-SPRAY.md) and [Cleric spell choices](SACRED-FLAME.md).
7. Enter a name, up to 40 characters. There is no separate HP step.
   Choose a complete portrait with the dropdown or Previous/Next buttons.
   Optional gender, class, and in-game race filters combine to narrow the list;
   All clears each filter. Filters never change character data or the current
   portrait. An empty result disables selection and navigation. Any portrait may
   be selected regardless of the character's traits. Initial recommendations favor
   race, then gender, then class; manual choices persist through later edits.
8. Customize combat head and weapon/body parts, tall/short art, and all twelve
   region colors. Select a region's Color-1 or Color-2 button, then a palette
   swatch. Enlarged ready and action previews update immediately, recoloring
   only that part, with the complete portrait visible above both
   poses. Controls show **Not present** when the selected parts omit
   that region in both poses. Its saved colors return when the part is present.
9. Show the character sheet: race, gender, class, level, background, six scores,
    saving throws, Hit Dice, HP and inventory (initially empty), plus skill bonuses,
    Expertise, tools, languages and their grant sources. Saving throws
    include the class's level-one proficiency bonus where applicable.
    A note explains that SRD 5.2.1 uses the maximum class Hit Die plus applicable
    modifiers at level one. Only the HP numbers are colored: yellow for a
    positive combined modifier, red for a negative modifier, normal for zero.
    **Modifiers** opens a modal with ability score adjustments, racial, class, background, equipped
    item and spell details, identifying the source of each applied modifier.
    Its ability section shows changes to the rolled scores, not derived bonuses.
    Unadjusted abilities are omitted. Each adjusted ability starts with its rolled
    score, followed by each source with its signed adjustment in parentheses,
    then the final score on a separate line. The dialog scrolls for longer content.
    Background and advancement points retain separate sources. For example,
    `Soldier background (+2)` and `Level 4 Ability Score Improvement (+2)` appear
    on separate lines, including after saving and loading. A feat applied to an
    ability outside the background's list is attributed only to that feat.
    **Saving Throws**, beside Modifiers on both sheets, opens a separate dialog
    with an editable target DC (initially 15), the required d20 roll for each save,
    and ability-score and class-proficiency sources. Invalid DC input shows a
    correction hint. Rolls that always meet or cannot reach the DC are identified;
    ordinary saves do not automatically fail on 1 or succeed on 20. Conditional
    racial traits and persistent spell effects are not implemented in these totals.
    Score numbers are yellow for positive ability modifiers, red for negative
    modifiers, and the normal text color for zero. Unimplemented effects are
    identified explicitly.
    The party screen uses this same sheet for its selected member, including
    live HP, equipment and resources; roster entries show character names.
    Ready and Action combat sprites appear side by side under the party portrait,
    with their labels below the images. Portrait selection has no separate step;
    its preview controls remain available throughout creation.

**Back** preserves selections and allows earlier edits. Derived scores and HP
update from the current choices. **Start over** clears the single character.

The [shared party preview](PARTY.md) now adds a roster, six PC/two NPC positions,
party sheets/inventories, town purchases/equipping and combat handoffs to this
scene. Character creation itself retains the same choices. Supported combat
profiles and remaining class/skill/spell/lineage limits are listed there.
Persistence remains separate work.

## Native boundaries and rules

- `OpenGold.Rules/character_rules.h` defines the optional `CharacterRules`
  capability, draft, retained dice, choices, adjustments and sheet values.
- `OpenGold.Rules.Srd5` supplies the SRD choices and arithmetic. It has no Godot,
  original-asset, or campaign dependency. Dice use seeded SplitMix64 with
  rejection sampling; invalid assignments and selections are rejected.
- `OpenGold.Core/character_creator.h` owns the injected rules module and the
  single character's sequence and appearance. A completed sheet must return to
  editing before changing choices or scores; appearance can change during sheet
  review. No rolled result can be assigned twice.
  `create_character()` exports a finished `Character` only after the last step.
- `OpenGold.Core/character.h` is the reusable, value-owned character model.
  It retains all creation choices, original dice, assignments and bonuses;
  the evaluated sheet with rules identity and HP; portrait/part references;
  both combat color banks; and an `Inventory`. It has no dependency on Godot
  or the lifetime of the creator, rules module, or loaded artwork. The final
  demo sheet reads this object. Appearance changes are validated, and copying
  a character produces independent appearance and inventory data.
- `OpenGold.Core/inventory.h` owns item stacks with stable inventory-local IDs,
  content definition keys, display names and quantities. It supports addition,
  lookup and partial/full removal, rejecting invalid operations without losing
  items. Separate additions remain separate stacks. Rules or content adapters
  resolve the definition keys; inventory storage imposes no edition-specific
  equipment rules or legacy item limit. Starting gear selection, equipping,
  shop integration and saving remain outside this standalone demo.
- `CharacterCreationView` is native GDExtension presentation and input, backed
  by a Godot scene. Godot owns scene nodes; resource references use Godot RAII.

The rolling and HP rules come from
[SRD 5.2.1, pp. 21–22](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=21),
backgrounds from p. 83, and Dwarven Toughness from p. 84. Unlimited full-set
rerolls are the user-approved customization. Attribution is in
[NOTICE.md](../data/rules/srd-5.2.1/NOTICE.md).

Saving-throw proficiencies follow the SRD's Core Class Traits tables; the native
rules layer adds +2 to the two proficient saves at level one. The modifier modal
reports the effects currently implemented by that rules layer, including active
equipment conversions. It does not imply full species traits or ongoing spell
effect support.

## Original character art

`CharacterArt` in `OpenGold.Core` reads the installed game's DAX files, retaining
archive IDs and indexed combat pixels. No extracted artwork is distributed.

- Complete portraits and metadata come from `art/portraits/portraits.json` and its
  sibling PNGs. Build copies go to `src/OpenGoldBox/godot/bin/portraits/`. The
  Godot boundary parses the catalog and loads full-resolution images, displayed
  with nearest filtering in creation, pool and party previews.
- Campaign saves store the selected basename;
  characters without a current catalog entry display a deterministic recommendation.
- Legacy head/body decoding remains for compatibility and the existing pool's
  combat-color assignment. Those parts are no longer selectable portraits.

### Combat parts

- Combat heads come from `CHEAD.DAX`, base IDs 0..13. Body/weapon components come
  from `CBODY.DAX`, base IDs 0..31, plus a derived wand-free body 32. Add 64 for tall components and 128 for the
  action pose. Both head and body use the same size/pose bank. The head overlays
  the body's upper rows to produce a 24 x 24 icon.
- Source pixel 0 is transparent; source pixel 8 is an opaque black outline.
  Recoloring operates on source indices, so choosing black never makes a region
  transparent. The two poses share the same customization settings.

| Region | Color-1 source index | Color-2 source index |
| --- | --- | --- |
| Weapon | 7 | 15 |
| Body | 1 | 9 |
| Hair / face | 4 (hair) | 12 (face) |
| Shield | 6 | 14 |
| Arms | 2 | 10 |
| Legs | 3 | 11 |

Indices 5 and 13 belong to the cap and remain fixed. This corrects the original
demo's misassignment of the weapon, shield and cap masks. In the installed
`CBODY.DAX`, base ID 0 is unarmed and has no weapon or shield indices; base ID 1
uses 7/15 for its bow and has no 6/14 shield indices. Base ID 4 includes both
weapon and shield regions. All six regions retain two independently selectable
colors from the 16-color EGA palette. Controls use visible pixels after head
composition to determine whether a region is available in either pose.
Portrait colors are separate from combat-icon colors.

The assembly and mapping were checked against the installed DAX records. The
original manual's Icon menu describes independent head/weapon selection, two
poses and two sizes. Stephen S. Lee's
[binary-format research](https://gamefaqs.gamespot.com/c64/578753-pool-of-radiance/faqs/73869)
also identifies the component archives and size bank. Its portrait-menu indices
are not used as an assumed list of available archive IDs.

## Verification

`build.cmd` and `demos\build-rolf.cmd` include `opengold_character_tests`. Synthetic
tests cover deterministic rolls, retained dice, swaps, all class HP values,
background bonuses, invalid selections, name validation, navigation, restart,
truncation, all twelve color controls, head composition, fixed pixels,
transparency, character data ownership, inventory operations, additional-head
IDs, panel fitting, approved color preservation, and body composition. Set
`OPENGOLD_GAME_DIR` to the directory containing
the DAX files to additionally check every original head/body combination in
both sizes and both poses, plus exact recolor masks and absent regions in the
original art:

```bat
set "OPENGOLD_GAME_DIR=D:\path\to\POOLRAD"
build.cmd
```

Run the Godot scene's automated creation check:

```cmd
godot --headless --path src/OpenGoldBox/godot res://scenes/character_creation.tscn -- --character-check
```

For a focused check of every catalog image and all 324 Gender/Class/Race
filter combinations, run from the repository root:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless --path src/OpenGoldBox/godot \
  res://scenes/character_creation.tscn -- --portrait-check
```

The full `--character-check` exercises choices, rolling, swaps, background
bonuses, HP, name entry, all catalog portraits, combined optional filters,
race/gender defaults, all twelve
colors, sheet review and restart. Each
palette click checks the preview texture pixels against the composed icon and
counts changed pixels in each pose; it also checks that the portrait is intact.
The completed sheet is checked against the reusable character and inventory. Buttons
and list selections use injected viewport mouse input; name entry uses keyboard
events. Background controls also exercise their native selection signals.
For local screenshots of attributes, appearance and the sheet, omit
`--headless` and append `--capture`. Captures go to ignored `user-data/` files.
## Future class planning foundation

The native draft retains multiple desired class IDs separately from its one
level-one class. Target eligibility can be queried with partial assignments;
unmet targets never prevent selecting a different qualified starting class.
On Attributes, compact rolled totals sit beside the ability boxes. The scrollable
**Target class(es)** checklist shows all twelve classes and their prerequisites.
Checkboxes record future goals even when the character is not yet qualified.
Unmet requirements turn the relevant score box dark red and show a readable
explanation immediately below it. Warnings update with targets, assignments,
swaps, rerolls, backgrounds and bonuses; Fighter's either/or condition clears
both warnings when either primary qualifies. The character sheet retains these
future goals separately from the starting class.
SRD 5.2.1 multiclassing acquires another class when gaining a level; OpenGoldBox
defers multiclassing ([SCOPE-1](SRD-DECISIONS.md#scope-1-2026-09-30-level-cap-deferrals-and-rare-situations)). Later
Ability Score Improvement features can raise scores and help meet prerequisites;
future increases are not applied to level-one scores.

Rules reference: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
Multiclassing and class Ability Score Improvement features.
