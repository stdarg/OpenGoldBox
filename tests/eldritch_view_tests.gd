extends SceneTree
var originals := {}
var fixtures := ""
var captures := ""
var path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--eldritch-fixtures="): fixtures = arg.trim_prefix("--eldritch-fixtures=")
        if arg.begins_with("--eldritch-capture="): captures = arg.trim_prefix("--eldritch-capture=")
    call_deferred("run_checks")
func restore_files() -> void:
    for name in originals:
        if originals[name] == null:
            if FileAccess.file_exists(name): DirAccess.remove_absolute(name)
        else:
            var file := FileAccess.open(name, FileAccess.WRITE)
            file.store_buffer(originals[name]); file.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func load_fixture(which: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(which + ".save"))
    require(not bytes.is_empty(), "Native fixture exists")
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()
func run_checks() -> void:
    path = ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        current_scene.set_process(false)
        await load_fixture("known")
        var spells: OptionButton = current_scene.get_node("Cantrip")
        var cast: Button = current_scene.get_node("CastCantrip")
        require(spells.item_count == 1 and spells.get_item_metadata(0) == "eldritch_blast", "Only the learned Eldritch Blast is listed")
        require(spells.get_item_text(0) == ("Eldritch Blast" if locale == "en" else "Descarga sobrenatural"), "Localized spell choice")
        require(not cast.disabled, "Ordinary Warlock can cast its learned cantrip")
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle()
            require(cast.get_rect().end.x < root.size.x - 300, "Translated controls fit")
            require(spells.get_rect().end.x + 8 <= cast.position.x, "Localized dropdown and Cast never overlap")
            if not captures.is_empty():
                DirAccess.make_dir_recursive_absolute(captures)
                await RenderingServer.frame_post_draw
                require(root.get_texture().get_image().save_png(captures.path_join("eldritch-" + locale + "-" + str(size.x) + ".png")) == OK, "Rendered controls captured")
        cast.grab_focus(); await key(KEY_ENTER)
        require(not cast.disabled, "Cast selects targets without spending the action")
        var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
        var tile := canvas.get_combined_minimum_size().x / 12.0
        var point := canvas.get_global_transform_with_canvas() * (Vector2(3.5, 1.5) * tile)
        for down in [true, false]:
            var event := InputEventMouseButton.new()
            event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = point
            root.push_input(event, true)
        await settle()
        require(cast.disabled and current_scene.selected_character_id() == 1, "Legal ally click casts without changing selection")
        require(current_scene.get_node("Log").get_parsed_text().contains("d20"), "Actual selected cast resolves an attack")
        cast.release_focus()
        await load_fixture("blocked")
        require(cast.disabled, "Untrained armor disables casting")
        await load_fixture("unknown")
        require(not cast.visible, "Unselected spell has no casting control")
        await load_fixture("both")
        require(spells.item_count == 2 and not cast.disabled, "Both selected Warlock cantrips available")
        var poison_index := -1
        for i in range(spells.item_count):
            if spells.get_item_metadata(i) == "poison_spray": poison_index = i
        require(poison_index >= 0, "Poison Spray is selectable")
        spells.select(poison_index); spells.item_selected.emit(poison_index)
        await settle()
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle()
            require(spells.get_rect().end.x + 8 <= cast.position.x and cast.get_rect().end.x < root.size.x - 300, "Poison controls fit")
            if not captures.is_empty():
                await RenderingServer.frame_post_draw
                require(root.get_texture().get_image().save_png(captures.path_join("warlock-poison-" + locale + "-" + str(size.x) + ".png")) == OK, "Poison controls captured")
        cast.grab_focus(); await key(KEY_ENTER)
        point = canvas.get_global_transform_with_canvas() * (Vector2(3.5, 1.5) * (canvas.get_combined_minimum_size().x / 12.0))
        for down in [true, false]:
            var event := InputEventMouseButton.new()
            event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = point
            root.push_input(event, true)
        await settle()
        require(cast.disabled and current_scene.selected_character_id() == 1 and current_scene.get_node("Log").get_parsed_text().contains("+ 6"), "Chosen Poison Spray casts at ally with Charisma without changing selection")
        cast.release_focus()
        require(not current_scene.get_node("Save").visible and not current_scene.get_node("Load").visible, "No player combat saving")
    restore_files()
    print("Eldritch Blast view checks passed")
    quit(0)
