# Shop and temple portraits

The [Shop & Temple Manager](../../../../dev_tools/shop_manager/README.md)
exports here the portraits drawn for it in
[`dev_tools/shop_manager/portraits/`](../../../../dev_tools/shop_manager/portraits/)
that a `data/shops` record uses: at once when a portrait is chosen, with
**Export portraits to game**, or with `--export-portraits`. It removes copies no
record uses. The game build ships them as `portraits/shops/`. Do not edit these
copies; redraw the source and export again. `--shop-check` and
`opengold_shop_data_tests` fail when a copy is missing or out of date.
