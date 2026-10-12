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


# The portrait page: the filters follow the title on its row when they fit
# before the preview at the top right, and otherwise take the next line,
# their lists narrowed to fit. The character's identity takes the line after
# them, wrapping before the preview, and the contact sheet starts below it.
func _place_portrait_rows(view: Control, identity: Label) -> void:
    var lists := ["PortraitGender", "PortraitClass", "PortraitRace"]
    var label: Control = view.get_node("PortraitFilters")
    if _authored_portrait_rows.is_empty():
        _authored_portrait_rows["list_width"] = (view.get_node(lists[0]) as Control).size.x
        _authored_portrait_rows["row"] = label.size.y + 10
    var gap := 8.0
    var row: float = _authored_portrait_rows["row"]
    var title: Label = view.get_node("PageTitle")
    var title_end := title.position.x + title.get_theme_font("font").get_string_size(title.text,
        HORIZONTAL_ALIGNMENT_LEFT, -1, title.get_theme_font_size("font_size")).x
    var right: float = (view.get_node("PortraitPreview") as Control).position.x - 16
    var list_width: float = _authored_portrait_rows["list_width"]
    var filters_width := label.size.x + gap + 3 * list_width + 2 * gap
    var x := title_end + 32
    var y := title.position.y
    if x + filters_width > right:
        x = title.position.x
        y += row
        list_width = minf(list_width, (right - x - label.size.x - 3 * gap) / 3)
    label.position = Vector2(x, y)
    x += label.size.x + gap
    for name in lists:
        var list: Control = view.get_node(name)
        list.position = Vector2(x, y)
        list.size.x = list_width
        x += list_width + gap
    identity.position.y = y + row
    # Measured from the font: the label re-wraps only on its next frame.
    identity.size.y = identity.get_theme_font("font").get_multiline_string_size(identity.text,
        HORIZONTAL_ALIGNMENT_LEFT, identity.size.x, identity.get_theme_font_size("font_size")).y
    (view.get_node("PortraitScroll") as Control).offset_top = identity.position.y \
        + identity.size.y + 12
