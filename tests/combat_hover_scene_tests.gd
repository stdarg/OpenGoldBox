extends SceneTree

func _initialize() -> void:
    call_deferred("check")

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
    root.size = Vector2i(1920, 1080)
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
    await process_frame
    var details: String = combat.get_node("HoverInfo/Details").text
    # Kobolds are SRD Kobold Warriors (MON-1); the leader wears the armor its record readies.
    if not tooltip.visible or not details.contains("Kobold Leader") \
            or not details.contains("AC 16") or not details.contains("HP 7/7") \
            or not details.contains("Weapon: Dagger"):
        push_error("Leader hover omitted its type, AC, HP, or current melee weapon: " + details)
        quit(1)
        return
    await RenderingServer.frame_post_draw
    var path := ProjectSettings.globalize_path("res://../../../build/checks/combat-hover.png")
    if root.get_texture().get_image().save_png(path) != OK:
        push_error("Could not capture combat hover")
        quit(1)
        return
    hover_cell(canvas, tile, Vector2(7, 4))
    await process_frame
    details = combat.get_node("HoverInfo/Details").text
    if not tooltip.visible or not details.contains("Kobold") or not details.contains("AC 14") \
            or not details.contains("HP 7/7") or not details.contains("Weapon: Dagger"):
        push_error("Ordinary Kobold hover did not show its dagger: " + details)
        quit(1)
        return
    hover_cell(canvas, tile, Vector2(7, 5))
    await process_frame
    if tooltip.visible:
        push_error("Party sprite incorrectly showed monster tooltip")
        quit(1)
        return
    hover_cell(canvas, tile, Vector2(1, 1))
    await process_frame
    if tooltip.visible:
        push_error("Empty square kept the monster tooltip")
        quit(1)
        return
    combat.get_node("ZoomIn10").emit_signal("pressed")
    for frame in range(4):
        await process_frame
    tile = canvas.custom_minimum_size.x / 12.0
    hover_cell(canvas, tile, Vector2(6, 4))
    await process_frame
    if not tooltip.visible or not combat.get_node("HoverInfo/Details").text.contains("Kobold Leader"):
        push_error("Hover mapping did not follow battlefield zoom")
        quit(1)
        return
    print("Combat hover checks passed: ", path)
    quit(0)
