extends SceneTree
var captures := ""
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--entangle-capture="): captures = arg.trim_prefix("--entangle-capture=")
        if arg.begins_with("--entangle-fixtures="): fixtures = arg.trim_prefix("--entangle-fixtures=")
    call_deferred("run_checks")
func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            file.store_buffer(originals[path]); file.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func load_fixture(which: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(which + ".save"))
    require(not bytes.is_empty(), "Native character fixture exists")
    var file := FileAccess.open(save_path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()
func click_cell(cell: Vector2, button := MOUSE_BUTTON_LEFT) -> void:
    var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
    var tile := canvas.get_combined_minimum_size().x / 12.0
    var position := canvas.get_global_transform_with_canvas() * ((cell + Vector2(0.5, 0.5)) * tile)
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = button; event.pressed = down; event.position = position
        root.push_input(event, true)
    await settle()
func cycle_to(combat: Node, label: String) -> bool:
    for attempt in range(30):
        await key(KEY_A)
        if combat.get_node("Prompt").text.contains(label): return true
    return false
func run_checks() -> void:
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[save_path + suffix] = FileAccess.get_file_as_bytes(save_path + suffix) if FileAccess.file_exists(save_path + suffix) else null
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        var combat := current_scene
        combat.set_process(false)
        root.size = Vector2i(1120, 800)
        await settle()
        await load_fixture("entangle")
        var label := "Entangle" if locale == "en" else "Enmarañar"
        var aim := "Aim the spell." if locale == "en" else "Apunta el conjuro."
        require(await cycle_to(combat, label), "The A cycle offers Entangle: " + locale)
        await key(KEY_SPACE)
        require(combat.get_node("Prompt").text.contains(aim), "Space starts aiming: " + locale)
        await key(KEY_RIGHT)
        await key(KEY_ESCAPE)
        require(not combat.get_node("Prompt").text.contains(aim), "Escape cancels the aim: " + locale)
        require(await cycle_to(combat, label), "Entangle is still offered after cancelling: " + locale)
        await click_cell(Vector2(6, 1))
        require(combat.get_node("Prompt").text.contains(aim), "A left click starts aiming at the square: " + locale)
        await click_cell(Vector2(6, 1), MOUSE_BUTTON_RIGHT)
        var cast := "Ranger casts Entangle." if locale == "en" else "Ranger lanza Enmarañar."
        require(combat.get_node("Log").get_parsed_text().contains(cast), "A right click casts: " + locale)
    TranslationServer.set_locale("en")
    restore_files()
    print("Entangle view checks passed")
    quit(0)
