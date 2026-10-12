extends Node

var _authored_column_width := -1
var _authored_portrait_rows := {}


func apply_layout() -> void:
    var view := get_parent() as Control
    var choices: ItemList = view.get_node("Choices")
    var page: Control = view.get_node("PageBounds")
    if _authored_column_width < 0:
        _authored_column_width = choices.fixed_column_width
    var design: Rect2 = page.get_meta("layout_reference")
    choices.fixed_column_width = _authored_column_width + int(
        (page.size.x - design.size.x) / maxi(1, choices.max_columns))
    # The race text starts beside the race portrait, which is square and as
    # tall as the page allows, so narrower in a short window. Elsewhere the
    # text starts at the page's left margin, like the race list.
    var description: Control = view.get_node("Description")
    if view.get_node("RaceSelection").visible:
        var portrait: Control = view.get_node("RacePortraitBounds")
        description.offset_left = portrait.position.x + minf(portrait.size.x, portrait.size.y) + 16
    else:
        description.offset_left = choices.offset_left
    # Reroll and its hint sit under the attributes table, their bottoms level
    # with the bottom of the hint under the desired classes, which is as tall
    # as its text in each language. Reroll's hint fills the space under the
    # table with its text on the bottom edge, however that text wraps.
    var target_hint: Control = view.get_node("TargetHint")
    if target_hint.visible:
        var bottom := target_hint.position.y + target_hint.size.y
        var roll: Control = view.get_node("Roll")
        roll.position.y = bottom - roll.size.y
        var last_row: Control = view.get_node("BonusScore5")
        var swap_hint: Control = view.get_node("SwapHint")
        swap_hint.position.y = last_row.position.y + last_row.size.y
        swap_hint.size.y = bottom - swap_hint.position.y
    var identity: Label = view.get_node("PortraitIdentity")
    if identity.visible:
        _place_portrait_rows(view, identity)


# The portrait filters sit on the title row, between the title and the
# character's identity at the top right. When they do not fit there, they
# start a line of their own at the left margin, and the rows below move down.
func _place_portrait_rows(view: Control, identity: Label) -> void:
    var filters := ["PortraitFilters", "PortraitGender", "PortraitClass", "PortraitRace"]
    var below := ["PortraitPrevious", "PortraitSelect", "PortraitNext", "PortraitScroll"]
    if _authored_portrait_rows.is_empty():
        for name in filters + below:
            var control: Control = view.get_node(name)
            _authored_portrait_rows[name] = Vector2(control.offset_left, control.offset_top)
    var text_width := identity.get_theme_font("font").get_string_size(identity.text,
        HORIZONTAL_ALIGNMENT_LEFT, -1, identity.get_theme_font_size("font_size")).x
    var race: Control = view.get_node("PortraitRace")
    var filters_end: float = _authored_portrait_rows["PortraitRace"].x + race.size.x
    var fits := filters_end + 24 <= identity.position.x + identity.size.x - text_width
    var line: float = 0.0 if fits else _authored_portrait_rows["PortraitSelect"].y \
        - _authored_portrait_rows["PortraitFilters"].y
    var indent: float = 0.0 if fits else _authored_portrait_rows["PortraitPrevious"].x \
        - _authored_portrait_rows["PortraitFilters"].x
    for name in filters:
        var control: Control = view.get_node(name)
        control.position = _authored_portrait_rows[name] + Vector2(indent, line)
    for name in below:
        var control: Control = view.get_node(name)
        if name == "PortraitScroll":
            control.offset_top = _authored_portrait_rows[name].y + line
        else:
            control.position.y = _authored_portrait_rows[name].y + line

