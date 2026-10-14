# Replacing choices on gaining a level

SRD 5.2.1 lets a character change some earlier choices when it gains a level.
The level-up window offers them on a **Replace** page, after any checkbox page
(Skilled, Metamagic, invocations) and before the spell page (rules 0.6.176).

| Class | May replace one… | With… |
| --- | --- | --- |
| Bard, Cleric, Druid, Sorcerer, Warlock | cantrip of its class | another cantrip from its class list |
| Paladin (Blessed Warrior), Ranger (Druidic Warrior) | Fighting Style cantrip | another Cleric or Druid cantrip |
| Bard, Sorcerer, Warlock | spell on its list | another class spell of a level it has slots for |
| Warlock | Eldritch Invocation | any invocation it qualifies for at the new level |
| Sorcerer (level 3 onward) | Metamagic option | one it does not know |

The Cleric, Druid, Paladin, Ranger and Wizard change prepared spells on a Long
Rest instead; the Wizard also swaps a cantrip on a Long Rest.

## In the window

Each row is a **Replace** dropdown starting at "Keep current" and a **With**
dropdown, enabled once something is chosen to replace. A row with a Replace
but no With yet holds Next back ("Choose what replaces it, or keep current.").
A row is shown only when there is something to replace and something to
replace it with: a Cleric who already knows every Cleric cantrip the game has
gets no cantrip row. A swapped spell's replacement takes its locked place on
the spell page; if the replacement had been one of the level's new picks, that
place is free to choose again.

## Rules notes

- A replacement keeps the replaced cantrip's place: same source and
  acquisition level.
- A replaced invocation records the level it was taken at, so the Warlock's
  level-1 invocation can become one that needs level 2 (the user chose the SRD
  rule over a simpler slot rule, 2026-10-14). Taking Pact of the Tome or a
  Lessons of the First Ones Magic Initiate this way asks for its spells on the
  spell page; replacing one drops the spells it brought.
- Agonizing Blast, Eldritch Spear and Repelling Blast need Eldritch Blast, so a
  level-up that swaps Eldritch Blast away while keeping one of them is refused
  with that reason.
- An invocation cannot be both swapped in and picked as one of the level's new
  invocations; the Replace list leaves out the ones picked.
- Saves record each swap in the level's advancement (campaign format 29).

## Verification

`opengold_level_swap_tests` (each class's offers, each swap's effect, a
refused swap, Pact of the Tome in and out, save replay) and
`opengold_godot_level_up_lab` (a Sorcerer's cantrip swap through the window,
seen again in the next level's list).
