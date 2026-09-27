extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--nick-demo": demo = true
        if arg.begins_with("--nick-fixtures="): fixtures = arg.trim_prefix("--nick-fixtures=")
        if arg.begins_with("--nick-captures="): captures = arg.trim_prefix("--nick-captures=")
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
    var host = current_scene.get_node("NickAttack") if current_scene and current_scene.get_node("NickAttack").visible else root
    for down in [true, false]:
        var e := InputEventKey.new(); e.keycode = code; e.pressed = down; host.push_input(e)
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
    await settle()
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir()); root.gui_embed_subwindows = true
    for locale in (["en"] if demo else ["en","es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(); current_scene.set_process(false)
        for size in [Vector2i(1120,800),Vector2i(1920,1080)]:
            root.size = size; await settle(); await load_fixture("before")
            var nick: Button = current_scene.get_node("Nick")
            var dialog: Window = current_scene.get_node("NickAttack")
            var choices: OptionButton = current_scene.get_node("NickAttack/Choices")
            var target: Button = current_scene.get_node("NickAttack/Target")
            require(nick.is_visible_in_tree() and nick.disabled, "Nick visible but disabled before Attack")
            require(nick.size == Vector2(174,36), "Approved Nick button size")
            require(nick.position == current_scene.get_node("Decline").position, "Nick reuses the ordinary-turn reaction slot")
            require(not current_scene.get_node("Decline").visible, "Nick never overlaps a visible Decline control")
            root.gui_release_focus()
            if demo:
                current_scene.get_node("Melee").grab_focus(); await key(KEY_SPACE); current_scene.get_node("Melee").release_focus()
            else:
                var selected_name: String = TranslationServer.translate("Melee attack")
                for cycle in range(16):
                    if current_scene.get_node("Prompt").text.contains(selected_name): break
                    await key(KEY_A)
                require(current_scene.get_node("Prompt").text.contains(selected_name), "A selects qualifying Attack")
            await click_target(); await resolve_damage(); expect_native("qualified")
            require(not nick.disabled, "Qualifying Attack enables Nick")
            var before := checkpoint(); nick.grab_focus(); await key(KEY_SPACE)
            require(dialog.visible and dialog.size == Vector2i(640,300), "Approved Nick selector opens by keyboard")
            require(choices.item_count == 4 and not target.disabled, "Named physical weapons and attack modes are offered")
            require(str(choices.get_item_metadata(choices.selected)) == "nick_melee#2", "Different physical weapon is selected; first weapon is disabled: " + str(choices.get_item_metadata(choices.selected)))
            require(choices.get_global_rect().end.x <= 640 and target.get_rect().end.y <= 300, "Translated modal controls fit")
            await capture("nick-" + locale + "-" + str(size.x))
            var esc := InputEventKey.new(); esc.keycode = KEY_ESCAPE; esc.pressed = true; dialog.window_input.emit(esc); await settle()
            require(not dialog.visible and checkpoint() == before, "Escape cancels dialog without expenditure")
            nick.pressed.emit(); await settle(); target.grab_focus(); await key(KEY_ENTER)
            require(not dialog.visible, "Target closes selector")
            await key(KEY_ESCAPE); require(checkpoint() == before, "Escape cancels targeting without expenditure")
            nick.pressed.emit(); await settle(); target.pressed.emit(); await settle()
            await key(KEY_RIGHT); await key(KEY_LEFT); await key(KEY_SPACE); await resolve_damage(); expect_native("after")
            require(nick.visible and nick.disabled and current_scene.get_node("UseCunningAction").disabled, "Shared Nick/Light allowance is consumed")
            await load_fixture("qualified"); nick.pressed.emit(); await settle()
            for i in range(choices.item_count):
                if str(choices.get_item_metadata(i)) == "nick_throw#2": choices.select(i); choices.item_selected.emit(i)
            target.pressed.emit(); await settle(); await click_target(); await resolve_damage(); expect_native("thrown")
            await load_fixture("spent-bonus"); require(not nick.disabled, "Nick remains available after unrelated Bonus Action")
            nick.pressed.emit(); await settle()
            for i in range(choices.item_count):
                if str(choices.get_item_metadata(i)) == "nick_melee#2": choices.select(i); choices.item_selected.emit(i)
            target.pressed.emit(); await settle(); await click_target(); await resolve_damage(); expect_native("spent-after")
            require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No combat save controls")
        current_scene.queue_free(); await settle()
    cleanup(); print("Nick UI/native checks passed"); quit(0)
