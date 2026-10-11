extends TextureRect

var elapsed := 0.0
var fading := false


func begin() -> void:
    elapsed = 0.0
    fading = true
    _apply_tint(0.0)
    set_process(true)


func cancel() -> void:
    fading = false
    set_process(false)


func _ready() -> void:
    set_process(false)


func _process(delta: float) -> void:
    if not fading:
        return
    var duration := float(get_theme_constant("startup_fade_duration_ms", "OpenGoldMetrics")) / 1000.0
    elapsed = minf(duration, elapsed + maxf(0.0, delta))
    _apply_tint(elapsed / duration if duration > 0.0 else 1.0)
    if elapsed >= duration:
        cancel()


func _apply_tint(alpha: float) -> void:
    var tint := get_theme_color("startup_fade_tint", "OpenGoldPalette")
    tint.a = alpha
    self_modulate = tint
