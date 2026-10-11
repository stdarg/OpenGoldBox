extends Control


func set_board_dimensions(columns: int, rows: int, zoom_percent: int) -> void:
	var tile := get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics")
	custom_minimum_size = Vector2(columns, rows) * tile * zoom_percent / 100.0
