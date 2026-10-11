extends SceneTree

const CASES := [
    ["sheet", Vector2i(1000, 960), [Vector2i(1000, 680), Vector2i(1000, 880), Vector2i(1000, 960)]],
    ["creation_modal", Vector2i(780, 960), [Vector2i(780, 680), Vector2i(780, 880), Vector2i(780, 960)]],
    ["tour_sheet", Vector2i(1800, 960), [Vector2i(1000, 680), Vector2i(1480, 880), Vector2i(1800, 960)]],
]
const VIEW_SIZES := [Vector2i(1120, 800), Vector2i(1600, 1000), Vector2i(1920, 1080)]


func _initialize() -> void:
    call_deferred("run_checks")


func run_checks() -> void:
    for item in CASES:
        var window := Window.new()
        window.size = item[1]
        window.set_meta("layout_reference_size", item[1])
        root.add_child(window)
        window.hide()
        var layout := Node.new()
        layout.set_script(load("res://scenes/window_layout.gd"))
        layout.set("layout_kind", item[0])
        window.add_child(layout)
        for index in VIEW_SIZES.size():
            layout.call("fit", VIEW_SIZES[index])
            if window.size != item[2][index]:
                push_error("%s window at %s: got %s, expected %s" %
                    [item[0], VIEW_SIZES[index], window.size, item[2][index]])
                quit(1)
                return
        window.queue_free()
        await process_frame
    print("Window layout scene passed")
    quit(0)
