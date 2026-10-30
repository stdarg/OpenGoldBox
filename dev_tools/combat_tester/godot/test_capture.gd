extends SceneTree

func _initialize() -> void:
    call_deferred("capture")

func capture() -> void:
    var scene: PackedScene = load("res://combat_tester/setup.tscn")
    root.add_child(scene.instantiate())
    await process_frame
    await process_frame
    RenderingServer.force_draw()
    var image := root.get_texture().get_image()
    if image == null:
        push_error("Viewport capture is unavailable")
        quit(1)
        return
    var result := image.save_png(OS.get_environment("OPENGOLD_COMBAT_TEST_CAPTURE"))
    if result != OK:
        push_error("Could not capture setup screen")
        quit(1)
        return
    quit(0)
