extends SceneTree
var originals := {}
var path := ""
var fixtures := ""
var captures := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--cunning-fixtures="): fixtures = arg.trim_prefix("--cunning-fixtures=")
        if arg.begins_with("--cunning-capture="): captures = arg.trim_prefix("--cunning-capture=")
    call_deferred("run_checks")
func restore_files() -> void:
    for name in originals:
        if originals[name] == null:
            if FileAccess.file_exists(name): DirAccess.remove_absolute(name)
        else:
            var file := FileAccess.open(name, FileAccess.WRITE)
            file.store_buffer(originals[name]); file.close()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()
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
    for name in ["level1", "available", "spent", "offturn", "reaction", "decision"]:
        await load_fixture(name)
        var button: Button = current_scene.get_node("UseCunningAction")
        if name == "level1": require(not button.visible, "Level one cannot use Cunning Action")
        elif name in ["available", "spent", "offturn", "decision"]:
            require(button.visible and button.disabled == (name != "available"), "Entitlement visible and availability follows rules")
        else: require(not button.visible or button.disabled, "Reaction window blocks Cunning Action")
        if name == "decision":
            current_scene.get_node("TemporaryHP/Keep").pressed.emit()
            await settle()
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        current_scene.set_process(false)
        await load_fixture("available")
        var choice: OptionButton = current_scene.get_node("CunningAction")
        var button: Button = current_scene.get_node("UseCunningAction")
        require(choice.item_count == 2, "Only implemented Dash and Disengage choices")
        require(current_scene.get_node("CunningActionLabel").text == ("Bonus Action" if locale == "en" else "Acción adicional"), "Approved translated Bonus Action label")
        require(button.text == ("Use Bonus Action" if locale == "en" else "Usar acción adicional"), "Translated activation control")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle()
            require(button.position.y > current_scene.get_node("AdrenalineRush").position.y, "Approved row is below combat buttons")
            require(button.get_rect().end.x < root.size.x - 300 and button.get_rect().end.y <= current_scene.get_node("LogStack/Log").global_position.y, "Controls fit before sidebar and log")
            require(choice.focus_mode == Control.FOCUS_ALL and button.focus_mode == Control.FOCUS_ALL, "Both controls support keyboard focus")
            if not captures.is_empty():
                DirAccess.make_dir_recursive_absolute(captures)
                await RenderingServer.frame_post_draw
                require(root.get_texture().get_image().save_png(captures.path_join("cunning-" + locale + "-" + str(size.x) + ".png")) == OK, "Render capture saved")
        choice.grab_focus(); await key(KEY_ENTER); await key(KEY_DOWN); await key(KEY_ENTER)
        require(choice.selected == 1, "Keyboard chooses Disengage without executing an action")
        button.grab_focus(); await key(KEY_ENTER)
        require(button.disabled and current_scene.get_node("LogStack/Log").get_parsed_text().contains("disengages" if locale == "en" else "se destraba"), "Focused Enter executes Bonus Disengage")
        await load_fixture("available")
        choice.select(0)
        button.grab_focus(); await key(KEY_SPACE)
        require(button.disabled, "Focused Space executes Bonus Dash once")
    restore_files()
    print("Cunning Action controls passed")
    quit(0)
