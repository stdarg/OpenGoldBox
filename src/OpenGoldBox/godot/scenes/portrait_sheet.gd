extends Control

# A contact sheet of portrait thumbnails. A click or the arrow keys choose a
# portrait; a double click shows it full size in the view's PortraitZoom.
signal portrait_chosen(index: int)

# False once the character joins the party: its portrait is then fixed.
var enabled := true

var _files := PackedStringArray()
var _captions := PackedStringArray()
var _selected := -1
var _thumbnails := {}
var _pending: Array[String] = []


func show_portraits(files: PackedStringArray, captions: PackedStringArray, selected: int,
        choosable: bool) -> void:
    _files = files
    _captions = captions
    _selected = selected
    enabled = choosable
    _pending.clear()
    for file in files:
        if not _thumbnails.has(file):
            _pending.append(file)
    _fit_height()
    _reveal.call_deferred()
    queue_redraw()


# Scrolls so the chosen portrait is in view.
func _reveal() -> void:
    var scroll := get_parent() as ScrollContainer
    if _selected < 0 or scroll == null:
        return
    var rect := _rect(_selected)
    if rect.position.y < scroll.scroll_vertical:
        scroll.scroll_vertical = int(rect.position.y)
    elif rect.end.y > scroll.scroll_vertical + scroll.size.y:
        scroll.scroll_vertical = int(rect.end.y - scroll.size.y)


func _cell() -> int:
    return get_theme_constant("portrait_thumbnail_size", "OpenGoldMetrics")


func _gap() -> int:
    return get_theme_constant("portrait_thumbnail_gap", "OpenGoldMetrics")


func _columns() -> int:
    return maxi(1, int((size.x + _gap()) / (_cell() + _gap())))


# Tall enough for every row, so the enclosing scroll container can scroll.
func _fit_height() -> void:
    var rows := ceili(float(_files.size()) / _columns())
    custom_minimum_size.y = rows * (_cell() + _gap())


func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        _fit_height()
        queue_redraw()
    elif what == NOTIFICATION_VISIBILITY_CHANGED and is_visible_in_tree():
        grab_focus.call_deferred()


# Source portraits are large, so thumbnails are made a few per frame, and
# only while the sheet is shown: the other creation pages would lag.
func _process(_delta: float) -> void:
    if _pending.is_empty() or not is_visible_in_tree():
        return
    for made in 8:
        if _pending.is_empty():
            break
        var file: String = _pending.pop_front()
        var image := (load(file) as Texture2D).get_image()
        image.resize(_cell(), _cell(), Image.INTERPOLATE_LANCZOS)
        _thumbnails[file] = ImageTexture.create_from_image(image)
    queue_redraw()


func _rect(index: int) -> Rect2:
    var step := _cell() + _gap()
    return Rect2(Vector2(index % _columns(), index / _columns()) * step, Vector2.ONE * _cell())


func _draw() -> void:
    for index in _files.size():
        var rect := _rect(index)
        var thumbnail: Texture2D = _thumbnails.get(_files[index])
        if thumbnail != null:
            draw_texture_rect(thumbnail, rect, false)
        else:
            draw_rect(rect, get_theme_color("creation_field", "OpenGoldPalette"))
        if index == _selected:
            draw_rect(rect.grow(1), get_theme_color("score_active_border", "OpenGoldPalette"),
                false, get_theme_constant("swatch_selected_border_width", "OpenGoldMetrics"))


func _gui_input(event: InputEvent) -> void:
    var key := event as InputEventKey
    if key != null and key.pressed and enabled and not _files.is_empty():
        var steps := {KEY_LEFT: -1, KEY_RIGHT: 1, KEY_UP: -_columns(), KEY_DOWN: _columns()}
        if steps.has(key.keycode):
            accept_event()
            var next: int = clampi(maxi(_selected, 0) + steps[key.keycode] if _selected >= 0 else 0,
                0, _files.size() - 1)
            if next != _selected:
                portrait_chosen.emit(next)
        return
    var click := event as InputEventMouseButton
    if click == null or not click.pressed or click.button_index != MOUSE_BUTTON_LEFT:
        return
    var step := _cell() + _gap()
    var cell := Vector2i(click.position / step)
    var index := cell.y * _columns() + cell.x
    if cell.x >= _columns() or index < 0 or index >= _files.size():
        return
    accept_event()
    grab_focus()
    if click.double_click:
        owner.get_node("PortraitZoom").call("open", _files[index], _captions[index])
    elif enabled:
        portrait_chosen.emit(index)
