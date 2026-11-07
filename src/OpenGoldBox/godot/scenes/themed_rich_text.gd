extends RichTextLabel

# The portrait, beside and below text of the last render_with_portrait(), laid
# out again when the label's width changes; empty after a plain render().
var portrait_sections: Array = []


func _ready() -> void:
    resized.connect(_on_resized)


func render(source: String) -> void:
    portrait_sections = []
    text = themed(source)


# The portrait at the top right with `beside` in a column to its left and
# `below` under both, scrolling together like the rest of the text.
func render_with_portrait(portrait: Texture2D, beside: String, below: String) -> void:
    portrait_sections = [portrait, beside, below]
    lay_out_portrait()


func lay_out_portrait() -> void:
    var portrait: Texture2D = portrait_sections[0]
    var side := get_theme_constant("sheet_portrait_size", "OpenGoldMetrics")
    var gap := get_theme_constant("sheet_portrait_gap", "OpenGoldMetrics")
    text = ""
    push_table(2)
    push_cell()
    append_text(themed(portrait_sections[1]))
    # A table is only as wide as its widest line, so an invisible line as wide
    # as the text column may be holds the portrait against the right edge. The
    # scroll bar's width is always left free: a line wider than the label
    # breaks the table.
    var column := size.x - get_theme_stylebox("normal").get_minimum_size().x \
        - get_v_scroll_bar().get_combined_minimum_size().x - side - gap
    if column >= 1:
        newline()
        add_image(column_spacer(), int(column), 1)
    pop()
    push_cell()
    set_cell_padding(Rect2(gap, 0, 0, 0))
    add_image(portrait, side, side)
    pop()
    pop()
    append_text(themed(portrait_sections[2]))


func _on_resized() -> void:
    if not portrait_sections.is_empty():
        lay_out_portrait()


static var spacer: Texture2D


# A transparent pixel, stretched to hold the text column open.
static func column_spacer() -> Texture2D:
    if spacer == null:
        spacer = ImageTexture.create_from_image(Image.create(1, 1, false, Image.FORMAT_RGBA8))
    return spacer


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
