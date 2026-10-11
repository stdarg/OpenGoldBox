extends Node

@export_enum("sheet", "creation_modal", "tour_sheet") var layout_kind := "sheet"

var _initial_size := Vector2i.ZERO


func fit(view_size: Vector2i) -> void:
    var window := get_parent() as Window
    if _initial_size == Vector2i.ZERO:
        _initial_size = window.size
    var width: float
    var height: float
    match layout_kind:
        "sheet":
            width = minf(window.get_theme_constant("sheet_max_width", "OpenGoldMetrics"),
                view_size.x - window.get_theme_constant("sheet_horizontal_margin", "OpenGoldMetrics"))
            height = view_size.y - window.get_theme_constant("sheet_vertical_margin", "OpenGoldMetrics")
        "creation_modal":
            width = minf(window.get_theme_constant("creation_modal_max_width", "OpenGoldMetrics"),
                view_size.x - window.get_theme_constant("creation_modal_horizontal_margin", "OpenGoldMetrics"))
            height = view_size.y - window.get_theme_constant("creation_modal_vertical_margin", "OpenGoldMetrics")
        "tour_sheet":
            var margin := window.get_theme_constant("tour_sheet_margin", "OpenGoldMetrics")
            width = view_size.x - margin
            height = view_size.y - margin
    var reference: Vector2i = window.get_meta("layout_reference_size", _initial_size)
    window.size = _initial_size + Vector2i(width, height) - reference
