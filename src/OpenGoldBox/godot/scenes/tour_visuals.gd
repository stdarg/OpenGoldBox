extends Control

# C++ supplies map state and decoded art. The Godot scene and theme own every
# on-screen rectangle, color, line width, and art scaling decision.


func _metric(name: String) -> float:
    return get_theme_constant(name, "OpenGoldMetrics")


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


func _ready() -> void:
    for node_name in ["SceneBounds", "MapBounds"]:
        (get_parent().get_node("%" + node_name) as Control).item_rect_changed.connect(queue_redraw)


# Where a frame the layout placed sits on this full-screen canvas.
func _bounds(view: Control, node_name: String) -> Rect2:
    var bounds := view.get_node("%" + node_name) as Control
    return Rect2(bounds.global_position - global_position, bounds.size)


func _draw() -> void:
    var view := get_parent()
    var data: Dictionary = view.tour_visuals()
    if data.is_empty():
        return
    if data.has("solar"):
        _draw_solar_map(view, data)
        _draw_solar_picture(view, data["solar"])
        return
    _draw_scene(view, data)
    _draw_map(view, data)


# The Buck Rogers games' solar map, square and centered in the 3D view's
# frame, with the orbits' faint dots, the course flown and the name of the
# body under the pointer drawn over the original's map.
func _draw_solar_map(view: Control, data: Dictionary) -> void:
    var bounds := _bounds(view, "SceneBounds")
    var side := minf(bounds.size.x, bounds.size.y)
    var rect := Rect2(bounds.position + (bounds.size - Vector2(side, side)) * 0.5, Vector2(side, side))
    draw_texture_rect(data["wall"], rect, false)
    var solar: Dictionary = data["solar"]
    var cell := side / float(solar["cells"])
    var dot := cell * _metric("tour_solar_dot_percent") / 100.0
    for point: Vector2 in solar["orbits"]:
        draw_circle(rect.position + (point + Vector2.ONE * 0.5) * cell, dot, _color("line"))
    for point: Vector2 in solar["course"]:
        draw_circle(rect.position + (point + Vector2.ONE * 0.5) * cell, dot * 1.5, _color("gold"))
    if solar.has("hover_name"):
        var font := get_theme_default_font()
        var font_size := int(_metric("tour_solar_name_size"))
        var name: String = solar["hover_name"]
        var at: Vector2 = rect.position + (solar["hover_cell"] + Vector2(1.25, 0.5)) * cell
        var text_size := font.get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
        # Kept inside the map: a body at its right edge is named to its left.
        if at.x + text_size.x > rect.end.x:
            at.x -= text_size.x + cell * 1.5
        var plate := Rect2(at - Vector2(4, text_size.y * 0.5 + 2), text_size + Vector2(8, 4))
        draw_rect(plate, _color("background"))
        draw_string(font, at + Vector2(0, font.get_ascent(font_size) - text_size.y * 0.5), name,
            HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, _color("gold"))


# The body's picture in the side column, framed in the original's color for
# it, its name under it.
func _draw_solar_picture(view: Control, solar: Dictionary) -> void:
    var bounds := _bounds(view, "MapBounds")
    var frame := _color("character_palette_%d" % solar["frame_color"])
    var border := _metric("tour_solar_frame_width")
    draw_rect(bounds, _color("map_black"))
    draw_rect(bounds.grow(-border * 0.5), frame, false, border)
    var font := get_theme_default_font()
    var font_size := int(_metric("tour_solar_name_size"))
    var name: String = solar["picture_name"]
    var name_height := font.get_height(font_size) if not name.is_empty() else 0.0
    var inner := bounds.grow(-border * 2.0)
    inner.size.y -= name_height
    if solar.has("picture"):
        var picture: Texture2D = solar["picture"]
        var scale := minf(inner.size.x / picture.get_width(), inner.size.y / picture.get_height())
        var shown := picture.get_size() * scale
        draw_texture_rect(picture, Rect2(inner.position + (inner.size - shown) * 0.5, shown), false)
    if not name.is_empty():
        draw_string(font, Vector2(inner.position.x, inner.end.y + font.get_ascent(font_size)), name,
            HORIZONTAL_ALIGNMENT_CENTER, inner.size.x, font_size, _color("map_wall"))


func _draw_scene(view: Control, data: Dictionary) -> void:
    var bounds := _bounds(view, "SceneBounds")
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
    # A design made with Unlimited Adventures' kit has no overhead map yet.
    if not data.has("cells"):
        return
    var bounds := _bounds(view, "MapBounds")
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
