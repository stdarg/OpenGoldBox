extends Control

# The native review view sends terrain, target, combatant, and art data. Godot
# computes all display rectangles, font placement, colors, and shape dimensions.


func _metric(name: String) -> float:
    return get_theme_constant(name, "OpenGoldMetrics")


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


func _draw() -> void:
    var view := get_parent()
    var data: Dictionary = view.combat_visuals()
    if data.is_empty():
        return
    var board: Rect2 = (view.get_node("BattlefieldBounds") as Control).get_rect()
    var tile := board.size.x / float(data["width"])
    var width: int = data["width"]
    var terrain: Array = data["terrain"]
    var textures: Array = data["textures"]
    for i in terrain.size():
        var cell := Vector2i(i % width, i / width)
        var rect := Rect2(board.position + Vector2(cell) * tile, Vector2.ONE * tile)
        var kind: int = terrain[i]
        var color_name := "terrain_obstacle" if kind == 2 else \
            "terrain_difficult" if kind == 1 else \
            "terrain_odd" if (cell.x + cell.y) % 2 else "terrain_even"
        draw_rect(rect, _color(color_name))
        if textures[i] is Texture2D:
            draw_texture_rect(textures[i], rect, false)
        else:
            draw_rect(rect, _color("terrain_missing"), false)
    var target_inset := _metric("combat_demo_target_inset")
    for overlay in data["overlays"]:
        var cell: Vector2i = overlay["cell"]
        var rect := Rect2(board.position + Vector2(cell) * tile + Vector2.ONE * target_inset,
            Vector2.ONE * (tile - 2.0 * target_inset))
        var color_name: String = {
            "effect_focus": "combat_demo_effect_focus",
            "target_focus": "combat_demo_target_focus",
            "target": "combat_demo_target_overlay",
        }.get(overlay["kind"], "combat_demo_target_overlay")
        draw_rect(rect, _color(color_name))
    var font := get_theme_default_font()
    for actor in data["actors"]:
        var cell: Vector2i = actor["cell"]
        var center := board.position + (Vector2(cell) + Vector2.ONE * 0.5) * tile
        var side_color := _color("combat_party" if actor["party"] else "combat_enemy")
        draw_circle(center, tile * _metric("combat_demo_actor_radius_percent") / 100.0,
            side_color if actor["conscious"] else _color("combat_demo_unconscious"))
        if actor["active"]:
            draw_arc(center, tile * _metric("combat_demo_active_radius_percent") / 100.0,
                0.0, TAU, int(_metric("combat_demo_arc_points")),
                _color("score_active_border"), _metric("combat_demo_arc_width"))
        if actor.has("texture"):
            var texture: Texture2D = actor["texture"]
            var image_size := texture.get_size()
            var scale := tile * _metric("combat_demo_sprite_fit_percent") / 100.0 \
                / maxf(image_size.x, image_size.y)
            var sprite_size := image_size * scale
            draw_texture_rect(texture, Rect2(center - sprite_size * 0.5, sprite_size), false,
                _color("combat_demo_sprite_full" if actor["conscious"] \
                    else "combat_demo_sprite_dim"))
        else:
            var number: String = actor["number"]
            var cursor := center + Vector2(
                -_metric("combat_demo_marker_char_width_tenths") / 10.0 * number.length(),
                _metric("combat_demo_marker_baseline"))
            for digit in number:
                draw_char(font, cursor, digit, int(_metric("combat_marker_font_size")),
                    _color("combat_demo_marker_text"))
                cursor.x += _metric("combat_marker_digit_advance")
        var bar_height := _metric("combat_demo_hp_bar_height")
        var bar_origin := center + Vector2(
            -tile * _metric("combat_demo_hp_left_percent") / 100.0,
            tile * _metric("combat_demo_hp_top_percent") / 100.0)
        var bar_width := tile * _metric("combat_demo_hp_width_percent") / 100.0
        draw_rect(Rect2(bar_origin, Vector2(bar_width, bar_height)),
            _color("combat_demo_hp_track"))
        draw_rect(Rect2(bar_origin, Vector2(bar_width * float(actor["hp"]) \
            / maxf(1.0, float(actor["maximum"])), bar_height)), side_color)
