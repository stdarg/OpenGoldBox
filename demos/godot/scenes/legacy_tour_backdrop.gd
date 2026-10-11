extends Control


func _ready() -> void:
	for name in ["SceneBounds", "MapBounds", "DialogueBounds"]:
		(get_parent().get_node(name) as Control).resized.connect(queue_redraw)
	resized.connect(queue_redraw)


func _draw() -> void:
	var parent := get_parent()
	var palette := "OpenGoldPalette"
	var metrics := "OpenGoldMetrics"
	draw_rect(Rect2(Vector2.ZERO, size), get_theme_color("background", palette))
	var margin := get_theme_constant("tour_header_margin", metrics)
	var y := get_theme_constant("tour_header_y", metrics)
	draw_line(Vector2(margin, y), Vector2(size.x - margin, y),
		get_theme_color("line", palette))
	var dialogue := (parent.get_node("DialogueBounds") as Control).get_rect()
	draw_rect(dialogue, get_theme_color("panel", palette))
	draw_rect(dialogue, get_theme_color("line", palette), false)
	draw_line(dialogue.position, dialogue.position + Vector2(dialogue.size.x, 0),
		get_theme_color("gold", palette),
		get_theme_constant("tour_dialogue_line_width", metrics))
	var scene := (parent.get_node("SceneBounds") as Control).get_rect()
	draw_rect(scene, get_theme_color("panel", palette))
	draw_rect(scene, get_theme_color("line", palette), false)
	var map := (parent.get_node("MapBounds") as Control).get_rect()
	draw_rect(map, get_theme_color("tour_map_backdrop", palette))
	draw_rect(map, get_theme_color("line", palette), false)
