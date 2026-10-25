extends SceneTree
# Issue #5's representative encounter in the combat view: the fixture from
# wizard_spell_tests.cpp is the Wizard's second turn, both foes scorched by
# Burning Hands. Casting Web through the view must catch both, as the native
# run of the same checkpoint does.
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--wizard-fixtures="): fixtures = arg.trim_prefix("--wizard-fixtures=")
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
func load_fixture() -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join("encounter.save"))
    require(not bytes.is_empty(), "Native encounter fixture exists")
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
        await load_fixture()
        var english: bool = locale == "en"
        require(await cycle_to(combat, "Web" if english else "Telaraña"), "The A cycle offers Web: " + locale)
        await click_cell(Vector2(3, 1))
        await click_cell(Vector2(3, 1), MOUSE_BUTTON_RIGHT)
        var log_text: String = combat.get_node("LogStack/Log").get_parsed_text()
        require(log_text.contains("Wizard casts Web." if english else "Wizard lanza Telaraña."), "A right click casts Web: " + locale)
        for foe in ["First", "Second"]:
            var held: String = foe + (" is Restrained by the webs." if english else " queda apresado por las telarañas.")
            require(log_text.contains(held), "The webs catch " + foe + ": " + locale)
    TranslationServer.set_locale("en")
    restore_files()
    print("Encounter view checks passed")
    quit(0)
