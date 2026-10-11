extends Button

# The selected color is character art data. Borders, padding, and text colors
# remain theme items so they can be adjusted without rebuilding the extension.


func _box(fill: Color, border: Color, width: int = 0) -> StyleBoxFlat:
    var style := StyleBoxFlat.new()
    style.bg_color = fill
    style.border_color = border
    style.set_border_width_all(width if width else get_theme_constant("swatch_border_width", "OpenGoldMetrics"))
    style.set_corner_radius_all(get_theme_constant("swatch_corner_radius", "OpenGoldMetrics"))
    style.set_content_margin_all(get_theme_constant("swatch_padding", "OpenGoldMetrics"))
    return style


func configure_palette(fill: Color) -> void:
    add_theme_stylebox_override("normal", _box(fill, get_theme_color("swatch_border", "OpenGoldPalette")))
    add_theme_stylebox_override("hover", _box(fill,
        get_theme_color("score_active_border", "OpenGoldPalette"),
        get_theme_constant("swatch_selected_border_width", "OpenGoldMetrics")))
    add_theme_stylebox_override("focus", _box(
        get_theme_color("swatch_focus_bg", "OpenGoldPalette"),
        get_theme_color("swatch_hover", "OpenGoldPalette"),
        get_theme_constant("swatch_focus_border_width", "OpenGoldMetrics")))


func configure_part(fill: Color, selected: bool) -> void:
    add_theme_stylebox_override("normal", _box(fill,
        get_theme_color("swatch_selected" if selected else "swatch_border", "OpenGoldPalette"),
        get_theme_constant("swatch_selected_border_width" if selected else "swatch_border_width",
            "OpenGoldMetrics")))
    add_theme_stylebox_override("hover", _box(fill,
        get_theme_color("swatch_hover", "OpenGoldPalette"),
        get_theme_constant("swatch_hover_border_width", "OpenGoldMetrics")))
    add_theme_stylebox_override("disabled", _box(
        get_theme_color("swatch_disabled_bg", "OpenGoldPalette"),
        get_theme_color("swatch_disabled_border", "OpenGoldPalette")))
    var contrast := (fill.r * get_theme_constant("swatch_red_luma", "OpenGoldMetrics")
        + fill.g * get_theme_constant("swatch_green_luma", "OpenGoldMetrics")
        + fill.b * get_theme_constant("swatch_blue_luma", "OpenGoldMetrics"))
    var foreground := get_theme_color(
        "swatch_dark_text" if contrast > get_theme_constant("swatch_contrast_threshold", "OpenGoldMetrics")
        else "swatch_light_text", "OpenGoldPalette")
    add_theme_color_override("font_color", foreground)
    add_theme_color_override("font_hover_color", foreground)
