extends Control

# Decorative creation panels and images use scene-authored bounds and theme
# colors. Character art itself is supplied by the native view.
var _images: Array = []
var _show_dice := false
var _show_portrait := false
var _show_icons := false
var _race_portrait: Texture2D


# `race_portrait` is a sample portrait of the race chosen on the first page,
# null on the other pages.
func set_images(portrait: Texture2D, ready: Texture2D, action: Texture2D,
        show_dice: bool, show_portrait: bool, show_icons: bool,
        race_portrait: Texture2D) -> void:
    _images = [portrait, ready, action]
    _show_dice = show_dice
    _show_portrait = show_portrait
    _show_icons = show_icons
    _race_portrait = race_portrait
    queue_redraw()


func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        queue_redraw()


func _bounds(name: String) -> Rect2:
    return (get_parent().get_node(name) as Control).get_rect()


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


# The largest rect with the texture's proportions that fits, centered. Combat
# icons are pixel art, so they only grow by whole multiples.
func _fitted(texture: Texture2D, bounds: Rect2, whole_multiples: bool) -> Rect2:
    var source := Vector2(texture.get_size())
    var scale := minf(bounds.size.x / source.x, bounds.size.y / source.y)
    if whole_multiples:
        scale = maxf(1.0, floorf(scale))
    var fitted := source * scale
    return Rect2((bounds.position + (bounds.size - fitted) / 2).floor(), fitted)


func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), _color("background"))
    var page := _bounds("PageBounds")
    draw_rect(page, _color("panel"))
    draw_rect(page, _color("creation_panel_border"), false)
    if _show_dice:
        for index in 6:
            var rect := _bounds("Dice%d" % index)
            draw_rect(rect, _color("creation_field"))
            draw_rect(rect, _color("creation_border"), false)
    if _race_portrait != null:
        # Kept to the column's top left so the text beside it starts level with it.
        var bounds := _bounds("RacePortraitBounds")
        var rect := _fitted(_race_portrait, bounds, false)
        rect.position = bounds.position
        draw_texture_rect(_race_portrait, rect, false)
    if _images.size() != 3:
        return
    if _show_portrait and _images[0] != null:
        draw_texture_rect(_images[0], _fitted(_images[0], _bounds("PortraitBounds"), false),
                false)
    if _show_icons:
        for index in [1, 2]:
            if _images[index] == null:
                continue
            var bounds := _bounds("ReadyBounds" if index == 1 else "ActionBounds")
            var rect := _fitted(_images[index], bounds, true)
            draw_rect(rect, _color("creation_field"))
            draw_texture_rect(_images[index], rect, false)
