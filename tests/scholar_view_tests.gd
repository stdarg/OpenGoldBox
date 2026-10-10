extends SceneTree
var fixture := ""
var captures := ""
var output := ""
var originals := {}
var locales := ["en", "es"]
const SLOT = "SRD Scholar test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--scholar-demo": locales = ["en"]
        if arg.begins_with("--scholar-fixture="): fixture = arg.trim_prefix("--scholar-fixture=")
        if arg.begins_with("--scholar-capture="): captures = arg.trim_prefix("--scholar-capture=")
        if arg.begins_with("--scholar-output="): output = arg.trim_prefix("--scholar-output=")
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
    require(not b.disabled, "Disabled button: " + path)
    b.pressed.emit(); await settle()
func load_slot() -> void:
    await press("PartyPanel/Load")
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    var index := -1
    for i in range(list.item_count):
        if list.get_item_text(i) == SLOT: index = i
    require(index >= 0, "Fixture save slot found")
    list.select(index); list.item_selected.emit(index)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    require(not current_scene.get_node("SaveSlots").visible, "Campaign loaded: " + current_scene.get_node("SaveSlots/Status").text)
func key(window: Window, code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down
        if code == KEY_ESCAPE: window.window_input.emit(event)
        else: window.push_input(event, true)
    await settle()
func capture(name: String) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Capture review layout")
func choose_scholar() -> void:
    var dropdown: OptionButton = current_scene.get_node("LevelUp/AdvancementTraining")
    require(dropdown.visible and dropdown.item_count == 2, "Only the proficient eligible skill is offered")
    require(current_scene.get_node("LevelUp/Confirm").disabled, "Scholar requires a deliberate selection")
    dropdown.grab_focus()
    await key(current_scene.get_node("LevelUp"), KEY_SPACE)
    dropdown.get_popup().set_focused_item(0)
    for code in [KEY_DOWN, KEY_ENTER]:
        for down in [true, false]:
            var event := InputEventKey.new(); event.keycode = code; event.pressed = down
            root.push_input(event, true)
        await settle()
    require(dropdown.selected == 1 and not current_scene.get_node("LevelUp/Confirm").disabled, "Keyboard selects Scholar and enables confirmation")
func run_checks() -> void:
    require(not fixture.is_empty(), "Fixture argument required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]:
        originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    var source := FileAccess.get_file_as_bytes(fixture)
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var file = FileAccess.open(slot, FileAccess.WRITE); file.store_buffer(source); file.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
        await press("Party"); await load_slot()
        var list: ItemList = current_scene.get_node("PartyPanel/Roster")
        require(list.item_count == 1, "Fixture holds the one Scholar Wizard")
        await press("PartyPanel/Roster/Advance1")
        await choose_scholar()
        await press("LevelUp/Cancel")
        await press("PartyPanel/Roster/Advance1")
        require(current_scene.get_node("LevelUp/AdvancementTraining").selected == 0, "Cancel discards level-up edits")
        await choose_scholar()
        for size in [Vector2i(1120,800), Vector2i(1920,1080)]:
            root.size = size; await settle(); current_scene.get_node("LevelUp").popup_centered(); await settle(); await capture("scholar-level-up-%s-%d" % [locale,size.x])
        await press("LevelUp/Confirm")
        await press("LevelUp/Confirm") # Wizard spell page follows the Scholar selection.
        require(not current_scene.get_node("LevelUp").visible, "Scholar level-up confirmed")
        await press("PartyPanel/Save")
        current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Scholar choices save")
        if not output.is_empty():
            file = FileAccess.open(output, FileAccess.WRITE); file.store_buffer(FileAccess.get_file_as_bytes(slot)); file.close()
        await load_slot()
        require(list.item_count == 1, "Reload preserves the Scholar party")
    restore_files()
    print("Scholar view checks passed")
    quit(0)
