extends SceneTree
var fixtures := ""
var captures := ""
var demo := false
var path := ""
var original: Variant = null
func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--light-demo": demo = true
        if arg.begins_with("--light-fixtures="): fixtures = arg.trim_prefix("--light-fixtures=")
        if arg.begins_with("--light-captures="): captures = arg.trim_prefix("--light-captures=")
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
    require(not bytes.is_empty(), "Actual Light fixture exists")
    var f = FileAccess.open(path, FileAccess.WRITE); f.store_buffer(bytes); f.close()
    current_scene.get_node("Load").pressed.emit(); await settle()
func checkpoint() -> PackedByteArray:
    current_scene.get_node("Save").pressed.emit()
    return FileAccess.get_file_as_bytes(path)
func expect_native(name: String) -> void:
    require(checkpoint() == FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save")), "UI/native state match: " + name)
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
    require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Capture Light controls")
func run_checks() -> void:
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir()); root.gui_embed_subwindows = true
    for locale in (["en"] if demo else ["en","es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(); current_scene.set_process(false)
        for size in [Vector2i(1120,800),Vector2i(1920,1080)]:
            root.size = size; await settle(); await load_fixture("before")
            var weapons: OptionButton = current_scene.get_node("Weapons")
            var bonus: OptionButton = current_scene.get_node("CunningAction")
            var use: Button = current_scene.get_node("UseCunningAction")
            require(weapons.visible and weapons.item_count == 2 and not weapons.disabled, "Two physical weapons have selectable controls")
            require(weapons.position.x == 214 and weapons.size == Vector2(450,36), "Approved weapon selector layout")
            require(not bonus.visible, "No Light attack before Attack action")
            require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No combat save controls")
            weapons.grab_focus(); await key(KEY_ENTER); require(weapons.get_popup().visible, "Keyboard opens Weapon")
            weapons.get_popup().set_focused_item(0); await key(KEY_DOWN); await key(KEY_ENTER); expect_native("selected")
            weapons.release_focus(); current_scene.get_node("Melee").pressed.emit(); await settle(); await click_target(); await resolve_damage(); expect_native("qualified")
            require(bonus.visible and not use.disabled, "Qualifying Attack offers usable Bonus Action")
            require(bonus.position.y == weapons.position.y + 44, "Bonus Action row follows Weapon by44px")
            require(weapons.get_global_rect().end.y < bonus.get_global_rect().position.y and use.get_global_rect().end.y < root.size.y, "Rows fit without overlap")
            require(bonus.get_global_rect().end.x <= use.get_global_rect().position.x, "Long translated choices do not expand over the action button")
            require("Dagger" in bonus.get_item_text(bonus.selected) or "Daga" in bonus.get_item_text(bonus.selected), "Named extra attack")
            await capture("light-" + locale + "-" + str(size.x))
            var before := checkpoint(); use.grab_focus(); await key(KEY_ENTER); await key(KEY_ESCAPE)
            require(checkpoint() == before, "Escape targeting preserves actions, weapon identities, RNG and HP")
            use.grab_focus(); await key(KEY_ENTER); await key(KEY_RIGHT); await key(KEY_LEFT); await key(KEY_SPACE); await resolve_damage(); expect_native("after")
            require(use.disabled, "Bonus Action spent disables further Light attack")
            await load_fixture("qualified"); use.pressed.emit(); await settle(); await click_target(); await resolve_damage(); expect_native("after")
        current_scene.queue_free(); await settle()
    cleanup(); print("Light attack UI/native checks passed"); quit(0)
