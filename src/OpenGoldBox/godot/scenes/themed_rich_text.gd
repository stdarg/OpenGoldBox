extends RichTextLabel


func render(source: String) -> void:
    text = themed(source)


# The portrait at the top left with `beside` in a column to its right and
# `below` under both, scrolling together like the rest of the text.
func render_with_portrait(portrait: Texture2D, beside: String, below: String) -> void:
    var side := get_theme_constant("sheet_portrait_size", "OpenGoldMetrics")
    text = ""
    push_table(2)
    push_cell()
    set_cell_padding(Rect2(0, 0, get_theme_constant("sheet_portrait_gap", "OpenGoldMetrics"), 0))
    add_image(portrait, side, side)
    pop()
    push_cell()
    append_text(themed(beside))
    pop()
    pop()
    append_text(themed(below))


# The sheet's theme tags turned into BBCode with the theme's sizes and colors.
func themed(source: String) -> String:
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
    return result
