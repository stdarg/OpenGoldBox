extends Button


func set_highlight(active: bool) -> void:
    theme_type_variation = &"BuyerRow" if active else &"ButtonText16"
