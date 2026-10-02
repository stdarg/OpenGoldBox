# New Phlan exploration demo

The [party preview](PARTY.md) can inject created PCs/NPCs into this same native
host, including shared purses/inventory, WHO and character/party queries.
The standalone launcher retains the single-fighter fixture described below.

`demos\review-rolf.cmd` runs the C++/Godot Rolf tour followed by free exploration of
the complete 16 x 16 New Phlan map, including building locations. The current
scope is the civilized town; ruined districts and boat destinations are outside
this host. A fixed level-1 fighter starts with 12/12 HP, 9,999 gold pieces and
an empty inventory. Combat rules remain the separate SRD 5.2.1 module.

## Playing

From CMD in the repository root, build once with `demos\build-rolf.cmd`, then run
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
main-loop detail. Recovery advances the campaign clock and ECL time fields;
ordinary movement and combat do not yet advance time.

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

| Shop | Example map position | `ITEM3.DAX` record | Stock entries |
| --- | --- | --- | --- |
| General supplies | (15,8) | 54 | 7 |
| Jewelry | (8,10) | 52 | 11 |
| Arms and armor | (13,8) | 53 | 57 |
| Silver items | (11,10) | 55 | 13 |

Original TREASURE instructions select these lists. The COMBAT command opens a
shop when script register `6E6C` is set. Closing resets that flag and resumes the
pending instruction. Each purchase charges the decoded item's gold value and
adds its stored item/bundle to inventory. Stock may be purchased repeatedly.
The demo has a 16-entry inventory limit and rejects invalid/stale requests,
unaffordable purchases and purchases when full without charging gold.
Selling, equipping and item activation are not implemented. Scripts that ask
for particular coins make change; see [coin payments](#coin-payments).

### Equipment conversion in the game

In the game, a bought item converts to SRD equipment by its original type
(`equipment_conversion`, `campaign_party.cpp`). Since [#18](https://github.com/stdarg/OpenGold/issues/18)
([SHOP-1](SRD-DECISIONS.md#shop-1-2026-10-02-shop-armor-and-unusable-stock))
every armor the arms shop sells converts by name: Padded, Studded Leather, Ring
Mail, Scale Mail, Splint Mail and Plate Mail (Silver Plate Mail too). SRD 5.2.1
has no Banded Mail, so it becomes Splint, the SRD heavy armor with the same
AD&D AC 4. Magic or cursed items keep their unsupported original record.

Stock with no conversion (jewelry, holy symbols, mirrors, oil, holy water) is
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
- Harbor: `'ROUND TRIP PASSAGE IS 1 PLATINUM PIECE.'` (travel itself is unsupported).
- Gambling: the wager is asked in platinum; the script copies `[6BC3]` to a
  scratch cell and writes the result back to `[6BC3]`.

A new party carries gold and silver, so taken literally these scripts could
never be paid. Approved policy (issue #17), **make change automatically**:

- The script sees, in each coin cell (`6BBB` cp, `6BBD` sp, `6BBF` ep, `6BC1` gp,
  `6BC3` pp), what the character's whole coin purse can afford at SRD rates
  (1 / 10 / 50 / 100 / 1,000 cp). 145 gp and 96 sp show as 15 pp, 154 gp, and so on.
- When the script stores the character, the change from that view is applied to
  the real purse exactly once. Coins the script adds are added as given. Coins it
  takes come from that denomination first; the rest is paid from the largest
  coins that do not overpay, then by breaking one larger coin, with change back
  in gold, silver and copper. Afterwards the script is shown the settled purse.
- Gems and jewelry (cells `6BC5`, `6BC7`) are not coins and are never converted.
- Prices are unchanged and no money is granted. A script that takes more than
  the purse is worth fails and the event rolls back.
- The exchange is logged under the script's text, for example
  "Arden pays 10 gp for 1 pp." (localized).

Reads that change nothing leave the purse alone. Shops price in gold and still
charge gold coins. Robbery and pickpocketing remain unsupported and roll back.
Native coverage: `tests/expedition_tests.cpp` (exchange rules, one charge across
repeated reads, and the installed inn route: decline, refusal, payment, rest).

## Validation and remaining work

Native synthetic tests cover buying, insufficient funds, capacity, stale
requests, replay, VM purse synchronization, unsupported-event rollback and
picture bounds/palette. With `OPENGOLD_GAME_DIR` set, the suite attempts routes
to every numbered cell. The current peaceful-choice run reaches 56 event cells
covering 30 distinct event IDs and all three town programs, and separately buys
an affordable item from each of the four shop types. Restricted routes and
unsupported branches are reported, not counted as successful execution.

The Godot check completes the tour, uses movement/button callbacks to reach
the arms shop, purchases a shield, opens inventory and leaves the shop:

```cmd
demos\review-rolf.cmd --headless -- --town-check
demos\review-rolf.cmd -- --town-check --capture
```

Captures are written under ignored `user-data/phlan-shop.png` and
`user-data/phlan-inventory.png`. Tested at 960 x 720 and 1280 x 900.

Unimplemented branches include town combat/duels and their party-strength
queries, training, temple spells beyond Cure Wounds, short rests and general
rest interruptions, robbery, recruitment,
non-shop treasure item lists and random items, and external travel. Script money awards follow [QUESTS.md](QUESTS.md#reward). The shared party flow supports [idle-town campaign saves](SAVES.md); pending events remain unsavable. Campaign
flags affect which dialogue branches can be reached; this test is not exhaustive
coverage of all choices, quest states, random outcomes, or camp interruptions.
