extends Control

const FORMAT := "opengold-combat-tester-parties-v1"
const ABILITY_NAMES := ["Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma"]

@onready var bridge: Node = $Bridge
var catalog: Dictionary = {}
var saved: Dictionary = {}
var session_parties: Array = []
var preset_index := 0
var member_index := 0
var config_path := ""
var setup_panel: Control
var preset_picker: OptionButton
var map_picker: OptionButton
var monster_picker: OptionButton
var monster_count: SpinBox
var leader_count: SpinBox
var seed_entry: SpinBox
var quick_magic: CheckBox
var cards: Array[Button] = []
var name_entry: LineEdit
var class_picker: OptionButton
var level_entry: SpinBox
var species_picker: OptionButton
var subclass_label: Label
var ability_entries: Array[SpinBox] = []
var weapon_list: ItemList
var armor_picker: OptionButton
var shield_picker: OptionButton
var gear_list: ItemList
var spell_list: ItemList
var cantrip_list: ItemList
var starting_group: OptionButton
var starting_list: ItemList
var starting_options: Array = []
var quick_check: CheckBox
var status_label: Label
var return_layer: CanvasLayer
var loading_member := false
var abilities_modified := false

func _ready() -> void:
    catalog = bridge.catalog()
    if catalog.has("error"):
        _fatal(str(catalog.error))
        return
    config_path = OS.get_environment("OPENGOLD_COMBAT_TEST_CONFIG")
    if config_path.is_empty():
        _fatal("OPENGOLD_COMBAT_TEST_CONFIG is not set")
        return
    var file := FileAccess.open(config_path, FileAccess.READ)
    if file == null:
        _fatal("Cannot read party configuration: " + config_path)
        return
    var parsed: Variant = JSON.parse_string(file.get_as_text())
    if not parsed is Dictionary or parsed.get("format") != FORMAT or parsed.get("parties", []).size() != 15:
        _fatal("Invalid combat tester party configuration")
        return
    saved = parsed
    session_parties = saved.parties.duplicate(true)
    _build_screen()
    _show_preset(0)

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
    control.custom_minimum_size = Vector2(125, 38)
    control.pressed.connect(action)
    return control

func _spin(low: float, high: float, value: float) -> SpinBox:
    var control := SpinBox.new()
    control.min_value = low
    control.max_value = high
    control.value = value
    control.custom_minimum_size.x = 90
    return control

func _picker(values: Array, selected: String = "") -> OptionButton:
    var control := OptionButton.new()
    control.custom_minimum_size.x = 180
    for value in values:
        control.add_item(str(value))
    _select_text(control, selected)
    return control

func _select_text(control: OptionButton, selected: String) -> void:
    for index in control.item_count:
        if control.get_item_text(index) == selected:
            control.select(index)
            return
    if control.item_count > 0:
        control.select(0)

func _row() -> HBoxContainer:
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 12)
    return row

func _build_screen() -> void:
    setup_panel = ScrollContainer.new()
    setup_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    setup_panel.offset_left = 24
    setup_panel.offset_top = 24
    setup_panel.offset_right = -24
    setup_panel.offset_bottom = -24
    add_child(setup_panel)
    var body := VBoxContainer.new()
    body.custom_minimum_size.x = 1200
    body.add_theme_constant_override("separation", 15)
    setup_panel.add_child(body)
    body.add_child(_label("Combat Tester", 32))
    body.add_child(_label("Choose a map and encounter, then edit any party member before starting."))
    var encounter := _row()
    body.add_child(encounter)
    encounter.add_child(_label("Map"))
    map_picker = _picker(catalog.maps)
    encounter.add_child(map_picker)
    encounter.add_child(_label("Seed"))
    seed_entry = _spin(0, 2147483647, 1)
    seed_entry.custom_minimum_size.x = 170
    encounter.add_child(seed_entry)
    encounter.add_child(_button("New seed", func(): seed_entry.value = randi_range(1, 2147483647)))
    encounter.add_child(_button("Copy seed", func(): DisplayServer.clipboard_set(str(int(seed_entry.value)))))
    var foes := _row()
    body.add_child(foes)
    foes.add_child(_label("Monster"))
    monster_picker = OptionButton.new()
    monster_picker.custom_minimum_size.x = 190
    for option in catalog.monsters:
        monster_picker.add_item(str(option.label))
        monster_picker.set_item_metadata(monster_picker.item_count - 1, option.id)
    for index in monster_picker.item_count:
        if monster_picker.get_item_metadata(index) == "orc": monster_picker.select(index)
    foes.add_child(monster_picker)
    foes.add_child(_label("Count"))
    monster_count = _spin(1, 40, 6)
    foes.add_child(monster_count)
    foes.add_child(_label("Leaders"))
    leader_count = _spin(0, 40, 0)
    foes.add_child(leader_count)
    monster_picker.item_selected.connect(_update_leader_control)
    monster_count.value_changed.connect(func(_v): _update_leader_control(monster_picker.selected))
    _update_leader_control(monster_picker.selected)
    var party_row := _row()
    body.add_child(party_row)
    party_row.add_child(_label("Party preset"))
    preset_picker = OptionButton.new()
    for level in range(1, 16): preset_picker.add_item("Level %d" % level)
    preset_picker.item_selected.connect(_show_preset)
    party_row.add_child(preset_picker)
    party_row.add_child(_button("Restore preset", _restore_preset))
    party_row.add_child(_button("Save party", _save_party))
    party_row.add_child(_button("Whole party Quick", _party_quick))
    quick_magic = CheckBox.new()
    quick_magic.text = "Quick magic"
    party_row.add_child(quick_magic)
    var grid := GridContainer.new()
    grid.columns = 3
    grid.add_theme_constant_override("h_separation", 14)
    grid.add_theme_constant_override("v_separation", 9)
    body.add_child(grid)
    for index in 6:
        var card := _button("Member %d" % (index + 1), _show_member.bind(index))
        card.toggle_mode = true
        card.custom_minimum_size = Vector2(355, 64)
        grid.add_child(card)
        cards.append(card)
    body.add_child(_label("Selected member", 22))
    var edit := GridContainer.new()
    edit.columns = 4
    edit.add_theme_constant_override("h_separation", 18)
    edit.add_theme_constant_override("v_separation", 10)
    body.add_child(edit)
    edit.add_child(_label("Name"))
    name_entry = LineEdit.new()
    name_entry.placeholder_text = "Game default"
    name_entry.custom_minimum_size.x = 185
    name_entry.text_changed.connect(func(_v): _store_member())
    edit.add_child(name_entry)
    edit.add_child(_label("Class"))
    class_picker = _picker(catalog.classes)
    class_picker.item_selected.connect(func(_v): _class_or_level_changed())
    edit.add_child(class_picker)
    edit.add_child(_label("Level"))
    level_entry = _spin(1, 15, 1)
    level_entry.value_changed.connect(func(_v): _class_or_level_changed())
    edit.add_child(level_entry)
    edit.add_child(_label("Species"))
    species_picker = _picker(["Game default"] + catalog.species)
    species_picker.item_selected.connect(func(_v): _store_member())
    edit.add_child(species_picker)
    edit.add_child(_label("Subclass"))
    subclass_label = _label("Game supported")
    edit.add_child(subclass_label)
    edit.add_child(_label(""))
    edit.add_child(_label(""))
    for name in ABILITY_NAMES:
        edit.add_child(_label(name + " base"))
        var ability := _spin(3, 18, 13)
        ability.value_changed.connect(func(_v): _ability_changed())
        edit.add_child(ability)
        ability_entries.append(ability)
    body.add_child(_label("Equipment and spells", 22))
    var equipment := _row()
    body.add_child(equipment)
    var weapon_column := VBoxContainer.new()
    weapon_column.add_child(_label("Weapons (select multiple)"))
    weapon_list = ItemList.new()
    weapon_list.select_mode = ItemList.SELECT_MULTI
    weapon_list.custom_minimum_size = Vector2(270, 200)
    weapon_list.add_item("Unarmed (remove kit)")
    weapon_list.set_item_metadata(0, 0)
    for option in catalog.weapons:
        weapon_list.add_item(str(option.label))
        weapon_list.set_item_metadata(weapon_list.item_count - 1, int(option.type))
    weapon_list.multi_selected.connect(func(_i, _s): _store_equipment("weapons"))
    weapon_column.add_child(weapon_list)
    equipment.add_child(weapon_column)
    var armor_column := VBoxContainer.new()
    armor_column.add_child(_label("Armor"))
    armor_picker = _picker(["Game default", "None", "Leather armor", "Chain mail"])
    armor_picker.item_selected.connect(func(_i): _store_equipment("armor"))
    armor_column.add_child(armor_picker)
    armor_column.add_child(_label("Shield"))
    shield_picker = _picker(["Game default", "Equipped", "None"])
    shield_picker.item_selected.connect(func(_i): _store_equipment("shield"))
    armor_column.add_child(shield_picker)
    equipment.add_child(armor_column)
    var gear_column := VBoxContainer.new()
    gear_column.add_child(_label("Other gear"))
    gear_list = ItemList.new()
    gear_list.select_mode = ItemList.SELECT_MULTI
    gear_list.custom_minimum_size = Vector2(200, 200)
    for option in catalog.gear:
        gear_list.add_item(str(option.label))
        gear_list.set_item_metadata(gear_list.item_count - 1, int(option.type))
    gear_list.multi_selected.connect(func(_i, _s): _store_equipment("gear"))
    gear_column.add_child(gear_list)
    equipment.add_child(gear_column)
    var spell_column := VBoxContainer.new()
    spell_column.add_child(_label("Prepared spells"))
    spell_list = ItemList.new()
    spell_list.select_mode = ItemList.SELECT_MULTI
    spell_list.custom_minimum_size = Vector2(270, 200)
    spell_list.multi_selected.connect(func(_i, _s): _store_equipment("spells"))
    spell_column.add_child(spell_list)
    equipment.add_child(spell_column)
    var starting_row := _row()
    body.add_child(starting_row)
    var cantrip_column := VBoxContainer.new()
    cantrip_column.add_child(_label("Starting cantrips (select required count)"))
    cantrip_list = ItemList.new()
    cantrip_list.select_mode = ItemList.SELECT_MULTI
    cantrip_list.custom_minimum_size = Vector2(270, 150)
    cantrip_list.multi_selected.connect(func(_i, _s): _store_equipment("cantrips"))
    cantrip_column.add_child(cantrip_list)
    starting_row.add_child(cantrip_column)
    var learned_column := VBoxContainer.new()
    learned_column.add_child(_label("Starting learned spells"))
    starting_group = OptionButton.new()
    starting_group.item_selected.connect(func(_i): _show_starting_group())
    learned_column.add_child(starting_group)
    starting_list = ItemList.new()
    starting_list.select_mode = ItemList.SELECT_MULTI
    starting_list.custom_minimum_size = Vector2(270, 150)
    starting_list.multi_selected.connect(func(_i, _s): _store_equipment("starting"))
    learned_column.add_child(starting_list)
    starting_row.add_child(learned_column)
    quick_check = CheckBox.new()
    quick_check.text = "This character uses Quick AI"
    quick_check.toggled.connect(func(_v): _store_member())
    body.add_child(quick_check)
    var action := _row()
    body.add_child(action)
    action.add_child(_button("Apply member edits", _store_member))
    action.add_child(_button("Restore class kit", _restore_kit))
    action.add_child(_button("Start combat", _start_combat))
    status_label = _label("")
    action.add_child(status_label)

func _update_leader_control(_selected: int) -> void:
    var option: Dictionary = catalog.monsters[monster_picker.selected]
    leader_count.editable = bool(option.leaders)
    leader_count.max_value = monster_count.value
    if not option.leaders: leader_count.value = 0

func _show_preset(index: int) -> void:
    preset_index = index
    preset_picker.select(index)
    _show_member(0)
    _refresh_cards()

func _member() -> Dictionary:
    return session_parties[preset_index].members[member_index]

func _show_member(index: int) -> void:
    loading_member = true
    member_index = index
    var member := _member()
    var defaults: Dictionary = catalog.class_defaults.get(str(member["class"]), {})
    name_entry.text = str(member.get("name", ""))
    name_entry.placeholder_text = "Game default: " + str(defaults.get("name", ""))
    _select_text(class_picker, str(member["class"]))
    level_entry.value = int(member["level"])
    species_picker.set_item_text(0, "Game default: " + str(defaults.get("species", "")))
    if str(member.get("species", "")).is_empty():
        species_picker.select(0)
    else:
        _select_text(species_picker, str(member.species))
    subclass_label.text = str(defaults.get("subclass", "Game default"))
    var scores: Array = member.get("abilities", [])
    if scores.size() != 6: scores = defaults.get("abilities", [13, 13, 13, 13, 13, 13])
    for n in 6: ability_entries[n].value = int(scores[n])
    abilities_modified = false
    quick_check.button_pressed = bool(member.get("quick", false))
    weapon_list.deselect_all()
    if member.get("weapon_types") is Array:
        for type in member.weapon_types:
            for item in weapon_list.item_count:
                if weapon_list.get_item_metadata(item) == type: weapon_list.select(item, false)
    armor_picker.select(max(0, [-1, 0, 50, 55].find(int(member.get("armor_type", -1)))))
    var shield: Variant = member.get("shield")
    shield_picker.select(0 if shield == null else (1 if shield else 2))
    gear_list.deselect_all()
    if member.get("gear_types") is Array:
        for type in member.gear_types:
            for item in gear_list.item_count:
                if gear_list.get_item_metadata(item) == type: gear_list.select(item, false)
    _populate_spells(member)
    status_label.text = "Editing member %d. Changes apply to the next fight until saved." % (index + 1)
    loading_member = false

func _store_member() -> void:
    if session_parties.is_empty() or loading_member: return
    var member := _member()
    member.name = name_entry.text.strip_edges()
    member["class"] = class_picker.get_item_text(class_picker.selected)
    member.level = int(level_entry.value)
    member.species = "" if species_picker.selected == 0 else species_picker.get_item_text(species_picker.selected)
    if abilities_modified:
        var scores: Array = []
        for entry in ability_entries: scores.append(int(entry.value))
        member.abilities = scores
    member.quick = quick_check.button_pressed
    _refresh_cards()

func _ability_changed() -> void:
    if loading_member: return
    abilities_modified = true
    _store_member()

func _class_or_level_changed() -> void:
    if loading_member: return
    _store_member()
    loading_member = true
    var defaults: Dictionary = catalog.class_defaults.get(str(_member()["class"]), {})
    if not abilities_modified and _member().get("abilities", []).is_empty():
        var scores: Array = defaults.get("abilities", [13, 13, 13, 13, 13, 13])
        for n in 6: ability_entries[n].value = int(scores[n])
    if str(_member().get("species", "")).is_empty():
        species_picker.set_item_text(0, "Game default: " + str(defaults.get("species", "")))
    subclass_label.text = str(defaults.get("subclass", "Game default"))
    _populate_spells(_member())
    loading_member = false

func _store_equipment(kind: String) -> void:
    if loading_member or session_parties.is_empty(): return
    var member := _member()
    match kind:
        "weapons":
            var weapons: Array = []
            for index in weapon_list.get_selected_items(): weapons.append(int(weapon_list.get_item_metadata(index)))
            if weapons.size() > 1 and weapons.has(0):
                weapons.erase(0)
                weapon_list.deselect(0)
            member.weapon_types = weapons
        "armor": member.armor_type = [-1, 0, 50, 55][armor_picker.selected]
        "shield": member.shield = null if shield_picker.selected == 0 else shield_picker.selected == 1
        "gear":
            var gear: Array = []
            for index in gear_list.get_selected_items(): gear.append(int(gear_list.get_item_metadata(index)))
            member.gear_types = gear
        "spells":
            var spells: Array = []
            for index in spell_list.get_selected_items(): spells.append(str(spell_list.get_item_metadata(index)))
            member.prepared_spells = spells
        "cantrips":
            var spells: Array = []
            for index in cantrip_list.get_selected_items(): spells.append(cantrip_list.get_item_text(index))
            member.cantrips = spells
        "starting":
            if starting_group.selected < 0 or starting_group.selected >= starting_options.size(): return
            var group: Dictionary = starting_options[starting_group.selected]
            var selected: Dictionary = member.get("starting_spells") if member.get("starting_spells") is Dictionary else {}
            var spells: Array = []
            for index in starting_list.get_selected_items(): spells.append(starting_list.get_item_text(index))
            selected[group.id] = spells
            member.starting_spells = selected

func _restore_kit() -> void:
    var member := _member()
    member.weapon_types = null
    member.armor_type = -1
    member.shield = null
    member.gear_types = null
    member.prepared_spells = null
    member.cantrips = null
    member.starting_spells = null
    _show_member(member_index)
    status_label.text = "Restored this class's game kit and spell choices."

func _populate_spells(member: Dictionary) -> void:
    spell_list.clear()
    var options: Dictionary = bridge.spell_options(member)
    for spell in options.get("prepared", []):
        spell_list.add_item(str(spell))
        spell_list.set_item_metadata(spell_list.item_count - 1, str(spell))
        if member.get("prepared_spells") is Array and spell in member.prepared_spells:
            spell_list.select(spell_list.item_count - 1, false)
    cantrip_list.clear()
    for spell in options.get("cantrips", []):
        cantrip_list.add_item(str(spell))
        if member.get("cantrips") is Array and spell in member.cantrips:
            cantrip_list.select(cantrip_list.item_count - 1, false)
    starting_options = options.get("starting_groups", [])
    starting_group.clear()
    for group in starting_options:
        starting_group.add_item("%s (%d picks)" % [group.label, group.count])
    _show_starting_group()

func _show_starting_group() -> void:
    starting_list.clear()
    if starting_group.selected < 0 or starting_group.selected >= starting_options.size(): return
    var group: Dictionary = starting_options[starting_group.selected]
    var selected: Dictionary = _member().get("starting_spells") if _member().get("starting_spells") is Dictionary else {}
    for spell in group.options:
        starting_list.add_item(str(spell))
        if spell in selected.get(group.id, []):
            starting_list.select(starting_list.item_count - 1, false)

func _refresh_cards() -> void:
    if session_parties.is_empty(): return
    for index in 6:
        var member: Dictionary = session_parties[preset_index].members[index]
        cards[index].text = "%d. %s · Level %d%s" % [index + 1, member["class"], member.level,
            " · Quick" if member.get("quick", false) else ""]
        cards[index].button_pressed = index == member_index

func _party_quick() -> void:
    _store_member()
    for member in session_parties[preset_index].members: member.quick = true
    quick_check.button_pressed = true
    _refresh_cards()

func _restore_preset() -> void:
    session_parties[preset_index] = saved.parties[preset_index].duplicate(true)
    _show_preset(preset_index)
    status_label.text = "Restored the saved preset."

func _save_party() -> void:
    _store_member()
    saved.parties[preset_index] = session_parties[preset_index].duplicate(true)
    var file := FileAccess.open(config_path, FileAccess.WRITE)
    if file == null:
        status_label.text = "Could not write party configuration."
        return
    file.store_string(JSON.stringify(saved, "  ") + "\n")
    status_label.text = "Saved the level %d party." % (preset_index + 1)

func _start_combat() -> void:
    _store_member()
    var request := {
        "members": session_parties[preset_index].members,
        "map": map_picker.get_item_text(map_picker.selected),
        "monster": monster_picker.get_item_metadata(monster_picker.selected),
        "count": int(monster_count.value),
        "leaders": int(leader_count.value),
        "seed": int(seed_entry.value),
        "quick_magic": quick_magic.button_pressed,
    }
    var error: String = bridge.start(request)
    if not error.is_empty():
        status_label.text = error
        return
    setup_panel.hide()
    status_label.text = ""

func _process(_delta: float) -> void:
    if setup_panel == null or setup_panel.visible or return_layer != null: return
    var result: String = bridge.outcome()
    if result.is_empty(): return
    return_layer = CanvasLayer.new()
    return_layer.layer = 5
    add_child(return_layer)
    var button := _button(result + " · Return to setup", _return_to_setup)
    button.position = Vector2(18, 18)
    button.custom_minimum_size = Vector2(260, 48)
    return_layer.add_child(button)

func _return_to_setup() -> void:
    bridge.leave()
    return_layer.queue_free()
    return_layer = null
    setup_panel.show()
    status_label.text = "Fight finished. Configuration retained."
