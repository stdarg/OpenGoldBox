extends Control


func set_board_dimensions(columns: int, rows: int, zoom_percent: int) -> void:
	var tile := get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics")
	custom_minimum_size = Vector2(columns, rows) * tile * zoom_percent / 100.0


func place_figure(name: String, source: Vector2, visible: Rect2, cell: Vector2,
		sizing: int, zoom_percent: int) -> void:
	var tile := get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics") \
		* zoom_percent / 100.0
	var sprite := get_node(name) as TextureRect
	var rect := Rect2(cell * tile, source * zoom_percent / 100.0)
	if sizing != 0 and visible.size.x > 0 and visible.size.y > 0:
		var height_scale := (get_theme_constant("combat_sprite_goliath_height_percent",
			"OpenGoldMetrics") / 100.0) * get_theme_constant(
			"combat_sprite_tile_pixels", "OpenGoldMetrics") / visible.size.y
		var art_scale := Vector2(get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics")
			/ visible.size.x if sizing == 1 else height_scale, height_scale)
		var occupied := Rect2(cell * tile + Vector2(0, tile), Vector2(tile, tile))
		var extent := occupied.size * visible.size * art_scale \
			/ get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics")
		var scale := extent / visible.size
		var top := occupied.position + Vector2(
			(occupied.size.x - extent.x) * 0.5, occupied.size.y - extent.y)
		rect = Rect2(top - visible.position * scale, source * scale)
	sprite.position = rect.position
	sprite.size = rect.size
