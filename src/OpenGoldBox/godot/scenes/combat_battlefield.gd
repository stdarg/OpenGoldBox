extends Control

# The native view supplies rules-owned cells, legal targets, and art. All canvas
# geometry, colors, and text drawing live with the Godot presentation.


func _palette(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


func _metric(name: String) -> float:
    return get_theme_constant(name, "OpenGoldMetrics")


func _cell_rect(cell: Vector2i, tile: float, inset: float = 0.0) -> Rect2:
    return Rect2(Vector2(cell) * tile + Vector2.ONE * inset,
        Vector2.ONE * (tile - 2.0 * inset))


func _sprite_rect(texture: Texture2D, visible: Rect2, cell: Rect2, goliath: bool) -> Rect2:
    if texture == null or visible.size.x <= 0 or visible.size.y <= 0:
        return Rect2()
    var source := texture.get_size()
    if goliath:
        var extent := cell.size * Vector2(1.0, _metric("combat_goliath_height_percent") / 100.0)
        var scale := extent / visible.size
        var top := cell.position + Vector2((cell.size.x - extent.x) * 0.5,
            cell.size.y - extent.y)
        return Rect2(top - visible.position * scale, source * scale)
    var scale := cell.size.x * _metric("combat_sprite_fit_percent") / 100.0 \
        / maxf(visible.size.x, visible.size.y)
    return Rect2(cell.get_center() - visible.get_center() * scale, source * scale)


func _draw() -> void:
    var view := get_parent().get_parent()
    var data: Dictionary = view.battlefield_visuals()
    if data.is_empty():
        return
    var tile: float = view.get_node("CombatLayout").base_tile
    var zoom: float = data["zoom"]
    var inset := _metric("combat_tile_inset")
    var width: int = data["width"]
    draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE * zoom)
    var terrain: Array = data["terrain"]
    var art: Array = data["terrain_textures"]
    for i in terrain.size():
        var cell := Vector2i(i % width, i / width)
        var rect := _cell_rect(cell, tile)
        var kind: int = terrain[i]
        var color_name := "terrain_obstacle" if kind == 2 else \
            "terrain_difficult" if kind == 1 else \
            "terrain_odd" if (cell.x + cell.y) % 2 else "terrain_even"
        draw_rect(rect, _palette(color_name))
        if art[i] is Texture2D:
            draw_texture_rect(art[i], rect, false)
        else:
            draw_rect(rect, _palette("terrain_missing"), false)
    for overlay in data["overlays"]:
        var cell: Vector2i = overlay["cell"]
        var kind: String = overlay["kind"]
        var rect := _cell_rect(cell, tile)
        if kind == "spiritual" or kind == "flame":
            var radius := _metric("combat_spiritual_radius_percent") if kind == "spiritual" \
                else _metric("combat_flame_radius_percent")
            draw_circle(rect.get_center(), tile * radius / 100.0,
                _palette("combat_spiritual_overlay" if kind == "spiritual" \
                    else "combat_flame_overlay"))
        elif kind == "selected":
            draw_rect(_cell_rect(cell, tile, inset), _palette("combat_selected"), false,
                _metric("combat_selected_border_width"))
        else:
            var color_name: String = {
                "reachable": "combat_reachable_overlay",
                "target": "combat_target_overlay",
                "target_focus": "combat_target_focus_overlay",
                "silence": "combat_silence_overlay",
                "moonbeam": "combat_moonbeam_overlay",
                "obscured": "combat_obscured_overlay",
                "area": "combat_area_overlay",
                "effect_focus": "combat_effect_focus_overlay",
            }.get(kind, "combat_target_overlay")
            draw_rect(rect if kind in ["silence", "moonbeam", "obscured"] \
                else _cell_rect(cell, tile, inset), _palette(color_name))
    var font := get_theme_default_font()
    var marker_size := int(_metric("combat_marker_font_size"))
    var advance := _metric("combat_marker_digit_advance")
    for actor in data["actors"]:
        var cell: Vector2i = actor["cell"]
        var rect := _cell_rect(cell, tile)
        if actor.has("skull"):
            draw_texture_rect(actor["skull"], rect, false)
        elif actor.has("texture"):
            var texture: Texture2D = actor["texture"]
            var sprite := _sprite_rect(texture, actor["visible"], rect, actor["goliath"])
            draw_texture_rect(texture, sprite, false,
                _palette("combat_reachable" if actor["conscious"] else "combat_unconscious"))
        elif actor.has("number"):
            var number: String = actor["number"]
            var cursor := rect.get_center() + Vector2(-advance * 0.5 * number.length(),
                _metric("combat_marker_baseline"))
            var color := _palette("combat_party" if actor["party"] else "combat_enemy")
            for digit in number:
                draw_char(font, cursor, digit, marker_size, color)
                cursor.x += advance
