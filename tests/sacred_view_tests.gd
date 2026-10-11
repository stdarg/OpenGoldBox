extends SceneTree
var captures := ""
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--sacred-capture="): captures = arg.trim_prefix("--sacred-capture=")
        if arg.begins_with("--sacred-fixtures="): fixtures = arg.trim_prefix("--sacred-fixtures=")
    call_deferred("run_checks")
func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            file.store_buffer(originals[path]); file.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func load_fixture(which: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(which + ".save"))
    require(not bytes.is_empty(), "Native character fixture exists")
    var file := FileAccess.open(save_path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()
func click_cell(cell: Vector2) -> void:
    var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
    var tile := canvas.get_combined_minimum_size().x / 12.0
    var position := canvas.get_global_transform_with_canvas() * ((cell + Vector2(0.5, 0.5)) * tile)
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = position
        root.push_input(event, true)
    await settle()
func run_checks() -> void:
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[save_path + suffix] = FileAccess.get_file_as_bytes(save_path + suffix) if FileAccess.file_exists(save_path + suffix) else null
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    combat.set_process(false)
    root.size = Vector2i(1120, 800)
    await settle()
    require(not combat.get_node("Save").visible and not combat.get_node("Load").visible, "No player combat saving")
    for which in ["blocked", "unknown", "known"]:
        await load_fixture(which)
        var sacred: Button = combat.get_node("CastCantrip")
        require(sacred.visible == (which != "unknown"), "Only Clerics knowing Sacred Flame see its button")
        var spells: OptionButton = combat.get_node("Cantrip")
        require(spells.item_count == (0 if which == "unknown" else 1), "Cleric list omits unlearned cantrips")
        if which != "unknown": require(spells.get_item_metadata(0) == "sacred_flame", "Cleric choice is Sacred Flame")
        require(sacred.disabled == (which != "known"), "Eligibility follows knowledge and armor training")
        require(sacred.position.x >= combat.get_node("AdrenalineRush").get_rect().end.x and sacred.get_rect().end.x < root.size.x - 300, "Sacred Flame occupies approved first spell position")
        var prompts := ""
        for i in range(18):
            await key(KEY_A)
            prompts += combat.get_node("Prompt").text + "\n"
        require(prompts.contains("Sacred Flame") == (which == "known"), "Keyboard cycle follows current eligibility")
    await load_fixture("known")
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        combat = current_scene
        combat.set_process(false)
        await load_fixture("known")
        require(combat.get_node("Cantrip").get_item_text(0) == ("Sacred Flame" if locale == "en" else "Llama sagrada"), "Rendered locale is active")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size
            await settle()
            require(combat.get_node("CastCantrip").get_rect().end.x < root.size.x - 300, "Translated spell control fits")
            if not captures.is_empty():
                DirAccess.make_dir_recursive_absolute(captures)
                await RenderingServer.frame_post_draw
                require(root.get_texture().get_image().save_png(captures.path_join("combat-" + locale + "-" + str(size.x) + ".png")) == OK, "Combat row rendered")
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    combat = current_scene
    combat.set_process(false)
    await load_fixture("known")
    root.size = Vector2i(1120, 800)
    await settle()
    var sacred: Button = combat.get_node("CastCantrip")
    sacred.grab_focus()
    await key(KEY_ENTER)
    require(combat.get_node("Prompt").text.contains("Sacred Flame") and not sacred.disabled, "Enter selects the spell without consuming Action")
    await click_cell(Vector2(3, 1))
    require(sacred.disabled, "Clicking a legal ally casts and consumes Action")
    require(combat.selected_character_id() == 1, "Spell target click does not switch selection to ally")
    require(combat.get_node("LogStack/Log").get_parsed_text().contains("Ally"), "Cast log identifies the chosen ally")
    sacred.release_focus()
    await load_fixture("known")
    var selected := false
    for attempt in range(20):
        await key(KEY_A)
        if combat.get_node("Prompt").text.contains("Sacred Flame"):
            selected = true
            break
    require(selected, "A selects the cantrip with the shared controls")
    await key(KEY_SPACE)
    require(sacred.disabled and combat.selected_character_id() == 1, "A/Space casts on the selected legal creature")
    restore_files()
    print("Sacred Flame view checks passed")
    quit(0)
