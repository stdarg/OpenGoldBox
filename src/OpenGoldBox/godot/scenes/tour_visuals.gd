extends Control

# C++ supplies map state and decoded art. The Godot scene and theme own every
# on-screen rectangle, color, line width, and art scaling decision.


func _metric(name: String) -> float:
    return get_theme_constant(name, "OpenGoldMetrics")


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


func _draw() -> void:
    var view := get_parent()
    var data: Dictionary = view.tour_visuals()
    if data.is_empty():
        return
    _draw_scene(view, data)
    _draw_map(view, data)


func _draw_scene(view: Control, data: Dictionary) -> void:
    var bounds: Rect2 = (view.get_node("SceneBounds") as Control).get_rect()
    var wall: Texture2D = data["wall"]
    var source := wall.get_size()
    var stretch := _metric("tour_wall_vertical_percent") / 100.0
    var scale := minf(bounds.size.x / source.x, bounds.size.y / (source.y * stretch))
    var pixel_scale := Vector2(scale, scale * stretch)
    var image_size := source * pixel_scale
    var image_rect := Rect2(bounds.position + (bounds.size - image_size) * 0.5, image_size)
    draw_texture_rect(wall, image_rect, false)
    if data.has("picture"):
        draw_texture_rect(data["picture"], image_rect, false)
    elif data.has("sprite"):
        var sprite_size: Vector2 = data["sprite_size"] * pixel_scale
        draw_texture_rect(data["sprite"], Rect2(
            image_rect.position + Vector2((image_rect.size.x - sprite_size.x) * 0.5,
                image_rect.size.y - sprite_size.y), sprite_size), false)


func _draw_map(view: Control, data: Dictionary) -> void:
    var bounds: Rect2 = (view.get_node("MapBounds") as Control).get_rect()
    var cell := bounds.size.x / 16.0
    var flags: Array = data["cells"]
    var inset := _metric("tour_map_grid_inset_tenths") / 10.0
    for i in flags.size():
        var entry: int = flags[i]
        if entry < 0:
            continue
        var x := i % 16
        var y := i / 16
        var origin := bounds.position + Vector2(x, y) * cell
        var rect := Rect2(origin, Vector2.ONE * cell)
        draw_rect(rect, _color("map_visited" if entry & 1 else "map_seen"))
        draw_rect(rect, _color("map_grid"), false)
        var corners := [origin + Vector2(inset, inset),
            origin + Vector2(cell - inset, inset),
            origin + Vector2(cell - inset, cell - inset),
            origin + Vector2(inset, cell - inset)]
        for direction in 4:
            var first: Vector2 = corners[direction]
            var second: Vector2 = corners[(direction + 1) % 4]
            if entry & (1 << (1 + direction)):
                draw_line(first, second, _color("map_wall"),
                    _metric("tour_map_wall_width_tenths") / 10.0)
            if entry & (1 << (5 + direction)):
                draw_line(first.lerp(second, _metric("tour_map_door_start_percent") / 100.0),
                    first.lerp(second, _metric("tour_map_door_end_percent") / 100.0),
                    _color("gold"), _metric("tour_map_door_width"))
    var pose: Vector2i = data["pose"]
    var center := bounds.position + (Vector2(pose) + Vector2.ONE * 0.5) * cell
    var directions := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
    var forward: Vector2 = directions[data["facing"]]
    var right := Vector2(-forward.y, forward.x)
    var tip := _metric("tour_map_arrow_tip_percent") / 100.0
    var back := _metric("tour_map_arrow_back_percent") / 100.0
    var notch := _metric("tour_map_arrow_notch_percent") / 100.0
    var side := _metric("tour_map_arrow_side_percent") / 100.0
    var arrow := PackedVector2Array([
        center + forward * cell * tip,
        center - forward * cell * back + right * cell * side,
        center - forward * cell * notch,
        center - forward * cell * back - right * cell * side,
    ])
    draw_circle(center, cell * _metric("tour_map_arrow_disc_percent") / 100.0,
        _color("background"))
    draw_colored_polygon(arrow, _color("party"))
