extends SceneTree

var saved_files := {}

# Checkpoint bodies in the current combat format for content creatures, which
# carry no character profile. The header, with the current rules identity, is
# taken from a checkpoint the game itself writes.
const ACTOR_TAIL := " 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 10 CN1 0"
# Each body adds one empty carried-inventory line per actor after it, then an
# empty list of members who fled.
const CONTINUATION_TAIL := "0\n0 0\n0\n0\n0\n0\n0 0\n0\n0\n0 \n0\n0\n0\n0\n0\n"

# The mover left an attack and a Second Wind behind, then tried to step from
# (2,2) to (1,2). The first guard declined; the second guard's reaction waits.
const MOVEMENT_BODY := "8 6\n" \
    + "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n" \
    + "6018027440424182934 5 0 1 0 4\n" \
    + "1 \"vanguard\" \"Mover\" 0 2 2 20 15 30 0 0 0 0 0 0 1 0 0 0 0 \"\"" + ACTOR_TAIL + "\n" \
    + "3 \"vanguard\" \"Guard\" 1 3 3 28 11 30 2 0 0 0 1 1 1 0 0 0 0 \"\"" + ACTOR_TAIL + "\n" \
    + "4 \"vanguard\" \"Second guard\" 1 3 1 28 9 30 2 0 0 0 1 1 1 0 0 0 0 \"\"" + ACTOR_TAIL + "\n" \
    + "2 \"vanguard\" \"Target\" 1 3 2 24 3 30 2 0 0 0 1 1 1 0 0 0 0 \"\"" + ACTOR_TAIL + "\n" \
    + "1 0\n1 2 \n3 1\n3 4 2 \n4\n" \
    + "\"Combat begins. Each square is 5 feet.\"\n\"Round 1: Mover acts.\"\n" \
    + "\"Mover recovers 10 HP.\"\n\"Mover -> Target: d20 16 + 5 vs AC 17 hits for 4 damage.\"\n" \
    + "1 0 100 4\nFX8 1 0 0\nFX8 1 0 0\nFX8 1 0 0\nFX8 1 0 0\n" + CONTINUATION_TAIL + "0\n0\n0\n0\n0\n"

# A mover in a walled corridor with a Stable, Unconscious enemy between it and a guard.
const TRANSIT_BODY := "6 3\n1 1 1 1 1 1 0 0 0 0 0 0 1 1 1 1 1 1 \n" \
    + "15755400384260043842 1 0 1 0 3\n" \
    + "1 \"vanguard\" \"Mover\" 0 0 1 28 15 30 2 0 0 0 1 1 1 0 0 0 0 \"\"" + ACTOR_TAIL + "\n" \
    + "3 \"bandit\" \"Guard\" 1 5 1 11 11 30 0 0 0 0 1 1 1 0 0 0 0 \"\"" + ACTOR_TAIL + "\n" \
    + "2 \"bandit\" \"Unconscious enemy\" 1 2 1 0 3 30 0 0 0 0 1 1 1 0 0 1 0 \"\"" + ACTOR_TAIL + "\n" \
    + "0 0\n\n0 0\n\n2\n\"Combat begins. Each square is 5 feet.\"\n\"Round 1: Mover acts.\"\n" \
    + "1 0 100 3\nFX8 1 0 0\nFX8 1 0 0\nFX8 1 0 0\n" + CONTINUATION_TAIL + "0\n0\n0\n0\n"

func _initialize() -> void:
    call_deferred("run_checks")

func restore_files() -> void:
    for path in saved_files:
        if saved_files[path] == null:
            if FileAccess.file_exists(path):
                DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            if file:
                file.store_buffer(saved_files[path])
                file.close()
    saved_files.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)

func key(code: Key) -> void:
    var event := InputEventKey.new()
    event.keycode = code
    event.pressed = true
    root.push_input(event)

func load_checkpoint(combat: Node, path: String, header: String, body: String) -> void:
    var file := FileAccess.open(path, FileAccess.WRITE)
    require(file != null, "Prepare checkpoint for the real Load control")
    file.store_string(header + "\n" + body)
    file.close()
    combat.get_node("Load").pressed.emit()

func run_checks() -> void:
    var path := ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        saved_files[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn")
    for frame in range(4):
        await process_frame
    var combat := current_scene
    combat.set_process(false)
    combat.get_node("Save").pressed.emit()
    var header := FileAccess.get_file_as_string(path).get_slice("\n", 0)
    require(header.begins_with("OGCOMBAT "), "The game writes a combat checkpoint header")

    load_checkpoint(combat, path, header, MOVEMENT_BODY)
    require(combat.get_node("StatusStack/Turn").text.contains("Second guard reaction"), "Movement queue retains its next reactor")
    require(combat.selected_character_cell() == Vector2i(2, 2), "Mover still waits before leaving reach")
    var before: String = combat.get_node("StatusStack/Turn").text
    key(KEY_ENTER)
    require(combat.get_node("StatusStack/Turn").text == before, "Keyboard cannot bypass the enemy's pending reaction")
    var expected_turn: String = combat.get_node("StatusStack/Turn").text
    var expected_roster: String = combat.get_node("StatusStack/Roster").text
    combat.get_node("Save").pressed.emit()
    require(combat.get_node("Prompt").text.contains("saved"), "Pending movement saves through existing controls")
    combat.get_node("Load").pressed.emit()
    require(combat.get_node("StatusStack/Turn").text == expected_turn and combat.get_node("StatusStack/Roster").text == expected_roster,
        "Subsequent reload preserves turn, resources and pending movement")

    # The existing movement input crosses an Unconscious enemy. The occupied
    # square itself remains forbidden.
    load_checkpoint(combat, path, header, TRANSIT_BODY)
    combat.get_node("Move").pressed.emit()
    root.size = Vector2i(1920, 1080)
    for frame in range(4): await process_frame
    var canvas: Control = combat.get_node("BattlefieldScroll/Canvas")
    var tile: float = canvas.custom_minimum_size.x / 6.0
    for cell in [Vector2i(2, 1), Vector2i(3, 1)]:
        for pressed in [true, false]:
            var click := InputEventMouseButton.new()
            click.button_index = MOUSE_BUTTON_LEFT; click.pressed = pressed
            click.position = canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)
            root.push_input(click)
        for frame in range(4): await process_frame
        require(combat.selected_character_cell() == (Vector2i(0, 1) if cell.x == 2 else cell), "Actual mouse input rejects occupied destination and crosses to free destination")
    require(combat.get_node("LogStack/LogHeader").text.contains("Move 10 ft | Action ready"), "UI reports twenty feet spent and preserves the Action")
    restore_files()
    print("Opportunity view checks passed: remaining movement, pending reaction, save/load, unconscious transit")
    quit(0)
