# Rolf's welcome

The implementation now uses C++20 and Godot through GDExtension. See
[the native tour](../ROLF.md) for launch commands, script evidence, tests and
remaining compatibility work. The earlier HTML mockup and its generator have
been removed.

The first-person view occupies the left side, with Rolf's encounter sprite
composited over the map geometry. A north-up map on the right shows the party's
position and facing. Dialogue and Continue sit below the first-person view.
Both views consume one native session snapshot.

The original ECL script supplies dialogue, sprite approach/clear requests and
the route through Rolf's farewell. Movement is locked while the tour runs.
Afterwards, the review scene permits conservative exploration and replay.

The implemented review layout has a 960 x 720 minimum window size, scrollable
dialogue, nearest-neighbor sprite enlargement and a full/visited map switch.
Walls are assembled from Phlan's original perspective art
([wall artwork](../phlan-wall-art.md)). Full map visibility and the isolated tour entry are review
features, not established campaign discovery or triggering rules.
