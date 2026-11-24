# Character creation

The standalone C++20/Godot demo creates one level-one character and shows the
character sheet. It uses SRD 5.2.1 and complete OpenGoldBox portraits and the original game's combat art.

Run from Windows CMD at the repository root:

```cmd
build-opengoldbox.cmd
demos\review-character.cmd
```

The game build uses the existing [build prerequisites](ROLF.md). The
launcher opens the game's `src/OpenGoldBox/godot/scenes/character_creation.tscn`.
Art loads from the Pool of Radiance folder the game finds
([TECH.md](TECH.md#finding-the-games-folders)).

## Creation flow

1. **Name, Race & Gender**: the Name field and the Gender dropdown share the
   top row; Next waits for a name. Below, under **Race Selection**, choose one of
   the nine species included in SRD 5.2.1 from the two-column list. Choosing a
   race shows a random portrait of that race and, beside it, a short
   description and the race's SRD traits, each marked when the game does not
   apply it yet. Traits SCOPE-2 and DM-1 left out are not listed. Gender does
   not alter stats or restrict other choices. All three persist when returning.
2. Select alignment.
3. The **Dice Rolls** area starts with six empty boxes to the right of the
   abilities. Roll attributes to fill those boxes with totals only. The native
   character retains the underlying dice. Ability boxes remain empty until you drag
   results into them. Assigned results leave their original boxes empty.
   Dragging between filled ability boxes swaps their results. Dropping an
   unassigned roll onto a filled ability returns the displaced result to the
   right. All six abilities must be assigned before continuing. **Reroll all six**
   replaces the complete set and empties ability boxes; attempts are unlimited.
   Each roll also rolls the character's starting gold, 3d6 × 10 gp, which it
   brings to the party ([START-1](SRD-DECISIONS.md#start-1-2026-10-11-starting-gold-as-the-original-game-gave-it)).
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
   A group with one choice (Fighting Style, Divine Order, Primal Order) uses radio
   buttons. Each choice is as wide as the group's longest and has what it gives
   written to its right: a skill's check bonus once chosen (and its bonus now),
   a weapon's mastery property, or a style's or order's benefit. Hovering a
   choice for a second names the class or background that offers it.
6. Spellcasting classes choose their cantrips and spells in **Spell Choices**,
   after Training, with labeled checkboxes, counts and keyboard access.
   Back preserves selections; unfilled SRD choices remain explicitly pending.
   Class changes clear invalid choices and hide their controls. Presets receive
   their supported class cantrips. See
   [Wizard spell choices](POISON-SPRAY.md) and [Cleric spell choices](SACRED-FLAME.md).
   An Acolyte or a Sage of any class also chooses Magic Initiate's ability,
   two cantrips and a level-one spell here; a class without spells of its own
   gets this step for them. See [Magic Initiate](MAGIC-INITIATE.md).
7. Customize combat head and weapon/body parts, tall/short art, and all twelve
   region colors. Select a region's Color-1 or Color-2 button, then a palette
   swatch. Ready and action previews update immediately, recoloring only that
   part. In the game they sit in the page between the colors and the palette, at
   288 pixels (twelve times the 24-pixel original) at 1920 x 1080 and smaller
   whole multiples in a smaller window. Controls show **Not present** when the selected parts omit
   that region in both poses. Its saved colors return when the part is present.
8. **Portrait**: the page names the character's race, gender and class above
   the picker. Choose a complete portrait with the dropdown or Previous/Next
   buttons. Optional gender, class, and in-game race filters combine to narrow
   the list; All clears each filter. Filters never change character data or the
   current portrait. An empty result disables selection and navigation. Any
   portrait may be selected regardless of the character's traits. Initial
   recommendations favor race, then gender, then class; manual choices persist
   through later edits. While the Options screen's **Portraits** are Original,
   or the character's new portrait cannot be loaded, the page builds the
   original game's head-and-body portrait instead: **◀ Head** and **Head ▶**
   step through the original's heads (a race the original lacks, such as the
   Goliath, wears its own head first), **◀ Body** and **Body ▶** through its
   bodies, starting at the first of each. A character made while it shows a
   new portrait never sees these, so its original portrait is chosen at random;
   either is chosen again for another race or gender until the player builds
   one.
9. Show the character sheet: race, gender, class, level, background, six scores,
    saving throws, Hit Dice, HP and inventory (initially empty), plus skill bonuses,
    Expertise and their grant sources. **Features and choices** lists every
    other detail by heading: the species' choices and traits, subclass, class
    features, each class choice (Divine Order, Eldritch Invocations, Metamagic
    and the like), feats, cantrips, the spellbook or known spells, Magic
    Initiate's spells and the species' own spells. Saving throws
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
    with their labels below the images.

The game's creation screen has no title band and no right-hand preview pane;
the step list and the page fill the window. The reference demo keeps its
preview pane, where its portrait controls stay available on every page.

**Back** preserves selections and allows earlier edits. Derived scores and HP
update from the current choices. **Start over** clears the single character.

The [shared party preview](PARTY.md) now adds a roster, six PC/two NPC positions,
party sheets/inventories, town purchases/equipping and combat handoffs to this
scene. Character creation itself retains the same choices. Supported combat
profiles and remaining class/skill/spell/lineage limits are listed there.
Campaign saves keep the party ([saves](SAVES.md)).

Under the original rules the party screen also offers each game's **Modify
character**, which changes a new member's scores and Hit Points (from *Curse*
on its name) before its first experience, and *Unlimited Adventures* offers
it on the roll too ([ORIGINAL-RULES.md](ORIGINAL-RULES.md#modify-character)).
The SRD rules' creation described here is unchanged and has no Modify.

## The Buck Rogers games

*Countdown to Doomsday* and *Matrix Cubed* use the same creation screen and
creator, under their original rules ([countdown.md](games/countdown.md) and
[matrix-cubed.md](games/matrix-cubed.md), "Rules"), with their own pages where
those rules differ. The pages follow the original's order (`chargen.pas`
`CreateCharacterMenu`), the name first as on every original-rules screen:

1. **Name, Race & Gender**: a name of up to 15 letters (the record's), and
   the six races the menu offers. The text beside the race gives its ability
   adjustments, movement and careers, read from the rules. The picture is the
   race's starting body icon (`SetDefaultIcon`): these games have no
   portraits, so the body icon is a character's picture wherever a portrait
   would show (the party's cards, the sheets, the fights; the user,
   2026-11-20). There is no alignment or portrait page.
2. **Career**: the five careers; those the race cannot take are gray. The
   text gives the career's lowest scores, its eight career skills and the
   level and experience it starts with.
3. **Attributes**: the seven abilities, Tech the seventh, each where the roll
   left it, with the race's adjustment and the career's minimum in two small
   columns beside it (the user kept them, 2026-11-20), and the Hit Points,
   age, level, experience and credits the roll made. **Roll again** keeps the
   age and rolls the abilities and Hit Points again, as the original's
   "Reroll stats?" does.
4. **Weapon specialization**, a Warrior's only: one in *Countdown*, three in
   *Matrix Cubed*, at most three in one weapon (`viewchar.pas`
   `SpecializationMenu`). Next waits for them; − takes one back.
5. **Skills** (`skills.pas` `EditSkills`): the points left, the most in one
   skill and the general skills known, then the skills by group, one tab a
   group (the career's skills, Technical, Intelligence, Dexterity, Charisma,
   Wisdom and all the general skills; the original pages through them with
   Prev and Next). Each row gives the skill's points, its group ability's
   bonus and the total checks roll against, with − and +. A skill whose
   prerequisites are unmet is red and marked locked; one at its cap says so;
   choosing a skill's name says what it needs. In *Countdown* the player
   spends 80 career and 40 general points, at most 30 a skill; in *Matrix
   Cubed* the career's preset skills are set and points move from one skill
   to another of the same kind, at most 105 above a skill's start. Leaving
   with points unspent asks first, as the original does.
6. **Combat icon**: the 36 body icons of `CHARS.DAX` in the original's 6 by 6
   grid (`CustomizeIcon`), the chosen one shown ready and in action.
7. **Character sheet**: level, race, gender, career, age, Hit Points,
   experience, credits, the seven abilities, the eight saving throws, every
   skill with its level and value, Armor Class and movement, a Warrior's
   specializations and the items.

The draft holds the character's record, whole (`buck_rogers_draft`,
`opengold/buck_rogers_creation.h`): the roll makes it
(`new_buck_rogers_character`), and the later pages change it through
`CharacterCreator::change_record`. Joining the party gives the two free items,
readied (`outfit_buck_rogers_character`), and the record saves and loads byte
for byte. Every state fits the smallest window, 1120 x 800: the new pages are
laid out in containers.

Verified by `opengold_buck_rogers_creation_tests` (the pages, the roll, the
skills, the specializations and the save) and the Godot checks
`opengold_godot_buck_rogers_creation_countdown_to_doomsday` and
`..._matrix_cubed`, which make a character on the real screen at 1120 x 800:
a Desert Runner Warrior with its specialization, a career skill at its cap
and Jury Rig unlocked by its prerequisites in *Countdown*, and an Engineer
moving a preset point in *Matrix Cubed*.

**Training** in the team menu (`chargen.pas` `TrainingMenu`) uses the same
pages. Train raises the level and the session waits in `TourPhase::training`
with the trainee's choices (`RolfTourSession::buck_rogers_training`, the rules'
`BuckRogersTrainingChoices`): at a Warrior's even level the specialization page
first, one weapon and only this training's choice taken back, then the skill
screen with 40 career and 20 general points, at most 15 in a skill and one new
general skill. Points may be left, after the same question, and are lost.
`finish_buck_rogers_training` keeps the level, specialization and skills in the
record at once and the team menu shows again; until then the record is
unchanged. The pages hide the town and nothing can be saved or loaded while
they show; the script waits on its menu, so no save holds a training half
made, and the save formats are unchanged. Verified by
`opengold_countdown_tests` (the rules), `opengold_countdown_campaign_tests`
(the session at Salvation's HQ) and the Godot check
`opengold_godot_buck_rogers_training`; the earned endings train through the
same commands.

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

- Complete portraits and metadata come from `portraits/portraits.json` and its
  sibling PNGs in the new art's folder (`new-art-pack/portraits/` in the
  repository), the separate download chosen on the Options screen. The game
  reads the PNGs from that folder as they are, at full resolution, displayed
  with nearest filtering in creation, pool and party previews. Without the new
  art every character shows its original head-and-body portrait.
- Campaign saves store the selected basename;
  characters without a current catalog entry display a deterministic recommendation.
- Every character also has an original head-and-body portrait, shown in place
  of the new one while the Options screen's Portraits are Original or the new
  portrait cannot be loaded; the log then names the file. A save from before
  characters chose original portraits (campaign format 45) still loads, and
  each member on the default head 1 and body 1 gets a random original
  portrait, kept from the next save on. Players may change any of them at any
  time from the member's sheet.

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

`build.cmd` and `build-opengoldbox.cmd` include `opengold_character_tests`. Synthetic
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
SRD 5.2.1 multiclassing acquires another class when gaining a level, on the
level-up dialog's Class row ([advancement](ADVANCEMENT.md#multiclassing)). Later
Ability Score Improvement features can raise scores and help meet prerequisites;
future increases are not applied to level-one scores.

Rules reference: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
Multiclassing and class Ability Score Improvement features.
