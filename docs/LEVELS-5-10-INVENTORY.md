# Levels 5–10: SRD 5.2.1 inventory for OpenGold

Planning inventory for raising the cap from level 4 to level 10 for the twelve
SRD classes and their twelve SRD subclasses ([plan](LEVELS-5-10.md), #176). It
is a factual extract plus a proposed spell triage. Nothing here is a decision:
every KEEP, CUT and UNSURE is a proposal; decisions are recorded in
[SRD-DECISIONS](SRD-DECISIONS.md).

**Sources.** [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), extracted with `pdftotext`. Page numbers are
the printed page numbers in the page footers ("NN System Reference Document
5.2.1"). I checked them against the PDF: PDF page N carries footer N (pp. 107,
150 and 175 checked). The two-column layout mixes text in the extraction, so
every spell entry was re-read from a single-column (`pdftotext` reading-order
or column-cropped) extraction of the same PDF. Repo context: `docs/SRD-DECISIONS.md`
(DM-1, DM-2, DM-3, SCOPE-1, SCOPE-2, CLASS-2, CLASS-10, CLASS-11, COVER-1,
MELEE-1, HIDE-1), `docs/SPELL-INVENTORY.md` and `docs/audits/srd-5.2.1-rules.md`
at `c4076002`.

**Already decided upstream (no new approval needed):** DM-2 (2026-09-30)
already cuts these level 3–5 spells by name: Clairvoyance, Major Image,
Sending, Speak with Dead, Tongues (level 3); Divination, Hallucinatory Terrain
(level 4); Commune, Contact Other Plane, Dream, Geas, Legend Lore, Modify
Memory, Scrying, Seeming (level 5). That is 15 of the 42 proposed cuts. DM-2
also cuts features that exist only to cast such spells. It names the Warlock's
Contact Patron as an example.

---

## Summary

### Spell counts (levels 3–5)

All **114** SRD spells of levels 3–5 (42 + 34 + 38) are on at least one of
the eight caster lists. Every one is reachable by level 10, because the full
casters and the Warlock all get level 3 slots at 5, level 4 slots at 7 and
level 5 slots at 9. Paladin and Ranger reach level 2 spells at 5 and level 3
spells at 9; they reach no level 4–5 spell by 10. The class-table union
matches the spell-description headers exactly. Unlike levels 0–2 (Phantasmal
Force, Mind Spike), there is no list/description mismatch at these levels.

| Spell level | KEEP | CUT | UNSURE | Total |
| --- | ---: | ---: | ---: | ---: |
| 3 | 22 | 16 | 4 | 42 |
| 4 | 24 | 9 | 1 | 34 |
| 5 | 17 | 17 | 4 | 38 |
| **All** | **63** | **42** | **9** | **114** |

UNSURE (9): Animate Dead, Magic Circle, Plant Growth, Tiny Hut (level 3);
Faithful Hound (level 4); Animate Objects, Mislead, Reincarnate, Telekinesis
(level 5).

### Biggest new systems needed

1. **Progression plumbing.** XP to 64,000; Proficiency Bonus +3 at 5 and +4
   at 9, reaching every derived number (the audit, G6, says +2 is hard-coded);
   spell slots of levels 3–5; Pact slots of levels 3–5; upcasting to those
   levels; a second ASI/feat (8, plus Fighter 6 and Rogue 10); cantrip
   upgrades at character level 5 (for example, Fire Bolt 2d10 on p. 132,
   Eldritch Blast two beams on p. 127).
2. **Extra Attack.** Barbarian, Fighter, Monk, Paladin and Ranger at 5. The
   Thirsting Blade invocation gives it too (Warlock 5+, Pact of the Blade).
   The repo has no Extra Attack code yet (a grep of `src` finds none). It must
   work with Weapon Mastery, Cleave, Horde Breaker, Flurry of Blows, Sneak
   Attack's once per turn and the Barbarian's Brutal Strike.
3. **Auras and moving emanations.** Paladin Aura of Protection (6), Aura of
   Devotion (7), Aura of Courage (10); Spirit Guardians, Aura of Life,
   Conjure Minor Elementals, Conjure Woodland Beings, Antilife Shell, Conjure
   Animals (a movable zone). These need on-enter, start-of-turn and
   end-of-turn triggers with "once per turn" limits. Moonbeam, Spike Growth,
   Web and Flaming Sphere suggest part of this exists.
4. **Persistent areas and walls.** Cloudkill (drifts), Insect Plague, Sleet
   Storm, Stinking Cloud, Black Tentacles, Guardian of Faith, Call Lightning;
   and walls with sides and line crossing: Wall of Fire, Wind Wall (deflects
   missiles), Wall of Stone, Wall of Force.
5. **Summons and controlled creatures.** Giant Insect, Summon Dragon, Animate
   Objects, Animate Dead and Find Steed (Paladin 5) are allies that act after
   the caster. Dominate Beast and Dominate Person need player control of an
   enemy. SCOPE-2 keeps "only spells that actually create creatures" in scope
   for companions (#173). A grep of `src` finds no summon code.
6. **Dispelling and countering.** Dispel Magic, Counterspell (a reaction to a
   creature beginning a cast, so monsters must cast), Remove Curse, Greater
   Restoration, Dispel Evil and Good. These need a per-target registry of
   ongoing spells and curses.
7. **New states and conditions.** Ethereal (Blink); banished (Banishment);
   sealed (Resilient Sphere); polymorphed others (Polymorph); Haste's extra
   action and lethargy; Slow's action limits; Confusion's behavior table;
   Exhaustion and Petrified (Greater Restoration removes them, and Tireless
   lowers Exhaustion; the grep finds neither); Stunned (Stunning Strike);
   Evasion.
8. **Resurrection** (kept by SCOPE-2, #175; the grep finds no code):
   Revivify within 1 minute (a combat use), Raise Dead (-4 to D20 Tests,
   recovering 1 per Long Rest), Reincarnate (UNSURE); Death Ward and Aura of
   Life as 0-HP protection.
9. **Long buffs that outlast a fight.** Death Ward (8 h), Stoneskin (Conc 1 h),
   Protection from Energy (Conc 1 h), Freedom of Movement (1 h), Fire Shield
   (10 min) and Contagion (up to 7 days). CLASS-2 currently ends Concentration
   when combat ends and tracks it in combat only.
10. **More reactions.** Barbarian Retaliation, Rogue Uncanny Dodge, Bard
    Countercharm, Counterspell; Fighter Indomitable and Fiend Dark One's Own
    Luck as post-roll choices. The memory note "no perfect-information
    automation" applies: prompt for them, or follow BARD-1's public-information
    pattern.

### Features that cannot work without systems the game lacks

- **Mounts.** Paladin **Faithful Steed** (5, free Find Steed): a mount with
  mounted combat (SRD p. 130). SCOPE-1 dropped the mounted Lance rules. The
  steed's Fly Speed needs a level 4+ spell, so it never flies by level 10.
  Phantom Steed (level 3) is also a mount.
- **Flight.** The Fly spell (and Draconic Sorcery always prepares it at 5);
  Druid Wild Shape fly forms from level 8 (p. 42; SCOPE-2 says no fly forms);
  the Dragonborn's Draconic Flight at character level 5 (p. 84). Fly Speeds on
  the Giant Insect (wasp) and Draconic Spirit stat blocks go unused.
- **Movement modes the map lacks.** Monk Acrobatic Movement (9: walls and
  liquids; SCOPE-2 already cuts it); Ranger Roving's Climb and Swim Speeds (6;
  SCOPE-2 keeps only the +10 ft); Gift of the Depths; Water Walk, Water
  Breathing, Tree Stride, Meld into Stone, Gaseous Form, Passwall.
- **Telepathy and remote senses.** Telepathic Bond, Sending, Dream; the Gaze
  of Two Minds invocation; Arcane Eye and Visions of Distant Realms;
  Clairvoyance and Scrying. The Dominate spells' "telepathic link" can be
  treated as plain control.
- **DM-adjudicated features.** Contact Patron (Warlock 9; DM-2 cuts it by
  name), Whispers of the Grave (Speak with Dead), Ascendant Step (Levitate,
  cut by DM-3), Master of Myriad Forms (Alter Self, cut by DM-3) and Investment
  of the Chain Master (Pact of the Chain was removed by DM-1).
- **Cut by an earlier decision.** Wizard Evocation Savant's free spell at each
  new slot level (SCOPE-2), Rogue Supreme Sneak's "Three-Quarters Cover"
  condition (COVER-1 leaves only total cover), Arcane Hand's Grasping Hand
  (grappling dropped) and Interposing Hand (no Half Cover).

### Open decisions this inventory surfaces

1. **Costly consumed components.** CLASS-11 removed all spell components. That
   makes Revivify (300 gp), Raise Dead (500 gp), Greater Restoration and
   Stoneskin (100 gp each), Glyph of Warding (200 gp) and Magic Circle (100 gp)
   free. Is free resurrection intended?
   Answered by LEVELS-1 (resurrection takes an offering) and COST-1 (the
   others stay free).
2. **Pre-fight buffs.** Should long buffs (Death Ward, Stoneskin, Protection
   from Energy, Freedom of Movement) cast in camp persist into the next fight?
   CLASS-2 ends Concentration at the end of combat.
3. **Summons and control.** Approve one shared "allied creature takes its turn
   after its caster" system for Giant Insect, Summon Dragon, Animate Objects,
   Animate Dead and Dominate Beast/Person? Or defer some of them?
4. **The nine UNSURE spells** (reasons in section 3).
5. **More Expertise** at Rogue 6, Bard 9 and Ranger 9. SCOPE-2 says
   "Expertise … no further work; already implemented Expertise stays" and cuts
   Ranger Deft Explorer's Expertise. Does that cover the new picks?
   Answered by LEVELS-1: the new picks are implemented.
6. **Daylight.** It is cut with Light under DM-3, but it is the counter to
   Darkness (which the game keeps). Keep it for that?
7. **Slow's 25% spell failure** applies to spells with a Somatic component,
   which CLASS-11 removed. Apply it to every spell, or drop it?
8. **Banishment** of extraplanar creatures that never return: count them as
   defeated for XP and loot?

---

## 1. Shared progression for levels 5–10

### Experience and Proficiency Bonus (Character Advancement table, p. 23)

| Level | XP | Proficiency Bonus | Change |
| ---: | ---: | ---: | --- |
| 4 | 2,700 | +2 | (current cap) |
| 5 | 6,500 | +3 | PB rises |
| 6 | 14,000 | +3 | |
| 7 | 23,000 | +3 | |
| 8 | 34,000 | +3 | |
| 9 | 48,000 | +4 | PB rises |
| 10 | 64,000 | +4 | |

Fixed Hit Points per level (p. 23): Barbarian 7, Fighter/Paladin/Ranger 6,
Bard/Cleric/Druid/Monk/Rogue/Warlock 5, Sorcerer/Wizard 4, each + Con modifier.
A Constitution modifier increase raises maximum HP by 1 per level reached
(p. 23).

### Full casters: Bard, Cleric, Druid, Sorcerer, Wizard

The five class tables are identical for slots (Bard p. 31, Cleric p. 36,
Druid p. 41, Sorcerer p. 65, Wizard p. 77).

| Level | 1st | 2nd | 3rd | 4th | 5th | New spell level |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 5 | 4 | 3 | 2 | — | — | **3rd** |
| 6 | 4 | 3 | 3 | — | — | |
| 7 | 4 | 3 | 3 | 1 | — | **4th** |
| 8 | 4 | 3 | 3 | 2 | — | |
| 9 | 4 | 3 | 3 | 3 | 1 | **5th** |
| 10 | 4 | 3 | 3 | 3 | 2 | |

Sorcerer **Creating Spell Slots** (p. 66): a level 3 slot costs 5 Sorcery
Points (minimum Sorcerer level 5), level 4 costs 6 (level 7), level 5 costs 7
(level 9). No slot above level 5 can be created. Wizard Arcane Recovery
(p. 78) and Land Druid Natural Recovery (p. 46) recover combined levels up to
half the class level (round up), with no slot of level 6+. At level 10 that is
5 levels, so a level 5 slot can now be recovered.

### Half casters: Paladin (p. 53) and Ranger (p. 58)

| Level | 1st | 2nd | 3rd | New spell level |
| ---: | ---: | ---: | ---: | --- |
| 5 | 4 | 2 | — | **2nd** |
| 6 | 4 | 2 | — | |
| 7 | 4 | 3 | — | |
| 8 | 4 | 3 | — | |
| 9 | 4 | 3 | 2 | **3rd** |
| 10 | 4 | 3 | 2 | |

New level 2 spells at 5 found only on these lists: **Find Steed** and
**Shining Smite** (Paladin). SPELL-INVENTORY.md already defers both "until
Paladin level 5".

### Warlock Pact Magic (p. 71)

| Level | Pact slots | Slot level | New spell level |
| ---: | ---: | ---: | --- |
| 5 | 2 | 3 | **3rd** |
| 6 | 2 | 3 | |
| 7 | 2 | 4 | **4th** |
| 8 | 2 | 4 | |
| 9 | 2 | 5 | **5th** |
| 10 | 2 | 5 | |

All Pact slots share one level, and lower spells are cast at that level
(p. 71). Slots recover on a Short or Long Rest. Magical Cunning (level 2)
restores up to half the maximum (round up), which is 1 slot.

### Other level 5–10 effects shared across classes

- **Cantrip upgrades at character level 5** (each cantrip's "Cantrip
  Upgrade"): damage dice double (for example Acid Splash p. 107, Fire Bolt
  p. 132), and Eldritch Blast gains a second beam (p. 127). Next step at 11.
- **Ability Score Improvement** (General feat, p. 87): +2 to one score or +1
  to two, maximum 20. It is repeatable. Each class's ASI says "or another
  feat of your choice for which you qualify". The other SRD feats are the
  Origin feats (Alert, Magic Initiate, Savage Attacker, Skilled), Grappler
  (General, dropped by SCOPE-2) and the Fighting Style feats (with the
  Fighting Style feature). Epic Boons need level 19.
- **Species at character level 5** (pp. 84–86; not class features, listed for
  completeness): Dragonborn Breath Weapon 2d10 and Draconic Flight; Goliath
  Large Form (SCOPE-2 cuts it); the level 5 lineage/legacy spells: High Elf
  Misty Step, Wood Elf Pass without Trace (cut by DM-3), Drow Darkness (Drow
  not offered, DROW-1), Abyssal Tiefling Hold Person, Chthonic Ray of
  Enfeeblement, Infernal Darkness.

---

## 2. Class and subclass features at levels 5–10

"Choice" marks a feature where the player picks. "SCOPE-2" and similar tags
point to existing repo decisions that already affect the feature.

### Barbarian (table p. 28) — Path of the Berserker (p. 30)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Extra Attack | Two attacks per Attack action. | 29 | |
| 5 | Fast Movement | +10 ft Speed without Heavy armor. | 29 | |
| 6 | Mindless Rage (Berserker) | Immune to Charmed and Frightened while raging; entering Rage ends them. | 30 | |
| 7 | Feral Instinct | Advantage on Initiative. | 29 | |
| 7 | Instinctive Pounce | Move up to half Speed as part of the Bonus Action that starts Rage. | 29 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 29 | Choice |
| 9 | Brutal Strike | With Reckless Attack, give up Advantage on one Strength attack: on a hit, +1d10 and either Forceful Blow (push 15 ft, then you may follow half your Speed without provoking) or Hamstring Blow (-15 ft Speed until your next turn). | 29–30 | Choice per use |
| 10 | Retaliation (Berserker) | Reaction: one melee attack against a creature within 5 ft that damaged you. | 30 | |

Table changes (p. 28): Rages 3 at 5, 4 from 6; Rage Damage +3 from 9; Weapon
Mastery kinds 4 at 10.

### Bard (table p. 31) — College of Lore (p. 35)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Font of Inspiration | Bardic Inspiration recovers on a Short or Long Rest; spend a spell slot (no action) to regain one use. | 32 | |
| 5 | Bardic die d8 | The die becomes a d8 (d10 at 10). | 32 | |
| 6 | Magical Discoveries (Lore) | Learn two spells from the Cleric, Druid or Wizard lists (cantrip or castable level), always prepared; swap one per Bard level. | 35 | Choice |
| 7 | Countercharm | Reaction: when you or a creature within 30 ft fails a save against Charmed or Frightened, it rerolls with Advantage. | 33 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 32 | Choice |
| 9 | Expertise | Expertise in two more skill proficiencies. | 32 | Choice |
| 10 | Magical Secrets | Each new prepared spell (and any replacement) may come from the Bard, Cleric, Druid or Wizard lists. | 33 | Choice |

Counts (p. 31): cantrips 3 (5–9), 4 at 10. Prepared spells 9, 10, 11, 12, 14,
15 (levels 5–10). Bard prepared spells change one per level gained (p. 32).

### Cleric (table p. 36) — Life Domain (p. 40)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Sear Undead | Turn Undead also deals Radiant damage of (Wis modifier, min 1)d8 to each Undead that fails; the turn effect continues. | 37 | |
| 5 | Life Domain Spells | Always prepared: Mass Healing Word, Revivify. | 40 | |
| 6 | Blessed Healer (Life) | After a slot-cast spell heals others, you regain 2 + slot level HP. | 40 | |
| 7 | Blessed Strikes | Choose Divine Strike (once per turn +1d8 Necrotic or Radiant on a weapon hit) or Potent Spellcasting (+Wis modifier to Cleric cantrip damage). | 38 | Choice |
| 7 | Divine Spark 2d8 | Channel Divinity's Divine Spark rolls 2d8. | 37 | |
| 7 | Life Domain Spells | Always prepared: Aura of Life, Death Ward. | 40 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 37 | Choice |
| 9 | Life Domain Spells | Always prepared: Greater Restoration, Mass Cure Wounds. | 40 | |
| 10 | Divine Intervention | Magic action: cast any Cleric spell of level 5 or lower that is not a Reaction, without a slot or Material components; once per Long Rest. | 38 | Choice per use |

Counts (p. 36): Channel Divinity uses 2 at 5, 3 from 6. Cantrips 4 (5–9), 5 at
10. Prepared 9, 10, 11, 12, 14, 15. SPARK-1 (Radiant only) applies to the 2d8
Spark. Divine Intervention lists may include cut spells, so offer only kept
ones.

### Druid (table p. 41) — Circle of the Land (p. 46)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Wild Resurgence | Once per turn with no Wild Shape left, spend a slot to gain one use; once per Long Rest, spend a use to gain a level 1 slot. | 43 | |
| 5 | Circle Spells | Arid: Fireball. Polar: Sleet Storm. Temperate: Lightning Bolt. Tropical: Stinking Cloud. | 46 | Land chosen each Long Rest |
| 6 | Natural Recovery (Land) | Once per Long Rest, cast one prepared Circle spell without a slot; on a Short Rest recover slots totaling half Druid level (round up), none level 6+. | 46 | |
| 7 | Elemental Fury | Choose Potent Spellcasting (+Wis modifier to Druid cantrip damage) or Primal Strike (once per turn +1d8 Cold/Fire/Lightning/Thunder on a weapon or Beast-form hit). | 43 | Choice |
| 7 | Circle Spells | Arid: Blight. Polar: Ice Storm. Temperate: Freedom of Movement. Tropical: Polymorph. | 46 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 43 | Choice |
| 8 | Wild Shape forms | 8 known forms, maximum CR 1, Fly Speed allowed. | 42 | Choice |
| 9 | Circle Spells | Arid: Wall of Stone. Polar: Cone of Cold. Temperate: Tree Stride. Tropical: Insect Plague. | 46 | |
| 10 | Nature's Ward (Land) | Immune to Poisoned; Resistance by current land: Fire (arid), Cold (polar), Lightning (temperate), Poison (tropical). | 46 | |
| 10 | Land's Aid 3d6 | Land's Aid damage and healing rise to 3d6. | 46 | |

Counts (p. 41): Wild Shape uses 2 at 5, 3 from 6. Cantrips 3 (5–9), 4 at 10.
Prepared 9, 10, 11, 12, 14, 15. Repo notes: CLASS-10 limits forms to beasts
with combat art, SCOPE-2 says no fly forms, and WILD-1 ends forms with the
fight. Level 8's CR 1 and fly forms need art and a ruling.

### Fighter (table p. 47) — Champion (p. 49)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Extra Attack | Two attacks per Attack action. | 48 | |
| 5 | Tactical Shift | When Second Wind is used as a Bonus Action, move up to half Speed without provoking. | 48 | |
| 6 | Ability Score Improvement | ASI feat or another feat (Fighter-only extra at 6). | 48 | Choice |
| 7 | Additional Fighting Style (Champion) | A second Fighting Style feat. | 49 | Choice |
| 8 | Ability Score Improvement | ASI feat or another feat. | 48 | Choice |
| 9 | Indomitable (one use) | Reroll a failed save with a bonus equal to Fighter level; once per Long Rest. | 48 | Choice per use |
| 9 | Tactical Master | On an attack with a mastered weapon, swap its mastery for Push, Sap or Slow. | 48 | Choice per use |
| 10 | Heroic Warrior (Champion) | In combat, gain Heroic Inspiration at the start of your turn if you lack it. | 49 | |

Counts (p. 47): Second Wind uses 3 (5–9), 4 at 10. Weapon Mastery kinds 4
(5–9), 5 at 10. Two Extra Attacks comes at 11. HEROIC-1's automatic use rule
would apply to Heroic Warrior.

### Monk (table p. 50) — Warrior of the Open Hand (p. 52)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Extra Attack | Two attacks per Attack action. | 51 | |
| 5 | Stunning Strike | Once per turn on a Monk weapon or Unarmed hit, 1 Focus: Con save or Stunned until your next turn; on a success, Speed halved and the next attack against it has Advantage. | 51 | Choice per use |
| 6 | Empowered Strikes | Unarmed Strikes may deal Force damage. | 51 | |
| 6 | Wholeness of Body (Open Hand) | Bonus Action: heal Martial Arts die + Wis modifier (min 1); uses = Wis modifier (min 1) per Long Rest. | 52 | |
| 7 | Evasion | Dex-save-for-half effects: no damage on success, half on failure (not while Incapacitated). | 51 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 51 | Choice |
| 9 | Acrobatic Movement | Move along vertical surfaces and across liquids without armor or shield. | 51 | |
| 10 | Heightened Focus | Flurry of Blows makes 3 strikes; Patient Defense with Focus grants Temporary HP of two Martial Arts dice; Step of the Wind with Focus can carry a willing adjacent creature. | 51–52 | |
| 10 | Self-Restoration | End Charmed, Frightened or Poisoned on yourself at the end of each turn; no Exhaustion from lack of food or drink. | 52 | |

Counts (p. 50): Martial Arts die 1d8 (5–10). Focus Points 5–10 (equal to
level). Unarmored Movement +10 ft at 5, +15 ft at 6–9, +20 ft at 10. The Open
Hand's next feature is at 11. SCOPE-2 already cuts Acrobatic Movement (wall
and water running).

### Paladin (table p. 53) — Oath of Devotion (p. 56)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Extra Attack | Two attacks per Attack action. | 55 | |
| 5 | Faithful Steed | Find Steed always prepared; one free cast per Long Rest. | 55 | |
| 5 | Oath of Devotion Spells | Always prepared: Aid, Zone of Truth (Zone of Truth cut by DM-1). | 56 | |
| 6 | Aura of Protection | 10-ft emanation: you and allies add your Cha modifier (min +1) to saves; inactive while you are Incapacitated; one aura per creature. | 55 | |
| 7 | Aura of Devotion (Devotion) | You and allies in the aura are immune to Charmed. | 56 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 55 | Choice |
| 9 | Abjure Foes | Magic action, 1 Channel Divinity: up to Cha modifier (min 1) creatures within 60 ft, Wis save or Frightened for 1 min or until damaged; while Frightened they can only move, act or take a Bonus Action. | 55 | Targets |
| 9 | Oath of Devotion Spells | Always prepared: Beacon of Hope, Dispel Magic. | 56 | |
| 10 | Aura of Courage | You and allies in the aura are immune to Frightened. | 55 | |

Counts (p. 53): Channel Divinity 2 (5–10). Prepared spells 6, 6, 7, 7, 9, 9.
Weapon Mastery stays 2 kinds (p. 54). Find Steed (level 2 Conjuration,
p. 130) summons an Otherworldly Steed (AC 10 + 1 per spell level, HP 5 + 10
per spell level, Fly only with a level 4+ spell, slam 1d8 + spell level). It
is a controlled mount under the mounted combat rules.

### Ranger (table p. 58) — Hunter (p. 61)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Extra Attack | Two attacks per Attack action. | 59 | |
| 6 | Roving | +10 ft Speed without Heavy armor; Climb and Swim Speeds equal to Speed. | 59 | |
| 7 | Defensive Tactics (Hunter) | Choose Escape the Horde (Opportunity Attacks against you have Disadvantage) or Multiattack Defense (a creature that hits you has Disadvantage on its other attacks against you this turn); swap on a rest. | 61 | Choice |
| 8 | Ability Score Improvement | ASI feat or another feat. | 59 | Choice |
| 9 | Expertise | Expertise in two skills. | 59 | Choice |
| 10 | Tireless | Magic action: 1d8 + Wis modifier Temporary HP, uses = Wis modifier (min 1) per Long Rest; each Short Rest lowers Exhaustion by 1. | 59 | |

Counts (p. 58): Favored Enemy (free Hunter's Mark casts) 3 at 5–8, 4 at 9–10.
Prepared 6, 6, 7, 7, 9, 9. Weapon Mastery stays 2. SCOPE-2 already keeps only
Tireless from the exploration half and cuts climb and swim speeds. Hunter has
no always-prepared spells. Superior Hunter's Prey comes at 11.

### Rogue (table p. 62) — Thief (p. 64)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Cunning Strike | When dealing Sneak Attack, forgo dice for an effect: Poison (1d6; needs a Poisoner's Kit; Con save or Poisoned 1 min), Trip (1d6; Dex save or Prone), Withdraw (1d6; move half Speed without provoking). Save DC 8 + Dex + PB. | 63 | Choice per use |
| 5 | Uncanny Dodge | Reaction: halve the damage of an attack that hits you, from an attacker you can see. | 63 | |
| 6 | Expertise | Expertise in two more skills. | 61 | Choice |
| 7 | Evasion | As the Monk's. | 63 | |
| 7 | Reliable Talent | Treat a d20 of 9 or lower as 10 on proficient checks. | 63 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 63 | Choice |
| 9 | Supreme Sneak (Thief) | New Cunning Strike, Stealth Attack (1d6): attacking does not end Hide's Invisible if you end the turn behind Three-Quarters or Total Cover. | 64 | |
| 10 | Ability Score Improvement | Rogue-only extra ASI at 10. | 63 | Choice |

Counts (p. 62): Sneak Attack 3d6 (5–6), 4d6 (7–8), 5d6 (9–10). Repo notes:
AUTO-1 applies Sneak Attack automatically, but Cunning Strike needs a choice
at that moment. DM-1 removed tools, which matters for Poison's Poisoner's Kit.
COVER-1 leaves only Total Cover for Supreme Sneak. Use Magic Device comes at 13.

### Sorcerer (table p. 65) — Draconic Sorcery (pp. 69–70)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Sorcerous Restoration | On a Short Rest regain Sorcery Points up to half Sorcerer level (round down); once per Long Rest. | 66 | |
| 5 | Draconic Spells | Always prepared: Fear, Fly. | 70 | |
| 6 | Elemental Affinity (Draconic) | Choose Acid, Cold, Fire, Lightning or Poison: Resistance to it, and add Cha modifier to one damage roll of spells of that type. | 70 | Choice |
| 7 | Sorcery Incarnate | With no Innate Sorcery uses left, activate it for 2 Sorcery Points; while it is active, use up to two Metamagic options per spell. | 66 | |
| 7 | Draconic Spells | Always prepared: Arcane Eye, Charm Monster. | 70 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 66 | Choice |
| 9 | Draconic Spells | Always prepared: Legend Lore, Summon Dragon. | 70 | |
| 10 | Metamagic | Two more Metamagic options. | 66 | Choice |

Counts (p. 65): Sorcery Points 5–10 (equal to level). Cantrips 5 (5–9), 6 at
10. Prepared 9, 10, 11, 12, 14, 15. Metamagic options (pp. 66–67): Careful,
Distant, Empowered, Extended, Heightened, Quickened, Seeking, Subtle, Transmuted,
Twinned. Subtle is no longer offered (CLASS-11). Note: Draconic grants Fly,
Arcane Eye and Legend Lore, which this inventory proposes to cut (Legend Lore
is cut by DM-2), so levels 5, 7 and 9 would each grant only one usable spell.

### Warlock (table p. 71) — Fiend Patron (p. 76)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Invocations 3 → 5 | Two new invocations; level 5+ prerequisites open (below). | 71–74 | Choice |
| 5 | Fiend Spells | Always prepared: Fireball, Stinking Cloud. | 76 | |
| 6 | Dark One's Own Luck (Fiend) | Add 1d10 to an ability check or save after seeing the roll; uses = Cha modifier (min 1) per Long Rest; once per roll. | 76 | Choice per use |
| 7 | Invocations 6 | One new invocation; level 7+ prerequisites open. | 71 | Choice |
| 7 | Fiend Spells | Always prepared: Fire Shield, Wall of Fire. | 76 | |
| 8 | Ability Score Improvement | ASI feat or another feat. | 72 | Choice |
| 9 | Contact Patron | Contact Other Plane always prepared; once per Long Rest free, auto-success on its save. | 72 | |
| 9 | Invocations 7 | One new invocation; level 9+ prerequisites open. | 71 | Choice |
| 9 | Fiend Spells | Always prepared: Geas, Insect Plague. | 76 | |
| 10 | Fiendish Resilience (Fiend) | On each Short or Long Rest choose a damage type other than Force; Resistance to it. | 76 | Choice |

Counts (p. 71): invocations 5, 5, 6, 6, 7, 7; cantrips 3 (5–9), 4 at 10;
prepared 6, 7, 8, 9, 10, 10. Contact Patron is cut by DM-2.

New invocation prerequisites in levels 5–10 (pp. 72–74):

| Invocation | Prereq | Effect | Note |
| --- | --- | --- | --- |
| Ascendant Step | 5 | Levitate on self at will | Levitate cut by DM-3, so this goes too |
| Eldritch Smite | 5, Pact of the Blade | Once per turn on a pact weapon hit, spend a Pact slot: +1d8 Force +1d8 per slot level, and Prone if Huge or smaller | KEEP candidate |
| Gaze of Two Minds | 5 | Perceive through a willing touched creature's senses | Remote senses; cut candidate |
| Gift of the Depths | 5 | Breathe underwater, Swim Speed, one free Water Breathing | Cut candidate |
| Investment of the Chain Master | 5, Pact of the Chain | Familiar upgrades | Pact of the Chain removed by DM-1 |
| Master of Myriad Forms | 5 | Alter Self at will | Alter Self cut by DM-3 |
| One with Shadows | 5 | Invisibility on self in Dim Light or Darkness | Needs light levels on battle maps (SCOPE-2 #46); UNSURE |
| Thirsting Blade | 5, Pact of the Blade | Extra Attack with the pact weapon | KEEP candidate |
| Whispers of the Grave | 7 | Speak with Dead at will | Speak with Dead cut by DM-2 |
| Gift of the Protectors | 9, Pact of the Tome | Names on a page; the first named creature dropped to 0 HP (not killed outright) goes to 1 HP instead, once per Long Rest | KEEP candidate (TOME-1: book not modeled) |
| Lifedrinker | 9, Pact of the Blade | Once per turn on a pact weapon hit, +1d6 Necrotic/Psychic/Radiant and may spend a Hit Die to heal | KEEP candidate |
| Visions of Distant Realms | 9 | Arcane Eye at will | Arcane Eye proposed CUT |

### Wizard (table p. 77) — Evoker (p. 82)

| Lvl | Feature | What it does | Page | Choice |
| ---: | --- | --- | ---: | --- |
| 5 | Memorize Spell | On a Short Rest, swap one prepared level 1+ spell for another from the spellbook. | 79 | Choice |
| 5, 7, 9 | Evocation Savant (Evoker, recurring) | On gaining a new slot level, add one Evocation spell of that level to the spellbook free. | 82 | Choice (SCOPE-2 cut) |
| 6 | Sculpt Spells (Evoker) | On an Evocation spell, choose 1 + spell level visible creatures: they auto-succeed and take no damage instead of half. | 82 | Targets |
| 8 | Ability Score Improvement | ASI feat or another feat. | 79 | Choice |
| 10 | Empowered Evocation (Evoker) | Add Int modifier to one damage roll of a Wizard Evocation spell. | 82 | |

Counts (p. 77): cantrips 4 (5–9), 5 at 10. Prepared 9, 10, 11, 12, 14, 15.
Spellbook gains two Wizard spells per level, of a level the Wizard has slots
for (p. 78). SCOPE-2 keeps Sculpt Spells and Potent Cantrip and cuts
Evocation Savant. Sculpt Spells works like ADAPT-1's Careful Spell
(protected allies chosen for the player).

---

## 3. Spells of levels 3–5

Lists: B Bard, C Cleric, D Druid, P Paladin, R Ranger, S Sorcerer, K Warlock,
W Wizard (the same letters SPELL-INVENTORY.md uses). "Feature routes" are
always-prepared grants at levels 5–10 with the character level, including
off-list grants (for example, Land Druids get Fireball and Lightning Bolt;
Devotion Paladins get Beacon of Hope). Bard Magical Secrets (10) and Lore
Magical Discoveries (6) open every Cleric, Druid and Wizard spell to Bards
and add no new names. "Conc" is Concentration. Summaries are paraphrased;
read the page before implementing.

Triage key: **KEEP** = resolvable in combat, camp or exploration as the game
stands or with the systems in the summary. **CUT** = needs a DM or a mechanic
the game lacks, using DM-1/DM-2/DM-3 categories. **UNSURE** = a real use
exists but depends on a design decision.

### Level 3 spells (earliest character level: 5)

| Spell | p. | Lists | Feature routes | Summary | Triage | Reason / notes |
| --- | ---: | --- | --- | --- | --- | --- |
| Animate Dead | 108 | C, W | — | 1-minute cast: a Small/Medium Humanoid corpse or bones rises as a Zombie or Skeleton you command with a Bonus Action for 24 h; recasting keeps up to 4 under control; +2 undead per slot above 3. | **UNSURE** | Needs corpses on the map, controlled undead that persist between fights (companions, #173) and a 1-minute cast. |
| Beacon of Hope | 112 | C | Devotion 9 | Conc 1 min: any number of creatures within 30 ft get Advantage on Wisdom saves and Death Saving Throws, and all healing they receive is maximized. | **KEEP** | — |
| Bestow Curse | 112 | B, C, W | — | Touch, Wis save; Conc 1 min: one curse of the caster's choice (Disadvantage on one ability's checks and saves; Disadvantage on attacks against the caster; Wis save each turn or Dodge; +1d8 Necrotic from the caster's hits). Higher slots lengthen it and drop Concentration. | **KEEP** | — |
| Blink | 113 | S, W | — | 1 min: at the end of each of the caster's turns roll 1d6; on 4-6 the caster is on the Ethereal Plane (cannot affect or be affected) until the start of its next turn, then returns within 10 ft. | **KEEP** | Ethereal can be modeled as 'off the board' for a round. |
| Call Lightning | 114 | D | — | Conc 10 min: storm cloud overhead; a bolt hits a point within 120 ft, Dex save for 3d10 Lightning within 5 ft (half on success); repeat each turn as a Magic action. +1d10 if cast outdoors in a storm; +1d10 per slot above 3. | **KEEP** | The cloud 'above yourself' indoors and the storm bonus need a ruling (ignore weather). |
| Clairvoyance | 115 | B, C, S, W | — | 10-minute cast: invisible sensor up to 1 mile away for seeing or hearing a location. | **CUT** | Named in DM-2; remote scouting. |
| Conjure Animals | 117 | D, R | — | Conc 10 min: an intangible Large pack of spirit animals that moves 30 ft when the caster moves; creatures within 10 ft of it make a Dex save once per turn or take 3d10 Slashing; caster has Advantage on Str saves near it. +1d10 per slot above 3. | **KEEP** | A movable damage zone, not a creature. |
| Counterspell | 120 | S, K, W | — | Reaction when a creature within 60 ft casts: it makes a Con save or the spell fails (its slot is not spent). | **KEEP** | Needs monster casters and a 'casting begins' reaction window (#41). |
| Create Food and Water | 120 | C, P | — | 10-minute cast: 45 lb of food and 30 gallons of water. | **CUT** | No food or water needs; DM-1 cut Create or Destroy Water and Purify Food and Drink. |
| Daylight | 122 | C, D, P, R, S | — | 1 h: 60-ft-radius sphere (or emanation from an object) of sunlight; dispels overlapping Darkness of level 3 or lower. | **CUT** | Light and darkness (DM-3 cut Light and Continual Flame). Flag: ending Darkness is its one combat effect. |
| Dispel Magic | 124 | B, C, D, P, R, S, K, W | Devotion 9 | Ends every spell of level 3 or lower on one creature, object or effect within 120 ft; higher spells need a spellcasting check (DC 10 + level); a higher slot auto-ends spells up to its level. | **KEEP** | Needs a registry of ongoing spells per target. |
| Fear | 130 | B, S, K, W | Draconic 5 | Conc 1 min, 30-ft cone, Wis save: Frightened, drops what it holds, Dashes away by the safest route; saves again only when out of line of sight. | **KEEP** | Flight AI exists (Dissonant Whispers); the dropped item conflicts with SIMPLIFY-1. |
| Fireball | 131 | S, W | Land (arid) 5, Fiend 5 | 20-ft-radius sphere within 150 ft: Dex save, 8d6 Fire (half on success); +1d6 per slot above 3. | **KEEP** | — |
| Fly | 133 | S, K, W | Draconic 5 | Conc 10 min, touch: Fly Speed 60 ft and hover; falls when it ends; +1 target per slot above 3. | **CUT** | Flight and falling are not modeled (DM-3 falling; SCOPE-2 no fly forms). Note Draconic Sorcery always prepares it at 5. |
| Gaseous Form | 134 | S, K, W | — | Conc 1 h: willing creature becomes mist: Fly 10 ft, Resistance to B/P/S, Advantage on Str/Dex/Con saves, cannot attack, cast or handle objects; slips through cracks. | **CUT** | Exploration movement and flight; little combat use. |
| Glyph of Warding | 136 | B, C, W | — | 1-hour cast (200 gp consumed): an inscribed trap that, when its trigger is met, explodes for 5d8 (20-ft radius, Dex half) or releases a stored spell of level 3 or lower. | **CUT** | Traps and set-up triggers (DM-3). |
| Haste | 139 | S, W | — | Conc 1 min: Speed doubled, +2 AC, Advantage on Dex saves, one extra action (one attack, Dash, Disengage, Hide or Utilize); when it ends the target is Incapacitated with Speed 0 for a turn. | **KEEP** | — |
| Hypnotic Pattern | 141 | B, S, K, W | — | Conc 1 min, 30-ft cube within 120 ft, Wis save: Charmed, Incapacitated and Speed 0; ends on damage or when someone shakes the creature awake (action). | **KEEP** | — |
| Lightning Bolt | 144 | S, W | Land (temperate) 5 | 100-ft by 5-ft Line: Dex save, 8d6 Lightning (half on success); +1d6 per slot above 3. | **KEEP** | — |
| Magic Circle | 145 | C, P, K, W | — | 1-minute cast (100 gp consumed), 1 h: 10-ft-radius cylinder against chosen Celestials, Elementals, Fey, Fiends or Undead: they cannot enter nonmagically, have Disadvantage attacking into it and cannot charm, frighten or possess those inside; can be reversed into a prison. | **UNSURE** | Combat-resolvable, but a 1-minute cast needs pre-fight casting or ten rounds. |
| Major Image | 147 | B, S, K, W | — | Conc 10 min: a convincing moving illusion up to a 20-ft cube. | **CUT** | Named in DM-2. |
| Mass Healing Word | 148 | B, C | Life 5 | Bonus Action: up to six creatures within 60 ft regain 2d4 + modifier HP; +1d4 per slot above 3. | **KEEP** | — |
| Meld into Stone | 148 | C, D, R | — | 8 h: caster merges into stone, undetectable; damage to the stone expels it with Force damage. | **CUT** | Exploration hiding; stone surfaces are not modeled. |
| Nondetection | 151 | B, R, W | — | 8 h: a creature, place or object cannot be targeted by Divination or scrying sensors. | **CUT** | Nothing in the game scries. |
| Phantom Steed | 152 | W | — | 1-minute cast, 1 h: a quasi-real riding horse (Speed 100 ft, 13 miles per hour); ends if it takes damage. | **CUT** | Mounts and overland travel speed. |
| Plant Growth | 153 | B, D, R | — | Overgrowth (action): plants in a 100-ft-radius area cost 4 ft of movement per 1 ft. Enrichment (8 h): crops in a half-mile radius yield double for a year. | **UNSURE** | Overgrowth is a combat terrain effect but needs plants marked on battle maps; Enrichment is cut. |
| Protection from Energy | 157 | C, D, R, S, W | — | Conc 1 h, touch: Resistance to one of Acid, Cold, Fire, Lightning or Thunder. | **KEEP** | — |
| Remove Curse | 158 | C, P, K, W | — | Touch: ends all curses on a creature or object; breaks Attunement to a cursed item. | **KEEP** | Needs curses to track (Bestow Curse, monster curses); cursed items are unsupported (SHOP-1). |
| Revivify | 159 | C, D, P, R | Life 5 | Touch (300 gp diamond consumed): a creature dead no longer than 1 minute returns with 1 HP. | **KEEP** | Resurrection is kept (SCOPE-2, #175). CLASS-11 removed components, so the diamond cost needs a decision. |
| Sending | 161 | B, C, W | — | A 25-word message to a known creature at any distance, with a reply. | **CUT** | Named in DM-2. |
| Sleet Storm | 163 | D, S, W | Land (polar) 5 | Conc 1 min, 20-ft-radius cylinder: Heavily Obscured, Difficult Terrain, flames doused; entering or starting a turn there: Dex save or Prone and lose Concentration. | **KEEP** | — |
| Slow | 163 | B, S, W | — | Conc 1 min, up to six creatures in a 40-ft cube, Wis save: half Speed, -2 AC and Dex saves, no Reactions, action or Bonus Action (not both), one attack, 25% failure on Somatic spells; resave each turn. | **KEEP** | CLASS-11 removed Somatic components, so the 25% clause needs a ruling. |
| Speak with Dead | 164 | B, C, W | — | A corpse answers up to five questions. | **CUT** | Named in DM-2. |
| Speak with Plants | 164 | B, D, R | — | 10 min: question plants about the last day; toggle plant Difficult Terrain in a 30-ft emanation. | **CUT** | Talking to plants (like Speak with Animals, DM-1); the terrain toggle has little use. |
| Spirit Guardians | 164 | C | — | Conc 10 min, 15-ft emanation moving with the caster: other creatures' Speed halved; Wis save once per turn on entering/ending a turn there: 3d8 Radiant (Necrotic if evil), half on success; +1d8 per slot above 3. | **KEEP** | Needs a moving aura with triggers. |
| Stinking Cloud | 165 | B, S, W | Land (tropical) 5, Fiend 5 | Conc 1 min, 20-ft-radius sphere within 90 ft, Heavily Obscured; starting a turn inside: Con save or Poisoned and no action or Bonus Action that turn. | **KEEP** | — |
| Tiny Hut | 169 | B, W | — | 1-minute cast, 8 h: 10-ft dome nobody else can enter; spells of level 3 or lower cannot pass; comfortable inside. | **UNSURE** | Could make a camp rest uninterruptible, a design call; ritual casting is gone (DM-1), so it costs a slot. |
| Tongues | 169 | B, C, S, K, W | — | 1 h: target understands and is understood in any language. | **CUT** | Named in DM-2. |
| Vampiric Touch | 171 | S, K, W | — | Conc 1 min: melee spell attack for 3d6 Necrotic, caster heals half; repeat as a Magic action each turn; +1d6 per slot above 3. | **KEEP** | — |
| Water Breathing | 173 | D, R, S, W | Gift of the Depths invocation | 24 h: up to ten creatures breathe underwater. | **CUT** | No underwater play. |
| Water Walk | 173 | C, D, R, S | — | 1 h: up to ten creatures walk on liquids. | **CUT** | No liquid surfaces. |
| Wind Wall | 174 | D, R | — | Conc 1 min: wall up to 50 ft long, 15 ft high; Str save for 4d8 Bludgeoning when it appears; ordinary arrows and bolts through it miss; blocks gases and small fliers. | **KEEP** | Needs wall geometry and line-crossing tests for missiles. |

### Level 4 spells (earliest character level: 7)

| Spell | p. | Lists | Feature routes | Summary | Triage | Reason / notes |
| --- | ---: | --- | --- | --- | --- | --- |
| Arcane Eye | 110 | W | Draconic 7; Visions of Distant Realms invocation (Warlock 9) | Conc 1 h: invisible scouting eye with 30-ft Darkvision, moved 30 ft per Bonus Action. | **CUT** | Scouting; the map has no hidden areas to reveal. |
| Aura of Life | 111 | C, P | Life 7 | Conc 10 min, 30-ft emanation: allies resist Necrotic and their HP maximum cannot be reduced; an ally at 0 HP starting its turn there regains 1 HP. | **KEEP** | — |
| Banishment | 112 | C, P, S, K, W | — | Conc 1 min, Cha save: target goes to a harmless demiplane (Incapacitated, gone); an extraplanar target does not come back if the spell lasts 1 minute; +1 target per slot above 4. | **KEEP** | Decide XP and loot for a creature that never returns. |
| Black Tentacles | 113 | W | — | Conc 1 min, 20-ft square: Difficult Terrain; Str save on cast, entering or ending a turn there: 3d6 Bludgeoning and Restrained; escape with Athletics vs DC. | **KEEP** | — |
| Blight | 113 | D, S, K, W | Land (arid) 7 | Con save, 8d8 Necrotic (half on success); Plant creatures fail; can wither a plant. | **KEEP** | — |
| Charm Monster | 114 | B, D, S, K, W | Draconic 7 | 1 h: any creature, Wis save (Advantage if fighting) or Charmed and Friendly until harmed; +1 target per slot above 4. | **KEEP** | Same handling as Charm Person, which the game has. |
| Compulsion | 117 | B | — | Conc 1 min: chosen creatures within 30 ft, Wis save or Charmed; a Bonus Action sets a direction they must move by the safest route, then they resave. | **KEEP** | — |
| Confusion | 117 | B, D, S, W | — | Conc 1 min, 10-ft-radius sphere, Wis save: no Bonus Actions or Reactions; d10 each turn: wander, do nothing, attack a random creature in reach, or act normally; resave each turn. | **KEEP** | — |
| Conjure Minor Elementals | 118 | D, W | — | Conc 10 min, 15-ft emanation: the caster's attacks against creatures in it deal +2d8 Acid/Cold/Fire/Lightning; Difficult Terrain for enemies; +1d8 per slot above 4. | **KEEP** | — |
| Conjure Woodland Beings | 118 | D, R | — | Conc 10 min, 10-ft emanation: Wis save once per turn for 5d8 Force (half on success); Disengage as a Bonus Action; +1d8 per slot above 4. | **KEEP** | — |
| Control Water | 119 | C, D, W | — | Conc 10 min: flood, part, redirect or whirlpool water in a 100-ft cube. | **CUT** | Bodies of water and vehicles are not modeled. |
| Death Ward | 122 | C, P | Life 7 | 8 h, touch: the first drop to 0 HP becomes 1 HP instead; also negates one instant-death effect. | **KEEP** | Needs buffs that last across fights. |
| Dimension Door | 123 | B, S, K, W | — | Teleport up to 500 ft, optionally with one willing adjacent creature; 4d6 Force and failure if the space is occupied. | **KEEP** | Combat use like Misty Step; teleporting through dungeon walls on the exploration map is a separate call. |
| Divination | 125 | C, D, W | — | Ask a deity one question about the next 7 days. | **CUT** | Named in DM-2. |
| Dominate Beast | 125 | D, R, S | — | Conc 1 min: a Beast, Wis save (Advantage if fighting) or Charmed; the caster commands it each turn; resave on damage. | **KEEP** | Needs player control of an enemy creature. |
| Fabricate | 129 | W | — | 10-minute cast: turns raw materials into finished objects. | **CUT** | Crafting needs a DM. |
| Faithful Hound | 129 | W | — | 8 h: an invisible, invulnerable watchdog; barks at intruders; at the start of each of the caster's turns bites one enemy within 5 ft (Dex save, 4d8 Force); a Magic action moves it 30 ft. | **UNSURE** | The biting half fits combat; the guard-dog half needs camp-interruption rules. |
| Fire Shield | 132 | D, S, W | Fiend 7 | 10 min: Resistance to Cold (warm) or Fire (chill); creatures within 5 ft that hit the caster in melee take 2d8 Fire or Cold. | **KEEP** | — |
| Freedom of Movement | 134 | B, C, D, R | Land (temperate) 7 | 1 h, touch: ignores Difficult Terrain; magic cannot reduce its Speed or Paralyze or Restrain it; escapes nonmagical restraints and grapples for 5 ft of movement; Swim Speed. | **KEEP** | Grapple and swim parts are moot (SCOPE-2). |
| Giant Insect | 135 | D | — | Conc 10 min: summons a giant centipede, spider or wasp ally (stat block scales with slot) that acts after the caster on its commands. | **KEEP** | Summon (SCOPE-2 keeps spells that create creatures); the wasp's Fly Speed goes unused. |
| Greater Invisibility | 137 | B, S, W | — | Conc 1 min, touch: Invisible for the duration (attacking does not end it). | **KEEP** | — |
| Guardian of Faith | 137 | C | — | 8 h: an invulnerable Large guardian; enemies that move within 10 ft of it or start a turn there make a Dex save for 20 Radiant (half); it vanishes after dealing 60 damage. | **KEEP** | — |
| Hallucinatory Terrain | 139 | B, D, K, W | — | 10-minute cast: natural terrain in a 150-ft cube looks like other terrain. | **CUT** | Named in DM-2. |
| Ice Storm | 141 | D, S, W | Land (polar) 7 | 20-ft-radius cylinder within 300 ft: Dex save, 2d10 Bludgeoning + 4d6 Cold (half); Difficult Terrain until the end of the caster's next turn. | **KEEP** | — |
| Locate Creature | 145 | B, C, D, P, R, W | — | Conc 1 h: sense the direction of a known creature or kind within 1,000 ft. | **CUT** | Exploration tracking; nothing to act on. |
| Phantasmal Killer | 152 | B, W | — | Conc 1 min: Wis save, 4d10 Psychic and Disadvantage on checks and attacks; repeats the damage on each failed end-of-turn save. | **KEEP** | — |
| Polymorph | 153 | B, D, S, W | Land (tropical) 7 | Conc 1 h: Wis save or a creature becomes a Beast of CR no higher than its own, with Temporary HP equal to the Beast's HP; cannot speak or cast. | **KEEP** | Reuse the Wild Shape form set (CLASS-10) for forms with combat art. |
| Private Sanctum | 156 | W | — | 10-minute cast, 24 h: an area blocks sound, sight, divination, teleportation and planar travel. | **CUT** | Nothing to act on. |
| Resilient Sphere | 158 | W | — | Conc 1 min: Dex save or a Large or smaller creature is sealed in an invulnerable sphere nothing passes through; it can roll the sphere at half Speed. | **KEEP** | — |
| Secret Chest | 160 | W | — | Hides a 5,000 gp chest on the Ethereal Plane, recallable at will. | **CUT** | Inventory storage; nothing to act on. |
| Stone Shape | 165 | C, D, W | — | Reshape stone up to 5 ft (open a passage, seal a door). | **CUT** | Map editing in exploration. |
| Stoneskin | 165 | D, R, S, W | — | Conc 1 h, touch (100 gp consumed): Resistance to Bludgeoning, Piercing and Slashing. | **KEEP** | — |
| Vitriolic Sphere | 172 | S, W | — | 20-ft-radius sphere within 150 ft: Dex save, 10d4 Acid now and 5d4 at the end of the next turn on a failure, half the initial damage on success; +2d4 per slot above 4. | **KEEP** | — |
| Wall of Fire | 172 | D, S, W | Fiend 7 | Conc 1 min: 60-ft opaque wall or 20-ft ring; Dex save 5d8 Fire when it appears; the chosen side burns creatures that end a turn within 10 ft or enter it (5d8). | **KEEP** | Needs wall geometry with a damaging side. |

### Level 5 spells (earliest character level: 9)

| Spell | p. | Lists | Feature routes | Summary | Triage | Reason / notes |
| --- | ---: | --- | --- | --- | --- | --- |
| Animate Objects | 108 | B, S, W | — | Conc 1 min: animate nonmagical loose objects (count = spellcasting modifier) into Construct allies (AC 15, 10/20/40 HP, Slam) commanded by Bonus Action. | **UNSURE** | Battle maps have no loose objects; otherwise a summon. |
| Antilife Shell | 109 | D | — | Conc 1 h, 10-ft emanation: creatures other than Constructs and Undead cannot pass or reach through it; ends if the caster pushes it onto them. | **KEEP** | Very strong with adjacent-only melee (MELEE-1). |
| Arcane Hand | 110 | S, W | — | Conc 1 min: a Large force hand (AC 20, HP = caster's maximum); Bonus Action to move 60 ft and punch (5d8 Force), shove, grapple-and-crush (4d6 + mod) or give Half Cover. | **KEEP** | Fist and shove fit; grappling is dropped (SCOPE-2) and Half Cover does not exist (COVER-1). |
| Awaken | 111 | B, D | — | 8-hour cast (1,000 gp agate): a Beast or plant gains Intelligence 10 and speech. | **CUT** | Creates a narrative NPC. |
| Cloudkill | 116 | S, W | — | Conc 10 min, 20-ft-radius poison fog, Heavily Obscured, drifts 10 ft away from the caster each turn: Con save once per turn, 5d8 Poison (half); +1d8 per slot above 5. | **KEEP** | — |
| Commune | 116 | C | — | Three yes/no questions to a deity. | **CUT** | Named in DM-2. |
| Commune with Nature | 116 | D, R | — | Learn three facts about the land within 3 miles. | **CUT** | Exploration knowledge from the GM. |
| Cone of Cold | 117 | D, S, W | Land (polar) 9 | 60-ft cone: Con save, 8d8 Cold (half); +1d8 per slot above 5. | **KEEP** | — |
| Conjure Elemental | 118 | D, W | — | Conc 10 min: an intangible Large elemental spirit; a creature entering its space or starting a turn within 5 ft makes a Dex save or takes 8d8 and is Restrained, then 4d8 each turn until it saves. | **KEEP** | — |
| Contact Other Plane | 119 | K, W | Contact Patron (Warlock 9) | DC 15 Int save, then five one-word answers from an otherworldly entity; failure: 6d6 Psychic and Incapacitated until a Long Rest. | **CUT** | Named in DM-2 (Contact Patron goes with it). |
| Contagion | 119 | C, D | — | Touch, Con save: 11d8 Necrotic and Poisoned with Disadvantage on one ability's saves; three successes end it, three failures make it last 7 days. | **KEEP** | Effect outlasts the fight. |
| Creation | 121 | S, W | — | 1-minute cast: a temporary shadow-stuff object up to a 5-ft cube. | **CUT** | Object creation needs a DM. |
| Dispel Evil and Good | 124 | C, P | — | Conc 1 min: Celestials, Elementals, Fey, Fiends and Undead have Disadvantage attacking the caster; end it to free a creature from their charm, fear or possession, or to send one home (Cha save). | **KEEP** | — |
| Dominate Person | 126 | B, S, W | — | Conc 1 min: a Humanoid, Wis save (Advantage if fighting) or Charmed and commanded by the caster; resave on damage. | **KEEP** | Needs player control of an enemy creature. |
| Dream | 126 | B, K, W | — | 8 h: message a sleeping creature in a dream; can ruin its rest and deal 3d6 Psychic. | **CUT** | Named in DM-2. |
| Flame Strike | 132 | C | — | 10-ft-radius, 40-ft-high cylinder: Dex save, 5d6 Fire + 5d6 Radiant (half); +1d6 each per slot above 5. | **KEEP** | — |
| Geas | 135 | B, C, D, P, W | Fiend 9 | 30 days: Charmed into obeying a command; 5d10 Psychic for defiance. | **CUT** | Named in DM-2. |
| Greater Restoration | 137 | B, C, D, P, R | Life 9 | Touch (100 gp consumed): removes one of 1 Exhaustion level, Charmed or Petrified, a curse, an ability score reduction or an HP maximum reduction. | **KEEP** | Exhaustion and Petrified are not modeled yet. |
| Hallow | 138 | C | — | 24-hour cast (1,000 gp): a permanent 60-ft ward against chosen creature types plus one extra effect. | **CUT** | Location ward set up over a day; campaign-site magic. |
| Hold Monster | 140 | B, S, K, W | — | Conc 1 min: any creature, Wis save or Paralyzed, resave each turn; +1 target per slot above 5. | **KEEP** | — |
| Insect Plague | 143 | C, D, S | Land (tropical) 9, Fiend 9 | Conc 10 min, 20-ft-radius swarm within 300 ft: Lightly Obscured, Difficult Terrain; Con save on appearing/entering/ending a turn, 4d10 Piercing (half); +1d10 per slot above 5. | **KEEP** | — |
| Legend Lore | 144 | B, C, W | Draconic 9 | Learn lore about a famous person, place or object. | **CUT** | Named in DM-2. |
| Mass Cure Wounds | 147 | B, C, D | Life 9 | Up to six creatures in a 30-ft-radius sphere regain 5d8 + modifier HP; +1d8 per slot above 5. | **KEEP** | — |
| Mislead | 150 | B, K, W | — | Conc 1 h: the caster becomes Invisible (until it attacks, deals damage or casts) and gets a movable illusory double it can see and hear through. | **UNSURE** | The invisibility works; the decoy needs monsters that can be fooled (HIDE-1: monsters never guess). |
| Modify Memory | 150 | B, W | — | Conc 1 min: rewrite a creature's memory of a recent event. | **CUT** | Named in DM-2. |
| Passwall | 151 | W | — | 1 h: a 5 by 8 by 20 ft passage through wood, plaster or stone. | **CUT** | Exploration map editing. |
| Planar Binding | 152 | B, C, D, K, W | — | 1-hour cast (1,000 gp): binds a Celestial, Elemental, Fey or Fiend to service for 24 h. | **CUT** | Open-ended service of a summoned outsider. |
| Raise Dead | 157 | B, C, P | — | 1-hour cast (500 gp diamond): a creature dead no longer than 10 days returns with 1 HP and a -4 penalty to D20 Tests that drops by 1 per Long Rest. | **KEEP** | Resurrection is kept (#175); a camp/temple spell. Component cost: see CLASS-11 note. |
| Reincarnate | 158 | D | — | 1-hour cast (1,000 gp): a dead Humanoid (no longer than 10 days) returns in a new body of a random species (d10 table). | **UNSURE** | Resurrection is kept, but a mid-campaign species change (traits, portrait) is a design call. |
| Scrying | 160 | B, C, D, K, W | — | 10-minute cast: watch a creature or place through a sensor. | **CUT** | Named in DM-2. |
| Seeming | 160 | B, S, W | — | 8 h: change the appearance of chosen creatures. | **CUT** | Named in DM-2. |
| Summon Dragon | 166 | W | Draconic 9 | Conc 1 h (500 gp focus): a Draconic Spirit ally (AC 14 + level, 50+ HP, Rend attacks, 30-ft cone breath) that acts after the caster; the caster shares one of its resistances. | **KEEP** | Summon; its Fly Speed goes unused. |
| Telekinesis | 167 | S, W | — | Conc 10 min, Magic action each turn: Str save or move a Huge or smaller creature 30 ft and Restrain it until the next turn (lifted creatures fall); or move and manipulate objects. | **UNSURE** | A horizontal shove plus Restrained fits; lifting, falling and object use do not. |
| Telepathic Bond | 168 | B, W | — | 1 h: up to eight creatures share telepathic speech. | **CUT** | Communication; nothing to act on. |
| Teleportation Circle | 168 | B, S, K, W | — | 1-minute cast: a portal to a permanent circle whose sigils the caster knows. | **CUT** | Needs permanent circles in the campaign; travel the DM controls. |
| Tree Stride | 170 | D, R | Land (temperate) 9 | Conc 1 min: step into a tree and out of another of the same kind within 500 ft. | **CUT** | Needs trees on maps; exploration movement. |
| Wall of Force | 172 | W | — | Conc 10 min: an invisible, indestructible wall, dome or globe (ten 10-ft panels) nothing passes through; immune to Dispel Magic. | **KEEP** | Needs wall geometry; can split an encounter. |
| Wall of Stone | 173 | D, S, W | Land (arid) 9 | Conc 10 min: ten 10-ft stone panels (AC 15, 30 HP per inch); permanent after full Concentration; can bridge chasms. | **KEEP** | The combat wall fits; permanence and bridging are exploration. |

### Always-prepared and free spells from features, levels 5–10

| Source | Lvl | Spells (spell level) | Page | Proposed status |
| --- | ---: | --- | ---: | --- |
| Life Domain | 5 | Mass Healing Word (3), Revivify (3) | 40 | both KEEP |
| Life Domain | 7 | Aura of Life (4), Death Ward (4) | 40 | both KEEP |
| Life Domain | 9 | Greater Restoration (5), Mass Cure Wounds (5) | 40 | both KEEP |
| Land: arid | 5 / 7 / 9 | Fireball (3) / Blight (4) / Wall of Stone (5) | 46 | KEEP / KEEP / KEEP |
| Land: polar | 5 / 7 / 9 | Sleet Storm (3) / Ice Storm (4) / Cone of Cold (5) | 46 | KEEP / KEEP / KEEP |
| Land: temperate | 5 / 7 / 9 | Lightning Bolt (3) / Freedom of Movement (4) / Tree Stride (5) | 46 | KEEP / KEEP / CUT |
| Land: tropical | 5 / 7 / 9 | Stinking Cloud (3) / Polymorph (4) / Insect Plague (5) | 46 | KEEP / KEEP / KEEP |
| Oath of Devotion | 5 | Aid (2), Zone of Truth (2) | 56 | Aid kept; Zone of Truth cut (DM-1) |
| Oath of Devotion | 9 | Beacon of Hope (3), Dispel Magic (3) | 56 | both KEEP |
| Paladin Faithful Steed | 5 | Find Steed (2), one free cast per Long Rest | 55 | Needs mounts and summons (see summary) |
| Draconic Sorcery | 5 | Fear (3), Fly (3) | 70 | KEEP / CUT |
| Draconic Sorcery | 7 | Arcane Eye (4), Charm Monster (4) | 70 | CUT / KEEP |
| Draconic Sorcery | 9 | Legend Lore (5), Summon Dragon (5) | 70 | CUT (DM-2) / KEEP |
| Fiend Patron | 5 | Fireball (3), Stinking Cloud (3) | 76 | both KEEP |
| Fiend Patron | 7 | Fire Shield (4), Wall of Fire (4) | 76 | both KEEP |
| Fiend Patron | 9 | Geas (5), Insect Plague (5) | 76 | CUT (DM-2) / KEEP |
| Warlock Contact Patron | 9 | Contact Other Plane (5), free once per Long Rest | 72 | CUT (DM-2 names it) |
| Lore Magical Discoveries | 6 | Two chosen Cleric/Druid/Wizard spells (cantrip to level 3 at Bard 6) | 35 | Choice among kept spells |
| Bard Magical Secrets | 10 | New prepared spells may come from Cleric/Druid/Wizard lists | 33 | Choice among kept spells |
| Cleric Divine Intervention | 10 | Any Cleric spell of level 5 or lower, free, once per Long Rest | 38 | Offer kept spells only |
| Invocations (Warlock 5–9) | 5–9 | Levitate, Alter Self, Invisibility, Water Breathing, Speak with Dead, Arcane Eye (at will or free) | 72–74 | See the invocation table in section 2 |

### Cross-checks done

- Union of the eight printed class lists at levels 3–5: 114 names (42/34/38).
  Count of level 3–5 spell-description headers on pp. 107–175: 114. Every
  description's class tag matches the printed tables (Blight, Animate Dead and
  ten others were checked by hand from a column-cropped extraction because
  the reading-order extraction mixed their columns).
- The Paladin level 3 list (p. 56) is printed with six spells (Create Food and
  Water, Daylight, Dispel Magic, Magic Circle, Remove Curse, Revivify). It is
  confirmed complete in the layout text, not a column split. The Ranger level
  3 list (p. 60) has twelve.
- Text-extraction caveat: on p. 120 the reading-order text mixes Create Food
  and Water with Control Weather. The summary above uses only the Create Food
  and Water sentences. The Reincarnate table (p. 158) did not extract in
  reading order, so it was read from the layout text: 1 roll again,
  2 Dragonborn, 3 Dwarf, 4 Elf, 5 Gnome, 6 Goliath, 7 Halfling, 8 Human,
  9 Orc, 10 Tiefling.

## Attribution

This work includes material from the System Reference Document 5.2.1 ("SRD
5.2.1") by Wizards of the Coast LLC, available at
https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative
Commons Attribution 4.0 International License, available at
https://creativecommons.org/licenses/by/4.0/legalcode.
