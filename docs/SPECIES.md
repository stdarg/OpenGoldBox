# Species

The species traits a party member gets from SRD 5.2.1 pp. 83-86, as the game
plays them. [SRD-DECISIONS](SRD-DECISIONS.md) records the traits that were cut
(Elf Trance, Goliath Powerful Build and Large Form, Dragonborn Draconic Flight,
Dwarf Stonecunning, the Forest and Rock Gnome lineages, Tiefling Thaumaturgy and
High Elf Prestidigitation).

## Choices at creation

The race page lists each species' choices under its description. Each starts
on its first option, so a new character is always complete.

| Species | Choice | Options |
| --- | --- | --- |
| Elf | Elven Lineage | High Elf, Wood Elf (no Drow: [DROW-1](SRD-DECISIONS.md#drow-1-2026-10-14-no-drow-lineage-for-now)) |
| Elf | Keen Senses | Insight, Perception or Survival |
| Elf, Tiefling | Spellcasting ability | Intelligence, Wisdom or Charisma |
| High Elf | Cantrip | any Wizard cantrip in the game |
| Tiefling | Fiendish Legacy | Abyssal, Chthonic, Infernal |
| Tiefling, Human | Size | Medium or Small |
| Dragonborn | Draconic Ancestry | the ten dragons |
| Goliath | Giant Ancestry | Cloud, Fire, Frost, Hill, Stone, Storm |
| Human | Skillful | any skill |
| Human | Versatile | an Origin feat the background does not give: Skilled (its three skills checked below it), Alert, Magic Initiate (Cleric, Druid or Wizard; spells on the spell page) or Savage Attacker |

A sheet stores each choice as a grant (`lineage:`, `legacy:`, `size:`,
`skill:`, `dragon:`, `giant:`, `spellcasting_ability:`, `cantrip:`).

- **Small:** a Small Tiefling or Human is Small for the rules that ask.
- **Wood Elf:** Speed 35 feet; Longstrider at level 3.
- **Resistances:** the Fiendish Legacy's (Poison, Necrotic or Fire) and the
  Draconic Ancestry's damage type.

## Lineage spells

An Elf's or Tiefling's lineage spells use the chosen spellcasting ability and
are always prepared. The cantrip comes at level 1 (a Tiefling's from its legacy,
the High Elf's chosen Wizard cantrip); the level-1 spell comes at level 3 and
is cast once per Long Rest without a slot (a Tiefling's Hellish Rebuke, Ray of
Sickness or False Life; a Wood Elf's Longstrider). A class with slots may also
cast it with a slot. The rest window shows the free cast as "Lineage: {spell}".

## Breath Weapon

A Dragonborn replaces the Attack action's attack with a 15-foot Cone or a
30-foot Line 5 feet wide, chosen each time (`breath_weapon_cone`,
`breath_weapon_line`). Each creature in the area makes a Dexterity save against
8 + Constitution modifier + Proficiency Bonus and takes 1d10 damage of the
ancestry's type, or half on a success. The die grows only at level 5, past the
level cap. Since no character makes Extra Attack's second attack before level 5,
the breath spends the Action.

Uses equal the Proficiency Bonus (2 through level 4) and return with a Long
Rest. The spent count is a lasting effect (`trait_uses`), cleared by a Long
Rest like the Orc's Relentless Endurance; the rest window and the combat
resources show "Breath Weapon" with the uses left.

In combat, A cycles to "Breath Weapon (cone)" or "Breath Weapon (line)";
Space or a click starts aiming it, as for an area spell.

## Giant Ancestry

A Goliath's boon has as many uses as the Proficiency Bonus (2 through level
4), back after a Long Rest; the rest window and combat resources show it under
the boon's name. Each use is asked about with the React and Decline buttons,
like Hellish Rebuke, except Cloud's Jaunt.

- **Cloud's Jaunt:** a Bonus Action. A cycles to "Cloud's Jaunt"; it is aimed
  like Misty Step, at an unoccupied square within 30 feet in sight. It is not a
  spell, so it does not end hiding, Invisibility or Sanctuary.
- **Fire's Burn, Frost's Chill, Hill's Tumble:** after the Goliath's attack
  roll hits a creature and damages it without dropping it, React adds 1d10
  Fire damage, or 1d6 Cold damage and Speed 10 feet lower until the start of
  the Goliath's next turn, or knocks a Large or smaller creature Prone (not
  asked when it already is). A Critical Hit doubles the boon's die. A spell
  with several attack rolls, such as Scorching Ray, asks once.
- **Stone's Endurance:** when an attack is about to damage the Goliath, React
  lessens the damage by 1d12 + Constitution modifier.
- **Storm's Thunder:** when an attack damages the Goliath and the attacker is
  within 60 feet, React deals it 1d8 Thunder damage.

Stone's Endurance and Storm's Thunder answer only an attack's damage so far,
as Hellish Rebuke does; damage from a save or an area waits on
[#164](https://github.com/stdarg/OpenGold/issues/164). The Quick-play AI uses
each boon at its first chance.

## Saves against conditions

Dwarven Resilience (Advantage against Poisoned), Fey Ancestry (against
Charmed) and Brave (against Frightened) have nothing to act on yet: no save in
the game makes a party member Poisoned or Frightened, and Charm Person, the
only charm, already gives its target Advantage while the caster's side fights
it. A new save against one of those conditions must check the trait.

## Naturally Stealthy

A Halfling may [Hide](HIDE.md) when the only thing between it and an enemy is
a creature at least one size larger.

## Resourceful

A Human gains Heroic Inspiration on finishing a Long Rest (a new character has
none yet). It is used without asking on the first failed attack roll, saving
throw or ability check a reroll could save: the d20 is rolled again and the
new roll stands ([HEROIC-1](SRD-DECISIONS.md#heroic-1-2026-10-14-heroic-inspiration-without-asking)).
The rest window and combat resources show it.
