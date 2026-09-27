extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--mastery-demo": demo = true
        if arg.begins_with("--mastery-fixtures="): fixtures = arg.trim_prefix("--mastery-fixtures=")
        if arg.begins_with("--mastery-captures="): captures = arg.trim_prefix("--mastery-captures=")
    call_deferred("run_checks")
func cleanup() -> void:
    if path.is_empty(): return
    if original == null:
        if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
    else:
        var f = FileAccess.open(path, FileAccess.WRITE); f.store_buffer(original); f.close()
func require(ok: bool, message: String) -> void:
    if not ok:
        cleanup(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(6): await process_frame
func key(code: Key) -> void:
    for down in [true, false]:
        var e := InputEventKey.new(); e.keycode = code; e.pressed = down; root.push_input(e)
    await settle()
func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Actual Mastery fixture exists")
    var f = FileAccess.open(path, FileAccess.WRITE); f.store_buffer(bytes); f.close()
    current_scene.get_node("Load").pressed.emit(); await settle()
func checkpoint() -> PackedByteArray:
    current_scene.get_node("Save").pressed.emit()
    return FileAccess.get_file_as_bytes(path)
func expect_native(name: String) -> void:
    var actual := checkpoint()
    var expected := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    if actual != expected:
        var output := FileAccess.open("/tmp/mastery-ui-actual.save", FileAccess.WRITE); output.store_buffer(actual); output.close()
    require(actual == expected, "UI/native state match: " + name)
func resolve_damage() -> void:
    for n in range(3):
        if current_scene.get_node("SavageAttacker").visible:
            current_scene.get_node("SavageAttacker/Skip").pressed.emit(); await settle()
func click_target() -> void:
    var point: Vector2
    if demo:
        var weapons: OptionButton = current_scene.get_node("Weapons")
        var tile := (weapons.position.y - 16 - 116) / 8.0
        point = Vector2(24,116) + Vector2(2.5,1.5) * tile
    else:
        var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
        point = canvas.get_global_transform_with_canvas() * (Vector2(2.5,1.5) * canvas.get_combined_minimum_size().x / 12.0)
    for down in [true,false]:
        var e := InputEventMouseButton.new(); e.button_index = MOUSE_BUTTON_LEFT; e.position = point; e.global_position = point; e.pressed = down; root.push_input(e,true)
    await settle()
func capture(name: String) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures); await RenderingServer.frame_post_draw
    require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Capture Mastery controls")
func run_checks() -> void:
    await settle() # Let startup settings finish before selecting locale and size.
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir()); root.gui_embed_subwindows = true
    for locale in (["en"] if demo else ["en","es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(); current_scene.set_process(false)
        for size in [Vector2i(1120,800),Vector2i(1920,1080)]:
            root.size = size; await settle()
            for weapon in ["mace", "rapier"]:
                await load_fixture(weapon + "-before")
                var melee: Button = current_scene.get_node("Melee")
                require(not melee.disabled, "Chosen mastery weapon can attack")
                root.gui_release_focus()
                if demo:
                    require(melee.is_visible_in_tree(), "Demo melee button is visible")
                    melee.grab_focus(); await key(KEY_SPACE); melee.release_focus()
                else:
                    var selected_name: String = TranslationServer.translate("Melee attack")
                    for cycle in range(16):
                        if current_scene.get_node("Prompt").text.contains(selected_name): break
                        await key(KEY_A)
                    require(current_scene.get_node("Prompt").text.contains(selected_name), "Keyboard action cycle selects melee")
                await click_target()
                expect_native(weapon + "-after")
                var log: RichTextLabel = current_scene.get_node("Log")
                var effect := ("Sap" if weapon == "mace" else "Vex") if locale == "en" else ("Debilitar" if weapon == "mace" else "Hostigar")
                var explanation := "Target gains " + effect + " from Master." if locale == "en" else "Target recibe " + effect + " de Master."
                require(log.is_visible_in_tree() and log.get_parsed_text().contains(explanation), "Visible combat log names applied mastery and source")
                log.scroll_to_line(log.get_line_count() - 1); await settle()
                require(log.get_global_rect().end.x <= root.size.x and log.get_global_rect().end.y < root.size.y, "Combat log fits viewport")
                require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No combat saving controls")
                await capture(weapon + "-" + locale + "-" + str(size.x))
                await load_fixture(weapon + "-after"); expect_native(weapon + "-after")
        current_scene.queue_free(); await settle()
    cleanup(); print("Sap/Vex UI/native checks passed"); quit(0)
