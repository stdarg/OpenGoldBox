extends MarginContainer

# The exploration screen's dialogue box: the speaker and text, a typed answer,
# then the list of choices and the buttons. Each kind of prompt arranges them
# as MODES says:
#
# - choices_beside_buttons: a menu's list runs down beside Continue, so a few
#   options show without scrolling; otherwise the buttons sit on a row below.
# - text_height: the text's height when a list is shown and fills the rest of
#   the box; 0 when the text fills the box itself.
const MODES := {
    "normal": {"choices_beside_buttons": false, "text_height": 0},
    "answer": {"choices_beside_buttons": false, "text_height": 0},
    "multiple": {"choices_beside_buttons": true, "text_height": 58},
    "shopping": {"choices_beside_buttons": false, "text_height": 36},
}

# The box's height while exploring, and in a shop or at a treasure pile, whose
# list of goods needs more room.
@export var height := 232.0
@export var shop_height := 312.0


# The bar that parts a shop's or a treasure pile's news from the header.
const NEWS_DIVIDER := "  |  "

# The news shown in the text's place in a shop or at a treasure pile.
var _news := ""


func _ready() -> void:
    (get_node("%Heading") as Control).resized.connect(_place_news)


func show_mode(mode: String) -> void:
    if mode != "shopping":
        _news = ""
        (get_node("%Heading") as BoxContainer).vertical = true
    var rules: Dictionary = MODES[mode]
    var text_fills: bool = rules["text_height"] == 0
    var fill := Control.SIZE_EXPAND_FILL
    for name in ["Heading", "Dialogue"]:
        (get_node("%" + name) as Control).size_flags_vertical = (
            fill if text_fills else Control.SIZE_FILL)
    for name in ["DialogueBottom", "Choices"]:
        (get_node("%" + name) as Control).size_flags_vertical = (
            Control.SIZE_FILL if text_fills else fill)
    (get_node("%Dialogue") as Control).custom_minimum_size.y = rules["text_height"]
    (get_node("%DialogueBottom") as BoxContainer).vertical = not rules["choices_beside_buttons"]


func fit_shop(shopping: bool) -> void:
    custom_minimum_size.y = shop_height if shopping else height


# A shop's or a treasure pile's news follows the header on its line, after a
# muted bar, when it fits there; otherwise it takes its own line under the
# header. A new window width can move it either way.
func show_news(news: String) -> void:
    _news = news
    _place_news()


func _place_news() -> void:
    if _news.is_empty():
        return
    var heading: BoxContainer = get_node("%Heading")
    var header: Label = get_node("%Speaker")
    var text: RichTextLabel = get_node("%Dialogue")
    var font := text.get_theme_font("normal_font")
    var font_size := text.get_theme_font_size("normal_font_size")
    var room := heading.size.x - header.get_minimum_size().x - heading.get_theme_constant("separation")
    var beside := font.get_string_size(NEWS_DIVIDER + _news, HORIZONTAL_ALIGNMENT_LEFT, -1,
        font_size).x <= room
    heading.vertical = not beside
    # Emptied first: set_text ignores the text the label already holds.
    text.text = ""
    if beside:
        text.push_color(text.get_theme_color("font_color", "LabelMuted"))
        text.add_text(NEWS_DIVIDER)
        text.pop()
    text.add_text(_news)
