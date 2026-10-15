# Monk

Levels 1–4 of the SRD 5.2.1 Monk (pp. 48–50). Unarmored Defense came earlier;
see [Armor](ARMOR.md). Slow Fall is dropped (SRD-DECISIONS).

## Martial Arts (level 1)

- While the Monk wears no armor, holds no Shield and wields nothing or only
  Monk weapons (Simple Melee weapons and Martial Melee weapons with the Light
  property), its Unarmed Strikes and Monk weapons use the better of Strength
  and Dexterity for attack and damage rolls, and deal at least the Martial
  Arts die, a d6 through level 4. An Unarmed Strike deals 1d6 + that modifier
  Bludgeoning damage.
- **Unarmed Strike** (Bonus Action): one more Unarmed Strike against an enemy
  within 5 feet, from the Bonus Action list, then a click on the target. It
  carries no weapon's mastery or feat.
- Grapple and Shove are dropped (SRD-DECISIONS).
- Verification: `opengold_monk_tests` (Dexterity-based Unarmed Strikes and
  their damage, the Bonus Action strike, a dagger as a Monk weapon, no Martial
  Arts in armor, a dagger dealing the d6 and a two-handed quarterstaff keeping
  its d8, and a Shield ending Martial Arts and Unarmored Defense).

## Levels 2–4

- Monks now advance to level 4.
- **Monk's Focus** (level 2): Focus Points equal to the Monk level, all back
  on a Short or Long Rest. They share the Action Surge store, which no Monk
  has, so vitals and checkpoints keep their format. From the Bonus Action list:
  - **Flurry of Blows** (1 Focus): two Unarmed Strikes at the chosen enemy
    within 5 feet; the second only if the first leaves it standing.
  - **Patient Defense: Disengage** (free), or **Disengage and Dodge** (1 Focus).
  - **Step of the Wind: Dash** (free), or **Disengage and Dash** (1 Focus). The
    doubled jump has no use.
- **Unarmored Movement** (level 2): 10 feet more Speed without armor or a
  Shield.
- **Uncanny Metabolism** (level 2, once per Long Rest; it shares Arcane
  Recovery's store): when Initiative is rolled and the Monk has spent Focus or
  lost Hit Points, the Initiative window (shared with Alert, now titled
  "Initiative") offers **Uncanny Metabolism**: all Focus Points back and 1d6 +
  the Monk level in Hit Points. A Monk with Alert picks either the swap or
  Uncanny Metabolism.
- Level 3 brings Deflect Attacks and the Warrior of the Open Hand (below).
  Level 4 brings a feat or ability points. Slow Fall is dropped
  (SRD-DECISIONS).
- Verification: `opengold_monk_tests` (the grants, four Focus and 40 feet at
  level 4, Flurry of Blows, Step of the Wind free and with a Focus Point (Dash
  and Disengage), Patient Defense free (no Dodge) and with a Focus Point (Dodge),
  no Focus option at 0 Focus, a Shield taking away Unarmored Movement, Uncanny
  Metabolism at Initiative and once per Long Rest), `opengold_archery_tests`
  (Monks advance).

## Deflect Attacks and Open Hand Technique (level 3)

- **Deflect Attacks** (Reaction): when an attack roll hits the Monk with
  Bludgeoning, Piercing or Slashing damage, play stops and the Monk is asked
  "You are hit. Deflect the attack ... or decline." (React/Decline, the attack
  total unseen, as for Shield). Deflecting lowers the hit's damage by 1d10 +
  Dexterity + Monk level. A critical hit can be deflected too.
- If that brings the damage to 0, the Monk spends a Focus Point and is near
  enough to the attacker (5 feet for melee, 60 for ranged), a second question
  asks whether to **redirect** it: the attacker makes a Dexterity save
  against 8 + Wisdom + Proficiency or takes 2d6 + Dexterity of the attack's
  type. Adaptation: the force always goes back at the attacker rather than at
  a chosen creature.
- Shield's question became a general hit-reaction question: the creature is
  asked once per command about each kind of moment (a hit, Magic Missile, a
  redirect), and its answers replay the command with the same dice.
  Checkpoints become `OGCOMBAT 40`.
- **Open Hand Technique**: Flurry of Blows is also offered as **Flurry of
  Blows: Addle / Push / Topple**, the technique applying to each of its hits.
  Addle: no Opportunity Attacks until the target's next turn. Push: a Strength
  save or pushed 15 feet. Topple: a Dexterity save or Prone. Adaptation: the
  technique is chosen with the Flurry, not after each hit.
- The computer always deflects and redirects when it can.
- Verification: `opengold_monk_tests` (the question and its checkpoint,
  Deflect's Reaction, the redirect question and its checkpoint, the redirect's
  Focus Point and save, Decline; Topple, Addle stopping Opportunity Attacks and
  Push's 15 feet), `opengold_wizard_spell_tests` (Shield
  still asked).

## Levels 5–15

The Training Hall trains a Monk, like every class, to level 15, the game's cap
([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

| Level | Gains |
| ---: | --- |
| 5 | Extra Attack; Stunning Strike; Martial Arts d8; Proficiency Bonus +3 |
| 6 | Empowered Strikes; Open Hand: Wholeness of Body; Unarmored Movement +15 feet |
| 7 | Evasion |
| 8 | A feat or ability points |
| 9 | Proficiency Bonus +4 (Acrobatic Movement is cut, SCOPE-2) |
| 10 | Heightened Focus; Self-Restoration; Unarmored Movement +20 feet |
| 11 | Open Hand: Fleet Step; Martial Arts d10 |
| 12 | A feat or ability points |
| 13 | Deflect Energy; Proficiency Bonus +5 |
| 14 | Disciplined Survivor; Unarmored Movement +25 feet |
| 15 | Perfect Focus |

- **Stunning Strike:** the A cycle offers "Melee attack (Stunning Strike)" once
  a turn while the Monk has Focus. On a hit it spends 1 Focus: the target makes
  a Constitution save against the Focus DC or is Stunned until the start of the
  Monk's next turn (Incapacitated, failing Strength and Dexterity saves, attacked
  with Advantage); on a success its Speed is halved and the next attack roll
  against it has Advantage. "Flurry of Blows: Stunning Strike" (2 Focus in
  all) tries it on the Flurry's first hit.
- **Empowered Strikes:** Unarmed Strikes deal Force damage instead of
  Bludgeoning, always: almost nothing resists Force, so the choice is never
  worth asking.
- **Wholeness of Body:** a Bonus Action heals the Martial Arts die + Wisdom
  modifier, a Wisdom modifier's worth of uses (at least 1) per Long Rest.
- **Evasion:** as the [Rogue's](ROGUE.md).
- **Heightened Focus:** Flurry of Blows makes three Unarmed Strikes, and
  Patient Defense with Focus also gives two Martial Arts dice of Temporary Hit
  Points. "Step of the Wind: Disengage and Dash, carrying an ally (1 Focus)"
  is offered for each ally beside the Monk, Large or smaller, standing or not
  (pulling a fallen friend out of melee is its use). The ally steps into each
  square the Monk leaves for the rest of the turn, provoking nothing; zones it
  passes through do not affect it.
- **Self-Restoration:** at the end of each of its turns, one of Charmed,
  Frightened or Poisoned ends on the Monk, without asking.
- **Fleet Step:** after any other Bonus Action, the A cycle still offers Step
  of the Wind that turn.
- **Deflect Energy:** Deflect Attacks is asked about hits of any damage type.
- **Disciplined Survivor:** proficiency in every saving throw. A failed save a
  20 could pass is rolled again for a Focus Point, without asking, after Heroic
  Inspiration; the combat log shows it with the new roll
  ([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)). Like
  Indomitable, it does not reroll the save repeated at the end of a turn.
- **Perfect Focus:** a Monk with fewer than 4 Focus Points when a fight starts
  has 4 (Uncanny Metabolism, when used, restores them all anyway).
- Verification: `opengold_monk_levels_tests` (the campaign to 15, grants,
  Unarmored Movement, save proficiencies, Extra Attack,
  Stunning Strike and on Flurry, Empowered Strikes, Wholeness of Body,
  Heightened Focus and the carried ally, Fleet Step and its checkpoint,
  Deflect Energy, Disciplined Survivor's reroll, Perfect Focus).
