extends Button


func set_selected_style(active: bool) -> void:
    theme_type_variation = &"SoundBoardButtonSelected" if active else &"SoundBoardButton"
