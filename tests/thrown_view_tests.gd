extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--thrown-demo": demo = true
        if arg.begins_with("--thrown-fixtures="): fixtures = arg.trim_prefix("--thrown-fixtures=")
        if arg.begins_with("--thrown-capture="): captures = arg.trim_prefix("--thrown-capture=")
    call_deferred("run_checks")
func cleanup() -> void:
    if path.is_empty(): return
    if original == null:
        if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
    else:
        var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(original)
func require(ok: bool, message: String) -> void:
    if not ok:
        cleanup(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(8): await process_frame
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down; root.push_input(event)
    await settle()
func load_fixture() -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join("before.save"))
    require(not bytes.is_empty(), "Actual carried inventory fixture exists")
    var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit(); await settle()
func checkpoint() -> PackedByteArray:
    current_scene.get_node("Save").pressed.emit()
    return FileAccess.get_file_as_bytes(path)
func capture(name: String) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Thrown controls rendered")
func run_checks() -> void:
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in (["en"] if demo else ["en", "es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        current_scene.set_process(false)
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle(); await load_fixture()
            var choices: OptionButton = current_scene.get_node("ThrownWeapon")
            var button: Button = current_scene.get_node("Throw")
            require(choices.visible and choices.item_count == 7 and not button.disabled, "All seven carried weapon types have legal controls")
            require(not ("×" in choices.get_item_text(0)) and not (("stow" if locale == "en" else "guardar") in choices.get_item_text(0)), "Thrown weapons list by name, like ammunition, with no count or stowing")
            require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No combat saving controls added")
            require(button.get_global_rect().end.y <= root.size.y and choices.get_global_rect().end.x <= button.get_global_rect().position.x, "Controls fit without overlap")
            require(current_scene.get_node("Log").get_rect().end.y <= current_scene.get_node("Footer").position.y, "Combat log remains above footer")
            if not demo:
                require(button.get_rect().end.x <= current_scene.get_node("BattlefieldScroll").get_rect().end.x, "Thrown controls stay inside combat column")
            await capture("choices-" + locale + "-" + str(size.x))
            var before := checkpoint()
            button.grab_focus(); await key(KEY_ENTER); await key(KEY_ESCAPE)
            require(checkpoint() == before, "Canceling keyboard targeting spends no item, action, movement or RNG")
            choices.grab_focus(); await key(KEY_ENTER)
            require(choices.get_popup().visible, "Keyboard opens weapon dropdown")
            choices.get_popup().set_focused_item(0); await key(KEY_DOWN); await key(KEY_ENTER)
            require(choices.selected == 1, "Dropdown supports keyboard weapon selection")
            button.grab_focus(); await key(KEY_ENTER); await key(KEY_RIGHT); await key(KEY_LEFT); await key(KEY_SPACE)
            require(button.disabled and choices.item_count == 7, "Keyboard confirmation spends the Action and keeps the thrown weapon")
            await capture("thrown-" + locale + "-" + str(size.x))
            await load_fixture()
            button.pressed.emit(); await settle()
            var point: Vector2
            if demo:
                var tile := minf((root.size.x - 358.0 - 72.0) / 10.0, (root.size.y - 368.0) / 8.0)
                point = Vector2(24, 116) + Vector2(2.5, 1.5) * tile
            else:
                var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
                point = canvas.get_global_transform_with_canvas() * (Vector2(2.5, 1.5) * (canvas.get_combined_minimum_size().x / 10.0))
            for down in [true, false]:
                var event := InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT
                event.position = point; event.global_position = point; event.pressed = down; root.push_input(event, true)
            await settle()
            require(button.disabled and choices.item_count == 7, "Click target throws the selected weapon and keeps it")
    cleanup(); print("Thrown weapon controls passed"); quit()
