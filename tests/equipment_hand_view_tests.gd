extends SceneTree
var fixture := ""
var captures := ""
var output := ""
var originals := {}
var locales := ["en", "es"]
const SLOT = "Equipment hands test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--hands-demo": locales = ["en"]
        if arg.begins_with("--hands-fixture="): fixture = arg.trim_prefix("--hands-fixture=")
        if arg.begins_with("--hands-captures="): captures = arg.trim_prefix("--hands-captures=")
        if arg.begins_with("--hands-output="): output = arg.trim_prefix("--hands-output=")
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
    require(not current_scene.get_node("SaveSlots").visible, "Fixture loaded: " + current_scene.get_node("SaveSlots/Status").text)
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
        var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(source); f.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        for member in [0,1]:
            var roster: ItemList = current_scene.get_node("PartyPanel/Roster")
            roster.select(member); roster.item_selected.emit(member); await settle()
            var inventory: ItemList = current_scene.get_node("PartyPanel/Inventory")
            var before: String = current_scene.get_node("PartyPanel/Sheet").text
            inventory.select(1); await press("PartyPanel/Equip")
            var dialog: Window = current_scene.get_node("EquipmentChoice")
            var hand: OptionButton = dialog.get_node("Hand")
            require(dialog.visible and dialog.size == Vector2i(660,340), "Approved hand dialog opens")
            require(hand.item_count == 2 and not hand.is_item_disabled(0) and hand.is_item_disabled(1), "Two-handed main weapon blocks the other hand")
            require(hand.focus_mode == Control.FOCUS_ALL and dialog.get_node("Equip").focus_mode == Control.FOCUS_ALL, "Keyboard access")
            await capture("hands-blocked-" + locale, dialog)
            await key(dialog, KEY_ESCAPE)
            require(not dialog.visible and current_scene.get_node("PartyPanel/Sheet").text == before, "Escape preserves inventory, wounds and equipment")
            inventory.select(1); await press("PartyPanel/Equip"); await press("EquipmentChoice/Cancel")
            require(current_scene.get_node("PartyPanel/Sheet").text == before, "Cancel preserves all state")
            inventory.select(1); await press("PartyPanel/Equip"); await press("EquipmentChoice/Equip")
            require(not dialog.visible and inventory.item_count == 5, "Main hand splits exactly one stack unit")
            inventory.select(1); await press("PartyPanel/Equip")
            require(not hand.is_item_disabled(1), "One-handed main weapon permits other hand")
            hand.select(1); hand.item_selected.emit(1); await settle()
            await capture("hands-other-" + locale, dialog)
            dialog.get_node("Equip").grab_focus(); await key(dialog, KEY_ENTER)
            require(not dialog.visible and inventory.item_count == 6, "Keyboard commits a distinct second stack unit")
            var main_label := "Main hand" if locale == "en" else "Mano principal"
            var other_label := "Other hand" if locale == "en" else "Otra mano"
            require(main_label in inventory.get_item_text(4) and other_label in inventory.get_item_text(5), "Inventory identifies both hands")
            var sheet: String = current_scene.get_node("PartyPanel/Sheet").text
            require(main_label in sheet and other_label in sheet, "Character sheet identifies both hands")
        await press("PartyPanel/Save"); current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Save through existing out-of-combat flow")
        if not output.is_empty():
            var target: String = output + "-" + locale + ".ogs"
            var saved = FileAccess.open(target, FileAccess.WRITE); saved.store_buffer(FileAccess.get_file_as_bytes(slot)); saved.close()
        current_scene.queue_free(); await settle()
    restore_files(); print("Equipment hand view checks passed"); quit(0)
