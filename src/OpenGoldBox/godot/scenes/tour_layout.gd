extends MarginContainer

# The exploration screen's layout. Its containers place every control; this
# script adds what changes in a shop or at a treasure pile: the dialogue box
# grows for the list of goods, and the party's cards, which then hold one
# column of lines, stand two to a row when the list is wide enough.

var _shopping := false
# The member card the party's list keeps in view: the selected member's.
var _shown_card: Control


func _ready() -> void:
    var party: Control = get_node("%PartyList")
    party.resized.connect(_fit_party_columns)
    party.resized.connect(_scroll_to_shown_card)


func apply_layout(shopping: bool) -> void:
    _shopping = shopping
    get_node("%DialogueBounds").call("fit_shop", shopping)
    _fit_party_columns()


# Scrolls the party's list to a member's card, and back to it whenever the
# list is resized. It waits a frame each time: the containers place the cards
# at the end of the frame the list changed in.
func show_party_member(card: Control) -> void:
    _shown_card = card
    _scroll_to_shown_card()


func _scroll_to_shown_card() -> void:
    await get_tree().process_frame
    if is_instance_valid(_shown_card) and _shown_card.visible:
        (get_node("%PartyList") as ScrollContainer).ensure_control_visible(_shown_card)


func _fit_party_columns() -> void:
    var party: Control = get_node("%PartyList")
    var two_fit := party.size.x >= get_theme_constant("tour_party_two_column_width", "OpenGoldMetrics")
    (party.get_node("Rows") as GridContainer).columns = 2 if _shopping and two_fit else 1
