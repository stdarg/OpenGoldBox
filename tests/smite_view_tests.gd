extends SceneTree
var captures := ""
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--smite-capture="): captures = arg.trim_prefix("--smite-capture=")
        if arg.begins_with("--smite-fixtures="): fixtures = arg.trim_prefix("--smite-fixtures=")
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
func click_cell(cell: Vector2) -> void:
    var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
    var tile := canvas.get_combined_minimum_size().x / 12.0
    var position := canvas.get_global_transform_with_canvas() * ((cell + Vector2(0.5, 0.5)) * tile)
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = position
        root.push_input(event, true)
    await settle()
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
        await load_fixture("after-hit")
        var label := "Divine Smite (Paladin's Smite)" if locale == "en" else "Castigo divino (Castigo del paladín)"
        var choice: OptionButton = combat.get_node("CunningAction")
        var found := -1
        for i in range(choice.item_count):
            if choice.get_item_text(i) == label: found = i
        require(found >= 0 and not choice.is_item_disabled(found), "The hit offers Paladin's Smite in the Bonus Action list: " + locale)
        choice.select(found); choice.item_selected.emit(found); await settle()
        combat.get_node("UseCunningAction").pressed.emit(); await settle()
        var log: String = combat.get_node("LogStack/Log").get_parsed_text()
        require(log.contains("Divine Smite" if locale == "en" else "Castigo divino") and log.contains("Target"), "Use casts the smite on the creature just hit: " + locale)
    TranslationServer.set_locale("en")
    restore_files()
    print("Smite view checks passed")
    quit(0)
