extends SceneTree
var originals := {}
var path := ""
var fixtures := ""
var captures := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--surge-capture="): captures = arg.trim_prefix("--surge-capture=")
        if arg.begins_with("--surge-fixtures="): fixtures = arg.trim_prefix("--surge-fixtures=")
    call_deferred("run_checks")
func restore_files() -> void:
    for name in originals:
        if originals[name] == null:
            if FileAccess.file_exists(name): DirAccess.remove_absolute(name)
        else:
            var file := FileAccess.open(name, FileAccess.WRITE)
            file.store_buffer(originals[name]); file.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()
func choose(label: String) -> void:
    for attempt in range(20):
        await key(KEY_A)
        if current_scene.get_node("Prompt").text.contains(label): return
    require(false, "Existing keyboard cycle lacks " + label)
func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Native fixture exists")
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
func run_checks() -> void:
    path = ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    current_scene.set_process(false)
    require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No player combat saving")
    for name in ["level1", "cleric", "available", "level3", "level4", "pending"]:
        await load_fixture(name)
        var surge: Button = current_scene.get_node("ActionSurge")
        var entitled: bool = name not in ["level1", "cleric"]
        require(surge.visible == entitled, "Button appears only for entitled Fighters")
        if entitled:
            require(surge.disabled == (name == "pending"), "Spent uses disable activation")
            require(surge.text == ("Action Surge (0/1)" if name == "pending" else "Action Surge (1/1)"), "Button displays remaining resource")
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        current_scene.set_process(false)
        await load_fixture("level4")
        var surge: Button = current_scene.get_node("ActionSurge")
        require(surge.text == ("Action Surge (1/1)" if locale == "en" else "Oleada de acción (1/1)"), "Translated resource label")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size
            await settle()
            var rush: Button = current_scene.get_node("AdrenalineRush")
            require(surge.position.y == rush.position.y and surge.position.x > rush.get_rect().end.x, "Surge is beside Adrenaline Rush")
            require(surge.get_rect().end.x < root.size.x - 300, "Translated control fits before party sidebar")
            require(surge.get_rect().end.y <= current_scene.get_node("Log").position.y, "Log does not overlap the control row")
            require(surge.focus_mode == Control.FOCUS_ALL, "Button supports keyboard focus")
            if not captures.is_empty():
                DirAccess.make_dir_recursive_absolute(captures)
                await RenderingServer.frame_post_draw
                require(root.get_texture().get_image().save_png(captures.path_join("surge-" + locale + "-" + str(size.x) + ".png")) == OK, "Rendered control capture")
        surge.grab_focus()
        await key(KEY_ENTER)
        require(surge.visible and surge.disabled and surge.text.ends_with("(0/1)"), "Focused Enter spends exactly one use and retains the disabled button")
        require(current_scene.get_node("LogHeader").text.contains("Action Surge action ready" if locale == "en" else "Acción de Oleada de acción disponible"), "Focused Enter activates without ending the turn")
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    current_scene.set_process(false)
    await load_fixture("available")
    var surge: Button = current_scene.get_node("ActionSurge")
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down
        event.position = surge.get_global_rect().get_center()
        root.push_input(event, true)
    await settle()
    require(surge.disabled and surge.text == "Action Surge (0/1)", "Mouse click immediately spends the use")
    surge.release_focus()
    await load_fixture("available")
    await choose("Action Surge")
    require(current_scene.get_node("Prompt").text == "Selected: Action Surge. Press Space to use it.", "Action Surge, used on oneself, asks for Space rather than a click")
    await key(KEY_SPACE)
    require(current_scene.get_node("Log").get_parsed_text().contains("uses Action Surge"), "Existing keyboard action activates the feature")
    await choose("Dash")
    await key(KEY_SPACE)
    require(current_scene.get_node("LogHeader").text.contains("Move 60 ft | Action ready"), "Extra Dash retains ordinary action")
    await choose("Dash")
    await key(KEY_SPACE)
    require(current_scene.get_node("LogHeader").text.contains("Move 90 ft | Action spent"), "Second Dash spends the ordinary action")
    var prompts := ""
    for i in range(18):
        await key(KEY_A)
        prompts += current_scene.get_node("Prompt").text + "\n"
    require(not prompts.contains("Action Surge") and not prompts.contains("Dash"), "Exhausted actions leave the cycle")
    restore_files()
    print("Action Surge existing keyboard checks passed")
    quit(0)
