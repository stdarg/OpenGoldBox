extends Control

# The team's ship's fuel on the solar map: a bar filled to what is left of a
# full tank, drawn in the low color under 100, below which the scripts' rescues
# top it up. The view sets the numbers; the theme owns the colors.

const LOW_FUEL := 100

var _left := 0
var _full := 1


func set_fuel(left: int, full: int) -> void:
    _left = left
    _full = maxi(full, 1)
    queue_redraw()


func _draw() -> void:
    var rect := Rect2(Vector2.ZERO, size)
    draw_rect(rect, get_theme_color("background", "OpenGoldPalette"))
    var filled := rect
    filled.size.x *= clampf(float(_left) / _full, 0.0, 1.0)
    draw_rect(filled, get_theme_color("score_negative" if _left < LOW_FUEL else "party",
        "OpenGoldPalette"))
    draw_rect(rect, get_theme_color("line", "OpenGoldPalette"), false)
