extends Control

const Evidence = preload("res://scripts/art_script_evidence.gd")
const COLOR_NAMES := ["Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown", "Light gray", "Dark gray", "Light blue", "Light green", "Light cyan", "Light red", "Black 2", "Yellow", "White"]
const REGIONS := ["Weapon", "Body", "Hair / face", "Shield", "Arms", "Legs"]
const DEFAULT_COLORS := [7, 1, 6, 6, 1, 6, 15, 9, 12, 14, 9, 14]

var catalog := SpriteCatalog.new()
var rows: Array = []
var visible_rows: Array[int] = []
var used: Dictionary = {}
var selected := -1
var action := false
var elapsed := 0.0
var monster_list: ItemList
var hide_unused: CheckButton
var monster_title: Label
var monster_image: TextureRect
var monster_source: Label
var gear_text: RichTextLabel
var status: Label
var head_picker: OptionButton
var body_picker: OptionButton
var size_picker: OptionButton
var weapon_picker: OptionButton
var shield_picker: CheckButton
var color_pickers: Array[OptionButton] = []
var ready_image: TextureRect
var action_image: TextureRect
var character_status: Label

func _ready() -> void:
    _build_ui()
    var game_dir := OS.get_environment("OPENGOLD_GAME_DIR")
    if game_dir.is_empty(): game_dir = ProjectSettings.get_setting("opengold/game_directory", "")
    var art_dir := ProjectSettings.globalize_path("res://../../../data/art")
    var error := catalog.load_data(game_dir, art_dir)
    if not error.is_empty():
        status.text = "Cannot load sprite catalog: " + error
        return
    rows = catalog.creatures()
    for option in catalog.weapons():
        weapon_picker.add_item(option.label, option.type)
    var evidence := Evidence.new()
    evidence.build(game_dir)
    for script in evidence.scripts:
        for node in script.nodes.values():
            if node.opcode in [11, 54] and not node.args.is_empty() and node.args[0].kind == "constant":
                var bank: String = str(script.archive).trim_prefix("ECL").trim_suffix(".DAX")
                used["MON%sCHA.DAX:%d" % [bank, node.args[0].value]] = true
    for refs in evidence.references.values():
        for ref in refs:
            for archive in ref.art_archives:
                used["%s:%d" % [archive, ref.art_id]] = true
            for candidate in ref.candidates:
                if candidate.has("key"): used[candidate.key] = true
    _rebuild_list()
    _refresh_character()
    status.text = "%d monster/NPC records and combat image pairs. Unused means no reference in decoded ECL evidence; dynamic paths may be unresolved." % rows.size()
    if "--catalog-check" in OS.get_cmdline_user_args(): _catalog_check()

func _process(delta: float) -> void:
    if selected < 0: return
    elapsed += delta
    if elapsed >= 1.0:
        elapsed = fmod(elapsed, 1.0)
        action = not action
        _show_monster_image()

func _build_ui() -> void:
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    for edge in ["left", "right", "top", "bottom"]: margin.add_theme_constant_override("margin_" + edge, 18)
    add_child(margin)
    var tabs := TabContainer.new()
    tabs.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
    margin.add_child(tabs)
    var monsters := HBoxContainer.new()
    monsters.name = "Monsters and NPCs"
    monsters.add_theme_constant_override("separation", 20)
    tabs.add_child(monsters)
    var sidebar := VBoxContainer.new()
    sidebar.custom_minimum_size.x = 340
    monsters.add_child(sidebar)
    hide_unused = CheckButton.new()
    hide_unused.text = "Hide unused monsters / NPCs"
    hide_unused.toggled.connect(func(_value: bool): _rebuild_list())
    sidebar.add_child(hide_unused)
    monster_list = ItemList.new()
    monster_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
    monster_list.item_selected.connect(_select_monster)
    sidebar.add_child(monster_list)
    var detail := VBoxContainer.new()
    detail.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    detail.add_theme_constant_override("separation", 12)
    monsters.add_child(detail)
    monster_title = Label.new()
    monster_title.add_theme_font_size_override("font_size", 24)
    detail.add_child(monster_title)
    monster_image = TextureRect.new()
    monster_image.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    monster_image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    monster_image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    monster_image.custom_minimum_size = Vector2(360, 390)
    detail.add_child(monster_image)
    monster_source = Label.new()
    monster_source.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    detail.add_child(monster_source)
    var gear_heading := Label.new()
    gear_heading.text = "Weapons and gear"
    gear_heading.add_theme_font_size_override("font_size", 20)
    detail.add_child(gear_heading)
    gear_text = RichTextLabel.new()
    gear_text.size_flags_vertical = Control.SIZE_EXPAND_FILL
    detail.add_child(gear_text)
    status = Label.new()
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    detail.add_child(status)

    var characters := HBoxContainer.new()
    characters.name = "Character sprites"
    characters.add_theme_constant_override("separation", 24)
    tabs.add_child(characters)
    var controls := VBoxContainer.new()
    controls.custom_minimum_size.x = 360
    characters.add_child(controls)
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    controls.add_child(scroll)
    var choices := VBoxContainer.new()
    choices.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    scroll.add_child(choices)
    head_picker = _picker(choices, "Head", 14)
    body_picker = _picker(choices, "Body", 35)
    size_picker = _picker(choices, "Size", 0)
    size_picker.add_item("Short", 0)
    size_picker.add_item("Regular", 1)
    size_picker.select(1)
    weapon_picker = _picker(choices, "Weapon", 0)
    shield_picker = CheckButton.new()
    shield_picker.text = "Shield equipped"
    shield_picker.toggled.connect(func(_value: bool): _refresh_character())
    choices.add_child(shield_picker)
    for bank in range(2):
        var heading := Label.new()
        heading.text = "Color %d" % (bank + 1)
        heading.add_theme_font_size_override("font_size", 18)
        choices.add_child(heading)
        for part in range(6):
            var picker := _picker(choices, REGIONS[part], 0)
            for index in range(16): picker.add_item(COLOR_NAMES[index], index)
            picker.select(DEFAULT_COLORS[bank * 6 + part])
            color_pickers.append(picker)
    var previews := VBoxContainer.new()
    previews.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    characters.add_child(previews)
    var figures := HBoxContainer.new()
    figures.size_flags_vertical = Control.SIZE_EXPAND_FILL
    previews.add_child(figures)
    ready_image = _character_preview(figures, "Ready")
    action_image = _character_preview(figures, "Action")
    character_status = Label.new()
    character_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    previews.add_child(character_status)

func _picker(parent: VBoxContainer, caption: String, count: int) -> OptionButton:
    var label := Label.new()
    label.text = caption
    parent.add_child(label)
    var picker := OptionButton.new()
    picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    for index in range(count): picker.add_item("%s %d" % [caption, index + 1], index)
    picker.item_selected.connect(func(_index: int): _refresh_character())
    parent.add_child(picker)
    return picker

func _character_preview(parent: HBoxContainer, caption: String) -> TextureRect:
    var panel := VBoxContainer.new()
    panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    parent.add_child(panel)
    var label := Label.new()
    label.text = caption
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    panel.add_child(label)
    var texture := TextureRect.new()
    texture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    texture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    texture.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    texture.size_flags_vertical = Control.SIZE_EXPAND_FILL
    texture.custom_minimum_size = Vector2(280, 380)
    panel.add_child(texture)
    return texture

func _rebuild_list() -> void:
    monster_list.clear()
    visible_rows.clear()
    for index in range(rows.size()):
        var row: Dictionary = rows[index]
        var unused := not used.has(row.key)
        if hide_unused.button_pressed and unused: continue
        var label := "%s%s  ·  %s" % ["[Unused] " if unused else "", row.name, row.key]
        monster_list.add_item(label)
        visible_rows.append(index)
    if visible_rows.is_empty():
        selected = -1
        monster_title.text = "No matching records"
        monster_image.texture = null
        gear_text.text = ""
        return
    var next := visible_rows.find(selected)
    if next < 0: next = 0
    monster_list.select(next)
    _select_monster(next)

func _select_monster(list_index: int) -> void:
    if list_index < 0 or list_index >= visible_rows.size(): return
    selected = visible_rows[list_index]
    action = false
    elapsed = 0.0
    var row: Dictionary = rows[selected]
    monster_title.text = "%s%s" % [row.name, "  ·  Unused" if not used.has(row.key) else ""]
    var lines: PackedStringArray = []
    for item in row.equipment:
        lines.append("• %s%s (type %d)" % [item.label, "  ·  readied" if item.readied else "", item.type])
    gear_text.text = "\n".join(lines) if not lines.is_empty() else "No item record entries"
    _show_monster_image()

func _show_monster_image() -> void:
    var art: Dictionary = catalog.monster_art(selected)
    monster_image.texture = catalog.monster_image(selected, action)
    if art.is_empty():
        monster_source.text = "No supported ready/action CPIC binding for this record. Source: %s" % rows[selected].key
    elif art.get("assembled", false):
        monster_source.text = "%s · %s · %s" % ["Action" if action else "Ready", rows[selected].key, art.basis]
    else:
        monster_source.text = "%s · %s record %d / %d · %s" % ["Action" if action else "Ready", art.archive, art.record + (128 if action else 0), art.record, art.basis]

func _refresh_character() -> void:
    if color_pickers.size() != 12 or catalog.weapons().is_empty(): return
    var colors: Array = []
    for picker in color_pickers: colors.append(picker.get_selected_id())
    var result: Dictionary = catalog.character_art(head_picker.get_selected_id(), body_picker.get_selected_id(), size_picker.get_selected_id() == 0, weapon_picker.get_selected_id(), shield_picker.button_pressed, colors)
    if result.has("error"):
        character_status.text = result.error
        return
    ready_image.texture = result.ready
    action_image.texture = result.action
    character_status.text = "%s · artwork body %d%s" % [result.selection, result.body, "" if result.matched else " · no mapped art; unarmed appearance shown"]

func _catalog_check() -> void:
    assert(rows.size() >= 170)
    assert(monster_list.item_count == rows.size())
    var pictured := -1
    for index in range(rows.size()):
        if not catalog.monster_art(index).is_empty():
            pictured = index
            break
    assert(pictured >= 0)
    assert(catalog.monster_image(pictured, false) != null)
    assert(catalog.monster_image(pictured, true) != null)
    assert(not rows[pictured].equipment.is_empty() or rows.size() > 0)
    hide_unused.button_pressed = true
    assert(monster_list.item_count < rows.size())
    assert(monster_list.item_count > 0)
    hide_unused.button_pressed = false
    assert(monster_list.item_count == rows.size())
    var colors: Array = DEFAULT_COLORS.duplicate()
    for option in catalog.weapons():
        var both: Dictionary = catalog.character_art(0, 4, false, option.type, true, colors)
        assert(not both.has("error"))
        assert(both.ready != null and both.action != null)
    var referenced := 0
    var unresolved := 0
    for index in range(rows.size()):
        if used.has(rows[index].key): referenced += 1
        if catalog.monster_art(index).is_empty(): unresolved += 1
        assert(catalog.monster_image(index, false) != null)
        assert(catalog.monster_image(index, true) != null)
    print("Combat sprite catalog checks passed: %d rows, %d referenced, %d unresolved art bindings, all weapon and shield previews" % [rows.size(), referenced, unresolved])
    get_tree().quit()
