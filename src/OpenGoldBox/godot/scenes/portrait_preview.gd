extends TextureRect

# The chosen portrait at the top right; a double click shows it full size.
var _file := ""
var _caption := ""


# The portrait catalog's picture of the file, which it has already read: the
# large source file is not read a second time.
func show_portrait(file: String, caption: String, picture: Texture2D) -> void:
    _file = file
    _caption = caption
    texture = picture


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
