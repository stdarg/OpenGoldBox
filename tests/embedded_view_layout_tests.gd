extends SceneTree

# The creation screen embeds the same town and combat scenes as its children.
# Full-rect scene anchors must size them without a C++ setter.
func _initialize() -> void:
    call_deferred("run_checks")


func run_checks() -> void:
    var host := Control.new()
    root.add_child(host)
    for scene_path in ["res://scenes/rolf_tour.tscn", "res://scenes/combat_demo.tscn"]:
        var screen: Control = load(scene_path).instantiate()
        host.add_child(screen)
        for dimensions in [Vector2i(1120, 800), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
            root.size = dimensions
            host.size = dimensions
            for frame in range(3):
                await process_frame
            if not screen.size.is_equal_approx(Vector2(dimensions)):
                push_error("Embedded %s does not fill its parent at %s" % [scene_path, dimensions])
                quit(1)
                return
        screen.queue_free()
        await process_frame
    host.queue_free()
    print("Embedded view layout passed")
    quit(0)
