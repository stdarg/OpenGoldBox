# Transit through Unconscious enemies

[#222](https://github.com/stdarg/OpenGold/issues/222) is a bounded child of
[#44](https://github.com/stdarg/OpenGold/issues/44). SRD 5.2.1 p.14 permits
movement through an Incapacitated creature's space. Another creature's space
is Difficult Terrain unless that creature is Tiny or an ally. Occupied spaces
are not legal voluntary destinations. This increment uses the existing
zero-HP Unconscious source of Incapacitated; it does not add other conditions.

Source: [official SRD](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

## Behavior

Both sides can cross living Unconscious enemies. Entering their square costs
ten feet on the current grid; underlying Difficult Terrain does not add a second
surcharge. Allies retain ordinary ground costs. Conscious enemies still block,
corpses retain existing treatment, and no creature's occupied square becomes a
voluntary destination. Existing mouse/keyboard targeting and movement previews
consume the corrected legal destinations without a new control layout.

The value-owned movement snapshot carries occupancy and incapacitation. It
uses the strongest restriction if creatures overlap: a conscious enemy blocks,
an Unconscious enemy imposes Difficult Terrain, and an ally adds no surcharge.
The same step calculation drives search, execution and saved-route validation.

Other conscious enemies can interrupt a crossing with an opportunity attack.
The paid prefix stays paid. If the mover falls unconscious in the occupied
square, its involuntary position persists through healing, death/recovery and
exit. The shared-space helper now recognizes opposing sides, and the existing
involuntary-overlap marker is cleared when a creature leaves or dies.

## Persistence and evidence

This is a movement correction: restored encounters receive the corrected rule
for future movement. Existing queued routes preserve their chosen cells, paid
prefix, RNG and action budgets. No character grants or capabilities change.

The graph oracle covers 7,776 mixed occupancy/terrain boards plus 6,561 terrain
boards. Focused cases cover overlapping restrictions, nonstacking Difficult
Terrain, budget boundaries and occupied endpoints. Native combat tests exercise
both sides, actual knockout, interrupted crossing, saved reaction suffixes,
healing without teleportation and exit. The existing
Godot opportunity test clicks through the actual loaded corridor, rejects the
occupied square and verifies the movement cost and unspent Action.

## Remaining parent scope

#44 remains open for Tiny/size-difference rules and Prone when a turn ends in
another creature's space; #45 covers large footprints and #35 other condition
sources. The overlap marker preserves state and is not a claim of Prone support.
The current enemy-death policy is unchanged: ordinary side-one enemies die at
zero HP. Actual knockout coverage therefore also exercises an enemy moving
through an unconscious party member; explicit saved Unconscious enemies cover
the reverse direction.

Verification completed: all 41 native/tool tests and all 17 Godot runtime tests
(with eight native prerequisites) pass. The extended opportunity UI case passes
separately; final death/natural-recovery cases pass in the rebuilt rules target.
Main/demo builds and 816-message localization validation pass. No controls,
layout or user-facing text changed, so no new layout approval was needed.
