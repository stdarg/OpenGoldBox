extends SceneTree

# Read-only audit of installed replacement art at the catalog's fixed preview
# scales. Run with OPENGOLD_GAME_DIR set, from the catalog Godot project.
func _initialize() -> void:
    call_deferred("_audit")


func _audit() -> void:
    var scene: Node = load("res://scenes/catalog.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    await process_frame

    var total := 0
    var fit := 0
    var overflow := 0
    var missing := 0
    var affected_records := {}
    for index in range(scene.rows.size()):
        scene.action = false
        scene._select_monster(index)
        var key: String = str(scene.rows[index].key)
        for variant_index in range(scene.variant_choice.item_count):
            scene.variant_choice.select(variant_index)
            var variant_id: String = str(scene.variant_choice.get_item_metadata(variant_index))
            for pose in ["ready", "action"]:
                scene.action = pose == "action"
                scene._show_monster_image()
                if not scene.preview_is_replacement:
                    missing += 1
                    print("MISSING\t%s\t%s\t%s" % [key, variant_id, pose])
                    continue
                total += 1
                var status: String = scene.preview_status.text
                if status.begins_with("Image fits"):
                    fit += 1
                else:
                    overflow += 1
                    affected_records[key] = true
                    print("OVERFLOW\t%s\t%s\t%s\t%s" % [key, variant_id, pose, status])

    print("AUDIT_TOTAL\t%d\tFIT\t%d\tOVERFLOW\t%d\tMISSING\t%d\tAFFECTED_RECORDS\t%d" % [
        total, fit, overflow, missing, affected_records.size()])
    quit()
