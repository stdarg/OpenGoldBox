extends SceneTree
var fixture := ""
var prefer_gwf := false
var selected_gwf := false
var captures := ""
var output := ""
var originals := {}
var locales := ["en", "es"]
const SLOT = "SRD Review Training test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--review-gwf": prefer_gwf = true
        if arg == "--review-demo": locales = ["en"]
        if arg.begins_with("--review-fixture="): fixture = arg.trim_prefix("--review-fixture=")
        if arg.begins_with("--review-capture="): captures = arg.trim_prefix("--review-capture=")
        if arg.begins_with("--review-output="): output = arg.trim_prefix("--review-output=")
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
func choose_missing() -> void:
    # Follow visible controls in order, querying again after each choice because
    # Rogue Expertise options depend on skills selected above it.
    for pass_index in range(4):
        for group in current_scene.get_node("TrainingReview/Training/Rows").get_children():
            if not group.visible: continue
            for control in group.get_children():
                if control is OptionButton and control.visible and not control.disabled and control.selected == 0:
                    var chosen := 1
                    if prefer_gwf:
                        for i in range(control.item_count):
                            if control.get_item_text(i) in ["Great Weapon Fighting", "Combate con armas a dos manos"] and not control.is_item_disabled(i):
                                chosen = i; selected_gwf = true
                    control.select(chosen); control.item_selected.emit(chosen); await settle()
                elif control is CheckBox and control.visible and not control.disabled and not control.button_pressed:
                    control.button_pressed = true; await settle()
    var style: OptionButton = current_scene.get_node_or_null("TrainingReview/Training/Rows/Group1/Choice")
    if current_scene.get_node("TrainingReview/Apply").disabled and style != null and style.visible and not style.disabled:
        require(not current_scene.get_node("TrainingReview/Error").text.is_empty(), "Conflicting advancement explains rejection")
        style.select(2); style.item_selected.emit(2); await settle()
    require(not current_scene.get_node("TrainingReview/Apply").disabled, "All required choices can be completed")
func run_checks() -> void:
    require(not fixture.is_empty(), "Fixture argument required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]:
        originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    var source := FileAccess.get_file_as_bytes(fixture)
    require(not source.is_empty(), "Native campaign fixture exists")
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var file = FileAccess.open(slot, FileAccess.WRITE); file.store_buffer(source); file.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
        await press("Party"); await load_slot()
        var list: ItemList = current_scene.get_node("PartyPanel/Roster")
        require(list.item_count == 5, "Prior-writer fixture contains PCs, NPC and reserve")
        for index in range(5):
            list.select(index); list.item_selected.emit(index); await settle()
            var button: Button = current_scene.get_node("PartyPanel/ReviewTraining")
            require(button.visible and not button.disabled, "Pending character offers review")
            var original_sheet: String = current_scene.get_node("PartyPanel/Sheet").text
            await press("PartyPanel/ReviewTraining")
            var window: Window = current_scene.get_node("TrainingReview")
            require(window.visible and current_scene.get_node("TrainingReview/Apply").disabled, "Review opens incomplete")
            if locale == "es": require(button.text == "Revisar formación", "Translated review button")
            var locked := []
            for group in window.get_node("Training/Rows").get_children():
                if not group.visible: continue
                for control in group.get_children():
                    if control is CheckBox and control.visible and control.button_pressed:
                        require(control.disabled, "Original checkbox is locked"); locked.append(control)
                    if control is OptionButton and control.visible and control.selected > 0:
                        require(control.disabled, "Original Fighting Style is locked"); locked.append(control)
            if index in [0,4]: require(not locked.is_empty(), "Fixture exercises locked selections")
            await choose_missing()
            for control in locked: require(control.disabled, "Existing choices remain locked after refresh")
            await press("TrainingReview/Cancel")
            require(not window.visible and current_scene.get_node("PartyPanel/Sheet").text == original_sheet, "Cancel discards changes")
            await press("PartyPanel/ReviewTraining")
            require(window.get_node("Apply").disabled, "Reopen restores original incomplete choices")
            await key(window, KEY_ESCAPE)
            require(not window.visible and current_scene.get_node("PartyPanel/Sheet").text == original_sheet, "Escape discards changes")
            await press("PartyPanel/ReviewTraining")
            # Keyboard focus cycles from Cancel to Apply (when enabled).
            await choose_missing()
            window.get_node("Cancel").grab_focus(); await key(window, KEY_TAB)
            require(window.get_node("Apply").has_focus(), "Keyboard reaches Apply")
            for size in [Vector2i(1120,800), Vector2i(1920,1080)]:
                root.size = size; await settle()
                require(button.get_rect().end.x <= size.x - 20 and button.position.y >= current_scene.get_node("PartyPanel/Inventory").get_rect().end.y, "Review button fits below inventory")
                if index in [0,2]: await capture("review-%s-%d-%d" % [locale,size.x,index])
            window.get_node("Apply").grab_focus(); await key(window, KEY_ENTER)
            require(not window.visible and not button.visible, "Apply completes training and hides review: " + window.get_node("Error").text)
        # Existing save controls exercise persistence; no combat saving is added.
        await press("PartyPanel/Save")
        current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Applied training saves")
        if not output.is_empty():
            file = FileAccess.open(output, FileAccess.WRITE); file.store_buffer(FileAccess.get_file_as_bytes(slot)); file.close()
        await load_slot()
        for index in range(5):
            list.select(index); list.item_selected.emit(index); await settle()
            require(not current_scene.get_node("PartyPanel/ReviewTraining").visible, "Reload preserves completed training")
        # Restore pending characters, enter combat, attempt the same review action.
        file = FileAccess.open(slot, FileAccess.WRITE); file.store_buffer(source); file.close()
        await load_slot(); await press("PartyPanel/Combat")
        current_scene.get_node("PartyPanel/ReviewTraining").pressed.emit(); await settle()
        require(not current_scene.get_node("TrainingReview").visible, "Combat blocks review even when invoked directly")
    require(not prefer_gwf or selected_gwf, "Review Training selected Great Weapon Fighting")
    restore_files()
    print("Review Training view checks passed")
    quit(0)
