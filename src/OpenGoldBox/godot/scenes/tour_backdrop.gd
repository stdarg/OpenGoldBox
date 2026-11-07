extends Control


func _ready() -> void:
	for node_name in ["HeaderRule", "SceneBounds", "MapBounds", "DialogueBounds"]:
		var bounds := get_parent().get_node("%" + node_name) as Control
		bounds.item_rect_changed.connect(queue_redraw)
	resized.connect(queue_redraw)


# Where a frame the layout placed sits on this full-screen backdrop.
func _bounds(node_name: String) -> Rect2:
	var bounds := get_parent().get_node("%" + node_name) as Control
	return Rect2(bounds.global_position - global_position, bounds.size)


func _draw() -> void:
	var palette := "OpenGoldPalette"
	var metrics := "OpenGoldMetrics"
	draw_rect(Rect2(Vector2.ZERO, size), get_theme_color("background", palette))
	var header := _bounds("HeaderRule")
	draw_line(header.position, header.end, get_theme_color("line", palette),
		get_theme_constant("tour_header_line_width", metrics))
	var dialogue := _bounds("DialogueBounds")
	draw_rect(dialogue, get_theme_color("panel", palette))
	draw_rect(dialogue, get_theme_color("line", palette), false)
	draw_line(dialogue.position, dialogue.position + Vector2(dialogue.size.x, 0),
		get_theme_color("gold", palette),
		get_theme_constant("tour_dialogue_line_width", metrics))
	var scene := _bounds("SceneBounds")
	draw_rect(scene, get_theme_color("panel", palette))
	draw_rect(scene, get_theme_color("line", palette), false)
	var map := _bounds("MapBounds")
	draw_rect(map, get_theme_color("map_black", palette))
	draw_rect(map, get_theme_color("line", palette), false)
