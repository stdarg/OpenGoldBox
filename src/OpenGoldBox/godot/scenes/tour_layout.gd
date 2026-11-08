extends MarginContainer

# The exploration screen's layout. Its containers place every control; this
# script grows the dialogue box in a shop or at a treasure pile, for the list
# of goods, and lays out the party's cards, the same everywhere: two to a row
# when the list is wide enough, in four rows sized to fit the list's height,
# room for six characters and two NPCs.

# The member card the party's list keeps in view: the selected member's.
var _shown_card: Control


func _ready() -> void:
    var party: Control = get_node("%PartyList")
    party.resized.connect(_fit_party_columns)
    party.resized.connect(_scroll_to_shown_card)


func apply_layout(shopping: bool) -> void:
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


# Six characters and two NPCs, two to a row.
const PARTY_ROWS := 4
# A card's height when the list has room to spare.
const CARD_HEIGHT := 112.0


func _fit_party_columns() -> void:
    var party: Control = get_node("%PartyList")
    var rows: GridContainer = party.get_node("Rows")
    var two_fit := party.size.x >= get_theme_constant("tour_party_two_column_width", "OpenGoldMetrics")
    rows.columns = 2 if two_fit else 1
    var gap := rows.get_theme_constant("v_separation")
    var fitted := floorf((party.size.y - gap * (PARTY_ROWS - 1)) / PARTY_ROWS)
    for card in rows.get_children():
        var least: float = card.call("compact_height")
        (card as Control).custom_minimum_size.y = clampf(fitted, least, CARD_HEIGHT)
