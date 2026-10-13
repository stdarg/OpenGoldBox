extends RichTextLabel


func render(source: String) -> void:
    var result := source
    for name in ["sheet_title", "sheet_note"]:
        var metric: String = name + "_font_size"
        result = result.replace("[theme_font=" + name + "]",
            "[font_size=" + str(get_theme_constant(metric, "OpenGoldMetrics")) + "]")
    result = result.replace("[/theme_font]", "[/font_size]")
    for name in ["score_positive", "score_negative", "hp_full"]:
        result = result.replace("[theme_color=" + name + "]",
            "[color=#" + get_theme_color(name, "OpenGoldPalette").to_html(false) + "]")
    result = result.replace("[/theme_color]", "[/color]")
    result = result.replace("[theme_table=abilities]", "[table=3]")
    result = result.replace("[theme_header]", "[cell][b]")
    result = result.replace("[/theme_header]", " ".repeat(
        get_theme_constant("sheet_header_pad_chars", "OpenGoldMetrics")) + "[/b][/cell]")
    result = result.replace("[theme_header_last]", "[cell][b]")
    result = result.replace("[/theme_header_last]", "[/b][/cell]")
    result = result.replace("[/theme_table]", "[/table]")
    # Section titles match the titles on the Training page.
    result = result.replace("[theme_title]",
        "[font_size=" + str(get_theme_font_size("font_size", "TrainingTitle")) + "]")
    result = result.replace("[/theme_title]", "[/font_size]")
    result = result.replace("[theme_emphasis]", "[b]")
    result = result.replace("[/theme_emphasis]", "[/b]")
    text = result
