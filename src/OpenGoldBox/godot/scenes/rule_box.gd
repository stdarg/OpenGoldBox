extends Container

# A row (or a column, when `vertical`) that places its controls like a box
# container, with optional size rules set as metadata on a control in the
# scene:
#
# - aspect: the control's shape, width / height. It takes the largest size of
#   that shape that fits: as tall as the row (as wide as the column), but no
#   longer than the room the other controls leave.
# - share: the control's length along the box as a percentage of the box,
#   kept between min_length and max_length (0 for no maximum).
#
# Other controls get their minimum length, and those set to expand share what
# is left by their stretch ratios. A shaped control adds nothing to the box's
# minimum size, so a smaller window shrinks it rather than pushing the page
# out of the window. The gap between controls is the theme's `separation`.

@export var vertical := false


func _notification(what: int) -> void:
    if what == NOTIFICATION_SORT_CHILDREN:
        _place_children()


func _get_minimum_size() -> Vector2:
    var axis := _axis()
    var total := Vector2.ZERO
    var shown := _shown()
    for control in shown:
        var least: Vector2 = Vector2.ZERO if control.has_meta("aspect") else control.get_combined_minimum_size()
        if control.has_meta("share"):
            least[axis] = maxf(least[axis], control.get_meta("min_length", 0.0))
        total[axis] += least[axis]
        total[1 - axis] = maxf(total[1 - axis], least[1 - axis])
    total[axis] += _gaps(shown.size())
    return total


func _place_children() -> void:
    var axis := _axis()
    var cross := 1 - axis
    var shown := _shown()
    var lengths := {}
    var free := size[axis] - _gaps(shown.size())
    for control in shown:
        if not control.has_meta("aspect"):
            lengths[control] = _fixed_length(control, axis)
            free -= lengths[control]
    for control in shown:
        if control.has_meta("aspect"):
            var aspect: float = control.get_meta("aspect")
            var wanted: float = size[cross] / aspect if vertical else size[cross] * aspect
            lengths[control] = clampf(wanted, 0.0, maxf(free, 0.0))
            free -= lengths[control]
    var stretch := 0.0
    for control in shown:
        if _expands(control):
            stretch += control.size_flags_stretch_ratio
    var position := 0.0
    for control in shown:
        var length: float = lengths[control]
        if _expands(control) and free > 0.0:
            length += free * control.size_flags_stretch_ratio / stretch
        var rect := Rect2()
        rect.position[axis] = position
        rect.size[axis] = length
        if control.has_meta("aspect"):
            var aspect: float = control.get_meta("aspect")
            rect.size[cross] = length * aspect if vertical else length / aspect
        else:
            rect.size[cross] = size[cross]
        fit_child_in_rect(control, rect)
        position += length + get_theme_constant("separation")


func _fixed_length(control: Control, axis: int) -> float:
    var length := control.get_combined_minimum_size()[axis]
    if control.has_meta("share"):
        var shared: float = size[axis] * control.get_meta("share") / 100.0
        shared = maxf(shared, control.get_meta("min_length", 0.0))
        var most: float = control.get_meta("max_length", 0.0)
        if most > 0.0:
            shared = minf(shared, most)
        length = maxf(length, shared)
    return length


func _expands(control: Control) -> bool:
    if control.has_meta("aspect") or control.has_meta("share"):
        return false
    var flags := control.size_flags_vertical if vertical else control.size_flags_horizontal
    return flags & SIZE_EXPAND != 0


func _shown() -> Array[Control]:
    var shown: Array[Control] = []
    for child in get_children():
        var control := child as Control
        if control != null and control.visible and not control.top_level:
            shown.append(control)
    return shown


func _axis() -> int:
    return 1 if vertical else 0


func _gaps(count: int) -> float:
    return get_theme_constant("separation") * maxi(count - 1, 0)
