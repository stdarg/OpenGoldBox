extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--posture-demo": demo = true
        if arg.begins_with("--posture-fixtures="): fixtures = arg.trim_prefix("--posture-fixtures=")
        if arg.begins_with("--posture-capture="): captures = arg.trim_prefix("--posture-capture=")
    call_deferred("run_checks")
func cleanup() -> void:
    if original == null:
        if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
    else:
        var file := FileAccess.open(path, FileAccess.WRITE)
        file.store_buffer(original)
func require(ok: bool, message: String) -> void:
    if not ok:
        cleanup(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down; root.push_input(event)
    await settle()
func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Native posture fixture exists")
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit(); await settle()
func run_checks() -> void:
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in (["en"] if demo else ["en", "es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        current_scene.set_process(false)
        # A character woken Prone by a rest-interrupting encounter, on its turn.
        await load_fixture("stand")
        var stand: Button = current_scene.get_node("StandUp")
        require(stand.visible and not stand.disabled, "A woken Prone character can stand")
        require(stand.text == ("Stand up" if locale == "en" else "Levantarse"), "Stand translation")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle()
            require(stand.get_rect().end.y <= current_scene.get_node("LogHeader").position.y, "Stand up fits above the log")
            require(current_scene.get_node("Log").get_rect().end.y - current_scene.get_node("LogHeader").position.y >= 48, "The posture row preserves readable log space")
            require(stand.focus_mode == Control.FOCUS_ALL, "Keyboard focus retained")
            if not captures.is_empty():
                DirAccess.make_dir_recursive_absolute(captures)
                await RenderingServer.frame_post_draw
                root.get_texture().get_image().save_png(captures.path_join(locale + "-" + str(size.x) + ".png"))
        stand.grab_focus(); await key(KEY_SPACE)
        require(not stand.visible, "Keyboard stand removes Prone")
        require(not current_scene.has_node("WakeAlly") and not current_scene.has_node("PickUp"), "No Wake ally or Pick up controls remain")
        require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No player combat saving")
    cleanup(); print("Posture controls passed"); quit()
