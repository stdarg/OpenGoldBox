extends Control


func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), get_theme_color("background", "OpenGoldPalette"))
    var margin := get_theme_constant("sound_board_margin", "OpenGoldMetrics")
    var y := get_theme_constant("sound_board_rule_y", "OpenGoldMetrics")
    draw_line(Vector2(margin, y), Vector2(size.x - margin, y),
        get_theme_color("line", "OpenGoldPalette"))
