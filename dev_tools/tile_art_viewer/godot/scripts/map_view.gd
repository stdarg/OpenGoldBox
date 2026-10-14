extends Control

signal pixel_clicked(pixel: Vector2i)
signal next_piece_requested

var source_image: Image
var texture: ImageTexture
var selected_pixel := Vector2i(-1, -1)

func set_image(image: Image) -> void:
    source_image = image
    texture = ImageTexture.create_from_image(image) if image != null else null
    selected_pixel = Vector2i(-1, -1)
    queue_redraw()

func set_selection(pixel: Vector2i) -> void:
    selected_pixel = pixel
    queue_redraw()

func _image_rect() -> Rect2:
    var side := minf(size.x, size.y)
    return Rect2((size - Vector2(side, side)) / 2.0, Vector2(side, side))

func _draw() -> void:
    if texture != null:
        draw_texture_rect(texture, _image_rect(), false)
    if source_image != null and selected_pixel.x >= 0 and selected_pixel.y >= 0:
        var rect := _image_rect()
        var center := rect.position + (Vector2(selected_pixel) + Vector2(0.5, 0.5)) / \
            Vector2(source_image.get_size()) * rect.size
        draw_arc(center, 9.0, 0.0, TAU, 32, Color(0.05, 0.08, 0.1), 5.0)
        draw_arc(center, 9.0, 0.0, TAU, 32, Color(1.0, 0.82, 0.28), 2.5)

func _gui_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo and \
            (event.keycode == KEY_ENTER or event.keycode == KEY_KP_ENTER or event.keycode == KEY_SPACE):
        next_piece_requested.emit()
        accept_event()
        return
    if not event is InputEventMouseButton or event.button_index != MOUSE_BUTTON_LEFT or not event.pressed:
        return
    if source_image == null:
        return
    var rect := _image_rect()
    if not rect.has_point(event.position):
        return
    var local: Vector2 = (event.position - rect.position) / rect.size
    var pixel := Vector2i(clampi(floori(local.x * source_image.get_width()), 0, source_image.get_width() - 1),
                          clampi(floori(local.y * source_image.get_height()), 0, source_image.get_height() - 1))
    grab_focus()
    pixel_clicked.emit(pixel)
    accept_event()
