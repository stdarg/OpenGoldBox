extends SceneTree

# Compare saved sprite footprints with both combat images decoded from the
# user's game files. A source combat tile is 24 by 24 pixels.
const SOURCE_TILE := 24


func _initialize() -> void:
    call_deferred("_audit")


func _audit() -> void:
    var scene: Node = load("res://scenes/catalog.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    await process_frame
    if scene.rows.is_empty():
        push_error("Cannot audit source sprite sizes: game catalog did not load")
        quit(1)
        return

    var checked := 0
    var mismatches := 0
    for index in range(scene.rows.size()):
        scene._select_monster(index)
        var row: Dictionary = scene.rows[index]
        var record: Dictionary = scene.draft_records[row.key]
        var native_index: int = int(row.get("native_index", -1))
        if native_index < 0:
            native_index = int(scene.native_index_by_key.get(row.get("origin_key", ""), -1))
        var ready: Texture2D = scene.catalog.monster_image(native_index, false) if native_index >= 0 else null
        var action: Texture2D = scene.catalog.monster_image(native_index, true) if native_index >= 0 else null
        if ready == null or action == null:
            mismatches += 1
            print("MISSING_SOURCE\t%s\t%s" % [row.key, row.name])
            continue
        var pixels := Vector2i(maxi(ready.get_width(), action.get_width()),
            maxi(ready.get_height(), action.get_height()))
        if pixels.x % SOURCE_TILE != 0 or pixels.y % SOURCE_TILE != 0:
            mismatches += 1
            print("NON_TILE_SOURCE\t%s\t%s\t%s" % [row.key, row.name, pixels])
            continue
        var expected := pixels / SOURCE_TILE
        var saved: Variant = record.get("sprite_size")
        var actual := Vector2i(int(saved.get("width_squares", 0)),
            int(saved.get("height_squares", 0))) if saved is Dictionary else Vector2i.ZERO
        checked += 1
        if actual != expected:
            mismatches += 1
            print("MISMATCH\t%s\t%s\t%s\t%s\t%s" % [
                row.key, row.name, pixels, expected, actual])
    print("SOURCE_SIZE_AUDIT\tchecked=%d\tmismatches=%d" % [checked, mismatches])
    quit(1 if mismatches > 0 else 0)
