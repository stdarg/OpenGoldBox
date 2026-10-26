extends SceneTree
# Compulsion's direction is aimed at a square through the combat window (the
# user's choice, 2026-10-25): the A cycle offers it, Space starts aiming at the
# nearest foe, the arrows move the aim and Space names the way it lies.
var captures := ""
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--compulsion-capture="): captures = arg.trim_prefix("--compulsion-capture=")
        if arg.begins_with("--compulsion-fixtures="): fixtures = arg.trim_prefix("--compulsion-fixtures=")
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
    require(not bytes.is_empty(), "Native combat fixture exists")
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
func cycle_to(combat: Node, label: String) -> bool:
    for attempt in range(30):
        await key(KEY_A)
        if combat.get_node("Prompt").text.contains(label): return true
    return false
func capture(name: String) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Capture the aim")
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
        await load_fixture("compulsion")
        var label := "Compulsion: name a direction" if locale == "en" else "Compulsión: indicar una dirección"
        var aim := "Aim the spell." if locale == "en" else "Apunta el conjuro."
        require(await cycle_to(combat, label), "The A cycle offers Compulsion's direction: " + locale)
        await key(KEY_SPACE)
        require(combat.get_node("Prompt").text.contains(aim), "Space starts aiming: " + locale)
        # The Bard stands at (1,1) and the aim starts on the foe at (4,1): east.
        # Three squares down it lies south-east.
        var way := "east"
        if locale == "en":
            for step in range(3): await key(KEY_DOWN)
            way = "southeast"
        await capture("compulsion-aim-" + locale)
        await key(KEY_SPACE)
        var named := "Bard compels the charmed to go " + way + "." if locale == "en" else "Bard obliga a los hechizados a ir hacia el este."
        require(combat.get_node("LogStack/Log").get_parsed_text().contains(named), "Space names the aimed way: " + locale)
        require(not await cycle_to(combat, label), "The Bonus Action is spent: " + locale)
    TranslationServer.set_locale("en")
    restore_files()
    print("Compulsion view checks passed")
    quit(0)
