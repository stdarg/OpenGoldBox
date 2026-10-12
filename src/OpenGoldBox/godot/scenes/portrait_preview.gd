extends TextureRect

# The chosen portrait at the top right; a double click shows it full size.
var _file := ""
var _caption := ""


func show_portrait(file: String, caption: String) -> void:
    _file = file
    _caption = caption
    texture = load(file)


func _gui_input(event: InputEvent) -> void:
    var click := event as InputEventMouseButton
    if click != null and click.pressed and click.double_click and not _file.is_empty():
        accept_event()
        owner.get_node("PortraitZoom").call("open", _file, _caption)
