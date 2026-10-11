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


func figure_color(index: int) -> Color:
	var name := "combat_party" if index < 2 else "score_active_border" if index == 2 \
		else "combat_sprite_goliath" if index == 3 else "combat_enemy"
	return get_theme_color(name, "OpenGoldPalette")


func report_heading() -> String:
	return "[b]Sprite dimensions — source → displayed pixels[/b]\n"


func report_figure_label(label: String, index: int) -> String:
	return "[color=#" + figure_color(index).to_html(false) + "]" + label + "[/color]"


func visible_size(name: String, source: Vector2, visible: Rect2) -> Vector2:
	var sprite := get_node(name) as TextureRect
	return visible.size * sprite.size / source


func center_cell(zoom_percent: int) -> Vector2:
	var scroll := get_parent() as ScrollContainer
	var tile := get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics") \
		* zoom_percent / 100.0
	return (Vector2(scroll.scroll_horizontal, scroll.scroll_vertical) + scroll.size * 0.5) / tile


func center_when_ready(cell: Vector2, zoom_percent: int) -> bool:
	if size.x < custom_minimum_size.x or size.y < custom_minimum_size.y:
		return false
	var scroll := get_parent() as ScrollContainer
	var tile := get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics") \
		* zoom_percent / 100.0
	var offset := cell * tile - scroll.size * 0.5
	scroll.scroll_horizontal = maxi(0, int(offset.x))
	scroll.scroll_vertical = maxi(0, int(offset.y))
	return true


func _draw() -> void:
	var data: Dictionary = get_parent().get_parent().sprite_visuals()
	if data.is_empty():
		return
	var metric := "OpenGoldMetrics"
	var palette := "OpenGoldPalette"
	var tile: float = get_theme_constant("combat_sprite_tile_pixels", metric) * data["zoom"] / 100.0
	var width: int = data["width"]
	var tiles: Array = data["tiles"]
	for i in tiles.size():
		var rect := Rect2(Vector2(i % width, i / width) * tile, Vector2.ONE * tile)
		draw_texture_rect(tiles[i], rect, false)
		draw_rect(rect, get_theme_color("combat_sprite_grid", palette), false,
			get_theme_constant("combat_sprite_grid_width", metric))
	var figures: Array = data["figures"]
	for i in figures.size():
		var figure: Dictionary = figures[i]
		var color := figure_color(i)
		var guide: Rect2 = Rect2(figure["cell"] * tile, Vector2.ONE * tile * figure["footprint"])
		draw_rect(guide, color, false, get_theme_constant("combat_sprite_guide_width", metric))
		if figure["sizing"] != 0:
			var height := get_theme_constant("combat_sprite_goliath_height_percent", metric) / 100.0
			var top := guide.position + Vector2(0, tile * (2.0 - height))
			var headroom := color
			headroom.a = get_theme_constant("combat_sprite_headroom_percent", metric) / 100.0
			draw_rect(Rect2(top, Vector2(tile, tile * get_theme_constant(
				"combat_sprite_headroom_height_percent", metric) / 100.0)), headroom)
			draw_line(top, top + Vector2(tile, 0), color,
				get_theme_constant("combat_sprite_grid_width", metric))
			draw_line(guide.position + Vector2(0, tile),
				guide.position + Vector2(tile, tile), color,
				get_theme_constant("combat_sprite_grid_width", metric))
			draw_line(guide.position + Vector2(0, 2.0 * tile),
				guide.position + Vector2(tile, 2.0 * tile), color,
				get_theme_constant("combat_sprite_baseline_width", metric))
		if i < 4 and tile >= get_theme_constant("combat_sprite_tile_pixels", metric):
			var captions := ["Short", "Human", "1 — Stretched", "2 — Proportional"]
			var caption_position := guide.position + Vector2(
				tile * get_theme_constant("combat_sprite_caption_x_percent", metric) / 100.0,
				guide.size.y + get_theme_constant("combat_sprite_caption_y", metric))
			draw_string(get_theme_default_font(), caption_position, captions[i],
				HORIZONTAL_ALIGNMENT_CENTER,
				tile * get_theme_constant("combat_sprite_caption_width_percent", metric) / 100.0,
				get_theme_constant("combat_sprite_caption_size", metric), color)
