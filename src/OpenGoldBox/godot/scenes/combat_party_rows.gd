extends Control

# The combat view supplies character state and art. This scene-owned canvas
# places and styles the party display using editable theme metrics.
var _rows: Array = []


func _metric(name: String) -> float:
    return get_theme_constant(name, "OpenGoldMetrics")


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


func set_rows(rows: Array) -> void:
    _rows = rows
    queue_redraw()


func slot_at_root(local: Vector2) -> int:
    if not Rect2(position, size).has_point(local):
        return -1
    return mini(int((local.y - position.y) / (size.y / 8.0)), 7)


func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        queue_redraw()


func _line(font: Font, value: String, x: float, y: float, font_size: int, color: Color) -> void:
    var cursor := Vector2(x, y)
    for index in value.length():
        cursor.x += font.draw_char(get_canvas_item(), cursor, value.unicode_at(index), font_size, color)


func _draw() -> void:
    if _rows.size() != 8:
        return
    var font := get_theme_default_font()
    var row_height := size.y / 8.0
    var name_size := int(_metric("combat_name_font_size"))
    var detail_size := int(_metric("combat_detail_font_size"))
    var quick_size := int(_metric("combat_quick_font_size"))
    for slot in 8:
        var entry: Dictionary = _rows[slot]
        var top := slot * row_height
        draw_rect(Rect2(0, top, size.x, row_height - _metric("combat_party_row_gap")),
            _color("combat_row_selected" if entry.get("selected", false) else "combat_row"))
        if entry.is_empty():
            continue
        var portrait_size := minf(_metric("combat_party_portrait_max"),
            row_height - _metric("combat_party_portrait_height_reserve"))
        var portrait_y := top + _metric("combat_party_portrait_top")
        var image_rect := Rect2(_metric("combat_party_portrait_left"), portrait_y,
            portrait_size, portrait_size)
        draw_rect(image_rect, _color("creation_field"))
        var portrait: Texture2D = entry.get("portrait")
        if portrait != null:
            draw_texture_rect(portrait, image_rect, false)
        var hp_y := portrait_y + portrait_size + _metric("combat_party_hp_gap")
        var hp_height := _metric("combat_party_hp_height")
        draw_rect(Rect2(image_rect.position.x, hp_y, portrait_size, hp_height),
            _color("combat_hp_track"))
        var hp: int = entry["hp"]
        var maximum: int = maxi(1, entry["maximum"])
        var hp_color := "score_negative" if hp <= 0 or hp * 100 <= \
            maximum * _metric("combat_low_hp_percent") else \
            "score_positive" if hp < maximum else "hp_full"
        draw_rect(Rect2(image_rect.position.x, hp_y,
            portrait_size * clampf(float(hp) / maximum, 0.0, 1.0), hp_height), _color(hp_color))
        var text_x := _metric("combat_party_text_left")
        _line(font, entry["name"], text_x,
            top + _metric("combat_party_name_baseline"), name_size, _color("combat_name"))
        if entry.get("quick", false):
            var tag := Rect2(size.x - _metric("combat_party_quick_right")
                - _metric("combat_party_quick_width"),
                top + _metric("combat_party_quick_top"),
                _metric("combat_party_quick_width"), _metric("combat_party_quick_height"))
            draw_rect(tag, _color("combat_quick_bg"))
            draw_rect(tag, _color("combat_quick_border"), false,
                _metric("combat_party_quick_border_width"))
            var quick_text: String = entry["quick_text"]
            var width := 0.0
            for index in quick_text.length():
                width += font.get_char_size(quick_text.unicode_at(index), quick_size).x
            _line(font, quick_text, tag.get_center().x - width / 2.0,
                top + _metric("combat_party_quick_text_baseline"), quick_size,
                _color("combat_quick_text"))
        _line(font, entry["detail"], text_x,
            top + _metric("combat_party_detail_baseline"), detail_size, _color("combat_detail"))
