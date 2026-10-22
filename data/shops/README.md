Each JSON file is an independent shop or temple record. `fixed_location`,
`kind`, and `id` identify an existing place and are immutable in the editor.
Retail `type`, `portrait`, and `offerings` are editable. Temple `type` is always
`Temple`; service price and `offered` are editable. Stock quantity `-1` means
unlimited, `0` means unavailable, and positive values set a limit. Prices use
integer gold pieces. New portraits live in
[`dev_tools/shop_manager/portraits/`](../../dev_tools/shop_manager/portraits/).

The [Shop & Temple Manager](../../dev_tools/shop_manager/README.md) edits these
files. The game reads the temples' services, prices and `offered` switches
([TEMPLE-1](../../docs/SRD-DECISIONS.md#temple-1-2026-10-22-temple-services)); it
does not read the shops yet
([#271](https://github.com/stdarg/OpenGold/issues/271)). Each temple lists the
original's nine services and OpenGoldBox's Resurrection, which the game offers
only on Easy and Normal.
