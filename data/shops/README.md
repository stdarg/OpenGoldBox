Each JSON file is an independent shop or temple record. `fixed_location`,
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
([#275](https://github.com/stdarg/OpenGold/issues/275)). It does not keep stock
limits yet: an item with quantity `0` is not sold and every other item is
unlimited. A portrait must be under `art/portraits/`, which ships with
the game, or the manager's `portraits/` folder, from which the manager exports
it to [`data/art/portraits/shops/`](../art/portraits/shops/README.md) for the
game to ship. Each temple lists the
original's nine services and OpenGoldBox's Resurrection, which the game offers
only on Easy and Normal. `opengold_shop_data_tests` reads these files as the
game does.
