extends Node

@export var metric_prefix: String


func _ready() -> void:
    var owner_control := get_parent() as Control
    var window := owner_control.get_window()
    window.min_size = Vector2i(
        owner_control.get_theme_constant(metric_prefix + "_min_width", "OpenGoldMetrics"),
        owner_control.get_theme_constant(metric_prefix + "_min_height", "OpenGoldMetrics"))
