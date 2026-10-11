extends Button


func _style(fill: Color, border: Color, width: int) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = fill
	style.border_color = border
	style.set_border_width_all(width)
	style.set_corner_radius_all(get_theme_constant("combat_swatch_corner_radius", "OpenGoldMetrics"))
	style.set_content_margin_all(get_theme_constant("combat_swatch_padding", "OpenGoldMetrics"))
	return style


func configure(fill: Color, selected: bool) -> void:
	var palette := "OpenGoldPalette"
	var metrics := "OpenGoldMetrics"
	add_theme_stylebox_override("normal", _style(fill,
		get_theme_color("score_active_border" if selected else "swatch_border", palette),
		get_theme_constant("combat_swatch_selected_width" if selected else "combat_swatch_width", metrics)))
	add_theme_stylebox_override("hover", _style(fill,
		get_theme_color("swatch_hover", palette),
		get_theme_constant("combat_swatch_hover_width", metrics)))
	add_theme_stylebox_override("pressed", _style(fill.darkened(
		get_theme_constant("combat_swatch_pressed_darkening_percent", metrics) / 100.0),
		get_theme_color("score_active_border", palette),
		get_theme_constant("combat_swatch_selected_width", metrics)))
	var contrast := (fill.r * 0.299 + fill.g * 0.587 + fill.b * 0.114)
	var foreground := get_theme_color(
		"swatch_dark_text" if contrast > 0.5 else "swatch_light_text", palette)
	for state in ["font_color", "font_hover_color", "font_pressed_color", "font_focus_color"]:
		add_theme_color_override(state, foreground)
