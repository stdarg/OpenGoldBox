extends PanelContainer

# The notice follows the viewport while it is visible. Width and margins are
# named theme constants; the content determines its height.
func _process(_delta: float) -> void:
    if not visible:
        return
    var viewport_size := get_tree().root.size
    var margin := get_theme_constant("screenshot_notice_margin", "OpenGoldMetrics")
    var max_width := get_theme_constant("screenshot_notice_max_width", "OpenGoldMetrics")
    var minimum := Vector2(maxi(1, mini(max_width, viewport_size.x - 2 * margin)), 0)
    custom_minimum_size = minimum
    size = minimum
    position = Vector2(margin, maxf(0.0, viewport_size.y - size.y - margin))
