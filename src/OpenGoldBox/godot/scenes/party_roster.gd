extends ItemList


func mark_leader(index: int) -> void:
    set_item_custom_fg_color(index, get_theme_color("gold", "OpenGoldPalette"))


func set_advancement(active: bool) -> void:
    theme_type_variation = &"LevelUpRoster" if active else &"ItemList"
