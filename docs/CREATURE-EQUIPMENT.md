# Creature equipment

Rules sometimes need to know what a monster carries. Heat Metal, for one, targets
only a creature wearing metal armor
([#231](https://github.com/stdarg/OpenGold/issues/231)). Player characters are
judged from their actual equipment; monsters from `equipment` rows in
[combat.rules](../data/rules/srd-5.2.1/combat.rules), next to their stat lines:

```
equipment slums-orc-leader metal_armor MON2CHA.DAX:5 chain_mail
```

The row names the combat creature, the fact (`metal_armor`: Medium or Heavy
armor other than Hide, the rule Heat Metal uses), the original record it comes
from and the armor. A creature without a row wears no metal armor. The loader
rejects a row for an unknown creature, an unknown fact, a repeated creature or
missing or extra fields. Only facts are stored; no original data is copied.

## Deriving the rows

The facts come from the armor each creature's original record readies, which
is also the armor its authored stat line assumes (a leader is its base stat
block wearing that armor). `opengold_creatures` prints the rows from the
player's install for review:

```sh
build/default/opengold_creatures /path/to/POOLRAD --equipment
```

It converts each record's readied armor with the game's own item conversion
(ignoring magic bonuses) and prints a row when that armor is metal, or a
comment otherwise:

| Creature | Record | Armor | Metal |
| --- | --- | --- | --- |
| slums-kobold | MON2CHA.DAX:0 | none | no |
| slums-kobold-leader, -sword | MON2CHA.DAX:1, 11 | studded leather | no |
| slums-goblin | MON2CHA.DAX:2 | studded leather | no |
| slums-goblin-leader | MON2CHA.DAX:3 | scale mail | yes |
| slums-orc | MON2CHA.DAX:4 | none | no |
| slums-orc-leader, -archer | MON2CHA.DAX:5 | chain mail | yes |
| slums-bugbear | MON2CHA.DAX:63 | none | no |

The training profiles (bandit, vanguard, scout, adept, healer) have no original
record and no rows. Encounter art is a tie-breaker only where a record and its
art disagree; none do yet.
