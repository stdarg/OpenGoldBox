extends SceneTree

const SIZES := [Vector2i(1920, 1080), Vector2i(1600, 1000), Vector2i(1120, 800)]
const SOURCE := Vector2(1672, 941)

func _initialize() -> void:
    call_deferred("run_checks")

func run_checks() -> void:
    for dimensions in SIZES:
        root.size = dimensions
        var scene: Control = load("res://scenes/startup.tscn").instantiate()
        root.add_child(scene)
        await process_frame
        await process_frame
        var image: TextureRect = scene.get_node("ImageFit/Image")
        var lettering: TextureRect = scene.get_node("ImageFit/Image/Text")
        var fit := minf(float(dimensions.x) / SOURCE.x, float(dimensions.y) / SOURCE.y)
        var expected_image := Rect2((Vector2(dimensions) - SOURCE * fit) / 2.0, SOURCE * fit)
        var expected_text := Rect2(expected_image.position + expected_image.size * Vector2(0.1, 0.07),
            expected_image.size * Vector2(0.8, 0.8))
        if not _near(image.get_global_rect(), expected_image) or not _near(lettering.get_global_rect(), expected_text):
            push_error("Startup scene layout differs at %s: image=%s text=%s" % [dimensions, image.get_global_rect(), lettering.get_global_rect()])
            quit(1)
            return
        scene.queue_free()
        await process_frame
    print("Startup layout scene passed")
    quit(0)

func _near(actual: Rect2, expected: Rect2) -> bool:
    return actual.position.distance_to(expected.position) < 1.0 and actual.size.distance_to(expected.size) < 1.0
