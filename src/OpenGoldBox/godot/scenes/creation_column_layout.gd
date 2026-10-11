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
    var preview: Control = view.get_node("PreviewSummary")
    preview.visible = page.size.y >= view.get_theme_constant(
        "preview_summary_min_height", "OpenGoldMetrics")
