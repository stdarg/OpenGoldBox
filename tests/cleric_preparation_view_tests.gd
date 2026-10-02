extends SceneTree
var fixture := ""
var captures := ""
var output := ""
var originals := {}
var locales := ["en", "es"]
const SLOT = "Cleric choices test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--cleric-demo": locales = ["en"]
        if arg.begins_with("--cleric-fixture="): fixture = arg.trim_prefix("--cleric-fixture=")
        if arg.begins_with("--cleric-captures="): captures = arg.trim_prefix("--cleric-captures=")
        if arg.begins_with("--cleric-output="): output = arg.trim_prefix("--cleric-output=")
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
func run_checks() -> void:
    require(not fixture.is_empty(), "Fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    var source := FileAccess.get_file_as_bytes(fixture)
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var heading := "Prepared spells" if locale == "en" else "Conjuros preparados"
        var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(source); f.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        await press("PartyPanel/Roster/Advance1")
        var level: Window = current_scene.get_node("LevelUp")
        require(not level.get_node("Spell0").visible, "The old Cleric spell checkboxes are gone")
        await press("LevelUp/Confirm")
        require(level.get_node("SpellChoicesPage").visible and level.get_node("Back").visible, "Cleric second page presents prepared spells")
        var rows = level.get_node("SpellChoicesPage/Rows")
        for spell in ["cure_wounds", "healing_word", "inflict_wounds"]:
            var box: CheckBox = rows.get_node("prepared/" + spell)
            require(box.button_pressed and box.disabled, "Earlier preparation is locked at level-up: " + spell)
        require(rows.get_node("prepared/Count").text == heading + " (3 / 5)" and rows.get_node("prepared/Pending").visible, "Level two prepares five, two pending")
        await press("LevelUp/Confirm")
        require(not level.visible, "Cleric level two completes")
        await press("PartyPanel/Roster/Advance1"); await press("LevelUp/Confirm")
        var blind: CheckBox = rows.get_node("prepared/blindness")
        require(blind.visible and blind.button_pressed and not blind.disabled, "Level three proposes a new level-two spell")
        blind.set_pressed(false); await settle()
        require(level.get_node("Confirm").disabled, "Every available spell must be prepared")
        blind.grab_focus(); await key(level, KEY_SPACE)
        require(blind.button_pressed and not level.get_node("Confirm").disabled, "Keyboard prepares Blindness")
        await capture("cleric-advance-" + locale, level)
        await press("LevelUp/Back"); await press("LevelUp/Confirm")
        require(blind.button_pressed, "Preparation survives Back")
        await press("LevelUp/Confirm")
        require(not level.visible, "Cleric level three completes")
        await press("PartyPanel/Save"); current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Cleric choices save")
        if not output.is_empty():
            f = FileAccess.open(output, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(slot)); f.close()
        await load_slot()
    restore_files(); print("Cleric choices view checks passed"); quit(0)
