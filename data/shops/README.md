Each JSON file is an independent shop or temple record. Every location but
one is the original's; `phlan-rogue-04-13`, the Rogue's Shop, is
OpenGoldBox's, and its entry in `locations.json` has an `entrance`: the side
of its square with the secret, locked door (`secret_door`), the map's wall
that draws that door once found (`door_wall`), and the walls that close its
other open sides (`walls`, by side). The game changes the map to match and
opens the shop itself on its square
([SECRET-1](../../docs/SRD-DECISIONS.md#secret-1-2026-10-29-the-rogues-shop-and-its-secret-door)).
`fixed_location`,
`kind`, and `id` identify an existing place and are immutable in the editor.
Retail `type`, `portrait`, and `offerings` are editable. Temple `type` is always
`Temple`; service price and `offered` are editable. Stock quantity `-1` means
unlimited, `0` means unavailable, and positive values set a limit. Prices use
integer gold pieces. New portraits live in
[`dev_tools/shop_manager/portraits/`](../../dev_tools/shop_manager/portraits/).

The [Shop & Temple Manager](../../dev_tools/shop_manager/README.md) edits these
files. The game reads the temples' services, prices and `offered` switches
([TEMPLE-1](../../docs/SRD-DECISIONS.md#temple-1-2026-10-22-temple-services)),
each shop's stock and prices, and every shopkeeper's and priest's portrait
([#275](https://github.com/stdarg/OpenGoldBox/issues/275)). It does not keep stock
limits yet: an item with quantity `0` is not sold and every other item is
unlimited. A `portrait` is named as the game finds it in its portraits folder,
such as `NPCs/bishop-braccio.png` from `art/portraits/`, or
`shops/temple-sune.png`, a copy the manager exports to
[`data/art/portraits/shops/`](../art/portraits/shops/README.md). The game reads
nothing from `dev_tools/`. Each temple lists the
original's nine services and OpenGoldBox's Resurrection, which the game offers
only on Easy and Normal. `opengold_shop_data_tests` reads these files as the
game does.
