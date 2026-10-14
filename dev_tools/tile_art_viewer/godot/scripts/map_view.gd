extends Control

signal pixel_clicked(pixel: Vector2i)

var source_image: Image
var texture: ImageTexture

func set_image(image: Image) -> void:
    source_image = image
    texture = ImageTexture.create_from_image(image) if image != null else null
    queue_redraw()

func _image_rect() -> Rect2:
    var side := minf(size.x, size.y)
    return Rect2((size - Vector2(side, side)) / 2.0, Vector2(side, side))

func _draw() -> void:
    if texture != null:
        draw_texture_rect(texture, _image_rect(), false)

func _gui_input(event: InputEvent) -> void:
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
    pixel_clicked.emit(pixel)
    accept_event()
