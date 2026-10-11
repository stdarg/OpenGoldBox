extends SceneTree
var originals := {}
var save_path := ""
var fixtures := ""
var captures := ""
var demo := false
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--rogue-fixtures="): fixtures = arg.trim_prefix("--rogue-fixtures=")
        if arg.begins_with("--rogue-captures="): captures = arg.trim_prefix("--rogue-captures=")
        if arg == "--rogue-demo": demo = true
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
func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Current native fixture exists")
    var file := FileAccess.open(save_path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
func press(window: Window, button: Button) -> void:
    button.grab_focus(); await settle()
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = KEY_ENTER; event.pressed = down
        window.push_input(event)
    await settle()
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down; root.push_input(event)
    await settle()
func capture(name: String, window: Window) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    require(window.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Decision render saved")
func run_checks() -> void:
    save_path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[save_path + suffix] = FileAccess.get_file_as_bytes(save_path + suffix) if FileAccess.file_exists(save_path + suffix) else null
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
    current_scene.set_process(false)
    require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No player combat-saving controls")
    var locales := ["en"] if demo else ["en", "es"]
    for locale in locales:
        TranslationServer.set_locale(locale)
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle()
            await load_fixture("aim-available")
            var choices: OptionButton = current_scene.get_node("CunningAction")
            var use_bonus: Button = current_scene.get_node("UseCunningAction")
            require(choices.visible and choices.item_count == 3 and not choices.is_item_disabled(2), "Steady Aim joins existing Bonus Action choices")
            require(choices.focus_mode == Control.FOCUS_ALL and use_bonus.focus_mode == Control.FOCUS_ALL, "Bonus controls support keyboard focus")
            if demo:
                require(choices.position.x == 214 and choices.size.x == 200 and use_bonus.position.x == 424 and use_bonus.size.x == 240, "Approved demo Bonus Action widths/placement")
                require(choices.position.y + 44 == current_scene.get_node("Stabilize").position.y, "Bonus row precedes recovery row by 44 pixels")
                require(use_bonus.get_rect().end.y <= current_scene.get_node("LogStack/Log").global_position.y and current_scene.get_node("LogStack/Log").get_global_rect().end.y <= current_scene.get_node("Footer").position.y, "Bonus row, log and footer do not overlap")
            for index in range(2):
                await load_fixture("aim-available")
                choices.select(index); choices.item_selected.emit(index); await settle()
                await press(root, use_bonus)
                current_scene.get_node("Save").pressed.emit(); await settle()
                var verb := "cunning_dash" if index == 0 else "cunning_disengage"
                require(FileAccess.get_file_as_bytes(save_path) == FileAccess.get_file_as_bytes(fixtures.path_join(verb + ".save")), "Bonus dropdown activates selected native command exactly")
                require(use_bonus.disabled, "Other Bonus Actions consume the shared budget")
            await load_fixture("aim-available")
            choices.grab_focus(); await key(KEY_ENTER)
            require(choices.get_popup().visible, "Keyboard opens Bonus Action choices")
            choices.get_popup().set_focused_item(1); await key(KEY_DOWN); await key(KEY_ENTER)
            require(choices.selected == 2, "Keyboard selects Steady Aim")
            require(not use_bonus.disabled, "Selected legal Steady Aim can activate")
            await capture(locale + "-" + str(size.x) + "-aim", root)
            await press(root, use_bonus)
            require(use_bonus.disabled and current_scene.get_node("Move").disabled, "Aim spends Bonus Action and blocks movement")
            current_scene.get_node("Save").pressed.emit(); await settle()
            require(FileAccess.get_file_as_bytes(save_path) == FileAccess.get_file_as_bytes(fixtures.path_join("aim-spent.save")), "UI Aim matches native spending exactly")
    restore_files(); print("Rogue attack UI checks passed"); quit(0)
