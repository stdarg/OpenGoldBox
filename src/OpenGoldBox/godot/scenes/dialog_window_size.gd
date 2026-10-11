extends Control


func apply_window(window: Window) -> void:
    var configured := Vector2i(size)
    window.size = configured
    window.min_size = configured
