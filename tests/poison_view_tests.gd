extends SceneTree
var captures := ""
var legacy := false
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--poison-capture="): captures = arg.trim_prefix("--poison-capture=")
        if arg == "--legacy": legacy = true
        if arg.begins_with("--poison-fixtures="): fixtures = arg.trim_prefix("--poison-fixtures=")
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
        var spells: OptionButton = combat.get_node("Cantrip")
        var cast: Button = combat.get_node("CastCantrip")
        require(spells.visible and cast.visible, "Known cantrips expose shared controls")
        require(spells.item_count == (1 if which == "unknown" else 2), "Dropdown includes only known cantrips")
        require(spells.get_item_metadata(0) == "fire_bolt", "Fire Bolt remains a known choice")
        spells.select(0); spells.item_selected.emit(0)
        require(cast.disabled == (which == "blocked"), "Fire Bolt follows casting eligibility")
        if which != "unknown":
            require(spells.get_item_metadata(1) == "poison_spray", "Poison Spray is selectable when known")
            spells.select(1); spells.item_selected.emit(1)
            require(cast.disabled == (which == "blocked"), "Selected Poison Spray follows casting eligibility")
        require(combat.get_node("CantripLabel").position.x >= combat.get_node("AdrenalineRush").get_rect().end.x, "Approved spell row stays right of Adrenaline Rush")
        require(cast.get_rect().end.x < root.size.x - 300 and cast.get_rect().end.y <= combat.get_node("LogStack/Log").global_position.y, "Shared controls fit and do not cover the log")
        var prompts := ""
        for i in range(18):
            await key(KEY_A)
            prompts += combat.get_node("Prompt").text + "\n"
        require(prompts.contains("Poison Spray") == (which == "known"), "Keyboard cycle follows current cantrip eligibility")
    await load_fixture("known")
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        combat = current_scene
        combat.set_process(false)
        await load_fixture("known")
        require(combat.get_node("Cantrip").get_item_text(1) == ("Poison Spray" if locale == "en" else "Rociada venenosa"), "Rendered locale is active")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size
            await settle()
            require(combat.get_node("Cantrip").get_rect().end.x + 10 <= combat.get_node("CastCantrip").position.x and combat.get_node("CastCantrip").get_rect().end.x < root.size.x - 300, "Translated spell controls fit without overlap")
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
    var spells: OptionButton = combat.get_node("Cantrip")
    spells.select(0); spells.item_selected.emit(0)
    spells.grab_focus()
    await key(KEY_ENTER)
    await key(KEY_DOWN)
    await key(KEY_ENTER)
    require(spells.get_item_metadata(spells.selected) == "poison_spray", "Keyboard chooses Poison Spray without ending the turn")
    var poison: Button = combat.get_node("CastCantrip")
    require(not poison.disabled, "Selecting a spell consumes no action")
    poison.grab_focus()
    await key(KEY_ENTER)
    require(combat.get_node("Prompt").text.contains("Poison Spray") and not poison.disabled, "Enter selects the spell without consuming Action")
    await click_cell(Vector2(3, 1))
    require(poison.disabled, "Clicking a legal ally casts and consumes Action")
    require(combat.selected_character_id() == 1, "Spell target click does not switch selection to ally")
    require(combat.get_node("LogStack/Log").get_parsed_text().contains("Ally"), "Attack log identifies the chosen ally")
    restore_files()
    print("Poison Spray view checks passed")
    quit(0)
