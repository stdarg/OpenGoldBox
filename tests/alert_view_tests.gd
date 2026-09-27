extends "optional_mastery_view_tests.gd"
func run_checks() -> void:
    await settle()
    path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if demo else "user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir()); root.gui_embed_subwindows = true
    for locale in (["en"] if demo else ["en","es"]):
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(); current_scene.set_process(false)
        for size in [Vector2i(1120,800),Vector2i(1920,1080)]:
            root.size = size; await settle(); await load_fixture("pending")
            var dialog: Window = current_scene.get_node("InitiativeChoice")
            var owners: OptionButton = dialog.get_node("Resolve")
            var allies: OptionButton = dialog.get_node("Ally")
            var swap: Button = dialog.get_node("Swap")
            var keep: Button = dialog.get_node("Keep")
            require(dialog.visible and dialog.size == Vector2i(640,360), "Approved centered Alert dialog")
            require(owners.visible and owners.item_count == 2 and allies.item_count == 2, "All holders and eligible allies listed")
            require(swap.disabled and current_scene.get_node("End").disabled, "Select ally first; ordinary actions wait")
            # Select the first holder by entity ID, then use actual keyboard selection.
            var first := owners.get_item_index(1)
            if owners.selected != first:
                owners.grab_focus(); await modal_key(KEY_SPACE); await modal_key(KEY_DOWN); await modal_key(KEY_ENTER)
            require(owners.get_selected_id() == 1, "Keyboard chooses first holder")
            allies.grab_focus(); await modal_key(KEY_SPACE); await modal_key(KEY_DOWN); await modal_key(KEY_ENTER)
            require(allies.get_selected_id() == 2 and not swap.disabled, "Keyboard selects eligible ally")
            await capture("alert-" + locale + "-" + str(size.x))
            await activate(swap); expect_native("swapped")
            require(dialog.visible and not owners.visible and owners.get_selected_id() == 2, "Other holder retains exactly one choice")
            await modal_key(KEY_ESCAPE); expect_native("done"); require(not dialog.visible, "Escape keeps remaining holder and starts combat")
            # Mouse opens a real OptionButton popup and selects the second holder.
            await load_fixture("pending")
            if owners.get_selected_id() != 2:
                var point := Vector2(dialog.position) + owners.get_global_rect().get_center()
                await mouse_point(point)
                var popup: PopupMenu = owners.get_popup()
                require(popup.visible, "Mouse opens resolver dropdown")
                var second := owners.get_item_index(2)
                var row_height := float(popup.size.y) / owners.item_count
                await mouse_point(Vector2(popup.position) + Vector2(popup.size.x / 2.0, row_height * (second + 0.5)))
            require(owners.get_selected_id() == 2, "Mouse selects second holder")
            await mouse_point(Vector2(dialog.position) + keep.get_global_rect().get_center()); expect_native("second-kept")
            await mouse_point(Vector2(dialog.position) + keep.get_global_rect().get_center()); expect_native("kept")
    cleanup(); print("Alert UI tests passed"); quit(0)
func mouse_point(point: Vector2) -> void:
    var motion := InputEventMouseMotion.new(); motion.position = point; motion.global_position = point; root.push_input(motion,true)
    for down in [true,false]:
        var event := InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.position = point; event.global_position = point; event.pressed = down; root.push_input(event,true)
    await settle()
