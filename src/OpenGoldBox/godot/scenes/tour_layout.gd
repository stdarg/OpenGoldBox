extends AnimationPlayer

var _authored_offsets: Dictionary = {}


func _restore_control(control: Control, parent_width: float) -> void:
    if not _authored_offsets.has(control):
        _authored_offsets[control] = [control.offset_left, control.offset_top,
            control.offset_right, control.offset_bottom]
    var offsets: Array = _authored_offsets[control]
    var delta := Rect2()
    var factor := 0.0
    if control.has_meta("layout_middle_delta"):
        delta = control.get_meta("layout_middle_delta")
        var small := control.get_theme_constant("layout_small_width", "OpenGoldMetrics")
        var middle := control.get_theme_constant("layout_middle_width", "OpenGoldMetrics")
        var design := control.get_theme_constant("layout_design_width", "OpenGoldMetrics")
        factor = (clampf((parent_width - small) / (middle - small), 0.0, 1.0)
            if parent_width <= middle else
            clampf((design - parent_width) / (design - middle), 0.0, 1.0))
    control.offset_left = offsets[0] + delta.position.x * factor
    control.offset_top = offsets[1] + delta.position.y * factor
    control.offset_right = offsets[2] + (delta.position.x + delta.size.x) * factor
    control.offset_bottom = offsets[3] + (delta.position.y + delta.size.y) * factor


func apply_layout(shopping: bool) -> void:
    var view := get_parent() as Control
    play("normal")
    advance(0)
    for child in view.get_children():
        if child is Control and child.has_meta("layout_reference"):
            _restore_control(child, view.size.x)
    if shopping:
        var small := view.get_theme_constant("layout_small_width", "OpenGoldMetrics")
        var middle := view.get_theme_constant("layout_middle_width", "OpenGoldMetrics")
        var design := view.get_theme_constant("layout_design_width", "OpenGoldMetrics")
        var position := (0.6 * clampf((view.size.x - small) / (middle - small), 0.0, 1.0)
            if view.size.x <= middle else
            0.6 + 0.4 * clampf((view.size.x - middle) / (design - middle), 0.0, 1.0))
        play("shopping")
        seek(position, true)
