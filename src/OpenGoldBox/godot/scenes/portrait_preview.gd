extends TextureRect

# The chosen portrait at the top right; a double click shows it full size.
var _file := ""
var _caption := ""


func show_portrait(file: String, caption: String) -> void:
    _file = file
    _caption = caption
    # A file in the new art's folder, read as it is rather than imported.
    var image := Image.load_from_file(file)
    texture = null if image == null else ImageTexture.create_from_image(image)


# A picture without a file to zoom into: an original head-and-body portrait.
func show_texture(picture: Texture2D) -> void:
    _file = ""
    _caption = ""
    texture = picture


func _gui_input(event: InputEvent) -> void:
    var click := event as InputEventMouseButton
    if click != null and click.pressed and click.double_click and not _file.is_empty():
        accept_event()
        owner.get_node("PortraitZoom").call("open", _file, _caption)
