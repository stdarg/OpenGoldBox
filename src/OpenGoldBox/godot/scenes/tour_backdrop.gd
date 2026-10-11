extends Control


func _ready() -> void:
	for node_name in ["HeaderRule", "SceneBounds", "MapBounds", "DialogueBounds"]:
		var bounds := get_parent().get_node(node_name) as Control
		bounds.resized.connect(queue_redraw)
	resized.connect(queue_redraw)


func _draw() -> void:
	var palette := "OpenGoldPalette"
	var metrics := "OpenGoldMetrics"
	var parent := get_parent()
	draw_rect(Rect2(Vector2.ZERO, size), get_theme_color("background", palette))
	var header := (parent.get_node("HeaderRule") as Control).get_rect()
	draw_line(header.position, header.end, get_theme_color("line", palette),
		get_theme_constant("tour_header_line_width", metrics))
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
	draw_rect(map, get_theme_color("map_black", palette))
	draw_rect(map, get_theme_color("line", palette), false)
