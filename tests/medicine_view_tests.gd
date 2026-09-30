extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--medicine-demo": demo = true
        if arg.begins_with("--medicine-fixtures="): fixtures = arg.trim_prefix("--medicine-fixtures=")
        if arg.begins_with("--medicine-capture="): captures = arg.trim_prefix("--medicine-capture=")
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
func key(window: Window, code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down; window.push_input(event)
    await settle()
func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Native Medicine fixture exists")
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit(); await settle()
func capture(name: String, window: Window) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    require(window.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Rendered check captured")
func run_checks() -> void:
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in (["en"] if demo else ["en", "es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        current_scene.set_process(false)
        var button: Button = current_scene.get_node("Stabilize")
        var mind: Window = current_scene.get_node("TacticalMind")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle(); await load_fixture("available")
            require(button.visible and not button.disabled, "Stabilize visible for legal target")
            require(button.text == ("Stabilize" if locale == "en" else "Estabilizar"), "Translated action label")
            require(button.get_rect().end.x <= root.size.x - 300 and current_scene.get_node("Log").size.y >= 48, "New control fits without hiding the log")
            await capture("row-" + locale + "-" + str(size.x), root)
            button.grab_focus(); await key(root, KEY_SPACE); await key(root, KEY_ESCAPE)
            require(not button.disabled, "Escape cancels targeting without spending Action")
            button.grab_focus(); await key(root, KEY_SPACE)
            var point: Vector2
            if demo:
                var tile := minf((root.size.x - 358.0 - 72.0) / 8.0, (root.size.y - 324.0) / 8.0)
                point = Vector2(24, 116) + Vector2(2.5, 1.5) * tile
            else:
                var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
                point = canvas.get_global_transform_with_canvas() * (Vector2(2.5, 1.5) * (canvas.get_combined_minimum_size().x / 8.0))
            for down in [true, false]:
                var event := InputEventMouseButton.new()
                event.button_index = MOUSE_BUTTON_LEFT; event.position = point; event.global_position = point; event.pressed = down
                root.push_input(event, true)
            await settle()
            require(mind.visible and button.disabled and current_scene.get_node("End").disabled, "Actual failed check opens exclusive choice after spending Action")
            var text: Label = mind.get_node("Text")
            require("10" in text.text and ("Second Wind" if locale == "en" else "Segundo aliento") in text.text, "Choice shows DC, modifier and resource")
            require(mind.get_node("Use").text == ("Use Tactical Mind" if locale == "en" else "Usar Mente táctica"), "Translated choice button")
            await capture("decision-" + locale + "-" + str(size.x), mind)
            mind.get_node("Skip").grab_focus(); await key(mind, KEY_ENTER)
            require(not mind.visible and button.disabled, "Keyboard decline resolves choice without restoring Action")
            await load_fixture("pending")
            require(mind.visible, "Pending native checkpoint restores visible decision")
            mind.get_node("Use").grab_focus(); await key(mind, KEY_ENTER)
            require(not mind.visible and not current_scene.get_node("End").disabled, "Keyboard use resolves decision and restores command access")
            require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No player combat saving")
            await load_fixture("available")
            button.grab_focus(); await key(root, KEY_SPACE)
            require("Patient" in current_scene.get_node("Prompt").text, "Keyboard targeting names the selected legal creature")
            if not demo:
                require(current_scene.get_node("Footer").is_visible_in_tree() and "Patient" in current_scene.get_node("Footer").text, "Keyboard target and instructions are visible in the main game")
            await key(root, KEY_RIGHT); await key(root, KEY_LEFT)
            await capture("keyboard-" + locale + "-" + str(size.x), root)
            await key(root, KEY_SPACE)
            require(mind.visible, "Entire stabilization action works without a mouse")
            mind.get_node("Skip").grab_focus(); await key(mind, KEY_ENTER)
            await load_fixture("combined")
            require(button.visible, "Stabilize remains visible beside Cunning Action")
            var names := ["Stabilize"] if demo else ["CunningActionLabel", "CunningAction", "UseCunningAction", "Stabilize"]
            for index in range(1, names.size()):
                var previous: Control = current_scene.get_node(names[index - 1])
                var next: Control = current_scene.get_node(names[index])
                require(previous.visible and next.visible and previous.get_rect().end.x <= next.position.x, "Combined action row has no overlap")
            await capture("combined-" + locale + "-" + str(size.x), root)

    cleanup(); print("Medicine and Tactical Mind controls passed"); quit()
