extends Control

# Decorative creation panels and images use scene-authored bounds and theme
# colors. Character art itself is supplied by the native view.
var _images: Array = []
var _show_dice := false


func set_images(portrait: Texture2D, ready: Texture2D, action: Texture2D,
        show_dice: bool) -> void:
    _images = [portrait, ready, action]
    _show_dice = show_dice
    queue_redraw()


func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        queue_redraw()


func _bounds(name: String) -> Rect2:
    return (get_parent().get_node(name) as Control).get_rect()


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), _color("background"))
    for name in ["PageBounds", "PreviewBounds"]:
        var rect := _bounds(name)
        draw_rect(rect, _color("panel"))
        draw_rect(rect, _color("creation_panel_border"), false)
    for name in ["ReadyBounds", "ActionBounds"]:
        draw_rect(_bounds(name), _color("creation_field"))
    if _show_dice:
        for index in 6:
            var rect := _bounds("Dice%d" % index)
            draw_rect(rect, _color("creation_field"))
            draw_rect(rect, _color("creation_border"), false)
    var portrait_rect := _bounds("PortraitBounds")
    draw_rect(portrait_rect, _color("creation_field"))
    if _images.size() == 3:
        if _images[0] != null:
            draw_texture_rect(_images[0], portrait_rect, false)
        if _images[1] != null:
            draw_texture_rect(_images[1], _bounds("ReadyBounds"), false)
        if _images[2] != null:
            draw_texture_rect(_images[2], _bounds("ActionBounds"), false)
