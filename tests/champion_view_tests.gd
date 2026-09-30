extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var creator := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--champion-demo": demo = true
        if arg == "--champion-creator": creator = true
        if arg.begins_with("--champion-fixtures="): fixtures = arg.trim_prefix("--champion-fixtures=")
        if arg.begins_with("--champion-capture="): captures = arg.trim_prefix("--champion-capture=")
    call_deferred("run_checks")
func cleanup() -> void:
    if path.is_empty(): return
    if original == null:
        if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
    else:
        var file := FileAccess.open(path, FileAccess.WRITE)
        file.store_buffer(original)
func require(ok: bool, message: String) -> void:
    if not ok:
        cleanup(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(8): await process_frame
func key(window: Window, code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down; window.push_input(event)
    await settle()
func load_fixture() -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join("pending.save"))
    require(not bytes.is_empty(), "Actual critical fixture exists")
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit(); await settle()
func capture(name: String, window: Window) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    require(window.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Champion render captured")
func run_checks() -> void:
    if not creator:
        path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
        original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
        DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in (["en"] if demo else ["en", "es"]):
        TranslationServer.set_locale(locale)
        if creator:
            change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await settle()
            var modal: Window = current_scene.get_node("LevelUp")
            require(modal.visible and "3" in modal.get_node("Title").text, "Ordinary level-three confirmation opens")
            require(("Champion" if locale == "en" else "Campeón") in modal.get_node("Note").text, "Confirmation names the SRD subclass and features")
            for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
                root.size = size; await settle(); await capture("acquisition-" + locale + "-" + str(size.x), modal)
            modal.get_node("Confirm").grab_focus(); await key(modal, KEY_ENTER)
            require(not modal.visible, "Keyboard confirmation applies level three")
            current_scene.get_node("PartyPanel/Roster/Advance1").pressed.emit(); await settle()
            require(modal.visible and "4" in modal.get_node("Title").text, "Confirmed Fighter is now ready for level four")
            continue
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        current_scene.set_process(false)
        var finish: Button = current_scene.get_node("End")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle(); await load_fixture()
            require(finish.visible and not finish.disabled and finish.text == ("Finish free move" if locale == "en" else "Finalizar mov. gratis"), "Existing End control offers Finish free move")
            require(current_scene.get_node("Dash").disabled, "Other actions wait")
            require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No combat saving controls")
            await capture("movement-" + locale + "-" + str(size.x), root)
            await key(root, KEY_LEFT)
            var hint: Label = current_scene.get_node("Prompt" if demo else "Footer")
            require("10" in hint.text, "Arrow movement spends five feet of free allowance")
            await key(root, KEY_ESCAPE)
            require(finish.text == ("End turn" if locale == "en" else "Terminar turno") and not finish.disabled, "Escape completes free move without ending turn")
            await load_fixture()
            var point: Vector2
            if demo:
                var tile := minf((root.size.x - 358.0 - 72.0) / 8.0, (root.size.y - 324.0) / 8.0)
                point = Vector2(24, 116) + Vector2(1.5, 2.5) * tile
            else:
                var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
                point = canvas.get_global_transform_with_canvas() * (Vector2(1.5, 2.5) * (canvas.get_combined_minimum_size().x / 8.0))
            for down in [true, false]:
                var event := InputEventMouseButton.new()
                event.button_index = MOUSE_BUTTON_LEFT; event.position = point; event.global_position = point; event.pressed = down
                root.push_input(event, true)
            await settle()
            require("10" in hint.text, "Clicking a legal battlefield destination spends free movement")
            await load_fixture()
            finish.grab_focus(); await key(root, KEY_SPACE)
            require(finish.text == ("End turn" if locale == "en" else "Terminar turno"), "Finish supports standard keyboard activation")
    cleanup(); print("Champion controls passed"); quit()
