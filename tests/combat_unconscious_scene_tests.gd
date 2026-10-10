extends SceneTree

func _initialize() -> void:
    call_deferred("check")

# The demo's Criminal heroes hold Alert, so each may swap Initiative before the
# first turn; the choice is modal, so Enter cannot end turns until it is kept.
func keep_initiative(combat: Node) -> void:
    var dialog: Window = combat.get_node("InitiativeChoice")
    while dialog.visible:
        dialog.get_node("Keep").pressed.emit()
        for frame in range(4):
            await process_frame

func check() -> void:
    root.size = Vector2i(1920, 1080)
    # This window takes the keyboard focus when it opens, so keys and clicks meant
    # for another app would reach it. Only the check's own pushed input drives it.
    DisplayServer.window_set_input_event_callback(func(_event: InputEvent) -> void: pass)
    # A fresh checkout has no build/checks folder for the screenshots yet.
    var checks := ProjectSettings.globalize_path("res://../../../build/checks")
    DirAccess.make_dir_recursive_absolute(checks)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    for frame in range(8):
        await process_frame
    await keep_initiative(current_scene)
    # Each enemy command waits 0.65 seconds; the first hero drops after about 2700 frames.
    for frame in range(4000):
        await process_frame
        var combat := current_scene
        if combat == null:
            continue
        if combat.get_node("Log").text.contains("Oren Quickwater falls unconscious."):
            await RenderingServer.frame_post_draw
            var path := ProjectSettings.globalize_path("res://../../../build/checks/combat-unconscious.png")
            if root.get_texture().get_image().save_png(path) != OK:
                push_error("Could not capture the unconscious combatant")
                quit(1)
                return
            if combat.get_node("DeathAudio").playing:
                push_error("Unconsciousness incorrectly played the death sound")
                quit(1)
                return
            print("Unconscious combatant captured: ", path)
            quit(0)
            return
        var end_key := InputEventKey.new()
        end_key.keycode = KEY_ENTER
        end_key.pressed = true
        root.push_input(end_key)
    push_error("Combat did not reach an unconscious party member")
    quit(1)
