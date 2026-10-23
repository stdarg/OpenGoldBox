# Humanoid combat sprite height audit

The [per-record estimates](humanoid-height-estimates.csv) cover all 175 saved
humanoid-shaped combat records: 14 Small, 143 Normal, and 18 Tall. Together they
have 221 ready/action weapon-set pairs. Identical image hashes reduce those to
157 distinct pairs; every distinct pair was inspected with both frames side by
side against the same fixed height line. Reused pairs inherit that visual review.

One combat square is 5 feet tall. The review measures the visible body from
planted feet to the top of the head, excluding weapons, raised arms, hair,
horns, and spell effects. Floating figures use their visible lower body or
vapor base. The preview uses the catalog's fixed scales: Small 1.0, Normal
1.45, and Tall 2.1. The ready frame establishes the base anchor for both
frames; the action frame must retain the same body height and position.

- Small: compare with each record's saved `height_feet` target, from 2 to 4 feet.
- Normal: a full 5-foot body, including every Orc.
- Tall: about 1.7 squares, or 8 to 9 feet, where its saved footprint permits.

The CSV's visual estimates are deliberately rounded art-direction judgments,
not anatomical measurements or new creature statistics. Each row gives the
record key, its saved tier and target, the number of reviewed pairs, and the
result. The ready/action review found no remaining clearly wrong-height pair
in the integrated replacement art. File existence and regeneration flags alone
were **not** used as evidence of height correctness.

The old main checkout had displayed a roughly 3-foot Orc because corrected
replacement art had remained on a separate branch. That art and the other
reviewed pairs were integrated into `main` after this visual audit. The
[generation and review workflow](../combat_sprite_catalog/generation/README.md)
remains the acceptance procedure for any later art replacement.
