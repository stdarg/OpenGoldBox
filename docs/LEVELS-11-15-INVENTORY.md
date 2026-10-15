# Levels 11–15: SRD 5.2.1 inventory for OpenGold

Planning inventory for raising the cap from level 10 to level 15 for the twelve
SRD classes and their twelve SRD subclasses: Berserker, Lore, Life, Land,
Champion, Open Hand, Devotion, Hunter, Thief, Draconic, Fiend and Evoker (the
same set as the [levels 5–10 inventory](LEVELS-5-10-INVENTORY.md)). Level 15
is SCOPE-1's rules-library target. This file is a factual extract plus a
proposed triage. Nothing here is a decision: every KEEP, CUT and UNSURE, and
every feature note, is a proposal; decisions are recorded in
[SRD-DECISIONS](SRD-DECISIONS.md).

**Sources.** [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), extracted with `pdftotext` in reading order and
with `-layout`. Page numbers are the printed footer numbers, which match the
PDF page numbers (checked as in the 5–10 inventory). Class tables were read
from the layout text; features and spell entries from the reading-order text,
with headers the reading order mixed re-read from the layout text (see
"Cross-checks done"). Repo context: `docs/SRD-DECISIONS.md` (SCOPE-1, SCOPE-2,
DM-1, DM-2, DM-3, LEVELS-1, CAP-1, CLASS-2, CLASS-5, CLASS-8, CLASS-11,
COVER-1, DEAF-1, WILD-1, BARD-1, HEROIC-1, ADAPT-1, PUSH-1, FLEE-1) and the
levels 5–10 inventory, at `902b3cf2`.

**Feature note key.** **Existing** = works with a system the game has, or one
the approved levels 5–10 plan builds (LEVELS-1). **New system** = needs
something neither has. **Cut candidate** = nothing to act on under a named
earlier decision.

**Already decided upstream (no new approval needed):**

- DM-2 cuts four of these spells by name: Find the Path (level 6), Programmed
  Illusion (6), Magnificent Mansion (7) and Mirage Arcane (7). That is 4 of
  the 21 proposed cuts.
- DM-3 removed Suggestion; Mass Suggestion goes with it.
- LEVELS-1 rulings carry over: summoned and controlled creatures act right
  after their caster under player control; long buffs without Concentration
  last into later fights; a banished extraplanar creature counts as defeated;
  fallen enemies' corpses exist only during the fight; 1-minute casts can be
  made in a fight over 10 rounds (Magic Circle); Revivify and Raise Dead cost
  an offering of 300 gp × level; Indomitable is used without asking.
- CAP-1 (2026-10-14) limits the campaign to the original Training Hall:
  Fighter, Barbarian, Paladin and Ranger stop at 8; Rogue, Bard and Monk at 9;
  Cleric, Druid, Wizard, Sorcerer and Warlock at 6. LEVELS-2 (2026-10-15)
  raises the cap to 15 for every class and replaces CAP-1's limits with one
  for every class (open decision 1, answered).

---

## Summary

### Spell counts (levels 6–8)

SRD 5.2.1 has **68** spells of levels 6–8 (31 + 20 + 17), and every one is on
at least one of the six full-caster or Warlock lists. Paladin and Ranger get
none: their spell levels stop at 4 by level 15. The full casters reach level 6
slots at 11, level 7 at 13 and level 8 at 15, one slot each. The Warlock never
gets Pact slots above level 5; it casts levels 6, 7 and 8 only through Mystic
Arcanum (one chosen spell per level, once per Long Rest), at the same class
levels. The class-list union matches the spell-description headers exactly.

| Spell level | KEEP | CUT | UNSURE | Total |
| --- | ---: | ---: | ---: | ---: |
| 6 | 18 | 10 | 3 | 31 |
| 7 | 10 | 7 | 3 | 20 |
| 8 | 11 | 4 | 2 | 17 |
| **All** | **39** | **21** | **8** | **68** |

UNSURE (8): Contingency, Magic Jar, Word of Recall (level 6); Reverse Gravity,
Simulacrum, Teleport (level 7); Clone, Tsunami (level 8).

Half casters at 11–15: Paladin and Ranger gain level 4 spells at 13, all
already triaged in the 5–10 inventory (section 3).

### Feature counts

Section 2 lists **52** feature rows across levels 11–15: 30 named class and
subclass features (Mystic Arcanum takes three rows), 13 Ability Score
Improvements (every class at 12, Fighter also at 14), four scaling steps
(Bardic die d12, Divine Spark 3d8, Land's Aid 4d6, Indomitable two uses), one
always-prepared spell grant (Devotion 13) and two invocation-count rows.

| Proposed note | Rows |
| --- | ---: |
| Existing | 49 |
| New system | 1 (Use Magic Device) |
| Cut candidate | 2 (Smite of Protection, Dragon Wings) |

One Existing row, Nature's Sanctuary, loses its Half Cover half to COVER-1.

### Biggest new systems needed

1. **Progression plumbing.** XP to 165,000; Proficiency Bonus +5 at 13,
   reaching every derived number; spell slots of levels 6–8 and upcasting into
   them; Mystic Arcanum as a per-spell free cast (free casts are kept by
   SCOPE-2, #200); an ASI or feat at 12 (Fighter also 14); the third cantrip
   step at character level 11 (Fire Bolt 3d10 p. 132, Eldritch Blast three
   beams p. 127, Sacred Flame 3d8, Dragonborn Breath Weapon 3d10 p. 84); three
   attacks per Attack action (Fighter 11, Devouring Blade invocation).
2. **Taking creatures off the board.** Maze, Forcecage (box), Hurl Through
   Hell, Divine Word's sending home, Prismatic Spray's violet ray. All can
   reuse the Banishment state the 5–10 work builds (gone, Incapacitated,
   returns to its space or the nearest free one).
3. **Antimagic and spell filters.** Antimagic Field (a moving zone that
   suppresses spells, Magic actions and ongoing effects), Globe of
   Invulnerability (blocks spells of level 5 or lower cast from outside). Both
   need 5–10's per-target registry of ongoing spells (Dispel Magic) and matter
   only against monster casters.
4. **Hit-Point thresholds and lasting harm.** Divine Word and Power Word Stun
   read the target's current HP; Petrified (Flesh to Stone, Prismatic Spray's
   indigo ray); Harm's HP-maximum reduction; Befuddlement's spell lock for 30
   days (ended by Heal or Greater Restoration); Disintegrate's dust, which
   only level 9 magic can revive (open decision 4).
5. **Larger and stranger areas.** Fire Storm (up to ten contiguous 10-ft
   cubes the caster arranges), Wall of Ice (panels with AC and HP that leave
   freezing air when broken), Blade Barrier (line or ring), Wall of Thorns
   (4 ft of movement per foot), Incendiary Cloud (drifts 10 ft a turn),
   Conjure Celestial (a pillar that moves with the caster and heals or burns
   each creature), Sunbeam (a line repeated each turn), Earthquake (100-ft
   radius, Prone and lost Concentration each turn), Holy Aura. These extend
   CLASS-5 aiming and 5–10's wall and moving-zone work.
6. **More controlled creatures.** Create Undead (Ghouls; Ghasts or Wights from
   a level 8 slot), Finger of Death's Zombie, Dominate Monster, Simulacrum
   (UNSURE) and Animal Shapes (allies turned into Beasts up to CR 4, which
   needs more beast forms with combat art). The first three use LEVELS-1's
   shared control system.
7. **Class-feature mechanics.** Two Cunning Strike effects on one Sneak
   Attack, plus Daze, Knock Out and Obscure; Staggering and Sundering Blow;
   Advantage after a miss (Studied Attacks); critical hits on 18–20; Radiant
   Strikes; Overchannel; proficiency in all saves (Disciplined Survivor) or
   Wisdom and Charisma saves (Slippery Mind); condition removal through Lay On
   Hands (Restoring Touch); a second Hunter's Mark target each turn (Superior
   Hunter's Prey); a reaction for Resistance (Superior Hunter's Defense).
8. **Magic items.** Use Magic Device (Thief 13: attunement, charges, Spell
   Scrolls) and Antimagic Field's item suppression. The game has no usable
   magic items yet, although the original's treasure generates scrolls and
   wands (`random_treasure.cpp`); Fast Hands and Detect Magic wait for them
   too.
9. **Resurrection, continued** (#175): Resurrection (dead up to a century,
   full HP, −4 to D20 Tests), Regenerate and Heal.

### Features that cannot work without systems the game lacks

- **Flight.** Sorcerer Dragon Wings (Draconic 14: Fly Speed 60 ft); Wind Walk;
  Find Steed's Fly Speed from a level 4 slot (Paladin 13; the steed is already
  blocked by mounts).
- **Partial cover** (COVER-1). Devotion's Smite of Protection (15) only grants
  Half Cover; Nature's Sanctuary (Land 14) loses its Half Cover half; Blade
  Barrier's Three-Quarters Cover has nothing to act on.
- **Other planes and long-range travel.** Plane Shift, Etherealness, Transport
  via Plants, Demiplane; Teleport and Word of Recall are UNSURE.
- **Weather and falling.** Control Weather; Reverse Gravity (UNSURE) depends on
  falling, which DM-3 left unmodeled.
- **DM-adjudicated.** DM-2 names Find the Path, Programmed Illusion,
  Magnificent Mansion and Mirage Arcane. By the same test: Planar Ally
  (bargaining for service), Glibness (social checks), Mass Suggestion (DM-3's
  Suggestion), Antipathy/Sympathy, Guards and Wards, Forbiddance and Symbol
  (wards and traps set up in advance).
- **Ranges beyond the battlefield.** Improved Elemental Fury's Potent
  Spellcasting adds 300 ft to Druid cantrip range, which likely exceeds every
  battle map (check map sizes).

The numbered open decisions are in section 4.

---

## 1. Shared progression for levels 11–15

### Experience and Proficiency Bonus (Character Advancement table, p. 23)

| Level | XP | Proficiency Bonus | Change |
| ---: | ---: | ---: | --- |
| 10 | 64,000 | +4 | (5–10 cap) |
| 11 | 85,000 | +4 | |
| 12 | 100,000 | +4 | |
| 13 | 120,000 | +5 | PB rises |
| 14 | 140,000 | +5 | |
| 15 | 165,000 | +5 | |

Fixed Hit Points per level are unchanged (p. 23): Barbarian 7,
Fighter/Paladin/Ranger 6, Bard/Cleric/Druid/Monk/Rogue/Warlock 5,
Sorcerer/Wizard 4, each plus the Constitution modifier.

### Full casters: Bard, Cleric, Druid, Sorcerer, Wizard

Identical slot columns on all five tables (Bard p. 31, Cleric p. 36, Druid
p. 41, Sorcerer p. 65, Wizard p. 77). No level 9 slot before 17.

| Level | 1st | 2nd | 3rd | 4th | 5th | 6th | 7th | 8th | New spell level |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 10 | 4 | 3 | 3 | 3 | 2 | — | — | — | |
| 11 | 4 | 3 | 3 | 3 | 2 | 1 | — | — | **6th** |
| 12 | 4 | 3 | 3 | 3 | 2 | 1 | — | — | |
| 13 | 4 | 3 | 3 | 3 | 2 | 1 | 1 | — | **7th** |
| 14 | 4 | 3 | 3 | 3 | 2 | 1 | 1 | — | |
| 15 | 4 | 3 | 3 | 3 | 2 | 1 | 1 | 1 | **8th** |

Slot recovery limits (unchanged rules, new numbers):

- Sorcerer **Creating Spell Slots** (p. 66) still stops at level 5. Converting
  a slot into Sorcery Points works with any slot, so a level 6–8 slot yields
  6–8 points.
- Wizard **Arcane Recovery** (p. 78) and Land **Natural Recovery** (p. 46)
  recover combined levels up to half the class level, rounded up (6, 6, 7, 7,
  8 at levels 11–15), and never a slot of level 6 or higher.
- Bard **Font of Inspiration** (p. 32) can spend any slot, including 6–8.

### Half casters: Paladin (p. 53) and Ranger (p. 58)

| Level | 1st | 2nd | 3rd | 4th | 5th | New spell level |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 10 | 4 | 3 | 2 | — | — | |
| 11 | 4 | 3 | 3 | — | — | |
| 12 | 4 | 3 | 3 | — | — | |
| 13 | 4 | 3 | 3 | 1 | — | **4th** |
| 14 | 4 | 3 | 3 | 1 | — | |
| 15 | 4 | 3 | 3 | 2 | — | |

Level 5 spells come at 17, outside the cap. A level 4 slot also raises
Divine Smite to 5d8 Radiant (2d8 plus 1d8 per level above 1), and upcasting
Hunter's Mark at 3–4 only lengthens a duration that CLASS-2 already ends with
the fight.

### Warlock Pact Magic and Mystic Arcanum (pp. 71–72)

| Level | Pact slots | Slot level | Mystic Arcanum | Invocations |
| ---: | ---: | ---: | --- | ---: |
| 10 | 2 | 5 | — | 7 |
| 11 | 3 | 5 | **level 6 spell** | 7 |
| 12 | 3 | 5 | level 6 | 8 |
| 13 | 3 | 5 | level 6, **level 7 spell** | 8 |
| 14 | 3 | 5 | level 6, 7 | 8 |
| 15 | 3 | 5 | level 6, 7, **level 8 spell** | 9 |

Mystic Arcanum: choose one Warlock spell of the stated level; cast it once
without a slot, regained on a Long Rest; on each Warlock level gained, one
arcanum may be swapped for another of the same level. Magical Cunning
(level 2) restores half the Pact slots rounded up: now 2 of 3.

### Other level 11–15 effects shared across classes

- **Cantrip upgrades at character level 11:** the third damage step (for
  example Fire Bolt 3d10, Sacred Flame 3d8) and Eldritch Blast's third beam.
  The next step is at 17.
- **Ability Score Improvement at 12** for every class, plus Fighter 14
  (Fighter p. 48). Rogue's extra one was at 10. Epic Boons need level 19.
- **Species at character level 11:** Dragonborn Breath Weapon rises to 3d10
  (p. 84). No other species trait changes between 11 and 15.

---

## 2. Class and subclass features at levels 11–15

"Choice" marks a feature where the player picks. Tags such as COVER-1 point to
existing repo decisions that affect the feature.

### Barbarian (table p. 28) — Path of the Berserker (p. 30)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Relentless Rage | Dropping to 0 HP (not killed outright) while raging allows a DC 10 Con save; success sets HP to twice the Barbarian level. Each later use adds 5 to the DC until a Short or Long Rest. | 30 | | **Existing.** A 0-HP intercept like the Orc's Relentless Endurance; automatic, no hidden information. |
| 12 | Ability Score Improvement | ASI feat or another feat. | 29 | Choice | **Existing.** |
| 13 | Improved Brutal Strike | Two more Brutal Strike effects: Staggering Blow (Disadvantage on the target's next save, and no Opportunity Attacks until your next turn) and Sundering Blow (the next attack by someone else against the target gets +5, before your next turn). | 30 | Choice per use | **Existing.** Two more A-cycle options; Sundering needs a one-shot +5 marker on the target. |
| 14 | Intimidating Presence (Berserker) | Bonus Action: chosen creatures in a 30-ft emanation make a Wis save (DC 8 + Str + PB) or are Frightened for 1 minute, resaving each turn. Once per Long Rest, or restore it by spending a Rage use. | 30 | Targets | **Built 2026-10-15** with the game's first Frightened condition (the inventory first said it existed; it did not). |
| 15 | Persistent Rage | Once per Long Rest, regain all Rage uses when rolling Initiative. Rage lasts 10 minutes with no upkeep and ends early only on Unconscious or donning Heavy armor. | 30 | Choice (at Initiative) | **Existing.** Ten minutes outlasts any fight, so only the dropped upkeep matters. Propose automatic use when a Rage use is spent (open decision 7). |

Counts (p. 28): Rages 4 at 11, 5 at 12–15. Rage Damage +3 (rises at 16).
Weapon Mastery 4. Improved Brutal Strike's second step (two effects, 2d10) is
at 17.

### Bard (table p. 31) — College of Lore (p. 35)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 12 | Ability Score Improvement | ASI feat or another feat. | 32 | Choice | **Existing.** |
| 14 | Peerless Skill (Lore) | After failing an ability check or attack roll, spend a Bardic Inspiration use to add the die to the d20; the use is not spent if the roll still fails. | 35 | Choice per use | **Existing.** Fits BARD-1's automatic pattern: apply it to a failed roll the die could fix (open decision 7). |
| 15 | Bardic die d12 | Bardic Inspiration die becomes a d12. | 32 | | **Existing.** |

Counts (p. 31): no class feature at 11 or 13. Cantrips 4. Prepared spells 16,
16, 17, 17, 18 (levels 11–15). Magical Secrets (10) and Magical Discoveries
(6) now reach level 6–8 spells from the Cleric, Druid and Wizard lists.

### Cleric (table p. 36) — Life Domain (p. 40)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 12 | Ability Score Improvement | ASI feat or another feat. | 37 | Choice | **Existing.** |
| 13 | Divine Spark 3d8 | Channel Divinity's Divine Spark rolls 3d8. | 37 | | **Existing.** SPARK-1 (Radiant only) applies. |
| 14 | Improved Blessed Strikes | Divine Strike rises to 2d8; or Potent Spellcasting also grants Temporary HP of twice the Wis modifier to you or a creature within 60 ft when a Cleric cantrip damages someone. | 38 | Target per use | **Existing.** The Potent half needs a target choice for the Temporary HP (open decision 7). |

Counts (p. 36): Channel Divinity 3. Cantrips 5. Prepared 16, 16, 17, 17, 18.
The Life Domain has no feature and no domain spell at 11–15 (its table ends
at 9; Supreme Healing is at 17). Divine Intervention still casts only spells
of level 5 or lower.

### Druid (table p. 41) — Circle of the Land (p. 46)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 12 | Ability Score Improvement | ASI feat or another feat. | 43 | Choice | **Existing.** |
| 14 | Nature's Sanctuary (Land) | Magic action, one Wild Shape use: a 15-ft cube of spectral trees within 120 ft for 1 minute; you and allies inside have Half Cover, and allies share your current Nature's Ward Resistance. Bonus Action: move it 60 ft. | 46 | Placement | **Existing, half cut.** A movable zone (5–10 auras work); the Half Cover half has nothing to act on (COVER-1); the Resistance half works. |
| 14 | Land's Aid 4d6 | Land's Aid damage and healing rise to 4d6. | 46 | | **Existing.** |
| 15 | Improved Elemental Fury | Primal Strike rises to 2d8; or Potent Spellcasting adds 300 ft to the range of Druid cantrips with a range of 10 ft or more. | 43 | | **Existing.** The range half likely exceeds every battle map. |

Counts (p. 41): no feature at 11 or 13. Wild Shape uses 3; known forms (8)
and maximum CR (1) unchanged since 8. Cantrips 4. Prepared 16, 16, 17, 17,
18. No Circle spells past level 9.

### Fighter (table p. 47) — Champion (p. 49)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Two Extra Attacks | Three attacks per Attack action. | 48 | | **Existing.** Extra Attack with a count of 3. |
| 12 | Ability Score Improvement | ASI feat or another feat. | 48 | Choice | **Existing.** |
| 13 | Indomitable (two uses) | Indomitable usable twice per Long Rest. | 48 | | **Existing.** Used without asking (LEVELS-1). |
| 13 | Studied Attacks | After missing a creature with an attack roll, Advantage on your next attack roll against it before the end of your next turn. | 48 | | **Existing.** A small per-target marker. |
| 14 | Ability Score Improvement | Fighter-only extra ASI. | 48 | Choice | **Existing.** |
| 15 | Superior Critical (Champion) | Weapon and Unarmed Strike attack rolls crit on 18–20. | 49 | | **Existing.** Improved Critical's threshold moves from 19 to 18. |

Counts (p. 47): Second Wind 4. Weapon Mastery 5 (6 at 16). Action Surge's
second use is at 17.

### Monk (table p. 50) — Warrior of the Open Hand (p. 52)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Fleet Step (Open Hand) | After any Bonus Action other than Step of the Wind, you may also use Step of the Wind right away (still paying its Focus Point). | 52 | Choice per use | **Existing.** Offer Step of the Wind after another Bonus Action. |
| 12 | Ability Score Improvement | ASI feat or another feat. | 51 | Choice | **Existing.** |
| 13 | Deflect Energy | Deflect Attacks works against any damage type, not only Bludgeoning, Piercing and Slashing. | 52 | | **Existing.** Drop the damage-type filter. |
| 14 | Disciplined Survivor | Proficiency in every saving throw; after failing a save, spend 1 Focus Point to reroll it (the new roll stands). | 52 | Choice per use | **Existing.** The reroll is a post-roll resource choice (open decision 7). |
| 15 | Perfect Focus | On rolling Initiative without using Uncanny Metabolism, Focus Points rise to 4 if 3 or fewer. | 52 | | **Existing.** Apply after the existing Initiative offer of Uncanny Metabolism is declined. |

Counts (p. 50): Martial Arts die 1d10 from 11. Focus Points 11–15 (equal to
level). Unarmored Movement +20 ft (11–13), +25 ft (14–15).

### Paladin (table p. 53) — Oath of Devotion (p. 56)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Radiant Strikes | Every hit with a melee weapon or Unarmed Strike deals +1d8 Radiant. | 55 | | **Existing.** A passive damage rider. |
| 12 | Ability Score Improvement | ASI feat or another feat. | 55 | Choice | **Existing.** |
| 13 | Oath of Devotion Spells | Always prepared: Freedom of Movement, Guardian of Faith (both level 4; neither is on the Paladin list). | 56 | | **Existing.** Both KEEP in the 5–10 inventory. |
| 14 | Restoring Touch | Lay On Hands can also remove Blinded, Charmed, Deafened, Frightened, Paralyzed or Stunned, at 5 pool points each (not also healing). | 55 | Choice per use | **Existing.** Deafened has no effect in this game (DEAF-1). |
| 15 | Smite of Protection (Devotion) | Casting Divine Smite gives you and allies in your Aura of Protection Half Cover until your next turn. | 56 | | **Cut candidate (COVER-1).** Half Cover does not exist (open decision 5). |

Counts (p. 53): Channel Divinity 3 from 11. Prepared spells 10, 10, 11, 11,
12. Weapon Mastery stays 2. Devotion's next spells (Commune, Flame Strike) are
at 17.

### Ranger (table p. 58) — Hunter (p. 61)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Superior Hunter's Prey (Hunter) | Once per turn, damaging a creature marked by your Hunter's Mark also deals the mark's extra damage to another creature you can see within 30 ft of it. | 61 | Target per use | **Existing.** Needs a second-target choice; ADAPT-1's Horde Breaker shows the pattern (open decision 7). |
| 12 | Ability Score Improvement | ASI feat or another feat. | 59 | Choice | **Existing.** |
| 13 | Relentless Hunter | Damage cannot break your Concentration on Hunter's Mark. | 59 | | **Existing.** |
| 14 | Nature's Veil | Bonus Action: Invisible until the end of your next turn; uses = Wis modifier (min 1) per Long Rest. | 59 | | **Existing.** The Invisible condition exists (Invisibility). |
| 15 | Superior Hunter's Defense (Hunter) | Reaction on taking damage: Resistance to that damage and any more of the same type until the end of the turn. | 61 | Choice per use | **Existing.** React/Decline, like Uncanny Dodge. |

Counts (p. 58): Favored Enemy (free Hunter's Mark casts) 4 at 11–12, 5 at
13–15. Prepared 10, 10, 11, 11, 12. Weapon Mastery stays 2. Hunter has no
always-prepared spells.

### Rogue (table p. 62) — Thief (p. 64)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Improved Cunning Strike | Up to two Cunning Strike effects on one Sneak Attack, paying both die costs. | 63 | Choice per use | **Existing.** The A cycle must offer pairs of effects within the Sneak Attack dice. |
| 12 | Ability Score Improvement | ASI feat or another feat. | 63 | Choice | **Existing.** |
| 13 | Use Magic Device (Thief) | Attune up to four items; a 6 on 1d6 saves a used charge; use any Spell Scroll with Intelligence (cantrip and level 1 reliably, higher ones after an Arcana check of DC 10 + spell level, or the scroll is lost). | 64 | | **New system.** Needs magic items, charges and scrolls (SCOPE-2 keeps the feature; open decision 8). |
| 14 | Devious Strikes | New Cunning Strike effects: Daze (2d6; Con save or only one of move, action or Bonus Action next turn), Knock Out (6d6; Con save or Unconscious for 1 minute or until damaged, resaving each turn), Obscure (3d6; Dex save or Blinded until the end of its next turn). | 63 | Choice per use | **Existing.** Daze matches Slow's one-of limit; Unconscious-until-damaged matches Sleep and Eyebite. |
| 15 | Slippery Mind | Proficiency in Wisdom and Charisma saves. | 63 | | **Existing.** |

Counts (p. 62): Sneak Attack 6d6 (11–12), 7d6 (13–14), 8d6 (15). Cunning
Strike save DC stays 8 + Dex + PB. Poison still waits for a Poisoner's Kit
(#233).

### Sorcerer (table p. 65) — Draconic Sorcery (p. 70)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 12 | Ability Score Improvement | ASI feat or another feat. | 66 | Choice | **Existing.** |
| 14 | Dragon Wings (Draconic) | Bonus Action: wings give a Fly Speed of 60 ft for 1 hour; once per Long Rest, or restore it for 3 Sorcery Points. | 70 | | **Cut candidate (flight).** SCOPE-2 allows no fly forms and the 5–10 triage cut Fly (open decision 6). |

Counts (p. 65): no feature at 11, 13 or 15. Sorcery Points 11–15 (equal to
level). Cantrips 6. Prepared 16, 16, 17, 17, 18. No new Metamagic until 17.
Draconic Sorcery grants no spells past level 9.

### Warlock (table p. 71) — Fiend Patron (p. 76)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 11 | Mystic Arcanum (level 6) | One level 6 Warlock spell, cast once per Long Rest without a slot. | 72 | Choice | **Existing.** The free-cast pattern (SCOPE-2, #200); needs the level 6 spells. |
| 12 | Ability Score Improvement | ASI feat or another feat. | 72 | Choice | **Existing.** |
| 12 | Invocations 8 | One new invocation; level 12+ prerequisites open (below). | 71 | Choice | **Existing.** |
| 13 | Mystic Arcanum (level 7) | Add one level 7 Warlock spell on the same terms. | 72 | Choice | **Existing.** |
| 14 | Hurl Through Hell (Fiend) | Once per turn on a hit with an attack roll: Cha save or the target vanishes, takes 8d10 Psychic (unless a Fiend) and is Incapacitated until the end of your next turn, then returns to its space or the nearest free one. Once per Long Rest, or restore it by spending a Pact slot. | 76 | Choice per use | **Existing.** Reuses the Banishment state from the 5–10 work. |
| 15 | Mystic Arcanum (level 8) | Add one level 8 Warlock spell on the same terms. | 72 | Choice | **Existing.** |
| 15 | Invocations 9 | One new invocation; level 15+ prerequisites open (below). | 71 | Choice | **Existing.** |

Counts (p. 71): invocations 7, 8, 8, 8, 9; cantrips 4; prepared 11, 11, 12,
12, 13; Pact slots 3 of level 5. The Fiend grants no spells past level 9.

New invocation prerequisites in levels 11–15 (pp. 72–74):

| Invocation | Prereq | Effect | Note |
| --- | --- | --- | --- |
| Devouring Blade | 12, Thirsting Blade | Thirsting Blade's Extra Attack becomes two extra attacks (three in all). | KEEP candidate; same as Two Extra Attacks. |
| Witch Sight | 15 | Truesight 30 ft. | KEEP candidate; extends See Invisibility, which the game has. |

### Wizard (table p. 77) — Evoker (p. 82)

| Lvl | Feature | What it does | Page | Choice | Proposed note |
| ---: | --- | --- | ---: | --- | --- |
| 12 | Ability Score Improvement | ASI feat or another feat. | 79 | Choice | **Existing.** |
| 14 | Overchannel (Evoker) | When casting a damaging Wizard spell with a level 1–5 slot, deal maximum damage that turn. Free the first time per Long Rest; each repeat deals 2d12 Necrotic per slot level to you (+1d12 per level for each further repeat), ignoring Resistance and Immunity. | 82 | Choice per use | **Existing.** A cast-time toggle; the self-damage warning comes from public numbers. |

Counts (p. 77): no feature at 11, 13 or 15. Cantrips 5. Prepared 16, 16, 17,
18, 19. The spellbook still gains two Wizard spells per level. Evocation
Savant stays cut (SCOPE-2), so no free level 6–8 Evocation spells.

---

## 3. Spells of levels 6–8

Lists: B Bard, C Cleric, D Druid, S Sorcerer, K Warlock (Mystic Arcanum
only), W Wizard. No level 6–8 spell is on the Paladin or Ranger list, and no
subclass grants one. Bard Magical Secrets (10) and Lore Magical Discoveries
(6) open every Cleric, Druid and Wizard spell to Bards and add no new names.
"Conc" is Concentration. Summaries are paraphrased; read the page before
implementing.

Triage key, as in the 5–10 inventory: **KEEP** = resolvable in combat, camp
or exploration as the game stands or with the systems in the summary.
**CUT** = needs a DM or a mechanic the game lacks, under DM-1/DM-2/DM-3 or
SCOPE-1/SCOPE-2. **UNSURE** = a real use exists but depends on a design
decision; the question is in the notes.

### Level 6 spells (castable at class level 11: full casters by slot, Warlock by Mystic Arcanum)

| Spell | p. | Lists | Summary | Triage | Reason / notes |
| --- | ---: | --- | --- | --- | --- |
| Blade Barrier | 113 | C | Conc 10 min: a wall of whirling force blades (line up to 100 ft, or a ring 60 ft across; 20 ft high, 5 ft thick), Difficult Terrain with Three-Quarters Cover; a creature in it, entering it or ending its turn there makes a Dex save once per turn for 6d10 Force (half on success). | **KEEP** | Wall geometry (5–10); its cover has nothing to act on (COVER-1). |
| Chain Lightning | 114 | S, W | One target within 150 ft plus up to three others within 30 ft of it, each struck once: Dex save, 10d8 Lightning (half); +1 extra target per slot above 6. | **KEEP** | CLASS-2 target clicks. |
| Circle of Death | 115 | S, K, W | 60-ft-radius sphere within 150 ft: Con save, 8d8 Necrotic (half); +2d8 per slot above 6. | **KEEP** | The 500 gp pearl is not consumed (CLASS-8). |
| Conjure Fey | 118 | D | Conc 10 min: a Medium fey spirit appears and makes a melee spell attack for 3d12 + modifier Psychic, leaving the target Frightened until your next turn; each later turn a Bonus Action teleports it 30 ft and attacks again; +1d12 per slot above 6. | **KEEP** | A movable attacker driven by the caster's Bonus Action, not a creature with its own turn. |
| Contingency | 119 | W | 10-minute cast, 10 days (1,500 gp statuette): stores a self-affecting spell of level 5 or lower with an action cast; it takes effect the first time a trigger you describe occurs. One at a time. | **UNSURE** | The free-text trigger needs a DM. Offer a fixed trigger menu (for example "when I drop to half HP", "when a fight starts"), or cut? |
| Create Undead | 121 | C, K, W | 1-minute cast, at night only: up to three Small or Medium Humanoid corpses within 10 ft become Ghouls you command with a Bonus Action for 24 h; recasting keeps control; a level 8 slot makes five Ghouls or two Ghasts or Wights. | **KEEP** | Follows LEVELS-1's Animate Dead rulings (fight corpses, shared control system, 10-round cast). "Night only" needs the campaign's time of day; how long the undead last is open decision 10. |
| Disintegrate | 124 | S, W | Dex save or 10d6 + 40 Force; a creature reduced to 0 HP turns to dust and only True Resurrection or Wish can revive it; also destroys a nonmagical object or a force creation (such as Wall of Force); +3d6 per slot above 6. | **KEEP** | Monster casters can dust a party member for good under a level 15 cap (open decision 4). |
| Eyebite | 129 | B, S, K, W | Conc 1 min: each turn (Magic action) one creature within 60 ft makes a Wis save or is Asleep (Unconscious until damaged or shaken), Panicked (Frightened, Dashes away) or Sickened (Poisoned); a creature that saves cannot be chosen again. | **KEEP** | The flee AI exists (Fear, Dissonant Whispers). |
| Find the Path | 131 | B, C, D | Concentration up to a day: sense the most direct route to a familiar place. | **CUT** | Named in DM-2. |
| Flesh to Stone | 133 | D, S, W | Conc 1 min: Con save or Restrained (Speed 0 for a turn on success); three failed end-of-turn saves Petrify it for the duration, three successes end the spell; held for the full minute, the Petrification lasts until Greater Restoration. | **KEEP** | Needs Petrified, already listed for the 5–10 Greater Restoration. |
| Forbiddance | 133 | C | 10-minute cast, 1 day (1,000 gp, kept): a large area warded against teleport and planar travel; chosen creature types take 5d10 Radiant or Necrotic entering or ending a turn there. | **CUT** | A site ward set up in advance, like Hallow (cut in 5–10). |
| Freezing Sphere | 134 | S, W | 60-ft-radius sphere within 300 ft: Con save, 10d6 Cold (half); can freeze water, or be kept as a throwable globe for a minute; +1d6 per slot above 6. | **KEEP** | Water freezing and the held globe are left out. |
| Globe of Invulnerability | 136 | S, W | Conc 1 min, 10-ft emanation: spells of level 5 or lower cast from outside have no effect inside; +1 level blocked per slot above 6. | **KEEP** | Matters only against monster casters. |
| Guards and Wards | 137 | B, W | 1-hour cast, 24 h: wards a building's floor area (fogged, misleading corridors; locked or hidden doors; webbed stairs; one more effect). | **CUT** | Defending a site set up over an hour; nothing for the party to defend. |
| Harm | 139 | C | Con save: 14d6 Necrotic and the HP maximum drops by the damage taken (not below 1); half damage and no reduction on a success. | **KEEP** | Needs HP-maximum reduction (Greater Restoration removes it). |
| Heal | 139 | C, D | A creature within 60 ft regains 70 HP and is cured of Blinded, Deafened and Poisoned; +10 HP per slot above 6. | **KEEP** | Deafened has no effect (DEAF-1). |
| Heroes' Feast | 140 | B, C, D | 10-minute cast (1,000 gp bowl consumed) and an hour of eating: up to twelve creatures gain, for 24 h, Poison Resistance, immunity to Frightened and Poisoned, and +2d10 HP maximum (healed by that much). | **KEEP** | A camp buff that lasts into later fights (LEVELS-1 long buffs). Component cost: open decision 3. |
| Instant Summons | 143 | W | 1-minute cast: mark an object; crush the sapphire later to bring it to your hand from anywhere. | **CUT** | Object targeting was dropped (SCOPE-2, #225); inventory recall. |
| Irresistible Dance | 143 | B, W | Conc 1 min, Wis save: on failure Charmed and dancing in place (no movement, Disadvantage on attacks and Dex saves, attackers have Advantage), resaving as an action; on success dances (no movement) until the end of its next turn. | **KEEP** | — |
| Magic Jar | 146 | W | 1-minute cast: your body goes catatonic and your soul enters a container, from which it can possess a Humanoid within 100 ft (Cha save), taking its HP and physical scores while its soul is trapped; risky returns. | **UNSURE** | Resolvable as Dominate plus a helpless caster body, but complex. Worth building? |
| Mass Suggestion | 148 | B, S, W | Up to twelve creatures, Wis save or Charmed and follow a 25-word suggestion for 24 h. | **CUT** | DM-3 removed Suggestion; the mass version needs the same DM judgment. |
| Move Earth | 151 | D, S, W | Conc 2 h: reshape dirt, sand or clay in a 40-ft area, each change taking 10 minutes. | **CUT** | Exploration map editing, like Stone Shape. |
| Planar Ally | 152 | C | 10-minute cast: an otherworldly patron sends a Celestial, Elemental or Fiend that serves only if you agree a price. | **CUT** | Bargaining for service needs a GM, like Planar Binding. |
| Programmed Illusion | 156 | B, W | An illusion that plays when a set condition occurs. | **CUT** | Named in DM-2. |
| Sunbeam | 166 | C, D, S, W | Conc 1 min: a 60-ft line, Con save, 6d8 Radiant and Blinded until your next turn (half and not Blinded on success); a Magic action fires a new line each turn; you shed sunlight. | **KEEP** | The sunlight matters for monsters with Sunlight Sensitivity. |
| Transport via Plants | 170 | D | Link a Large plant to any plant you have seen; for 1 minute creatures step from one to the other. | **CUT** | Long-range travel between plants, like Tree Stride. |
| True Seeing | 171 | B, C, S, K, W | Touch, 1 h (25 gp consumed): Truesight 120 ft. | **KEEP** | Extends See Invisibility, implemented for Wizards. |
| Wall of Ice | 172 | W | Conc 10 min: a dome, globe or ten 10-ft panels 1 ft thick; creatures in its path are pushed aside and make a Dex save for 10d6 Cold (half); each section has AC 12 and 30 HP and is vulnerable to Fire; a broken section leaves freezing air (Con save, 5d6 Cold, first pass each turn); higher slots add 2d6 and 1d6. | **KEEP** | Wall geometry with breakable sections. |
| Wall of Thorns | 173 | D | Conc 10 min: a 60-ft wall or 20-ft ring that blocks sight; Dex save, 7d8 Piercing as it appears; moving through costs 4 ft per foot, and entering or ending a turn in it is a Dex save for 7d8 Slashing once per turn; +1d8 per slot above 6. | **KEEP** | — |
| Wind Walk | 174 | D | 1-minute cast, 8 h: you and up to ten creatures become clouds with a 300-ft Fly Speed, able only to Dash or start a 1-minute reversion. | **CUT** | Overland flight (SCOPE-2 no fly forms). |
| Word of Recall | 175 | C | You and up to five willing creatures within 5 ft teleport to a sanctuary prepared earlier with this spell. | **UNSURE** | A party escape to a chosen temple in Phlan. Should a dungeon be escapable instantly? |

### Level 7 spells (castable at class level 13)

| Spell | p. | Lists | Summary | Triage | Reason / notes |
| --- | ---: | --- | --- | --- | --- |
| Arcane Sword | 110 | B, W | Conc 1 min: a spectral sword makes a melee spell attack for 4d12 + modifier Force; each later turn a Bonus Action moves it 30 ft and attacks again. | **KEEP** | The 250 gp sword is not consumed (CLASS-8). |
| Conjure Celestial | 118 | C | Conc 10 min: a 10-ft-radius pillar of light; each creature in it either regains 4d12 + modifier HP or makes a Dex save for 6d12 Radiant (half); it moves up to 30 ft when you move and affects creatures it moves onto, that enter it or end a turn there, once per turn; +1d12 per slot above 7. | **KEEP** | A moving zone (5–10 auras); propose heal for allies and harm for enemies without asking. |
| Delayed Blast Fireball | 122 | S, W | Conc 1 min: a bead explodes when the spell ends: 20-ft radius, Dex save, 12d6 Fire plus 1d6 for each of your turns it waited (half); a creature touching it may set it off or throw it; +1d6 per slot above 7. | **KEEP** | Needs a way to end Concentration on purpose; touching and throwing the bead left out. |
| Divine Word | 125 | C | Bonus Action: chosen creatures within 30 ft make a Cha save; on a failure, by current HP: 20 or fewer die, 21–30 Blinded, Deafened and Stunned for 1 h, 31–40 Blinded and Deafened for 10 min, 41–50 Deafened for 1 min; failing Celestials, Elementals, Fey and Fiends are also sent home for 24 h. | **KEEP** | Reads current HP; sent-home creatures count as defeated, as Banishment does (LEVELS-1). |
| Etherealness | 128 | B, C, S, K, W | Up to 8 h in the Border Ethereal: unseen and untouchable, moving through walls and vertically; +3 willing creatures per slot above 7. | **CUT** | Exploration movement through walls; in a fight it amounts to leaving the battlefield, which fleeing already covers (FLEE-1). |
| Finger of Death | 131 | S, K, W | Con save, 7d8 + 30 Necrotic (half); a Humanoid it kills rises at the start of your next turn as a Zombie that obeys you. | **KEEP** | The Zombie uses the shared control system; how long it lasts is open decision 10. |
| Fire Storm | 132 | C, D, S | Up to ten contiguous 10-ft cubes within 150 ft, arranged freely: Dex save, 7d10 Fire (half). | **KEEP** | Needs a multi-cube shape tool for CLASS-5 aiming (or preset shapes). |
| Forcecage | 134 | B, K, W | Conc 1 h (1,500 gp consumed): an invisible immobile prison, a 20-ft cage of bars or a solid 10-ft box; creatures inside cannot leave except by teleport after a Cha save; immune to Dispel Magic. | **KEEP** | Blocking squares; the box takes creatures out of the fight. Component cost: open decision 3. |
| Magnificent Mansion | 147 | B, W | A door to a furnished extradimensional dwelling for 24 h. | **CUT** | Named in DM-2. |
| Mirage Arcane | 149 | B, D, W | Terrain up to a mile square looks and feels like other terrain for 10 days. | **CUT** | Named in DM-2. |
| Plane Shift | 153 | C, D, S, K, W | You and up to eight willing creatures go to another plane. | **CUT** | The campaign has no other planes. |
| Prismatic Spray | 154 | B, S, W | 60-ft cone, Dex save; each creature gets a random ray: 12d6 Fire, Acid, Lightning, Poison or Cold (half); indigo (Restrained; three failed Con saves Petrify, three successes end it); violet (Blinded, then a Wis save or sent to another plane); or two rays. | **KEEP** | Violet's plane sending counts as defeated (Banishment); needs Petrified. |
| Project Image | 156 | B, W | Conc up to a day: an illusory double up to 500 miles away that you see and hear through. | **CUT** | Remote senses, like Arcane Eye and Clairvoyance. |
| Regenerate | 158 | B, C, D | 1-minute cast, touch: 4d8 + 15 HP, then 1 HP at the start of each of the target's turns for 1 hour; lost body parts regrow. | **KEEP** | In camp it amounts to a full heal over the hour; in a fight the cast takes 10 rounds (Magic Circle precedent). |
| Resurrection | 158 | B, C | 1-hour cast (1,000 gp diamond consumed): a creature dead up to a century (not of old age, not Undead) returns with full HP, cured of poison, missing parts restored; −4 to D20 Tests, easing by 1 per Long Rest; reviving one dead a year or more drains the caster until a Long Rest. | **KEEP** | Resurrection is kept (#175). Offering: open decision 3. |
| Reverse Gravity | 159 | D, S, W | Conc 1 min, 50-ft-radius, 100-ft-high cylinder: unanchored creatures fall upward (a Dex save grabs something fixed), hitting a ceiling as in a fall or hovering at the top, then fall when it ends. | **UNSURE** | Falling is not modeled (DM-3). Resolve it as fall damage on the PUSH-1 scale, out of melee reach while lifted, and damage again at the end? |
| Sequester | 161 | W | Until dispelled (5,000 gp consumed): an object or willing creature becomes invisible and undetectable, a creature in suspended animation, until a set condition. | **CUT** | Nothing to act on, like Secret Chest. |
| Simulacrum | 162 | W | 12-hour cast (1,500 gp consumed): a snow copy of a Beast or Humanoid that stayed within 10 ft the whole time; half its HP maximum, obeys you, acts on your turn, cannot gain levels or rest, repaired at 100 gp per HP; one at a time. | **UNSURE** | A permanent extra party member (for example a copy of the party's Fighter). Does the roster allow a controlled creature outside a summon's duration? |
| Symbol | 167 | B, C, D, W | 1-minute cast (1,000 gp consumed): a hidden glyph with a set trigger; when triggered, creatures in 60 ft suffer Death, Discord, Fear, Pain, Sleep or Stunning for 10 minutes. | **CUT** | A trap with a set trigger, like Glyph of Warding (cut in 5–10, DM-3 traps). |
| Teleport | 168 | B, S, W | You and up to eight willing creatures (or one object) go to a destination on the same plane; how well you know it sets the chance of a mishap, a similar place, off target or on target. | **UNSURE** | Fast travel to places the party has visited. Offer only "very familiar" destinations (visited towns), or cut with Teleportation Circle? |

### Level 8 spells (castable at class level 15)

| Spell | p. | Lists | Summary | Triage | Reason / notes |
| --- | ---: | --- | --- | --- | --- |
| Animal Shapes | 108 | D | 24 h: any number of willing creatures become Large or smaller Beasts of CR 4 or lower, keeping their own HP and mental scores, with Temporary HP equal to the form's HP and no spellcasting; a Magic action changes forms. | **KEEP** | Reuse the Wild Shape and Polymorph form set; needs CR 2–4 beasts with art. Duration across fights: open decision 9. |
| Antimagic Field | 109 | C, W | Conc 1 h, 10-ft emanation around you: no spells, Magic actions or magical effects inside; magic items stop working; ongoing spells are suppressed; no teleport in or out. | **KEEP** | Needs the 5–10 ongoing-spell registry plus suppression; it also blocks the caster's own spells. |
| Antipathy/Sympathy | 109 | B, D, W | 1-hour cast, 10 days: a creature or object repels (Frightened) or attracts (Charmed) a named kind of creature that fails a Wis save within 120 ft. | **CUT** | A lure or ward set up over an hour; where the creatures go needs a DM. |
| Befuddlement | 112 | B, D, K, W | Int save: 10d12 Psychic and no spellcasting or Magic actions, resaving every 30 days; Greater Restoration, Heal or Wish end it; half damage only on a success. | **KEEP** | A lasting effect like Contagion; Heal and Greater Restoration end it. |
| Clone | 115 | W | 1-hour cast (1,000 gp diamond consumed, 2,000 gp vessel): an inert copy grows for 120 days; if the original dies after that, its soul moves into the copy, without equipment. | **UNSURE** | Resurrection insurance (#175). Needs a stored vessel and 120 days of campaign time. Worth it under the cap? |
| Control Weather | 120 | C, D, W | 10-minute cast outdoors, Conc 8 h: shift precipitation, temperature and wind within 5 miles. | **CUT** | Weather is not modeled. |
| Demiplane | 122 | S, K, W | 1 h: a door to an empty 30-ft room, new or made earlier. | **CUT** | Storage and hiding; Tiny Hut already gives an uninterruptible rest (LEVELS-1). |
| Dominate Monster | 125 | B, S, K, W | Conc 1 h: any creature, Wis save (Advantage if fighting you) or Charmed and commanded; resaves when damaged. | **KEEP** | LEVELS-1's shared control system. |
| Earthquake | 127 | C, D, S | Conc 1 min, 100-ft radius: Difficult Terrain; on casting and at the end of each of your turns, creatures on the ground make a Dex save or fall Prone and lose Concentration; also opens fissures and damages or collapses structures. | **KEEP** | The tremor only; fissures and collapsing buildings need elevation and structure HP, left out as Wall of Stone's bridging was. |
| Glibness | 136 | B, K | 1 h: treat Charisma check rolls as 15; truth-detecting magic reads you as truthful. | **CUT** | Social checks (DM-1; campaign skill checks dropped, SCOPE-2). |
| Holy Aura | 141 | C | Conc 1 min, 30-ft emanation: chosen creatures have Advantage on all saves and attackers have Disadvantage against them; a Fiend or Undead that hits one in melee makes a Con save or is Blinded until the end of its next turn. | **KEEP** | Moving aura (5–10). |
| Incendiary Cloud | 142 | D, S, W | Conc 1 min, 20-ft-radius Heavily Obscured cloud within 150 ft: Dex save, 10d8 Fire (half) on appearing and when it moves onto, is entered or ends a turn on a creature, once per turn; drifts 10 ft away from you each turn. | **KEEP** | Drifts like Cloudkill. |
| Maze | 148 | W | Conc 10 min, no save: the target is lost in a maze demiplane and escapes only with a DC 20 Intelligence (Investigation) check as a Study action; it returns to its space. | **KEEP** | The Banishment off-board state. |
| Mind Blank | 149 | B, W | Touch, 24 h: Immunity to Psychic damage and Charmed; mind reading, scrying and mind control fail. | **KEEP** | A long buff that lasts into later fights (LEVELS-1); the anti-divination half has nothing to act on. |
| Power Word Stun | 154 | B, S, K, W | A creature with 150 HP or fewer is Stunned, ending it with a Con save at the end of each of its turns; above 150, its Speed is 0 until your next turn. | **KEEP** | Reads current HP. |
| Sunburst | 167 | C, D, S, W | 60-ft-radius sphere within 150 ft: Con save, 12d6 Radiant and Blinded for 1 minute, resaving each turn (half and not Blinded on success); dispels spell Darkness in the area. | **KEEP** | — |
| Tsunami | 171 | D | 1-minute cast, Conc 6 rounds: a wall of water up to 300 ft long and high and 50 ft thick; Str save, 6d10 Bludgeoning; each turn it moves 50 ft away, sweeping creatures (Str save, 5d10, 1d10 less each round) and losing 50 ft of height; caught creatures must swim. | **UNSURE** | Larger than any battle map, a 1-minute cast and swimming (SCOPE-2). Keep as a moving damage-and-push wall, or cut? |

### Level 3–5 spells newly reachable by half casters at 11–15

Paladin and Ranger gained level 3 spells at 9 (in the 5–10 inventory). At
11–15 their only new spell level is 4, at class level 13; level 5 comes at 17.
Every spell below was already triaged in the 5–10 inventory.

| Spell (level 4) | p. | Paladin | Ranger | 5–10 triage |
| --- | ---: | --- | --- | --- |
| Aura of Life | 111 | 13 | — | KEEP |
| Banishment | 112 | 13 | — | KEEP |
| Conjure Woodland Beings | 118 | — | 13 | KEEP |
| Death Ward | 122 | 13 | — | KEEP |
| Dominate Beast | 125 | — | 13 | KEEP |
| Freedom of Movement | 134 | Devotion 13 (always prepared, off-list) | 13 | KEEP |
| Guardian of Faith | 137 | Devotion 13 (always prepared, off-list) | — | KEEP |
| Locate Creature | 145 | 13 | 13 | CUT |
| Stoneskin | 165 | — | 13 | KEEP |

### Always-prepared and free spells from features, levels 11–15

| Source | Lvl | Spells (spell level) | Page | Proposed status |
| --- | ---: | --- | ---: | --- |
| Oath of Devotion | 13 | Freedom of Movement (4), Guardian of Faith (4) | 56 | both KEEP |
| Warlock Mystic Arcanum | 11 | One of Circle of Death, Create Undead, Eyebite, True Seeing (6) | 72, 75 | all four KEEP |
| Warlock Mystic Arcanum | 13 | One of Etherealness, Finger of Death, Forcecage, Plane Shift (7) | 72, 75 | Finger of Death and Forcecage KEEP; the other two CUT |
| Warlock Mystic Arcanum | 15 | One of Befuddlement, Demiplane, Dominate Monster, Glibness, Power Word Stun (8) | 72, 76 | three KEEP; Demiplane and Glibness CUT |
| Bard Magical Secrets / Lore Magical Discoveries | 11–15 | Cleric, Druid and Wizard spells up to level 8 | 33, 35 | Choice among kept spells |
| Thief Use Magic Device | 13 | Any Spell Scroll, cast with Intelligence | 64 | Waits for magic items |
| Life, Land, Draconic, Fiend, Hunter | — | None at 11–15 | 40, 46, 70, 76, 61 | — |

### Cross-checks done

- Union of the six printed class lists at levels 6–8: 68 names (31/20/17).
  Count of level 6–8 spell-description headers on pp. 108–175: 31, 20 and 17.
  Each description's class tag matches the printed lists (Bard pp. 34–35,
  Cleric pp. 39–40, Druid p. 45, Sorcerer p. 69, Warlock pp. 75–76, Wizard
  pp. 81–82).
- The Paladin level 4 list (p. 56: Aura of Life, Banishment, Death Ward,
  Locate Creature) and Ranger level 4 list (p. 60: Conjure Woodland Beings,
  Dominate Beast, Freedom of Movement, Locate Creature, Stoneskin) were read
  from the layout text. The Devotion table (p. 56) gives Freedom of Movement
  and Guardian of Faith at 13.
- Class-table rows for 11–16 were read from each table's layout text, with
  column headers checked for Barbarian, Fighter, Monk, Paladin, Ranger, Rogue,
  Sorcerer and Warlock.
- Text-extraction caveat: the reading-order text lost or mixed the headers of
  Antipathy/Sympathy, Control Weather, Earthquake, Regenerate, Reverse Gravity
  and Sunbeam; their casting time, duration and text were taken from the
  layout text of pp. 109, 120, 127, 158, 159 and 166. Programmed Illusion's
  body runs onto p. 157 and was not summarized, since DM-2 cuts it by name.

---

## 4. Open decisions this inventory surfaces

1. **CAP-1 vs levels 11–15.** Answered by LEVELS-2: the cap is 15 for every
   class, and the Training Hall's limit is one level for every class.
2. **The eight UNSURE spells** (questions in section 3): Contingency, Magic
   Jar, Word of Recall, Reverse Gravity, Simulacrum, Teleport, Clone, Tsunami.
   Answered by [LEVELS-3](SRD-DECISIONS.md#levels-3-2026-10-15-the-open-decisions-of-levels-1115): all kept but Clone.
3. **Costly consumed components.** CLASS-11 makes them free, and LEVELS-1
   priced only Revivify and Raise Dead. Should Resurrection (1,000 gp diamond)
   take the same 300 gp × level offering? And Heroes' Feast (1,000 gp),
   Forcecage (1,500 gp), Simulacrum (1,500 gp), Clone (1,000 gp) and True
   Seeing (25 gp): free, or charged? Answered by LEVELS-3: only Resurrection
   pays the offering.
4. **Disintegrate's dust.** Only True Resurrection or Wish (level 9, beyond
   the cap) revive a disintegrated creature, so a party member dusted by a
   monster caster is gone for good. Accept that, or let Resurrection work?
   Answered by LEVELS-3: Resurrection works.
5. **Smite of Protection** (Devotion 15) only grants Half Cover, which COVER-1
   removed, so the Devotion Paladin would get nothing at 15. Answered by
   LEVELS-2: it gives Half Cover's +2 AC and +2 to Dexterity saves instead.
   (Nature's Sanctuary keeps its Resistance half.)
6. **Dragon Wings** (Draconic 14): cut with flight, as Fly was? Draconic
   Sorcery then has no usable feature at 14. Answered by LEVELS-3: the wings
   let the Sorcerer ignore Difficult Terrain and move through enemies.
7. **Timing and post-roll choices.** Proposed: Peerless Skill applies on its
   own (BARD-1); Disciplined Survivor's Focus reroll is used without asking,
   like Indomitable (LEVELS-1); Persistent Rage triggers at Initiative when a
   Rage use is spent; Perfect Focus applies after Uncanny Metabolism is
   declined; Superior Hunter's Defense uses React/Decline like Uncanny Dodge;
   Superior Hunter's Prey and Improved Blessed Strikes' Temporary HP pick
   their target by click. Approve?
8. **Use Magic Device** (Thief 13) needs magic items, charges and Spell
   Scrolls, which the game lacks though the original's treasure has scrolls
   and wands. Defer it until magic items exist, or build scrolls with it?
9. **Animal Shapes** lasts 24 hours without Concentration. Do its forms end
   with the fight, as Wild Shape does (WILD-1), or last into later fights like
   other long buffs (LEVELS-1)? It also needs CR 2–4 beast forms with art.
   Answered by LEVELS-3: they end with the fight.
10. **Raised undead.** Do Create Undead's Ghouls and Finger of Death's Zombie
    stay as companions between fights, or end with the fight in which their
    corpses were raised (as LEVELS-1 limits Animate Dead's corpses)?
    Answered by LEVELS-3: they end with the fight.

## Attribution

This work includes material from the System Reference Document 5.2.1 ("SRD
5.2.1") by Wizards of the Coast LLC, available at
https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative
Commons Attribution 4.0 International License, available at
https://creativecommons.org/licenses/by/4.0/legalcode.
