# Shop & Temple Manager

This standalone Godot 4 tool edits the existing merchandise shops and service
temples. It does not move, create, or delete locations, and it does not alter
the game application. Its compact overhead map is a selector; the larger panel
edits one location's offerings, prices, quantities or service availability,
type (shops only), and portrait.

Choose **New Phlan** or **Podol Plaza** in the map dropdown. Click a colored
marker or use the location list. The map contains twelve New Phlan shops, three
New Phlan temples, and the hidden Temple of Ilmater in Podol Plaza. Multiple
entrances to one temple select the same record. Green markers are shops and
violet crosses are temples. The walls and doors come from locally decoded
`GEO*.DAX` files; the fixed locations are recorded in
[`data/shops/locations.json`](../../data/shops/locations.json). There are no
position controls.

All twelve shops start with stock decoded from `ITEM3.DAX` records 52–55. The
general shops also start with the current game's added gear: ten Torches, Acid,
Alchemist's Fire, and eight levels of Magic Ink priced at 1,000 gp per level.
Every shop has an independent
JSON record, even if it shares an original stock list with other shops. Use
**Find an original item to add**, **Add item**, **Remove**, **Price (gp)**, and
**Stock** to change it. Stock `-1` means unlimited, matching the original shop
behavior; `0` means out of stock. The item picker can search decoded item lists
from `ITEM1.DAX` through `ITEM8.DAX` and keeps each added item's source identity.
It also offers the game's added gear, and the Poisoner's Kit (50 gp) and Basic
Poison (100 gp), which no shop sells by default
([#271](https://github.com/stdarg/OpenGold/issues/271)).

The four temples each have the nine original curative services and
OpenGoldBox's Resurrection, which the game offers only on Easy and Normal, with
independent editable prices and **Offered** switches. Their type is fixed to Temple. Shops
can change among Arms & Armor, General Items, Silver Items, and Jewelry, but
cannot become temples. Changing a shop type preserves its independently edited
offerings so the user can adjust its stock deliberately.

Portraits can be selected from `art/portraits/NPCs/` or
[`portraits/`](portraits/). Add a new PNG to the latter folder and press
**Refresh portraits**. The game ships `art/portraits/` itself. A portrait from
[`portraits/`](portraits/) is exported to
[`data/art/portraits/shops/`](../../data/art/portraits/shops/README.md), which
the game ships, as soon as it is chosen; **Export portraits to game** exports
again after a PNG is redrawn. Unused copies are removed. A record names its
portrait as the game finds it, never by this folder: `art/portraits/NPCs/x.png`
is saved as `NPCs/x.png`, and `portraits/x.png` as `shops/x.png`, its exported
copy.
The default Tyr record uses Bishop Braccio's existing portrait. Sune, Tempus,
and Ilmater use the new portraits in [`portraits/`](portraits/).

Changes save immediately to one file per location under
[`data/shops/`](../../data/shops/). Each file has a stable ID, a fixed map and
marker list, a shop or temple kind, the editable type and portrait, and its
offerings. The tool validates the fixed location and the shop/temple boundary
before replacing a file. The game reads the temples' services, prices and
**Offered** switches, each shop's stock and prices, and every shopkeeper's and
priest's portrait ([#275](https://github.com/stdarg/OpenGold/issues/275)). It keeps no stock
limits yet: Stock `0` hides an item and any other value sells it without limit.

## Run

From the repository root:

```bash
./dev_tools/shop_manager/run.sh /path/to/POOLRAD
```

The path defaults to `OPENGOLD_GAME_DIR`, then `~/POOLRAD`. Set `GODOT_BIN` if
Godot is not on `PATH` or at `/Applications/Godot_mono.app/Contents/MacOS/Godot`.
The launcher builds the existing native tile/map decoder, copies its extension
into this separate Godot project, and starts the editor. CMake, Ninja, and
Godot 4.5+ are required. The tool reads the user's original game files locally
and does not commit or edit them.

For a data and source check after launching once:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless \
  --path dev_tools/shop_manager/godot -- --shop-check
```

The check also fails when an exported portrait is missing or out of date. The
optional `--export-portraits` mode exports the portraits without opening the
window. The optional `--seed` mode creates any missing default location JSON
files; it leaves existing edits intact. The optional `--select=podol-temple-ilmater`
opens the Podol Plaza temple directly. A visible Godot run can append
`--capture=/tmp/shop-manager.png` to save a review screenshot.

## Sources and limits

The [original Cluebook](../../GameDocs/Cluebook.pdf) identifies the New Phlan
shop types, three temples, and Podol Plaza's hidden Temple of Ilmater. The
installed game scripts and map records establish the fixed locations. The
decoded item lists and gold values provide shop defaults. The complete temple
menu and starting gold prices follow the [Pool of Radiance walkthrough's Temple
Spells table](https://easydamus.s3.us-east-1.amazonaws.com/PoR.pdf), section
8.5: Cure Blindness 1000, Cure Disease 1000, Cure Light Wounds 100, Cure Serious
Wounds 350, Cure Critical Wounds 600, Neutralize Poison 1000, Raise Dead 5500,
Remove Curse 3500, and Stone to Flesh 2000 gp. Resurrection, at 11000 gp, is
OpenGoldBox's. These prices are editable. The game casts each service as an SRD
5.2.1 spell
([TEMPLE-1](../../docs/SRD-DECISIONS.md#temple-1-2026-10-22-temple-services)).
