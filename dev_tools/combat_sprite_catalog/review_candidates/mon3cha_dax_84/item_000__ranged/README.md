# MON3CHA.DAX:84/item_000__ranged correction candidate

The installed javelin action extends the offhand, although the javelin in ready
is held by the opposite arm. This candidate keeps ready byte-for-byte and
changes action so the former javelin arm crosses the chest into an empty
screen-right follow-through. The offhand stays near the chest. The installed
lower-body rows 67..127 are preserved byte-for-byte in action.

The PNG check passes. Inspect this exact pair alternating in the Combat Sprite
Catalog at normal size; trace the javelin arm from its ready shoulder to the
empty action hand. Also check the torso, belt and hips for a composite seam on
light, dark and contrasting backgrounds. Confirm the catalog loaded these exact
file paths and hashes, complete `review.json`, run `accept`, then replace the
installed action. The installed pair remains until that review succeeds.
