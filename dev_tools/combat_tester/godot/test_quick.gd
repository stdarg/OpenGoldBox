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
    var members: Array = defaults.parties[9].members.duplicate(true)
    for member in members: member.quick = true
    members[0]["class"] = "wizard"
    var spells: Dictionary = bridge.spell_options(members[0])
    if spells.has("error") or spells.get("prepared", []).is_empty():
        push_error("Wizard spell options unavailable: %s" % spells)
        quit(1)
        return
    members[0].prepared_spells = [spells.prepared[0]]
    if spells.cantrip_count > 0:
        members[0].cantrips = spells.cantrips.slice(0, spells.cantrip_count)
    if not spells.starting_groups.is_empty():
        var first: Dictionary = spells.starting_groups[0]
        members[0].starting_spells = {first.id: first.options.slice(0, first.count)}
    var request := {
        "members": members, "map": bridge.catalog().maps[0], "monster": "orc",
        "count": 1, "leaders": 0, "seed": 777, "quick_magic": true,
    }
    var error: String = bridge.start(request)
    if not error.is_empty():
        push_error("Quick combat failed: " + error)
        quit(1)
        return
    for frame in 60:
        await process_frame
    if setup.get_node_or_null("CombatTestFight") == null:
        push_error("Quick game combat scene disappeared")
        quit(1)
        return
    print("COMBAT_TEST_QUICK_OK prepared_spell=", members[0].prepared_spells[0])
    bridge.leave()
    await process_frame
    quit(0)
