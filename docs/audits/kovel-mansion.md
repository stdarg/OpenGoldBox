# Kovel Mansion audit (2026-10-24)

What playing Kovel Mansion needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL3.DAX 14` (and the
Central Wilderness's `ECL7.DAX 26` for the way in), `opengold_creatures GAME`,
`opengold_maps GAME`, coab's `CMD_CheckParty` and `CMD_Damage`, and SRD 5.2.1.

## The area

- **Map:** `GEO3.DAX:14`, the city block northeast of the old city where the
  thieves' guild has its house. One script, `ECL3.DAX:14`: wall banks `LOAD
  PIECES 1,3,5` before `LOAD FILES 14,2,255` (`0xB572`), so the banks are
  loaded while the area the party came from is still current.
- **The way in:** the wilderness map's city edges: the northern edge (12,26),
  (13,26) offers the city or the graveyard, and the city leads to (4,0) facing
  south; the eastern edge (13,27) offers north, south or the boat, and north
  leads to (15,4) facing west. New Phlan's harbor master sells passage to the
  bay, which lands at the eastern edge.
- **The way out** (the step entry `0x9914`): north to the wilderness at
  (13,26), east to (13,27), south to the Temple of Bane's street (`ECL1:24`,
  in the game since 2026-10-24); the west edge leads nowhere.
- **Camps** (`0xB4CC`): while the guild stands (`0x4AB2` below 254), three
  thieves with slings break up every camp: three sling bullets of 1d4+1.
- **Events** (the event byte's low seven bits index the jump table at
  `0x9991`):
  - **1, 2** (21 squares): a thief with a sword steps out of nowhere, while
    fewer than 21 thieves are beaten and not twice on the same event. On d100
    below 30 (35 once 12 are beaten) he backstabs a member (2d8, or 3d8
    once 12 are beaten) and vanishes through a wall; otherwise the party stops
    him and may attack him (a 1st-level thief, or a 6th-level one once 18
    are beaten) or let him go.
  - **3-8** (seven squares): trapped doorways: a scything blade (1d6), poison
    gas (1d3 to one member, save), a net of blades (1d6 to one member, save)
    and three more blades. Each springs once.
  - **9** (10,9): loot, item list 56.
  - **10** (14,10): three cabinets. A is locked (pick, break, cast Knock):
    files for journal entries 23 and 14. B is locked: 1,500 silver pieces. C
    may be trapped (a needle, 1d8) and is locked: 1,800 gold pieces.
  - **11** (14,14): rugs and tapestries, item list 24.
  - **12** (3,13): a wooden cabinet, maybe trapped (yellow gas, 2d6 to
    everyone, save): scrolls, item list 25.
  - **13** (6,7): four caskets, each maybe trapped (a poisoned needle, a dart,
    a noxious powder over everyone, a shower of twelve darts): small gems,
    jewelry, fine gems and pearls.
  - **14** (3,4): weapons and shields, item list 27. Taking them, unless the
    party searched and disarmed the pile, sets off a stunning gas and a group
    of thieves sized by those beaten so far (`0xAA1E`); once 18 are beaten
    the gas only stinks.
  - **15** (6,5): two thieves flee east. Chasing them, the script walks the
    party through the house (`0xACD7`): they split south and east, and the
    one caught claims to be the guild master. Attacked, he fights; released,
    he runs.
  - **16** (8,13), (7,14): an ambush by 15 thieves and 3 6th-level thieves,
    once; with more than 15 beaten, the guild master is cornered there
    instead.
  - **17** (8,14): files for journal entries 38 and 51.
  - **18** (10,12): the jail, where ten young thugs beat an old thief to death.
    Beaten, the old thief gives his papers (journal entry 48) and dies.
  - **19** (5,14): a slate with the northwest castle's maze (journal entry 41).
  - **20** (4,5): a map of the northeast castle's maze (journal entry 29).
- **Traps** (`0xB3A2`): before each trap the party may see it, by Find Traps
  in effect or on d100 below the party's best chance to find traps; then it
  may avoid the trap or disarm it, on d100 below that same chance.
- **The guild beaten:** with 21 thieves beaten, the guild no longer troubles
  the party's camps (`0x4AB2` = 254 at `0xB359`).

## Fights

| Fight | `LOAD MONSTER` (MON3CHA record × count) |
| --- | --- |
| A thief stopped | 1st Level Thief (45) × 1, or 6th Level Thief (51) once 18 are beaten |
| The weapons' gas | 6th Level Thieves (51) and 1st Level Thieves (45) by those beaten |
| The ambush | 1st Level Thief (45) × 15, 6th Level Thief (51) × 3 |
| The jail | 1st Level Thief (45) × 10 |

## Monster conversions

| Record | Original | Conversion |
| --- | --- | --- |
| 45 | 1st Level Thief (HP 4, long sword, leather) | **new:** SRD 5.2.1 Bandit with 4 Hit Points |
| 51 | 6th Level Thief (HP 28, long sword, leather) | **new:** SRD 5.2.1 Spy with 28 Hit Points: Shortsword or Hand Crossbow with 2d6 Poison, Cunning Action |

## Engine support

Missing before this audit, now done:

1. **The area row** and its conversions; the wilderness's city edges already
   ran `NEW ECL 14`.
2. **`LOAD PIECES` before `LOAD FILES`**: the banks are checked against the
   script's own district, not the area the party came from.
3. **`CHECK PARTY`'s other forms** (coab): the lowest, highest and average of a
   thief skill (the character record's bytes `0xA5`-`0xAC`), and, for
   attribute 0, whether a spell effect is on the party (effect 19, Find
   Traps). The script reads byte `0xA7` to find and remove traps and byte
   `0xA5` after "who will pick the lock?"; both are a Rogue's Thieves' Tools
   chance (KOVEL-1), and the record's bytes carry it.
4. **`SPELL`**, which the original PC game skipped: asked for Knock (31), it
   names a member who can cast Knock, who spends it.
5. **`DAMAGE`'s whole-party form** (`0xC0`): every member saves or takes the
   dice. Volleys take their damage type from the call (blades, darts, sling
   bullets), arrows otherwise.
6. **A monster's Cunning Action** (`cunning_action`), for the Spy.

## Party level and balance

The guild's fights are many small ones and one large ambush; a level-5 party
clears the house at the default encounter challenge, and the ambush is trimmed
to the party's budget. Camping is impossible until the guild is beaten.

## Status

- **Increment 1 (done, 2026-10-24):** all of the above, by
  [KOVEL-1](../SRD-DECISIONS.md#kovel-1-2026-10-24-kovel-mansion). Played
  (`opengold_expedition_tests`, a new level-5 party with a Rogue and a wizard
  with Knock): the harbor's boat to the bay and the city's eastern edge, a save
  in the house, a camp broken up by the slingers, a trapped doorway, the three
  cabinets (Knock, force and their coins), the jail, the chase and the guild
  master, the ambush, the weapons' gas, the guild beaten, and the way out north
  onto the wilderness map.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)). The
  cabinets are now opened after the guild is beaten, when the wizard can camp
  for Knock: earlier fights may spend it.
