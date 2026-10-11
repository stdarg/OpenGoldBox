extends Button


func configure_score(unmet: bool, change: int) -> void:
    theme_type_variation = "ScoreUnmet" if unmet else "ScoreNormal"
    var color := get_theme_color("score_positive" if change > 0 else
        "score_negative" if change < 0 else "score_neutral", "OpenGoldPalette")
    for state in ["font_color", "font_hover_color", "font_pressed_color", "font_focus_color"]:
        add_theme_color_override(state, color)
