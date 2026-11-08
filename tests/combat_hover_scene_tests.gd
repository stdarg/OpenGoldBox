extends SceneTree

# The zoom buttons save the zoom in the game's settings.cfg (the check's own
# when ctest names one, else the project's), so the file is put back as it was
# before this check ends.
var settings_path := OS.get_environment("OPENGOLD_SETTINGS_FILE") if OS.has_environment("OPENGOLD_SETTINGS_FILE") else ProjectSettings.globalize_path("res://settings.cfg")
var kept_settings = null

func keep_settings() -> void:
    if FileAccess.file_exists(settings_path):
        kept_settings = FileAccess.get_file_as_bytes(settings_path)

func finish(code: int) -> void:
    if kept_settings == null:
        if FileAccess.file_exists(settings_path):
            DirAccess.remove_absolute(settings_path)
    else:
        var file := FileAccess.open(settings_path, FileAccess.WRITE)
        file.store_buffer(kept_settings)
        file.close()
    quit(code)

func _initialize() -> void:
    call_deferred("check")

# push_input updates the hover at once. Checks read it before the next frame,
# when a refresh would follow the real cursor in this window instead.
func hover_cell(canvas: Control, tile: float, cell: Vector2) -> void:
    var motion := InputEventMouseMotion.new()
    motion.position = canvas.get_global_transform_with_canvas() * ((cell + Vector2(0.5, 0.5)) * tile)
    root.push_input(motion)

# The demo's Criminal heroes hold Alert, so each may swap Initiative before the
# first turn; the choice is modal, so hovering waits until it is kept.
func keep_initiative(combat: Node) -> void:
    var dialog: Window = combat.get_node("InitiativeChoice")
    while dialog.visible:
        dialog.get_node("Keep").pressed.emit()
        for frame in range(4):
            await process_frame

func check() -> void:
    keep_settings()
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
    var combat := current_scene
    await keep_initiative(combat)
    var canvas: Control = combat.get_node("BattlefieldScroll/Canvas")
    var tile: float = canvas.custom_minimum_size.x / 12.0
    var tooltip: PanelContainer = combat.get_node("HoverInfo")
    hover_cell(canvas, tile, Vector2(6, 4))
    var details: String = combat.get_node("HoverInfo/Details").text
    # Kobolds are SRD Kobold Warriors (MON-1); the leader wears the armor its record readies.
    if not tooltip.visible or not details.contains("Kobold Leader") \
            or not details.contains("AC 16") or not details.contains("HP 7/7") \
            or not details.contains("Weapon: Dagger"):
        push_error("Leader hover omitted its type, AC, HP, or current melee weapon: " + details)
        finish(1)
        return
    # A hidden or minimized window skips drawing, so frame_post_draw may never
    # come; draw explicitly after one processed frame.
    await process_frame
    RenderingServer.force_draw(false)
    var path := ProjectSettings.globalize_path("res://../../../build/checks/combat-hover.png")
    if root.get_texture().get_image().save_png(path) != OK:
        push_error("Could not capture combat hover")
        finish(1)
        return
    hover_cell(canvas, tile, Vector2(7, 4))
    details = combat.get_node("HoverInfo/Details").text
    if not tooltip.visible or not details.contains("Kobold") or not details.contains("AC 14") \
            or not details.contains("HP 7/7") or not details.contains("Weapon: Dagger"):
        push_error("Ordinary Kobold hover did not show its dagger: " + details)
        finish(1)
        return
    hover_cell(canvas, tile, Vector2(7, 5))
    if tooltip.visible:
        push_error("Party sprite incorrectly showed monster tooltip")
        finish(1)
        return
    hover_cell(canvas, tile, Vector2(1, 1))
    if tooltip.visible:
        push_error("Empty square kept the monster tooltip")
        finish(1)
        return
    combat.get_node("ZoomIn10").emit_signal("pressed")
    for frame in range(4):
        await process_frame
    tile = canvas.custom_minimum_size.x / 12.0
    hover_cell(canvas, tile, Vector2(6, 4))
    if not tooltip.visible or not combat.get_node("HoverInfo/Details").text.contains("Kobold Leader"):
        push_error("Hover mapping did not follow battlefield zoom")
        finish(1)
        return
    print("Combat hover checks passed: ", path)
    finish(0)
