extends "nick_view_tests.gd"
func activate(button: Button) -> void:
    button.grab_focus()
    for down in [true,false]:
        var e := InputEventKey.new(); e.keycode = KEY_SPACE; e.pressed = down; root.push_input(e,true)
    await settle()
func modal_key(code: int) -> void:
    for down in [true,false]:
        var e := InputEventKey.new(); e.keycode = code; e.pressed = down; root.push_input(e,true)
    await settle()
func click_cell(cell: Vector2i) -> void:
    var point: Vector2
    if demo:
        var tile: float = (current_scene.get_node("Weapons").position.y - 16 - 116) / 8.0
        point = Vector2(24,116) + (Vector2(cell) + Vector2(0.5,0.5)) * tile
    else:
        var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
        point = canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5,0.5)) * canvas.get_combined_minimum_size().x / 12.0)
    for down in [true,false]:
        var e := InputEventMouseButton.new(); e.button_index = MOUSE_BUTTON_LEFT; e.position = point; e.global_position = point; e.pressed = down; root.push_input(e,true)
    await settle()
func run_checks() -> void:
    await settle()
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir()); root.gui_embed_subwindows = true
    for locale in (["en"] if demo else ["en","es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(); current_scene.set_process(false)
        for size in [Vector2i(1120,800),Vector2i(1920,1080)]:
            root.size = size; await settle()
            var dialog: Window = current_scene.get_node("OptionalEffect")
            for weapon in ["javelin","maul","greataxe","warhammer"]:
                await load_fixture(weapon + "-before")
                if demo: current_scene.get_node("Melee").pressed.emit()
                else:
                    root.gui_release_focus()
                    var label: String = TranslationServer.translate("Melee attack")
                    for cycle in range(16):
                        if current_scene.get_node("Prompt").text.contains(label): break
                        await key(KEY_A)
                await settle(); await click_target()
                require(dialog.visible, "Actual hit offers " + weapon); expect_native(weapon + "-pending")
                require(not dialog.get_node("Resolve").visible, "One effect needs no ordering dropdown")
                await capture(weapon + "-" + locale + "-" + str(size.x))
                await activate(dialog.get_node("Use"))
                if weapon in ["greataxe","warhammer"]:
                    require(not dialog.visible and not current_scene.get_node("End").disabled, "Targeting has a usable Skip effect control")
                    expect_native(weapon + "-targeting"); await capture(weapon + "-targeting-" + locale + "-" + str(size.x))
                    root.gui_release_focus(); await key(KEY_RIGHT); await key(KEY_LEFT); await key(KEY_SPACE)
                require(not dialog.visible,"Use resolves effect"); expect_native(weapon + "-used")
                await load_fixture(weapon + "-pending")
                var escape := InputEventKey.new(); escape.keycode = KEY_ESCAPE; escape.pressed = true
                dialog.window_input.emit(escape); await settle(); expect_native(weapon + "-skipped")
                if weapon in ["greataxe","warhammer"]:
                    await load_fixture(weapon + "-targeting"); root.gui_release_focus(); await key(KEY_ESCAPE); expect_native(weapon + "-cancelled")
                    await load_fixture(weapon + "-targeting"); await click_cell(Vector2i(2,2) if weapon == "greataxe" else Vector2i(3,1)); expect_native(weapon + "-used")
            await load_fixture("ordered-pending")
            var resolve: OptionButton = dialog.get_node("Resolve")
            require(resolve.visible and resolve.item_count == 2,"Resolve next lists separate pending effects")
            resolve.select(0); resolve.item_selected.emit(0); await settle(); resolve.grab_focus()
            await modal_key(KEY_SPACE); await modal_key(KEY_DOWN); await modal_key(KEY_ENTER)
            require(resolve.get_selected_id() == 2,"Keyboard selects Champion movement independently")
            require(dialog.get_node("Use").get_rect().end.y <= dialog.size.y and resolve.get_rect().end.x <= dialog.size.x,"Translated ordering controls fit")
            await capture("ordered-" + locale + "-" + str(size.x))
            await activate(dialog.get_node("Use")); require(not dialog.visible,"Selected movement starts"); expect_native("ordered-moving")
            root.gui_release_focus(); await key(KEY_ESCAPE); expect_native("ordered-mastery")
            require(dialog.visible and not resolve.visible,"Finishing movement retains mastery choice")
            await activate(dialog.get_node("Use")); expect_native("ordered-used")
        current_scene.queue_free(); await settle()
    cleanup(); print("Optional mastery controls passed"); quit(0)
