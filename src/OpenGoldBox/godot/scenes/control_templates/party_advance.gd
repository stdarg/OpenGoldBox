extends Button

# The same scene is instanced beside names in the roster ItemList and in the
# town's party row buttons. Theme metrics control the gap and edge insets.
func place_in_roster(index: int, display_name: String) -> bool:
    var list := get_parent() as ItemList
    var rect := list.get_item_rect(index)
    var y := rect.position.y - list.get_v_scroll_bar().value
    var font := list.get_theme_font("font")
    var width := font.get_string_size(display_name, HORIZONTAL_ALIGNMENT_LEFT, -1,
        list.get_theme_font_size("font_size")).x
    position = Vector2(minf(list.call("text_left", index) + width
        + list.get_theme_constant("party_arrow_gap", "OpenGoldMetrics"),
        list.size.x - list.get_theme_constant("party_arrow_right_inset", "OpenGoldMetrics")),
        y + list.get_theme_constant("party_arrow_offset_y", "OpenGoldMetrics"))
    return position.y >= 0 and position.y + size.y <= list.size.y


# The name the arrow follows in a town party row.
var _member_name := ""


# Beside the member's name, and again whenever the row is resized: a row that
# has just been shown has no width yet, and the containers size it later.
func place_in_member_row(display_name: String) -> void:
    _member_name = display_name
    var row := get_parent() as Button
    if not row.resized.is_connected(_place_beside_member_name):
        row.resized.connect(_place_beside_member_name)
    _place_beside_member_name()


func _place_beside_member_name() -> void:
    var row := get_parent() as Button
    var font := row.get_theme_font("font")
    var width := font.get_string_size(_member_name, HORIZONTAL_ALIGNMENT_LEFT, -1,
        row.get_theme_font_size("font_size")).x
    position = Vector2(minf(row.call("text_left") + width
        + row.get_theme_constant("party_arrow_gap", "OpenGoldMetrics"),
        row.size.x - row.get_theme_constant("tour_arrow_right_inset", "OpenGoldMetrics")),
        row.get_theme_constant("tour_arrow_top_inset", "OpenGoldMetrics"))
