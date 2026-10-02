# Shared campaign party preview

Issue [#2](https://github.com/stdarg/OpenGold/issues/2) connects the existing
character creator, town and combat scenes through reusable C++20 classes.
Run from PowerShell:

```powershell
.\demos\build-rolf.cmd
.\demos\review-character.cmd
```

## Preview flow

Losing shared-party combat opens a defeat window with **Reload a Saved Game**
and **Exit to OS**. Return to party is blocked; defeat does not revive anyone.
Reload uses the existing named-save dialog. Cancelling or rejecting a damaged
save retains the defeated campaign; a confirmed valid load replaces it and
removes the old combat scene. Standalone combat demos retain their restart controls.

Run `.\demos\review-character.cmd --defeat-check` for the automated loss/reload/exit
check and a capture at `user-data/party-defeat.png`. It uses an isolated save
profile. The [first Slums expedition](EXPEDITION.md) connects original district
travel, automatic combat, original tactical geometry and persistent rewards.

1. Finish a character, then select **Add to party** on the character sheet.
   The preview grants each new PC 250 gp. **View party** opens the roster.
   Alternatively, **Character Pool** opens 48 level-one characters, four for each
   SRD class, with varied alignments, genders, strong ability scores, and selected
   portrait heads/bodies. The ready/action sprite palettes use colors sampled
   from their portraits and matched to the original 16-color sprite palette.
   Select a candidate to preview the sheet and art, then **Add to party**.
   Added candidates receive a free class kit: a held melee weapon, the armor and
   shield the class is trained with (none for Monk, Sorcerer and Wizard; shield
   only for Barbarian), and a bow or crossbow with 20 arrows or bolts in the pack.
   Equip the bow in town to switch to ranged; equipment is locked during combat.
   The same candidate cannot be added twice; use Rejoin for a reserved member.
2. Select **Create character** to make additional PCs. Six PC positions and two
   separate NPC positions are available. Adding retains the finished character
   by value; subsequently editing the creator's draft does not edit that member.
3. Select a roster member by name to see the same character sheet as the creator,
   including gender between race and class, portrait, scores, saving throws,
   live HP, gold, combat AC, resources and inventory. **Modifiers** opens the
   detailed modifier modal for that member. Selecting an active member also selects
   that character for exploration/shop interactions.
4. **Remove member** places a member in reserve. **Rejoin party** restores the
   same member without resetting their possessions or live state.
5. **Recruit preview guard** supplies an authored level-one fighter companion.
   Remove/recruit preserves that same guard. This is preview content, not an
   identification or automatic conversion of an original NPC.
6. **Explore New Phlan** runs Rolf's tour and the existing original town host.
   Visit the arms shop at (13,8), accept its offer, buy equipment and leave.
   In a shop, click a member row, press Tab, or press a number key (1 for the
   first row) to choose the buyer; the buyer's row turns gold and the shop header
   shows their gold and item count. The shop script receives the final buyer.
   **Return to party** becomes available when the event finishes. The street or
   encounter view occupies only its required width at the left. Immediately to
   its right, scrollable member rows show name, class, AC and current/maximum HP.
   Clicking a row opens the character sheet without leaving town. During an
   active dialogue it is read-only; outside events it also selects that member.
   Down turns 180 degrees in place; Up steps forward and Left/Right turn 90 degrees.
7. In town, **Inventory** offers **Equip** and **Unequip** for the selected item,
   with the training penalty visible when selecting and equipping it. The party
   screen also offers **Equip selected**. Equipping a weapon replaces the previous weapon atomically. Unequip existing
   armor before equipping its replacement. Invalid combinations leave equipment
   unchanged. Ready/Action previews immediately reflect the equipped weapon and
   shield for PCs and recruited NPCs, using the same [artwork mappings](COMBAT-BODY-ASSIGNMENTS.md)
   as combat. **Party combat** opens the existing tactical UI against a Bandit.
8. Finish the fight and return. HP, death state and spent resources persist.
   Reopening exploration resumes the town session at its previous position.

Use [Save game](SAVES.md) before closing the application to retain the campaign.
The small arrow beside an eligible name opens [manual advancement](ADVANCEMENT.md).
The [Slums expedition](EXPEDITION.md) opens its mapped encounters automatically;
Party combat remains the explicitly launched training fight.

## Supported combat profiles

The rules module evaluates the created scores and equipment; these characters
do not select the old Vanguard/Adept/Healer fixture statistics. All twelve
classes can enter combat using their derived HP, AC, equipment, movement and
basic attacks. The classes with further features and the levels they reach are
summarized in the [README's current status](../README.md#current-status) (as of
2026-10-02); the [SRD coverage ledger](SRD-COVERAGE.md) has the detail. The list
below describes this party increment as first delivered, at levels 1–2.

- Fighter: ordinary attacks and two Second Wind uses.
- Cleric: ordinary attacks and Cure Wounds, with two level-one spell slots
  (three at character level 2).
- Wizard: ordinary attacks, Fire Bolt and single-target Magic Missile, with two
  level-one spell slots (three at character level 2).
- Unarmed attacks, one equipped weapon, one armor and one shield are
  modeled. Ability modifiers, proficiency, AC, maximum HP and movement come from
  SRD calculations. Dwarven starting HP and Goliath speed are included.
- Other class/species/background features, skills, origin feats, lineage and
  spell selection, components, weapon mastery,
  size-specific movement and expanded spells remain unimplemented. The displayed
  combat subset is not a complete SRD character implementation.

Original merchandise maps by item type, not its display name. Every one of the
47 weapon types in the artwork reviewer is accepted by the game equipment path.
Legacy names absent from SRD 5.2.1 use these explicit SRD equivalents:

| Original types | SRD rules key |
| --- | --- |
| 1 / 2 | battleaxe / handaxe |
| 3, 5, 10, 11, 14, 15, 16, 17, 40 | glaive |
| 4, 18, 19 | halberd |
| 6, 22, 33 | quarterstaff |
| 7 / 8 / 9 / 12 | club / dagger / dart / flail |
| 13, 25, 27, 29, 32 | pike |
| 20 / 21 / 23 / 24 / 26 | warhammer / javelin / mace / morningstar / war_pick |
| 30 / 31 | scimitar / spear |
| 34, 35, 36 | longsword |
| 37 / 38 / 39 | shortsword / greatsword / trident |
| 41, 43, 45 | longbow |
| 42, 44 | shortbow |
| 46 / 47 | light_crossbow / sling |
| 79 | wand (held focus; no charged spell is granted) |
| 50 / 51 / 52 / 53 / 54 | leather / padded / studded_leather / ring_mail / scale_mail |
| 55 / 56, 57 / 58 / 59 | chain_mail / splint (Banded Mail has no SRD armor) / plate / shield |

Damage dice, finesse, thrown/ranged distances, reach and mandatory two-hand
requirements follow the SRD weapon table.

### Versatile grip

There is no grip control ([AUTO-1](SRD-DECISIONS.md#auto-1-2026-09-30-automatic-choices-with-logging)).
The grip follows the other hand at attack time: a Versatile melee weapon is
wielded two-handed when the other hand is empty, and one-handed when a shield
or a second weapon is held. Quarterstaff (including Bo Stick and Jo Stick) and
Spear use 1d6/1d8; Battleaxe, Longsword, Trident, Warhammer and War Pick use
1d8/1d10. The larger die applies only to two-handed melee, including critical
hits and opportunity attacks; thrown attacks always use the base die. The damage
line of a Versatile melee hit names the grip, for example `... hits for 9
damage (two-handed).` Nothing about the grip is saved: it is derived from the
equipped items. Existing original artwork is selected by weapon/shield
combination and does not add new grip-specific poses.

All starting classes are proficient with simple weapons;
Barbarian, Fighter, Paladin and Ranger are proficient with all martial weapons.
Rogue is proficient with martial weapons that have Finesse or Light; Monk is
proficient with martial weapons that have Light. Shortsword and Scimitar therefore
include their proficiency bonus for both classes. The catalog records Light
for proficiency; its extra-attack mechanics remain unimplemented. Optional
feature-granted and multiclass-entry training remain separate work.
Weapon mastery, ammunition consumption/recovery,
and charged magical wand effects are not added
by this change. Bows and a plain focus retain unarmed melee; ranged weapons
provide actual ranged combat commands. Enchanted/effect-bearing original items
still require a supported conversion rather than silently receiving plain stats.

Equipment metadata comes from the rules module, not from the original edition's
`ITEMS` hand counts. Replacing a weapon retains a compatible shield. A rejected
two-handed weapon/shield combination leaves the previous loadout untouched.
The rules module includes the Rogue/Monk weapon proficiency, death-save,
Constitution/HP-history, remaining-turn-resource and opportunity-trigger corrections,
plus allied transit, the automatic Versatile grip and feature/training grant provenance.
It recomputes weapon bonuses from saved
class/equipment choices and retains the conversion of previously unsupported
ordinary weapons with verified original provenance. Campaign saves and combat
checkpoints retain feature/feat grant provenance, choices and training selections. See [advancement](ADVANCEMENT.md)
and [training support](TRAINING.md). Combat checkpoints also retain [Temporary HP](TEMPORARY-HP.md), Hit Dice and [recovery clocks](RECOVERY-CLOCKS.md); see also [rest resources](REST-RESOURCES.md).
Saves from older formats or rules versions are rejected; see [saves](SAVES.md#pre-10-format-policy).

Untrained use is allowed under [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf):
- Weapons omit the +2 level-one proficiency bonus on attacks.
- Shields provide no AC bonus without training.
- Armor retains its AC but imposes disadvantage on Strength/Dexterity D20 Tests
  and prevents spellcasting. Combat applies this to initiative and physical
  attacks; the saving-throw dialog identifies disadvantage on Strength/Dexterity
  saves. General ability checks and additional saving-throw combat effects are
  not yet implemented.

Training uses the base level-one class traits; optional Cleric/Druid orders that
add training are not selected by this creator. Equipping or removing gear rebuilds
these effects from the current loadout. Duplicate weapon/armor/shield slots remain
invalid in raw profiles; the campaign equip operation replaces the weapon slot, and heavy armor's low-Strength speed penalty is shown separately.

Enchanted, cursed, effect-bearing and other types remain purchasable inventory
but reject equipping until explicitly converted. The decoded original item and
template are retained as provenance, alongside the stable inventory item ID.
Original records and raw AD&D statistics are never rewritten into SRD values.

## Reusable native boundaries

`CampaignParty` owns an injected rules module and a value-owned `PartyState`:
roster, stable member IDs, active slots and selected slot. `PartyMember` owns a
`Character`, its one authoritative `Inventory`, equipment IDs, original item
provenance, purse, NPC source/morale and live `VitalState`.
All public member views are const; reacquire them after mutations.

The creator, `RolfTourSession` and `CombatDemo` share the same `CampaignParty`
owner. The native core has no Godot dependency. Godot owns presentation nodes;
temporary instantiated scenes have RAII owners until attached to the scene tree.

`RulesModule::character_profile` is an optional capability. It validates a
character sheet and equipped content keys and returns a module-owned profile
recipe and display statistics. `Participant` carries that recipe and optional
live vitals into combat. The SRD session owns turn state and exposes an opaque,
versioned resource continuation for each actor in its snapshot.

`CombatDemo` synchronizes accepted combat changes into the party. Campaign
victories award 300 XP once under a stable encounter key; reaching 300 XP
advances the supported character to level 2 and updates the rules profile's HP
and caster slots. The authored preview award is one-time, including across scene
recreation; original rewards require explicit mappings. Roster, shop,
equipment and script writes are locked during combat. When combat finishes,
editing resumes; no fresh HP or spell resources are granted on the next fight.
The module identity is now 0.3.0 and combat checkpoint format is version 2,
including character recipes. Old module saves fail the identity check explicitly.
[Campaign file saves](SAVES.md) now retain the complete supported party and idle New Phlan state. Native `PartyState` checkpoints remain the in-session rollback mechanism. Pending dialogue/services and combat are not campaign save boundaries.

## ECL adapter

The town adapter now connects LOAD CHARACTER/store, WHO, FIND ITEM,
PARTYSTRENGTH, movement CHECKPARTY and explicitly profiled ADD NPC requests to
the shared party. WHO presents actual members. Empty slots read as empty.
Purchases charge the selected member and synchronize the VM on shop exit.
LOAD CHARACTER's temporary script cursor can scan every slot without changing
the player's selection; WHO and the roster explicitly change that selection.
Event rollback restores the whole party as well as VM state.

The PC 1.3 command contracts are described in sections 7.14 and 12.3 of
[Lee's technical research](https://gamefaqs.gamespot.com/c64/578753-pool-of-radiance/faqs/73869).
Movement CHECKPARTY returns minimum, maximum, mean and zero; aliased output
addresses receive the last write. ADD NPC uses a monster ID and morale.
These contracts are reference-derived and synthetic-tested, not new DOS traces.

The SRD compatibility mapping is explicit: script movement is speed in five-foot
squares; incapacitated members contribute zero movement.

Party strength (PARTY STRENGTH) uses the original per-member formula (coab
`CMD_PartyStrength`): `(4 x Cleric level + current HP + 5 x max(0, -AC) +
5 x max(0, 21 - THAC0) + 8 x magic-user level) / 10`, summed with byte wrap.
It is fed what an equivalent AD&D character would have, not converted SRD bonuses:

- **THAC0** from the Gold Box class table by level (coab `engine/ovr018.cs`
  `thac0_table`), by nearest original class group: Fighter, Barbarian, Paladin
  and Ranger use the fighter row (20 at levels 1-2, then 21 - level); Cleric,
  Druid and Monk the cleric row (20, 18, 16, 14 by three levels); Wizard,
  Sorcerer and Warlock the magic-user row (21 to level 5, 19 to 10, then 17);
  Rogue and Bard the thief row (20 to level 4, 19 to 8, then 16). The table ends
  at level 12; levels 13-15 keep the level-12 value. The best magic to-hit bonus
  of an equipped original item lowers THAC0.
- **AC**: 10, less the AD&D value of the armor worn (padded and leather 8,
  studded leather and ring mail 7, scale mail 6, chain mail 5, splint 4, plate 3;
  the SRD-only suits use the nearest: hide and chain shirt 6, breastplate 5,
  half plate 4), 1 for a shield, and original magic bonuses on armor and shield.
  Dexterity and Strength adjustments are not included.
- **Cleric levels** count SRD Cleric levels only (the original counts its
  Cleric class alone); **magic-user levels** count Wizard, Sorcerer and Warlock.

Dead members contribute zero. Example: six level-two fighters in chain mail and
shield at 20 HP give `6 x (20 + 5) / 10 = 12`; the conversion this replaced
(`20 - SRD AC`, `20 - attack bonus`) gave 30 for the same party at +5 to hit.

Thief-skill/effect CHECKPARTY variants still fault. `PhlanResources::npc_profiles`
accepts explicit native character conversions for this bank; unregistered NPCs
and the special hostile NPC 24 fault. General bank resolution, morale behavior
and original NPC conversion coverage remain campaign work. An eligible,
uninterrupted long rest advances campaign time by eight hours. The original
city-watch interruption grants no recovery. Temple Cure Wounds costs 100 gp for
a wounded living active member; resurrection is unsupported. See
[service limits and checks](RECOVERY.md). The preview guard
and synthetic ADD NPC test establish ownership/recruitment without inventing
conversions for original content.

## Verification

`opengold_party_tests` covers roster capacity, independent ownership, NPC live
state, purchasing/equipping and rejected operations, script-selected characters,
WHO tickets, party queries, ADD NPC, FIND ITEM branching, event rollback, combat
handoffs, no resource refill and deterministic character checkpoint continuation.
The [recovery route](RECOVERY.md) adds XP thresholds, scene reentry, long rest,
original temple/inn services, interruption and payment/rollback checks.

The Godot acceptance route uses the actual installed original town shop and
native control callbacks, then equips and fights with that same party:

```powershell
godot --headless --path demos/godot res://scenes/character_creation.tscn -- --party-check
```

For local screenshots, omit `--headless` and append `--capture`. Inspect at
1280x900 and 1120x800. Existing character, town and combat checks remain available.

Orc Adrenaline Rush uses and pending Temporary HP replacement also persist;
see [Temporary HP](TEMPORARY-HP.md).
