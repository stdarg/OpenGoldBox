Each JSON file is an independent shop or temple record. `fixed_location`,
`kind`, and `id` identify an existing place and are immutable in the editor.
Retail `type`, `portrait`, and `offerings` are editable. Temple `type` is always
`Temple`; service price and `offered` are editable. Stock quantity `-1` means
unlimited, `0` means unavailable, and positive values set a limit. Prices use
integer gold pieces. New portraits live in `../../portraits/`.
