extends SceneTree

var saved_files := {}
var fixtures := ""

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--grip-fixtures="): fixtures = arg.trim_prefix("--grip-fixtures=")
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

func press_key(viewport: Viewport, code: Key) -> void:
    for pressed in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code
        event.pressed = pressed
        viewport.push_input(event)

func settle() -> void:
    for frame in range(4):
        await process_frame

func run_checks() -> void:
    var path := ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        saved_files[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    combat.set_process(false)
    var file := FileAccess.open(path, FileAccess.WRITE)
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join("grips.save"))
    require(not bytes.is_empty(), "Native grip fixture exists")
    file.store_buffer(bytes)
    file.close()
    combat.get_node("Load").pressed.emit()
    var grip: OptionButton = combat.get_node("Grip")
    require(grip.visible and grip.item_count == 2 and grip.selected == 1, "Two-handed battleaxe grip is shown as two hands")
    require(grip.get_item_text(0) == "One hand — 1d8" and grip.get_item_text(1) == "Two hands — 1d10", "Dropdown displays rules-owned damage dice")
    for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
        root.size = size
        await settle()
        require(grip.position.y == combat.get_node("End").position.y, "Grip is beside the turn/reaction controls")
        require(grip.get_rect().end.x <= combat.get_node("BattlefieldScroll").get_rect().end.x, "Grip fits within the combat column")
        require(grip.get_rect().end.y <= combat.get_node("Log").position.y, "Grip does not overlap the combat log")
    require(grip.focus_mode == Control.FOCUS_ALL, "Grip supports keyboard focus")
    var before: String = combat.get_node("Turn").text
    var cell: Vector2i = combat.selected_character_cell()
    grip.grab_focus()
    press_key(root, KEY_DOWN)
    require(combat.selected_character_cell() == cell, "Arrow on the focused dropdown does not move a character")
    press_key(root, KEY_ENTER)
    await settle()
    require(grip.get_popup().visible, "Enter opens the focused Grip dropdown")
    require(combat.get_node("Turn").text == before, "Opening Grip does not end the turn")
    grip.get_popup().set_focused_item(0)
    press_key(root, KEY_ENTER)
    await settle()
    require(grip.selected == 0, "Keyboard chooses one hand")
    require(combat.get_node("Turn").text == before and not combat.get_node("Dash").disabled, "Changing grip preserves turn and action")
    combat.get_node("Save").pressed.emit()
    combat.get_node("Load").pressed.emit()
    require(grip.selected == 0, "Combat save and reload retain the keyboard-selected grip")
    grip.select(1)
    grip.item_selected.emit(1)
    require(grip.selected == 1, "Two-hand selection can be restored without an action cost")
    combat.get_node("Save").pressed.emit()
    combat.get_node("Load").pressed.emit()
    require(grip.selected == 1 and combat.get_node("Turn").text == before, "Two-hand save retains all turn resources")
    restore_files()
    print("Grip view checks passed: layout, dice, keyboard, resources and save/load")
    quit(0)
