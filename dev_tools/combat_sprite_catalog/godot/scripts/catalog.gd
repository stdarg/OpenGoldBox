extends Control

const Evidence = preload("res://scripts/art_script_evidence.gd")

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
var character_panel

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
    character_panel.set_catalog(catalog)
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

    character_panel = preload("res://scenes/character_sprite_panel.tscn").instantiate()
    tabs.add_child(character_panel)

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
    var colors: Array = character_panel.colors.duplicate()
    for option in catalog.weapons():
        var both: Dictionary = catalog.character_art(0, 4, 1, option.type, true, colors)
        assert(not both.has("error"))
        assert(both.ready != null and both.action != null)
    var character_error: String = character_panel.check_controls()
    if not character_error.is_empty():
        push_error(character_error)
        get_tree().quit(1)
        return
    var referenced := 0
    var unresolved := 0
    for index in range(rows.size()):
        if used.has(rows[index].key): referenced += 1
        if catalog.monster_art(index).is_empty(): unresolved += 1
        assert(catalog.monster_image(index, false) != null)
        assert(catalog.monster_image(index, true) != null)
    print("Combat sprite catalog checks passed: %d rows, %d referenced, %d unresolved art bindings, all weapon and shield previews" % [rows.size(), referenced, unresolved])
    get_tree().quit()
