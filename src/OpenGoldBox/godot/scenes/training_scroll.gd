extends ScrollContainer

var _groups := ""


func reset_for(signature: String) -> void:
    if signature != _groups:
        scroll_vertical = 0
        _groups = signature
