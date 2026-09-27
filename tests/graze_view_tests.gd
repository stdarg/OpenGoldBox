extends "nick_view_tests.gd"
func run_checks() -> void:
    await settle()
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir()); root.gui_embed_subwindows = true
    for locale in (["en"] if demo else ["en","es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(); current_scene.set_process(false)
        for size in [Vector2i(1120,800),Vector2i(1920,1080)]:
            root.size = size; await settle(); await load_fixture("before")
            if demo: current_scene.get_node("Melee").pressed.emit()
            else:
                root.gui_release_focus()
                var label: String = TranslationServer.translate("Melee attack")
                for cycle in range(16):
                    if current_scene.get_node("Prompt").text.contains(label): break
                    await key(KEY_A)
                require(current_scene.get_node("Prompt").text.contains(label), "A selects melee Attack")
            await settle(); await click_target()
            var dialog: Window = current_scene.get_node("OptionalEffect")
            require(dialog.visible, "Actual weapon miss opens optional Graze")
            require(dialog.get_node("Use").has_focus(), "Use receives keyboard focus")
            require(current_scene.get_node("End").disabled, "Original Action is spent and other actions wait")
            await capture("graze-" + locale + "-" + str(size.x)); expect_native("pending")
            var escape := InputEventKey.new(); escape.keycode = KEY_ESCAPE; escape.pressed = true
            dialog.window_input.emit(escape); await settle()
            require(not dialog.visible, "Escape skips optional damage"); expect_native("skipped")
            await load_fixture("pending"); require(dialog.visible, "Pending decision restores")
            dialog.get_node("Use").grab_focus()
            for down in [true,false]:
                var e := InputEventKey.new(); e.keycode = KEY_SPACE; e.pressed = down; root.push_input(e,true)
            await settle(); require(not dialog.visible, "Keyboard Use resolves damage"); expect_native("used")
            await load_fixture("pending")
            var point: Vector2 = Vector2(dialog.position) + dialog.get_node("Use").get_global_rect().get_center()
            var motion := InputEventMouseMotion.new(); motion.position = point; motion.global_position = point; root.push_input(motion,true); await settle()
            for down in [true,false]:
                var e := InputEventMouseButton.new(); e.button_index = MOUSE_BUTTON_LEFT; e.position = point; e.global_position = point; e.pressed = down; root.push_input(e,true)
            await settle(); require(not dialog.visible, "Mouse Use resolves damage"); expect_native("used")
        current_scene.queue_free(); await settle()
    cleanup(); print("Graze controls passed"); quit(0)
