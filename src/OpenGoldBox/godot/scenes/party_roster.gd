extends ItemList

# Each member is a photo beside four lines: name, class, race and gender.
# ItemList draws only one line beside an icon, so the item text stays (for
# keyboard search and checks) but is hidden, and the lines are drawn here.
var _members: Array = []
var _leader := -1
# While a member is dragged: the row the line marks (-1 for none), and whether
# the line runs below it rather than above.
var _drop_row := -1
var _drop_below := false


func show_members(members: Array) -> void:
    _members = members
    for index in item_count:
        set_item_custom_fg_color(index, Color.TRANSPARENT)
    queue_redraw()


func mark_leader(index: int) -> void:
    _leader = index


func show_drop(row: int, below: bool) -> void:
    if row == _drop_row and below == _drop_below:
        return
    _drop_row = row
    _drop_below = below
    queue_redraw()


func _notification(what: int) -> void:
    if what == NOTIFICATION_DRAG_END:
        show_drop(-1, false)


func set_advancement(active: bool) -> void:
    theme_type_variation = &"LevelUpRoster" if active else &"ItemList"


# Where an item's text starts, past its photo.
func text_left(index: int) -> float:
    return get_item_rect(index).position.x + fixed_icon_size.x + get_theme_constant("h_separation")


func _draw() -> void:
    var font := get_theme_font("font")
    var font_size := get_theme_font_size("font_size")
    var line := font.get_height(font_size)
    var scroll := get_v_scroll_bar().value
    for index in mini(_members.size(), item_count):
        var lines: PackedStringArray = _members[index]
        var rect := get_item_rect(index)
        var top := rect.position.y - scroll + (rect.size.y - line * lines.size()) / 2
        var color := get_theme_color("font_selected_color" if is_selected(index) else "font_color")
        for row in lines.size():
            var shade := get_theme_color("gold", "OpenGoldPalette") \
                if row == 0 and index == _leader else color
            draw_string(font, Vector2(text_left(index), top + font.get_ascent(font_size) + row * line),
                lines[row], HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, shade)
    if _drop_row >= 0 and _drop_row < item_count:
        var target := get_item_rect(_drop_row)
        var y := target.position.y - scroll + (target.size.y if _drop_below else 0.0)
        draw_line(Vector2(target.position.x, y), Vector2(target.end.x, y),
            get_theme_color("gold", "OpenGoldPalette"), 3.0)
