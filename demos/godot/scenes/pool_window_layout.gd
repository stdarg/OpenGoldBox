extends Window


func fit(view_size: Vector2i) -> void:
	var metrics := "OpenGoldMetrics"
	size = Vector2i(
		mini(get_theme_constant("pool_max_width", metrics),
			view_size.x - get_theme_constant("pool_horizontal_margin", metrics)),
		view_size.y - get_theme_constant("pool_vertical_margin", metrics))
