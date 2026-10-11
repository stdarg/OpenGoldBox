extends Control


func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        queue_redraw()


func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), get_theme_color("background", "OpenGoldPalette"))
    var scroll: ScrollContainer = get_parent().get_node("BattlefieldScroll")
    draw_rect(scroll.get_rect(), get_theme_color("combat_board", "OpenGoldPalette"))
