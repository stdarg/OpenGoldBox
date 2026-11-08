extends Button

# A party member's row in the town's party list: the portrait as the party
# roster shows it, the identity lines beside it (name, class, race, gender)
# and a second column close by (Armor Class and hit points, weapon, armor).
# A compact row, as a shop shows, has no second column: its Armor Class and
# hit points line comes under the identity lines. The current hit points take
# their own color when they run low.
# Button draws only one text, so the button's own text stays empty and the
# lines are drawn here; the tooltip holds them all.
signal opened

const PORTRAIT_SIZE := Vector2(100, 100)
const PORTRAIT_GAP := 10.0
# Little space between the two columns, as the roster's lines are read across.
const COLUMN_GAP := 24.0

var _portrait: Texture2D
var _identity := PackedStringArray()
# The Armor Class and hit points line in three parts: before, at and after the
# current hit points, which are drawn in `_health_color` when it names one.
var _health := PackedStringArray()
var _health_color := &""
var _condition := PackedStringArray()
var _leader := false
# A member who will fight for the monsters: its name is drawn in the original's
# yellow.
var _hostile := false


# A row with no `condition` lines is compact.
func show_member(portrait: Texture2D, identity: PackedStringArray, health: PackedStringArray,
        health_color: StringName, condition: PackedStringArray, leader: bool,
        hostile := false) -> void:
    _portrait = portrait
    _identity = identity
    _health = health
    _health_color = health_color
    _condition = condition
    _leader = leader
    _hostile = hostile
    tooltip_text = "\n".join(identity + PackedStringArray(["".join(health)]) + condition)
    queue_redraw()


func set_highlight(active: bool) -> void:
    theme_type_variation = &"SelectedMemberRow" if active else &"ButtonText16"


# The name's color: the original's yellow for a member who will fight for the
# monsters, gold for the leader, else the text's.
func name_color() -> Color:
    if _hostile:
        return get_theme_color("hostile", "OpenGoldPalette")
    if _leader:
        return get_theme_color("gold", "OpenGoldPalette")
    return get_theme_color("font_color")


# Where the lines start, past the portrait.
func text_left() -> float:
    return get_theme_stylebox("normal").get_margin(SIDE_LEFT) + PORTRAIT_SIZE.x + PORTRAIT_GAP


# How wide the identity lines are, with the level-up arrow beside the name
# when it shows.
func identity_width() -> float:
    var font := get_theme_font("font")
    var font_size := get_theme_font_size("font_size")
    var widest := 0.0
    for row in _identity.size():
        widest = maxf(widest,
            font.get_string_size(_identity[row], HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x)
    var arrow := get_node_or_null("Advance") as Control
    if arrow and arrow.visible:
        widest = maxf(widest, arrow.position.x + arrow.size.x - text_left())
    return widest


# The second column lines up across the rows: it starts past the widest
# identity of any member shown.
func _second_column() -> float:
    var widest := identity_width()
    for row in get_parent().get_children():
        if row != self and row.has_method("identity_width") and row.visible:
            widest = maxf(widest, row.identity_width())
    return text_left() + widest + COLUMN_GAP


func _gui_input(event: InputEvent) -> void:
    var click := event as InputEventMouseButton
    if click and click.double_click and click.pressed and click.button_index == MOUSE_BUTTON_LEFT:
        opened.emit()


func _draw() -> void:
    var style := get_theme_stylebox("normal")
    var top := style.get_margin(SIDE_TOP)
    if _portrait:
        draw_texture_rect(_portrait, Rect2(Vector2(style.get_margin(SIDE_LEFT),
            (size.y - PORTRAIT_SIZE.y) / 2), PORTRAIT_SIZE), false)
    var font := get_theme_font("font")
    var font_size := get_theme_font_size("font_size")
    var line := font.get_height(font_size)
    var color := get_theme_color("font_color")
    var baseline := top + font.get_ascent(font_size)
    var right := size.x - style.get_margin(SIDE_RIGHT)
    var left := text_left()
    for row in _identity.size():
        var shade := name_color() if row == 0 else color
        draw_string(font, Vector2(left, baseline + row * line),
            _identity[row], HORIZONTAL_ALIGNMENT_LEFT, right - left, font_size, shade)
    if _condition.is_empty():
        _draw_health(Vector2(left, baseline + _identity.size() * line), right)
        return
    var second := _second_column()
    _draw_health(Vector2(second, baseline), right)
    for row in _condition.size():
        draw_string(font, Vector2(second, baseline + (row + 1) * line),
            _condition[row], HORIZONTAL_ALIGNMENT_LEFT, right - second, font_size, color)


# Draws the Armor Class and hit points line from `at`, clipped at `right`.
func _draw_health(at: Vector2, right: float) -> void:
    var font := get_theme_font("font")
    var font_size := get_theme_font_size("font_size")
    var color := get_theme_color("font_color")
    for part in _health.size():
        var width := right - at.x
        if width <= 0:
            return
        var shade := (get_theme_color(_health_color, "OpenGoldPalette")
            if part == 1 and not _health_color.is_empty() else color)
        draw_string(font, at, _health[part], HORIZONTAL_ALIGNMENT_LEFT, width, font_size, shade)
        at.x += font.get_string_size(_health[part], HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
