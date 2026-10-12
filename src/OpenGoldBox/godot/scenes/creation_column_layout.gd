extends Node

var _authored_column_width := -1


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
