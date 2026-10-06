# Druid

Levels 1–4 of the SRD 5.2.1 Druid (pp. 41–46). Wisdom is the spellcasting
ability.

## Spellcasting, Primal Order and the Circle of the Land

- Druids prepare spells from the Druid list like Clerics: two cantrips (three
  from level 4) and 4/5/6/7 prepared spells, with full-caster slots, chosen at
  creation and on gaining a level; a Long Rest may change them. The
  implemented Druid spells through level 2 are on the list. Replacing a
  cantrip on gaining a level is not offered yet.
- **Primal Order** (level 1), chosen with the Training controls:
  **Magician** learns one more Druid cantrip; **Warden** trains Martial
  weapons and Medium armor.
- **Produce Flame** (cantrip, Bonus Action, 10 minutes): a flame in hand,
  shown as a condition. While it lasts, "Hurl flame" is an Action (a Magic
  action, not a casting) making a ranged spell attack for 1d8 Fire within 60
  feet; the flame stays. Casting it again replaces it.
- **Shillelagh** (cantrip, Bonus Action, 1 minute): offered only while holding
  a Club or Quarterstaff. Melee attacks with it use the spellcasting ability
  for the attack and damage rolls and a d8. The weapon keeps its Bludgeoning
  damage; the Force alternative is not offered.
- **Barkskin** (level 2, Bonus Action, touch, 1 hour): the creature's AC is 17
  if it was lower.
- **Circle of the Land** (level 3), the SRD's only Druid subclass: the land
  type is a Training choice at level-up. Its Circle Spells are always
  prepared: Arid (Blur, Burning Hands, Fire Bolt), Polar (Fog Cloud, Hold
  Person, Ray of Frost), Temperate (Misty Step, Shocking Grasp, Sleep) or
  Tropical (Acid Splash, Ray of Sickness, Web). The land is not changed on a
  Long Rest.
- Druidic and Wild Companion are removed (SRD-DECISIONS). Wild Shape and
  Land's Aid follow in a later increment.
- Verification: `opengold_druid_tests` (Produce Flame and its hurl,
  Shillelagh's Wisdom attack bonus, Magician's cantrip and Warden's armor,
  the Arid Circle Spells and Fire Bolt, Barkskin's AC, a third cantrip at
  level 4), `opengold_spell_table_tests` (Druid probes).
