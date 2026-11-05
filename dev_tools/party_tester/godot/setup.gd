extends Control

# The Party tester's screen: a party from the game's Character Pool, levelled
# with the game's level-up dialog, then fights on an area's battlefield on the
# game's combat screen. The bridge (PartyTestBridge) runs the game's code.

const LARGEST_PARTY := 8

@onready var bridge: Node = $Bridge
var catalog: Dictionary = {}
var groups: Array = []
var setup_panel: Control
var pool_list: ItemList
var party_list: ItemList
var level_entry: SpinBox
var area_picker: OptionButton
var seed_entry: SpinBox
var difficulty_picker: OptionButton
var creature_picker: OptionButton
var count_entry: SpinBox
var group_list: ItemList
var status_label: Label
var sheet_window: Window
var sheet_text: RichTextLabel
var return_layer: CanvasLayer

func _ready() -> void:
    catalog = bridge.catalog()
    if catalog.has("error"):
        _fatal(str(catalog.error))
        return
    bridge.member_advanced.connect(_member_advanced)
    bridge.failed.connect(_show_status)
    _build_screen()

func _fatal(message: String) -> void:
    var label := Label.new()
    label.text = message
    label.add_theme_font_size_override("font_size", 24)
    label.position = Vector2(32, 32)
    add_child(label)

func _label(text: String, size: int = 17) -> Label:
    var control := Label.new()
    control.text = text
    control.add_theme_font_size_override("font_size", size)
    return control

func _button(text: String, action: Callable) -> Button:
    var control := Button.new()
    control.text = text
    control.pressed.connect(action)
    return control

func _spin(low: int, high: int, value: int) -> SpinBox:
    var control := SpinBox.new()
    control.min_value = low
    control.max_value = high
    control.value = value
    return control

func _row(controls: Array) -> HBoxContainer:
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 8)
    for control in controls:
        row.add_child(control)
    return row

func _build_screen() -> void:
    setup_panel = MarginContainer.new()
    setup_panel.set_anchors_preset(Control.PRESET_FULL_RECT)
    for side in ["left", "right", "top", "bottom"]:
        setup_panel.add_theme_constant_override("margin_" + side, 18)
    add_child(setup_panel)
    var page := VBoxContainer.new()
    setup_panel.add_child(page)
    page.add_child(_label("Party tester", 26))
    var columns := HBoxContainer.new()
    columns.size_flags_vertical = Control.SIZE_EXPAND_FILL
    columns.add_theme_constant_override("separation", 24)
    page.add_child(columns)
    columns.add_child(_party_column())
    columns.add_child(_fight_column())
    var note := "Choose up to eight pool characters and press Build party."
    if not catalog.left_out.is_empty():
        note += " Not listed, as their scripts give their icon only while running: " + \
            ", ".join(catalog.left_out) + "."
    status_label = _label(note)
    status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    page.add_child(status_label)
    _build_sheet_window()

func _party_column() -> VBoxContainer:
    var column := VBoxContainer.new()
    column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    column.add_child(_label("Character Pool (up to eight)", 20))
    pool_list = ItemList.new()
    pool_list.name = "Pool"
    pool_list.select_mode = ItemList.SELECT_MULTI
    pool_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
    for entry in catalog.pool:
        pool_list.add_item(entry)
    column.add_child(pool_list)
    column.add_child(_button("Build party", _build_party))
    column.add_child(_label("Party", 20))
    party_list = ItemList.new()
    party_list.name = "Party"
    party_list.custom_minimum_size = Vector2(0, 236)
    party_list.item_activated.connect(func(_index: int) -> void: _show_sheet())
    column.add_child(party_list)
    level_entry = _spin(1, 15, 1)
    level_entry.name = "Level"
    column.add_child(_row([_label("Level"), level_entry, _button("Level up", _level_up),
                           _button("Use defaults for the rest", _use_defaults),
                           _button("Sheet", _show_sheet)]))
    return column

func _fight_column() -> VBoxContainer:
    var column := VBoxContainer.new()
    column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    column.add_child(_label("Fight", 20))
    area_picker = OptionButton.new()
    area_picker.name = "Area"
    for label in catalog.areas:
        area_picker.add_item(label)
    seed_entry = _spin(1, 999999, 1)
    seed_entry.name = "Seed"
    column.add_child(_row([_label("Area"), area_picker]))
    difficulty_picker = OptionButton.new()
    difficulty_picker.name = "Difficulty"
    for name in catalog.difficulties:
        difficulty_picker.add_item(name.capitalize())
        difficulty_picker.set_item_metadata(difficulty_picker.item_count - 1, name)
        if name == "normal":
            difficulty_picker.select(difficulty_picker.item_count - 1)
    column.add_child(_row([_label("Square seed"), seed_entry, _label("Difficulty"),
                           difficulty_picker]))
    column.add_child(_label("Monsters and NPCs", 20))
    creature_picker = OptionButton.new()
    creature_picker.name = "Creature"
    creature_picker.fit_to_longest_item = false
    creature_picker.custom_minimum_size = Vector2(420, 0)
    for label in catalog.creatures:
        creature_picker.add_item(label)
    count_entry = _spin(1, 56, 4)
    count_entry.name = "Count"
    column.add_child(_row([creature_picker, count_entry, _button("Add", _add_group)]))
    group_list = ItemList.new()
    group_list.name = "Groups"
    group_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
    column.add_child(group_list)
    column.add_child(_row([_button("Remove", _remove_group), _button("Fight", _start_fight)]))
    return column

func _build_sheet_window() -> void:
    sheet_window = Window.new()
    sheet_window.name = "Sheet"
    sheet_window.title = "Character sheet"
    sheet_window.size = Vector2i(720, 760)
    sheet_window.visible = false
    sheet_window.close_requested.connect(sheet_window.hide)
    sheet_text = RichTextLabel.new()
    sheet_text.set_script(load("res://scenes/themed_rich_text.gd"))
    sheet_text.set_anchors_preset(Control.PRESET_FULL_RECT)
    sheet_window.add_child(sheet_text)
    add_child(sheet_window)

func _show_status(message: String) -> void:
    status_label.text = message

func _refresh_party() -> void:
    party_list.clear()
    for member in bridge.members():
        var line := "%s - %s - HP %d/%d - XP %d" % [member.name, member.classes,
            member.hit_points, member.maximum_hit_points, member.experience]
        if member.can_advance:
            line += " - level to take"
        party_list.add_item(line)

func _build_party() -> void:
    var chosen := pool_list.get_selected_items()
    if chosen.size() > LARGEST_PARTY:
        _show_status("Choose at most eight pool characters.")
        return
    var error: String = bridge.build_party(Array(chosen))
    if not error.is_empty():
        _show_status(error)
        return
    level_entry.value = 1
    _refresh_party()
    _show_status("Party built at level 1 with each class's kit. Choose a level and press Level up.")

func _level_up() -> void:
    var error: String = bridge.set_level(int(level_entry.value))
    if not error.is_empty():
        _show_status(error)
        return
    _refresh_party()
    _open_next_level_up()

func _open_next_level_up() -> void:
    var member: String = bridge.open_level_up()
    if member.is_empty():
        _show_status("Every member is level %d." % int(level_entry.value))
    else:
        _show_status("Levelling %s with the game's level-up dialog." % member)

func _member_advanced() -> void:
    _refresh_party()
    _open_next_level_up()

func _use_defaults() -> void:
    var error: String = bridge.advance_with_defaults()
    if not error.is_empty():
        _show_status(error)
        return
    _refresh_party()
    _show_status("Every remaining level taken with the game's default choices.")

func _show_sheet() -> void:
    var selected := party_list.get_selected_items()
    if selected.is_empty():
        _show_status("Select a party member first.")
        return
    sheet_text.call("render", bridge.sheet(selected[0]))
    sheet_window.popup_centered()

func _add_group() -> void:
    if creature_picker.selected < 0:
        return
    groups.append({"creature": creature_picker.selected, "count": int(count_entry.value)})
    group_list.add_item("%d x %s" % [int(count_entry.value),
        creature_picker.get_item_text(creature_picker.selected)])

func _remove_group() -> void:
    var selected := group_list.get_selected_items()
    if selected.is_empty():
        return
    groups.remove_at(selected[0])
    group_list.remove_item(selected[0])

func _start_fight() -> void:
    var request := {
        "area": area_picker.selected,
        "seed": int(seed_entry.value),
        "difficulty": difficulty_picker.get_item_metadata(difficulty_picker.selected),
        "groups": groups,
    }
    var error: String = bridge.start_fight(request)
    if not error.is_empty():
        _show_status(error)
        return
    setup_panel.hide()

func _process(_delta: float) -> void:
    if setup_panel == null or setup_panel.visible or return_layer != null:
        return
    var result: String = bridge.outcome()
    if result.is_empty():
        return
    return_layer = CanvasLayer.new()
    return_layer.layer = 5
    add_child(return_layer)
    var button := _button(result + " - Return to setup", _return_to_setup)
    button.name = "Return"
    button.position = Vector2(18, 18)
    button.custom_minimum_size = Vector2(260, 48)
    return_layer.add_child(button)

func _return_to_setup() -> void:
    bridge.leave()
    return_layer.queue_free()
    return_layer = null
    setup_panel.show()
    _show_status("Fight finished. The party is as it was before it.")
