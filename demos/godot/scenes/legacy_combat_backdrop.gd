extends Control


func _ready() -> void:
	(get_parent().get_node("BattlefieldBounds") as Control).resized.connect(queue_redraw)
	resized.connect(queue_redraw)


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), get_theme_color("background", "OpenGoldPalette"))
	var board := (get_parent().get_node("BattlefieldBounds") as Control).get_rect()
	draw_rect(board, get_theme_color("combat_board", "OpenGoldPalette"))
