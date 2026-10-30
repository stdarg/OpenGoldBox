extends SceneTree

func _initialize() -> void:
    call_deferred("run_check")

func run_check() -> void:
    var setup: Control = load("res://combat_tester/setup.tscn").instantiate()
    root.add_child(setup)
    await process_frame
    var bridge: Node = setup.get_node("Bridge")
    var file := FileAccess.open(OS.get_environment("OPENGOLD_COMBAT_TEST_CONFIG"), FileAccess.READ)
    var defaults: Dictionary = JSON.parse_string(file.get_as_text())
    var request := {
        "members": defaults.parties[0].members, "monster": "orc",
        "count": 6, "leaders": 0, "seed": 12345, "quick_magic": false,
    }
    var checked := 0
    for map in bridge.catalog().maps:
        request.map = map
        var error: String = bridge.start(request)
        if not error.is_empty():
            push_error("Map %s failed: %s" % [map, error])
            quit(1)
            return
        bridge.leave()
        await process_frame
        checked += 1
    print("COMBAT_TEST_MAPS_OK count=", checked)
    quit(0)
