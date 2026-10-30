extends SceneTree

func _initialize() -> void:
    call_deferred("run_check")

func run_check() -> void:
    var packed: PackedScene = load("res://combat_tester/setup.tscn")
    var setup: Control = packed.instantiate()
    root.add_child(setup)
    await process_frame
    var bridge: Node = setup.get_node("Bridge")
    var options: Dictionary = bridge.catalog()
    if options.has("error") or options.get("maps", []).is_empty():
        push_error("Catalog unavailable: %s" % options.get("error", "no maps"))
        quit(1)
        return
    var file := FileAccess.open(OS.get_environment("OPENGOLD_COMBAT_TEST_CONFIG"), FileAccess.READ)
    var defaults: Dictionary = JSON.parse_string(file.get_as_text())
    for level in 15:
        var request := {
            "members": defaults.parties[level].members,
            "map": options.maps[0],
            "monster": "orc",
            "count": 6,
            "leaders": 1,
            "seed": 12345,
            "quick_magic": false,
        }
        var error: String = bridge.start(request)
        if not error.is_empty():
            push_error("Level %d combat handoff failed: %s" % [level + 1, error])
            quit(1)
            return
        if setup.get_node_or_null("CombatTestFight") == null:
            push_error("Game combat view was not attached")
            quit(1)
            return
        print("COMBAT_TEST_SMOKE_OK level=", level + 1, " map=", request.map)
        bridge.leave()
        await process_frame
    var customized: Array = defaults.parties[4].members.duplicate(true)
    customized[0].species = "gnome"
    customized[0].abilities = [14, 15, 16, 17, 13, 12]
    customized[0].weapon_types = [36, 41]
    customized[0].armor_type = 50
    customized[0].shield = true
    customized[0].gear_types = [86, 200]
    customized[0].quick = true
    for option in options.monsters:
        var request := {
            "members": customized,
            "map": options.maps[0],
            "monster": option.id,
            "count": 4,
            "leaders": 1 if option.leaders else 0,
            "seed": 72815,
            "quick_magic": true,
        }
        var error: String = bridge.start(request)
        if not error.is_empty():
            push_error("Custom encounter failed for %s: %s" % [option.id, error])
            quit(1)
            return
        print("COMBAT_TEST_CUSTOM_OK monster=", option.id)
        bridge.leave()
        await process_frame
    quit(0)
