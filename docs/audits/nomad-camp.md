# Nomad camp audit (2026-10-24)

What playing the nomad camp needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL7.DAX 17` and the
Central Wilderness's `ECL7.DAX 26` (the way in), `opengold_creatures GAME`,
`opengold_maps GAME`, coab's `CMD_Program` and `CMD_EclClock`, and SRD 5.2.1.

## The area

- **Map:** `GEO7.DAX:17`, Hassad Ahwal's camp of tents and huts north of the
  city, with the wilderness's wall banks `LOAD PIECES 7,8,23`. One script,
  `ECL7.DAX:17`: the entry (`0x9ED7`) loads the map and sets the party at
  (1,14) facing east.
- **The way in:** the wilderness map's (12,11) offers to enter the camp.
- **The way out** (the step entry `0x9DE7`): walking off the map asks whether
  to go back into the wilderness; the script's own departures (`0xA1C2`) send
  the party back to (12,11) on the wilderness map.
- **The camp's state** (`0x4A7C`): 1 the party accepted the feast, 2 it is a
  traitor, 4 the camp turned on it, 8 the story is over; `0x4A7D` counts the
  nights (1 lodged, 2 the morning after, 3 staying to fight); `0x4A7E` counts
  the kobold battle's phases. The script reads the hour (`0x49C9`) throughout,
  and sets it: the feast lasts until hour 18.
- **The story:**
  - **The meeting** (the perimeter, event 1): by day hidden sentries give word,
    and the nomads wait for the party's first move (the encounter menu); by
    night a trip wire rings bells. Fought, the nomads swell into a battle and
    the headman rallies the tribe (`LOAD MONSTER` nomads, guards, Hassad and
    the shaman).
  - **The headman** (`0xA393`) offers a feast tonight (or tomorrow); declined,
    he asks the party to leave, and insulted he attacks.
  - **The feast:** a day of contests, the feast, Hassad's talk (journal entry
    55), his fear of a kobold band, and lodging in a hut; the script walks the
    party there (`0x9CC7`, a step of the clock each square) and opens the
    camp menu (`PROGRAM 9`).
  - **The night:** the camp's interruption checks are set for the hour the
    script wants (`0x9A12`), and the interrupt brings the headman back: stay
    and fight, or leave.
  - **The kobolds** come the next evening, in phases, one each time the party
    steps or rests: a horde attacking the camp, kobolds in melee with nomads,
    and the leader's huge group, which the headman asks the party to finish
    with him. Fled, the headman charges in with his guards and pays a reward
    "equal to your honor" (500 gold pieces); fought beside him, the shaman pays
    the promised one (5,000 gold pieces and item list 37), and the nomads ask
    the party to leave them to their mourning.
  - **Treachery:** wandering by night past the guard and stabbing him turns
    the camp, and the kobolds join the nomads against the party.

## Fights

| Fight | `LOAD MONSTER` (MON7CHA record × count) |
| --- | --- |
| The meeting fought | Nomad (40) × by the party's strength |
| The tribe rallied | 4th Level Fighter (41) × 5, Hassad (42), Shaman (43), Nomad (40) × 49 |
| The kobold horde | Nomads (40) and kobolds (0, 123) × by the party's strength |
| The leader's group | Hassad (42), 4th Level Fighter (41) × 2, kobolds (0) × 27, (123) × 26 |
| Treachery | 4th Level Fighter (41) × 5, Hassad, Shaman, kobolds (123) × 49 |

## Monster conversions

| Record | Original | Conversion |
| --- | --- | --- |
| 0, 123 | Kobold | `slums-kobold` |
| 40 | Nomad (level-1 fighter, HP 6) | the Bandit with 6 Hit Points, as on the wilderness map |
| 41 | 4th Level Fighter (HP 30) | **new:** SRD 5.2.1 Bandit Captain with 30 Hit Points, without its Pistol; its Parry since 2026-10-25 |
| 42 | Hassad (level-6 fighter, HP 45, AC 2) | **new:** SRD 5.2.1 Warrior Veteran with 45 Hit Points; its Parry since 2026-10-25 |
| 43 | Shaman (level-5 magic-user, HP 20) | **new:** the Slums magic-user's Wizard profile at level 4 with 20 Hit Points |

## Engine support

Missing before this audit, now done:

1. **The area row** and its conversions; the wilderness map already ran
   `NEW ECL 17`.
2. **`ECL CLOCK`**: time passes as for its operand's steps.
3. **A clock the script sets:** a script's write to the clock cells moves the
   campaign clock on to that hour and minute, as the original's cells were its
   clock.
4. **`PROGRAM 9` outside an inn** opens the camp menu and ends the script
   (coab: TryEncamp, then exit). The game view shows its camp dialog; the
   camp's checks use the interruption the script set.
5. **The location label** names the areas added since Kuto's Well.

## Party level and balance

A level-5 party survives the kobold battle at the default encounter challenge;
the huge groups are trimmed to the party's budget.

## Status

- **Increment 1 (done, 2026-10-24):** all of the above, by
  [NOMAD-1](../SRD-DECISIONS.md#nomad-1-2026-10-24-the-nomad-camp). Played
  (`opengold_expedition_tests`, a new level-5 party from the harbor's western
  boat): the way to (12,11) across the wilderness, the nomads met in peace,
  the feast until evening, the lodging and the camp it opens, the headman's
  midnight request, the kobolds the next evening, the reward, and the way
  back out to the wilderness map.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)).
