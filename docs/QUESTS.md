# Original quests

Each quest below is taken from the user's original files, as listed by
`opengold_scripts --inspect GAME_DIR ECL2.DAX 20` (sort its lines by address).
Addresses are instruction addresses in that record; flags are ECL cells.

## Ohlo's potion delivery

Issue [#14](https://github.com/stdarg/OpenGold/issues/14), recommended by the
[first-adventure audit](audits/issue13-first-adventure.md). Slums script
`ECL2.DAX:20`, map `GEO2.DAX:20`. Coordinates are zero-based (east +X, south +Y).

| Step | Original evidence | Outcome here |
| --- | --- | --- |
| Ohlo's door | West edge of `(14,10)` has door code 2 (locked). | [Locked door](#locked-doors): Bash or Exit. |
| Meet Ohlo | Search event 3, entry `0x9f13`, cells `(11..13,9..10)`; SETUP MONSTER record 24. | His picture and dialogue. |
| Accept | TALK, then PARLAY NICE or MEEK reaches the offer `0xa1bc`; ACCEPT THE COMMISSION writes `0x4A04=250` (`0xa251`) and puts the party outside at `(14,10)` facing east. | Supported. |
| Fetch the potion | Booth at `(15,12)`, search event 19, entry `0xae1e`, entered east from `(14,12)`. SPEAK, INPUT STRING `0xaf9f`, compared with `"OHLO"` (`0xafa5`); a match writes `0x4A81=250` (`0xb048`). The booth does not check the commission flag. | Supported. Typed text is upper case, as the original input routine returns it. |
| Hand in | Event 3 with `0x4A81=250` offers GIVE (`0xa29d`). GIVE: CLEAR MONSTERS `0xa38f`, TREASURE `0xa390` `[0,0,0,0,150,0,1,129]`, COMBAT `0xa3a1` with no monsters, then `0x4A04=255` and `0x4A81=255`. | [Reward](#reward) |
| Revisit | Event 3 exits when `0x4A04` or `0x4A81` is 255; the booth exits when `0x4A81 >= 250`. | Nothing repeats. |

`0x4A04` (commission), `0x4A19` and `0x4A1D` (booth state) are area-local:
entering the Slums clears `0x4A00..0x4A1F`. `0x4A81` is the persistent
quest state, so a completed quest stays complete after leaving and returning.

Alternative branches stay explicit. HAUGHTY or ABUSIVE parley, ATTACK, ASK FOR
MORE MONEY and REFUSE TO GIVE start fights with MON2CHA records 6, 5, 94 and 4;
attacking the booth uses records 6 and 7. Records 6, 7 and 94 have no creature
conversion, so those events report a diagnostic and roll back. SLY or
REJECT IT ends the conversation; a wrong word at the booth gets "I CANNOT HELP YOU".

### Route

From New Phlan `(0,4)` step west into the Slums at `(15,4)`. Walk to `(14,10)`,
face west and step; Bash until the door opens, stepping again after each failure.
Choose TALK, NICE, ACCEPT THE COMMISSION and continue; the party is back outside.
The booth cannot be reached without forcing more locks: the fewest are the west
doors of `(9,2)` and `(7,3)`, then through the Old Rope Guild to `(14,12)`. Step
east into the booth, SPEAK and type OHLO. Return to `(14,10)`, step west (the
door stays open during this visit) and choose GIVE. Leave by `(15,4)` east.

## Locked doors

GEO door codes are 1 (door), 2 (locked) and 3 (wizard locked); see
[MAPS.md](MAPS.md#door-codes). In the Gold Box engine, as reconstructed for
Curse of the Azure Bonds ([coab](https://github.com/simeonpilgrim/coab)
`engine/ovr015.cs`, `locked_door`), a step into a locked edge happens after the
pre-step script: the prompt "Locked." offers Bash, Pick (when a member has thief
skills), Knock (when a member has Knock memorized) and Exit. One try per step;
success unlocks both faces in the loaded map and moves the party. The
destination search then runs whether or not the party moved.

The conversion here:

- **Bash**: each conscious, living active member in party order makes a
  Strength (Athletics) check (SRD 5.2.1, advantage and disadvantage included)
  against DC 15 (Medium) until one succeeds. Rolls use the campaign service
  random state and each check is listed under "Locked.".
- **Exit**: the party stays; nothing is rolled.
- Pick is not offered (tool proficiencies were removed) and Knock is not
  supported. Code 3 stays blocked.
- A lock on either face of the edge offers the menu (the existing two-sided
  collision policy is kept). An opened door stays open until the party leaves
  the district or loads a save. If the destination's event fails, the move, the
  opened door and the rolls are rolled back with the rest of the event.

## Reward

TREASURE operands are copper, silver, electrum, gold, platinum, gems, jewelry
and an item code: below 128 an `ITEMn` list, 128 + n for n random items, 255 for
none ([Gold Box Explorer `CMD_Treasure`](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Commands.cs);
coab `CMD_Treasure`). Without loaded monsters, the original COMBAT only hands out
that treasure. Ohlo pays 150 pp, 1 jewelry and 1 random item.

- **Money** is awarded by that COMBAT through the same atomic loot service as
  encounter loot. Its stable identity is the script and instruction:
  `por:ECL2:20:treasure:a390:v1`. Recipients follow the loot policy: the first
  living active member in party order, overflowing to the next member.
- **XP**: none. The original grants experience only for defeated monsters and
  this script awards none.
- **Full purses** defer the whole award as pending loot, saved with the
  campaign and collected when a later event starts and there is room. It is
  never split or paid twice.
- **The random item** is not awarded and the dialogue says so. The PoR random
  item tables are not decoded and no item is invented.
- Claim, purses, pending loot and the ECL flags commit with the event. A later
  failure in the same event rolls all of them back. A script reaching an
  already-claimed TREASURE again fails explicitly instead of paying twice.
- Non-shop item lists and treasure added to a fight are still unsupported and
  roll the event back with a diagnostic.

## Validation and limits

The normal gate runs synthetic native tests in `tests/expedition_tests.cpp`:
Ohlo's reward shape with failure, rollback and retry; the refused repeat;
deferred collection across a save and reload; and Bash, Exit and rollback at a
locked door. `tests/ecl_tests.cpp` covers upper-case string input.

With `OPENGOLD_GAME_DIR` and `OPENGOLD_OHLO_ROUTE=1`, `opengold_expedition_tests`
also plays the route with the six created fighters after the inn rest, spending
the orcs' XP on level two, and revisits Ohlo and the booth after a fresh-process
reload. **This route currently fails**: roaming groups cannot always be fled.
On the first walk from the gate the party is surprised at `(14,7)` by four goblin
leaders and sixteen goblins and loses. Group size comes from PARTY STRENGTH
(24 for this party), whose conversion ([PARTY.md](PARTY.md)) treats an SRD
attack bonus of +5 as THAC0 15. With the result divided by six in a local,
uncommitted experiment, the full route, reward, reload and revisits passed.
Encounter balance needs a decision before the route can be part of the gate.
