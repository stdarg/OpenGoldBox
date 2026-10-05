extends SceneTree
var fixture := ""
var captures := ""
var output := ""
var originals := {}
var locales := ["en", "es"]
const SLOT = "Blessed Warrior test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--blessed-demo": locales = ["en"]
        if arg.begins_with("--blessed-fixture="): fixture = arg.trim_prefix("--blessed-fixture=")
        if arg.begins_with("--blessed-captures="): captures = arg.trim_prefix("--blessed-captures=")
        if arg.begins_with("--blessed-output="): output = arg.trim_prefix("--blessed-output=")
    call_deferred("run_checks")
func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var f = FileAccess.open(path, FileAccess.WRITE); f.store_buffer(originals[path]); f.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(5): await process_frame
func press(path: String) -> void:
    var b: Button = current_scene.get_node(path)
    require(not b.disabled, "Disabled control: " + path)
    b.pressed.emit(); await settle()
func key(window: Window, code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down
        if code == KEY_ESCAPE: window.window_input.emit(event)
        else: window.push_input(event, true)
    await settle()
func load_slot() -> void:
    await press("PartyPanel/Load")
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    var index := -1
    for i in range(list.item_count):
        if list.get_item_text(i) == SLOT: index = i
    require(index >= 0, "Fixture slot found")
    list.select(index); list.item_selected.emit(index)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    require(not current_scene.get_node("SaveSlots").visible, "Fixture loaded")
func capture(name: String, window: Window) -> void:
    for size in [Vector2i(1120,800), Vector2i(1920,1080)]:
        root.size = size; await settle(); window.popup_centered(); await settle()
        if not captures.is_empty():
            DirAccess.make_dir_recursive_absolute(captures); await RenderingServer.frame_post_draw
            require(root.get_texture().get_image().save_png(captures.path_join(name + "-" + str(size.x) + ".png")) == OK, "Capture layout")
func select_style(level: Window, id_label: String) -> void:
    var style: OptionButton = level.get_node("FightingStyle")
    for i in range(style.item_count):
        if style.get_item_text(i) == id_label:
            style.select(i); style.item_selected.emit(i); await settle(); return
    require(false, "Fighting Style offered: " + id_label)
func run_checks() -> void:
    require(not fixture.is_empty(), "Fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    var source := FileAccess.get_file_as_bytes(fixture)
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var blessed := "Blessed Warrior" if locale == "en" else "Guerrero bendito"
        var defense := "Defense" if locale == "en" else "Defensa"
        var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(source); f.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        await press("PartyPanel/Roster/Advance1")
        var level: Window = current_scene.get_node("LevelUp")
        await select_style(level, blessed)
        await press("LevelUp/Confirm")
        var rows = level.get_node("SpellChoicesPage/Rows")
        var flame = rows.get_node_or_null("cantrips_2/sacred_flame")
        require(level.get_node("SpellChoicesPage").visible and flame != null and flame.visible, "Blessed Warrior offers Cleric cantrips: " + locale)
        flame.grab_focus(); await key(level, KEY_SPACE)
        require(flame.button_pressed, "Keyboard learns Sacred Flame: " + locale)
        await capture("blessed-warrior-" + locale, level)
        await press("LevelUp/Back")
        await select_style(level, defense)
        await press("LevelUp/Confirm")
        var gone = rows.get_node_or_null("cantrips_2")
        require(gone == null or not gone.visible, "A Fighting Style feat offers no cantrips: " + locale)
        await press("LevelUp/Back")
        await select_style(level, blessed)
        await press("LevelUp/Confirm")
        flame = rows.get_node("cantrips_2/sacred_flame")
        require(flame.visible and not flame.button_pressed, "Changing the style dropped the earlier pick: " + locale)
        flame.grab_focus(); await key(level, KEY_SPACE)
        await press("LevelUp/Confirm")
        require(not level.visible, "Blessed Warrior level two completes: " + locale)
        await press("PartyPanel/Save"); current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Blessed Warrior saves")
        var saved := FileAccess.get_file_as_bytes(slot).get_string_from_utf8()
        require(saved.contains("feature:blessed_warrior") and saved.contains("spell:sacred_flame"), "The save records Blessed Warrior and Sacred Flame: " + locale)
        if not output.is_empty():
            f = FileAccess.open(output, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(slot)); f.close()
        await load_slot()
    restore_files(); print("Blessed Warrior view checks passed"); quit(0)
