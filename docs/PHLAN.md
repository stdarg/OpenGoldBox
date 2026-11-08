# New Phlan exploration demo

The [party preview](PARTY.md) can inject created PCs/NPCs into this same native
host, including the party purse, inventories, WHO and character/party queries.
The standalone launcher retains the single-fighter fixture described below.

`demos\review-rolf.cmd` runs the C++/Godot Rolf tour followed by free exploration of
the complete 16 x 16 New Phlan map, including building locations. The current
scope is the civilized town; ruined districts and boat destinations are outside
this host. A fixed level-1 fighter starts with 12/12 HP, 9,999 gold pieces and
an empty inventory. Combat rules remain the separate SRD 5.2.1 module.

## Playing

From CMD in the repository root, build once with `build-opengoldbox.cmd`, then run
`demos\review-rolf.cmd`. Existing game-directory configuration is unchanged.

- Complete Rolf's eight dialogue pauses with Continue/Enter.
- Arrow keys or the turn/step buttons move the party; stepping crosses ordinary
  doors. Solid walls, locks, and script-imposed restrictions still apply.
- Look (L) runs the search entry at the current position and facing.
- Camp (C) runs the original pre-camp check. The city watch interrupts street
  camping; the original inn offers safe paid rest. See [recovery limits](RECOVERY.md).
- Select an original dialogue choice, then Choose/Enter. Numeric/string prompts
  accept a typed answer followed by Submit/Enter.
- Accept a shopkeeper's offer, select merchandise, and Buy/Enter. The list
  scrolls. Leave shop resumes the original script and restores movement.
- Inventory shows purchased items and gold. Replay tour resets the whole demo,
  including purchases, flags and money. Closing the application does not save.

## Script and state boundary

The shared, immutable resource profile contains `GEO3.DAX:0`, the verified Phlan
wall bank, and `ECL3.DAX` records 0 (town), 8 (City Hall) and 11 (training hall).
Scripts themselves request the building transitions; coordinates are not used
to replace original dialogue or choose a handcrafted event.

The host schedules slot 0 before resolving a forward step, then slot 1 after
resolution. Turning and Look invoke slot 1. `NEW ECL` preserves campaign state,
clears area-local flags through the VM and schedules slot 4 followed by slot 1.
Camp exposes the pre-camp entry and the supported interruption/recovery paths.
This is a bounded adapter policy, not a verified reproduction of every DOS
main-loop detail. Recovery advances the campaign clock and ECL time fields. In
the game, a forward step also advances six seconds and combat advances the
clock by its elapsed time ([status effects](STATUS-EFFECTS.md)); this standalone
tour advances time only for recovery.

Logical campaign/local/register/scratch ranges are initialized explicitly.
LOAD CHARACTER and WHO expose the single fighter and empty remaining party
slots. Known name, status, HP, Constitution and purse fields are mapped; this
is not a complete character-record or SRD character-sheet adapter. Inventory
and purse use native value ownership. The script character view is synchronized
at character switches, event completion and shop exit. While shopping, the VM
is suspended and the native purse owns transactions.

Each location invocation keeps a native checkpoint. Unsupported engine services
produce a visible notice and a retained diagnostic, restore script/party state,
and allow Continue back to exploration. They are not reported as successful
combat, training, healing or quest completion. Original scripts can still deny
entry or move the party out of restricted rooms.

## Original merchandise and artwork

The [original cluebook's New Phlan map](https://www.mocagh.org/ssi/pool-hintbook.pdf)
(printed page 3) marks twelve physical shops. Each has its own record in
[`data/shops`](../data/shops/README.md), which the Shop & Temple Manager edits:
its shopkeeper portrait, its stock and its prices. The records start from the
original merchandise lists, so shops of the same type sell the same items.
The arms shop at (9,13) has a second door at (8,13), into the same room, and
both doors open its record. The cluebook supplies shop types and locations,
not the shopkeepers' identities or appearances, so these portraits are original
character designs.

| Shop | Map position | `ITEM3.DAX` record | Stock entries | Portrait basename |
| --- | --- | ---: | ---: | --- |
| Arms and armor | (13,8) | 53 | 55 | `phlan-arms-13-08.png` |
| Arms and armor | (8,11) | 53 | 55 | `phlan-arms-08-11.png` |
| Arms and armor | (11,12) | 53 | 55 | `phlan-arms-11-12.png` |
| Arms and armor | (9,13), (8,13) | 53 | 55 | `phlan-arms-09-13.png` |
| General items | (15,8) | 54 | 18 | `phlan-general-15-08.png` |
| General items | (9,10) | 54 | 18 | `phlan-general-09-10.png` |
| General items | (12,10) | 54 | 18 | `phlan-general-12-10.png` |
| General items | (9,11) | 54 | 18 | `phlan-general-09-11.png` |
| General items | (11,11) | 54 | 18 | `phlan-general-11-11.png` |
| Silver items | (11,10) | 55 | 11 | `phlan-silver-11-10.png` |
| Silver items | (10,13) | 55 | 11 | `phlan-silver-10-13.png` |
| Jewelry | (8,10) | 52 | 11 | `phlan-jeweler-08-10.png` |

A general store's 18 entries are the original list's 7 and the 11 the game
adds (below). The arms and silver shops leave out the original's arrows and
quarrels, as the game has no ammunition
([AMMO-UNLIMITED](SRD-DECISIONS.md#unlimited-ranged-ammunition--approved-exception)). The portrait files are under `art/portraits/NPCs/`; a record may
also name `shops/<file>`, a portrait made for the manager that it exports to
`data/art/portraits/shops/`, which the game ships. The shopkeeper fills the
existing view window for the shop's greeting and throughout the shopping phase,
including stock selection and purchases; leaving the shop restores the
exploration view. Ordinary shop keys remain available while the portrait is
shown.

Original TREASURE instructions select these lists. The COMBAT command opens a
shop when script register `6E6C` is set; the game then sells what the record
for the shop's square lists instead, at the record's prices, leaving out items
whose stock is 0. A shop with no record sells the original list at the original
values. Closing resets that flag and resumes the pending instruction. Each
purchase charges the item's price and adds its stored item/bundle to inventory.
Stock may be purchased repeatedly: the game keeps no stock limits yet.
The demo has a 16-entry inventory limit and rejects invalid/stale requests,
unaffordable purchases and purchases when full without charging gold.
The inventory's Sell, shown in a shop, sells any item but a cursed one for half
what the shops sell it for, bargained by Charisma
([the sale price](SRD-DECISIONS.md#the-sale-price)), and Drop, anywhere outside a fight, throws it away
([SELL-1](SRD-DECISIONS.md#sell-1-2026-11-05-selling-and-dropping-items)); see
[party](PARTY.md) for equipping and using items.
What the chosen buyer cannot use fully is listed in dark red, with a note
that it can still be bought; what the purse cannot pay for in yellow; and
armor or a shield that would not raise the buyer's Armor Class in gray
([party](PARTY.md)). Scripts that ask
for particular coins make change; see [coin payments](#coin-payments).

### Equipment conversion in the game

In the game, a bought item converts to SRD equipment by its original type
(`equipment_conversion`, `campaign_party.cpp`). Since [#18](https://github.com/stdarg/OpenGoldBox/issues/18)
([SHOP-1](SRD-DECISIONS.md#shop-1-2026-10-02-shop-armor-and-unusable-stock))
every armor the arms shop sells converts by name: Padded, Studded Leather, Ring
Mail, Scale Mail, Splint Mail and Plate Mail (Silver Plate Mail too). SRD 5.2.1
has no Banded Mail, so it becomes Splint, the SRD heavy armor with the same
AD&D AC 4. Magic and cursed items convert with their enchantment or curse
([magic items](MAGIC_ITEMS.md#in-opengoldbox)).

Each general store sells the original's Flask of Oil, which converts to SRD
Oil, and items its record adds from the SRD's adventuring gear: Torches (ten
for 1 gp, as prices are whole gold), a Vial of Acid (25 gp), a Flask of
Alchemist's Fire (50 gp) and Magic Ink of levels 1 to 8 (1,000 gp a level); see
[GEAR-1](SRD-DECISIONS.md#gear-1-2026-10-08-fire-and-acid-for-every-party).
Every starting kit also carries a Torch.

Stock with no conversion (jewelry, holy symbols, mirrors, holy water) is
still for sale, and its entry reads "{item} / {price} gp / cannot be equipped"
before anything is spent. It keeps its original record, as treasure does.
`opengold_expedition_tests` checks the conversions, a Cleric buying and wearing
Scale Mail, and, with original files, that the whole arms shop converts.
`opengold_godot_shop_disclosure` opens the jeweler in the game in English and
Spanish and reads the marked list.

HEAD3/BODY3 records provide 88 x 40 heads and 88 x 48 bodies, combined according
to the script's portrait selection. PIC3 and SPRIT3 supply supported pictures
and encounter sprites. All images and items load from the user's installation;
none of the original bytes, pictures or text are bundled in this repository.

Mapped registers and shop/treasure dispatch were cross-checked with the
[PC 1.3 technical analysis](https://gamefaqs.gamespot.com/c64/578753-pool-of-radiance/faqs/73869),
then exercised against the installed ECL and item records. Dialogue, stock
counts and item prices are taken from those records, not copied from the guide.

## Coin payments

Original New Phlan scripts test and take one denomination from the selected
character. `ECL3.DAX` record 0, as listed by
`opengold_scripts --inspect GAME_DIR ECL3.DAX 0`:

- Inn: `'IT WILL COST YOU 1 PLATINUM PIECE TO REST HERE.'`, then
  `COMPARE 1, [6BC3]` / `IF >` to `"YOU DON'T HAVE ENOUGH PLATINUM."`, otherwise
  `SUBTRACT 1 FROM [6BC3]`, `LOAD CHARACTER 128 + [6DB4]` (store) and PROGRAM 9.
- Harbor: `'ROUND TRIP PASSAGE IS 1 PLATINUM PIECE.'`
- Gambling: the wager is asked in platinum; the script copies `[6BC3]` to a
  scratch cell and writes the result back to `[6BC3]`.

The party owns all its money in one purse ([#232](https://github.com/stdarg/OpenGoldBox/issues/232),
a [deviation](DEVIATIONS.md#money) from the original's purse per character).
Every member a script reads shows the party purse, and every payment comes from
it.

A new party carries gold and silver, so taken literally these scripts could
never be paid. Approved policy (issue #17), **make change automatically**:

- The script sees, in each coin cell (`6BBB` cp, `6BBD` sp, `6BBF` ep, `6BC1` gp,
  `6BC3` pp), what the party's whole coin purse can afford at SRD rates
  (1 / 10 / 50 / 100 / 1,000 cp), up to 65,535 of each, the most a cell holds.
  145 gp and 96 sp show as 15 pp, 154 gp, and so on.
- When the script stores the character, the change from that view is applied to
  the real purse exactly once. Coins the script adds are added as given. Coins it
  takes come from that denomination first; the rest is paid from the largest
  coins that do not overpay, then by breaking one larger coin, with change back
  in gold, silver and copper. Afterwards the script is shown the settled purse.
- Gems and jewelry (cells `6BC5`, `6BC7`) are not coins and are never converted.
  The script's change to them is applied to the purse.
- Prices are unchanged and no money is granted. A script that takes more than
  the purse is worth fails and the event rolls back.
- The exchange is logged under the script's text, for example
  "Arden pays 10 gp for 1 pp." (localized), naming the member the script read.

Reads that change nothing leave the purse alone.

Every price is in gold and takes any coin (the user's decision, 2026-11-04):
shops, temples, the training halls, identifying and offerings pay with gold
when the party's gold covers the price, otherwise with the largest coins that
do not overpay and then one larger coin, with change in gold, silver and
copper (`pay_gold`, `coin_purse.h`). The town shows the party's money as what
all its coins are worth in gold, so a party holding Ohlo's 150 pp shows
1,500 gp more and can spend it. The item goes to the chosen buyer. Robbery other than a share of the coins remains unsupported and rolls back
([ECL-COVERAGE.md](ECL-COVERAGE.md)).
Native coverage: `tests/expedition_tests.cpp` (exchange rules, one charge across
repeated reads, and the installed inn route: decline, refusal, payment, rest).

## Fights in New Phlan

New Phlan's own fights use its `MON3CHA` records, converted in
[`conversions.json`](../data/encounters/README.md) (user's decision, 2026-11-04,
each record as it fights elsewhere in the game):

| Fight | Record | Original | Fights as |
| --- | ---: | --- | --- |
| The city watch, when a camp in the street stays | 94 | Level 3 MU | `slums-magic-user` |
| | 84 | 6th level fighter | `trapped-fighter` |
| | 53 | Aides | `outpost-guard` |
| | 40 | Nomad | `wilderness-bandit` |
| Tempus's priests, against a party that fought the watch | 83 | 7th level cleric | `bane-priest` |
| | 41 | 4th level fighter | `nomad-guard` |
| | 85 | 8th level fighter | `castle-knight` |

As every scripted fight, each is sized to the party. After the watch's fight
the city's temples and shops refuse the party all service, and the watch hunts
it in the streets (`opengold_expedition_tests` `city_watch_fight`).

## Validation and remaining work

Native synthetic tests cover buying, insufficient funds, capacity, stale
requests, replay, VM purse synchronization, unsupported-event rollback and
picture bounds/palette. With `OPENGOLD_GAME_DIR` set, the suite attempts routes
to every numbered cell. The current peaceful-choice run reaches 56 event cells
covering 30 distinct event IDs and all three town programs, and separately buys
an affordable item from each of the four shop types. Restricted routes and
unsupported branches are reported, not counted as successful execution.

`opengold_godot_town_services` goes through every door of New Phlan's shops,
temples and inns on the game's town screen, from saves of a party one step
outside each (`tests/town_services_tests.gd`). Each shop must list its
`data/shops` record and sell every item at its price to the buyer; each temple
must list its record's services and prices and offer each only to the members
it helps (wounded, Blinded, Poisoned, diseased, cursed, Petrified, dead, dust),
charging once paid and nothing on Back; each inn must charge one platinum a
night, restore the party on the first and say the second gives no Long Rest.
Parties too poor are refused with nothing spent, parties paying in platinum
alone are served with change, a party that fought the city watch is refused
service (Tempus's priests attack it instead), and staying when the watch
rousts a camp starts the watch's fight on the combat screen. Three inn squares are inner rooms
with no street door and are not entered. On Hard, a temple offers no
Resurrection and its Raise Dead list names those it cannot help
(`opengold_expedition_tests`).

`opengold_godot_slums_events` does the same for the Slums (ECL2, area 20)
with a level-4 party (`tests/area_events_tests.gd`). It loads a save from one
step before each event square, one for each facing, then steps in and takes
every branch, loading the save again for each: every entry of a choice list,
the right word and a wrong one for a typed answer, 0 and 100 for a number. A
branch ends back in the street, in a fight on the combat screen or in a shop.
After a street ending it looks around and then steps out and back in, so an
event that pays twice is caught. The first fight of each event is played on
Quick until it ends. The check fails on a "not supported" notice, on a dead end
(nothing to press, or a pause longer than ten seconds) or on a fight that does
not end. The suite explores every eighth save (`OPENGOLD_SLUMS_EVENT_SAMPLE`
in `tests/expedition_fixtures.cmake`); for all of them, write the saves with
`OPENGOLD_SLUMS_EVENT_FIXTURES` set and run the script on that folder. Two
saves start Ohlo's quest midway: one with the commission accepted and one with
his potion found.

The town screen joins printed pieces of text so the words read as written: a
space is added where two pieces would run words together
("LEADING" then "A PARTY OF"), and a doubled space after a word is closed up
("SEEDY-LOOKING  MAN"). Two spaces after a full stop are kept
(`join_script_text`).

The Godot check completes the tour, uses movement/button callbacks to reach
the arms shop, purchases a shield, opens inventory, has the shop identify it,
makes the member a Bard who casts Detect Magic from the Cast menu and Identify
from the inventory, and leaves the shop:

```cmd
demos\review-rolf.cmd --headless -- --town-check
demos\review-rolf.cmd -- --town-check --capture
```

Captures are written under ignored `user-data/phlan-shop.png`,
`user-data/phlan-inventory.png`, `phlan-cast.png`, `phlan-cast-done.png`,
`phlan-detect.png` and `phlan-identify.png`. Tested at 960 x 720 and 1280 x 900.

Town duels (the training hall's arena master pits a member against a copy of
itself, as the original did) and party-strength queries, training, short rests and rest
interruptions, robbery (coins only), recruitment, script treasure and travel
beyond the town now run; [ECL-COVERAGE.md](ECL-COVERAGE.md) lists the variants
each still refuses. Script money awards follow [QUESTS.md](QUESTS.md#reward). The shared party flow supports [idle-town campaign saves](SAVES.md); pending events remain unsavable. Campaign
flags affect which dialogue branches can be reached; this test is not exhaustive
coverage of all choices, quest states, random outcomes, or camp interruptions.
