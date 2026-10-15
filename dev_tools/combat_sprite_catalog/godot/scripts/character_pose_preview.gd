extends Control

# Match the character creation preview: Short and Regular use a square field;
# Large stretches the tall art into the full-height field with feet aligned.
var sprite: Texture2D
var size_index := 1

func set_pose(texture: Texture2D, icon_size: int) -> void:
    sprite = texture
    size_index = icon_size
    queue_redraw()

func _notification(what: int) -> void:
    if what == NOTIFICATION_RESIZED:
        queue_redraw()

func _draw() -> void:
    if sprite == null:
        return
    var source := Vector2(sprite.get_size())
    var large_height := get_theme_constant("combat_goliath_height_percent", "OpenGoldMetrics") / 100.0
    var scale := maxf(1.0, floorf(minf(size.x / source.x,
        size.y / (source.y * large_height))))
    var field_size := source * scale * Vector2(1.0, large_height)
    var field := Rect2(((size - field_size) / 2.0).floor(), field_size)
    draw_rect(field, get_theme_color("preview_field", "SpritePreview"))
    if size_index != 2:
        draw_texture_rect(sprite, Rect2(field.end - source * scale, source * scale), false)
        return
    var visible := Rect2(sprite.get_image().get_used_rect())
    if visible.size.x <= 0 or visible.size.y <= 0:
        return
    var stretch := field.size / visible.size
    draw_texture_rect(sprite, Rect2(field.position - visible.position * stretch,
        source * stretch), false)
