extends Control

# Decorative creation panels and images use scene-authored bounds and theme
# colors. Character art itself is supplied by the native view.
var _images: Array = []
var _show_dice := false
var _show_icons := false
var _large_icons := false
var _race_portrait: Texture2D


# `race_portrait` is a sample portrait of the race chosen on the first page,
# null on the other pages. `large_icons` draws the combat icons at the Large
# size.
func set_images(ready: Texture2D, action: Texture2D,
        show_dice: bool, show_icons: bool,
        race_portrait: Texture2D, large_icons: bool) -> void:
    _images = [ready, action]
    _show_dice = show_dice
    _show_icons = show_icons
    _large_icons = large_icons
    _race_portrait = race_portrait
    queue_redraw()


func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        queue_redraw()


func _bounds(name: String) -> Rect2:
    return (get_parent().get_node(name) as Control).get_rect()


func _color(name: String) -> Color:
    return get_theme_color(name, "OpenGoldPalette")


# The largest rect with the texture's proportions that fits, centered.
func _fitted(texture: Texture2D, bounds: Rect2) -> Rect2:
    var source := Vector2(texture.get_size())
    var scale := minf(bounds.size.x / source.x, bounds.size.y / source.y)
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
        var rect := _fitted(_race_portrait, bounds)
        rect.position = bounds.position
        draw_texture_rect(_race_portrait, rect, false)
    if _images.size() != 2:
        return
    if _show_icons:
        for index in [0, 1]:
            if _images[index] != null:
                _draw_icon(_images[index], _bounds("ReadyBounds" if index == 0 else "ActionBounds"))


# Every size is drawn at the whole-multiple scale that leaves a Large figure
# room, so the sizes compare. The field is one square wide and as tall as a
# Large figure; Short and Regular figures fill its bottom square, and a Large
# one is stretched over its visible pixels to the field's full width and
# height, feet on the bottom edge, as combat draws it.
func _draw_icon(texture: Texture2D, bounds: Rect2) -> void:
    var field := _icon_field(texture, bounds)
    draw_rect(field, _color("creation_field"))
    var square := _icon_square(texture, bounds)
    var source := Vector2(texture.get_size())
    if not _large_icons:
        draw_texture_rect(texture, square, false)
        return
    var visible := Rect2(texture.get_image().get_used_rect())
    if visible.size.x <= 0 or visible.size.y <= 0:
        return
    var stretch := field.size / visible.size
    draw_texture_rect(texture, Rect2(field.position - visible.position * stretch,
        source * stretch), false)


func _icon_scale(texture: Texture2D, bounds: Rect2) -> float:
    var source := Vector2(texture.get_size())
    return maxf(1.0, floorf(minf(bounds.size.x / source.x,
        bounds.size.y / (source.y * _large_height()))))


func _large_height() -> float:
    return get_theme_constant("combat_goliath_height_percent", "OpenGoldMetrics") / 100.0


# One square wide and as tall as a Large figure, centered in the bounds.
func _icon_field(texture: Texture2D, bounds: Rect2) -> Rect2:
    var size := Vector2(texture.get_size()) * _icon_scale(texture, bounds) \
        * Vector2(1.0, _large_height())
    return Rect2((bounds.position + (bounds.size - size) / 2).floor(), size)


# The field's bottom square, where Short and Regular figures stand.
func _icon_square(texture: Texture2D, bounds: Rect2) -> Rect2:
    var size := Vector2(texture.get_size()) * _icon_scale(texture, bounds)
    return Rect2(_icon_field(texture, bounds).end - size, size)
