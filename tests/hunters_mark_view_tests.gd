extends SceneTree
var captures := ""
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--ranger-capture="): captures = arg.trim_prefix("--ranger-capture=")
        if arg.begins_with("--ranger-fixtures="): fixtures = arg.trim_prefix("--ranger-fixtures=")
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
        await load_fixture("ranger")
        var label := "Hunter's Mark (Favored Enemy)" if locale == "en" else "Marca del cazador (Enemigo predilecto)"
        require(await cycle_to(combat, label), "The A cycle offers the free Hunter's Mark: " + locale)
        await click_cell(Vector2(6, 1))
        var log: String = combat.get_node("Log").get_parsed_text()
        var cast := "Ranger casts Hunter's Mark (Favored Enemy)." if locale == "en" else "Ranger lanza Marca del cazador (Enemigo predilecto)."
        require(log.contains(cast), "Clicking the enemy marks it: " + locale)
        require(not await cycle_to(combat, label), "The spent Bonus Action ends further marking: " + locale)
        var strider := "Longstrider" if locale == "en" else "Zancada prodigiosa"
        require(await cycle_to(combat, strider), "The A cycle offers Longstrider: " + locale)
        await load_fixture("hunter")
        require(await cycle_to(combat, "Melee attack" if locale == "en" else "Ataque cuerpo a cuerpo"), "The Hunter attacks: " + locale)
        await click_cell(Vector2(2, 1))
        var horde := "Horde Breaker" if locale == "en" else "Rompehordas"
        require(await cycle_to(combat, horde), "The A cycle offers Horde Breaker after the attack: " + locale)
        await click_cell(Vector2(2, 2))
        var broke := "Ranger uses Horde Breaker." if locale == "en" else "Ranger usa Rompehordas."
        require(combat.get_node("Log").get_parsed_text().contains(broke), "Clicking the second enemy attacks it: " + locale)
    TranslationServer.set_locale("en")
    restore_files()
    print("Hunter's Mark view checks passed")
    quit(0)
