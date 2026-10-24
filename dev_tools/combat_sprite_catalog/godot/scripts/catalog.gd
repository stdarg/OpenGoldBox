extends Control

const Evidence = preload("res://scripts/art_script_evidence.gd")
const MonsterMetadata = preload("res://scripts/monster_metadata.gd")
const MonsterExport = preload("res://scripts/monster_export.gd")
const GEAR_ROLES := ["one_hand", "two_hand", "ranged", "shield", "gear"]
const HUMANOID_SIZES := ["na", "small", "normal", "tall"]
const GRID_CELL := 128.0
const GRID_ORIGIN := Vector2(96.0, 16.0)
const GRID_CELLS := 3

var catalog := SpriteCatalog.new()
var rows: Array = []
var visible_rows: Array[int] = []
var used: Dictionary = {}
var selected := -1
var action := false
var elapsed := 0.0
var monster_list: ItemList
var hide_unused: CheckButton
var name_filter: LineEdit
var monster_title: Label
var name_edit: LineEdit
var monster_id_edit: LineEdit
var monster_image: TextureRect
var image_frame: Control
var preview_is_replacement := false
var preview_ready_bottom := 128.0
var preview_status: Label
var monster_source: Label
var description_edit: TextEdit
var sprite_size_choice: OptionButton
var humanoid_size_choice: OptionButton
var height_edit: SpinBox
var height_source_label: Label
var gear_rows: VBoxContainer
var gear_source_hint: Label
var add_type_choice: OptionButton
var regeneration_flag: CheckButton
var show_new_art: CheckButton
var variant_choice: OptionButton
var save_button: Button
var metadata_status: Label
var draft_records: Dictionary = {}
var load_errors: Dictionary = {}
var metadata_root := ""
var native_index_by_key: Dictionary = {}
var art_users: Dictionary = {}
var status: Label
var character_panel

func _ready() -> void:
    if "--export-to-game" in OS.get_cmdline_user_args():
        var export_error: String = MonsterExport.export_all()
        if not export_error.is_empty():
            push_error(export_error)
            get_tree().quit(1)
            return
        print("Exported the monsters to " + MonsterExport.game_folder())
        get_tree().quit()
        return
    if "--metadata-check" in OS.get_cmdline_user_args():
        metadata_root = ProjectSettings.globalize_path("user://metadata-check-%d" % Time.get_ticks_usec())
    _build_ui()
    var game_dir := OS.get_environment("OPENGOLD_GAME_DIR")
    if game_dir.is_empty(): game_dir = ProjectSettings.get_setting("opengold/game_directory", "")
    var art_dir := ProjectSettings.globalize_path("res://../../../data/art")
    var error := catalog.load_data(game_dir, art_dir)
    if not error.is_empty():
        status.text = "Cannot load sprite catalog: " + error
        return
    rows = catalog.creatures()
    for index in range(rows.size()):
        rows[index]["native_index"] = index
        rows[index]["origin_key"] = str(rows[index].key)
        native_index_by_key[rows[index].key] = index
    character_panel.set_catalog(catalog)
    var evidence := Evidence.new()
    evidence.build(game_dir)
    _apply_script_bindings(evidence)
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
    _apply_shared_art_identities()
    for row in MonsterMetadata.list_custom_rows(metadata_root):
        rows.append(row)
        used[row.key] = true
    for index in range(rows.size()):
        var row: Dictionary = rows[index]
        var loaded: Dictionary = MonsterMetadata.load_record(row, _art_for(index), metadata_root)
        if loaded.error.is_empty():
            row.name = loaded.data.name
    _rebuild_list()
    status.text = "%d monster/NPC records and combat image pairs. Unused means no reference in decoded ECL evidence; dynamic paths may be unresolved." % rows.size()
    if "--metadata-check" in OS.get_cmdline_user_args():
        _metadata_check()
    elif "--catalog-check" in OS.get_cmdline_user_args():
        _catalog_check()

func _process(delta: float) -> void:
    if selected < 0: return
    elapsed += delta
    if elapsed >= 1.0:
        elapsed = fmod(elapsed, 1.0)
        action = not action
        _show_monster_image()

func _apply_script_bindings(evidence) -> void:
    var choices := {}
    for art_key in evidence.references:
        for ref in evidence.references[art_key]:
            if ref.family != "CPIC": continue
            for candidate in ref.candidates:
                if not candidate.has("key") or not native_index_by_key.has(candidate.key): continue
                var creature_key := str(candidate.key)
                if not choices.has(creature_key): choices[creature_key] = {}
                for archive in ref.art_archives:
                    var key := "%s:%d" % [archive, ref.art_id]
                    if not choices[creature_key].has(key):
                        choices[creature_key][key] = {"archive":archive,"record":ref.art_id,"count":0}
                    choices[creature_key][key].count += 1
                    if not art_users.has(key): art_users[key] = {}
                    art_users[key][creature_key] = true
    for creature_key in choices:
        var best_key := ""
        var best_score := -1
        for art_key in choices[creature_key]:
            var option: Dictionary = choices[creature_key][art_key]
            var same_bank := str(option.archive).substr(4, 1) == str(creature_key).substr(3, 1)
            var score := int(option.count) * 2 + (1 if same_bank else 0)
            if score > best_score or (score == best_score and (best_key.is_empty() or art_key < best_key)):
                best_key = art_key
                best_score = score
        var chosen: Dictionary = choices[creature_key][best_key]
        var index: int = native_index_by_key[creature_key]
        var count: int = choices[creature_key].size()
        var basis := "ECL LOAD MONSTER icon (%d script candidate%s)" % [count, "s" if count != 1 else ""]
        if catalog.bind_monster_art(index, str(chosen.archive), int(chosen.record), basis):
            rows[index].art_basis = basis
            rows[index]["script_alternatives"] = choices[creature_key].keys()
    for index in range(rows.size()):
        var row: Dictionary = rows[index]
        if not str(row.key).begins_with("CPIC") or not art_users.has(row.key): continue
        var keys: Array = art_users[row.key].keys()
        keys.sort()
        var names: Array[String] = []
        for key in keys:
            names.append(str(rows[native_index_by_key[key]].name))
        row["linked_name"] = "%s combat icon" % names[0] if keys.size() == 1 else "Shared combat icon: %s" % ", ".join(names)
        row["linked_keys"] = keys
        if keys.size() == 1:
            row.equipment = rows[native_index_by_key[keys[0]]].equipment.duplicate(true)
            row.wealth = rows[native_index_by_key[keys[0]]].wealth.duplicate(true)

func _apply_shared_art_identities() -> void:
    for group in catalog.identical_art_groups():
        var users := {}
        for art_key in group:
            for creature_key in art_users.get(art_key, {}):
                users[creature_key] = true
        if users.is_empty(): continue
        var keys: Array = users.keys()
        keys.sort()
        var names: Array[String] = []
        for key in keys:
            var name := str(rows[native_index_by_key[key]].name)
            if not names.has(name): names.append(name)
        names.sort()
        for art_key in group:
            if not native_index_by_key.has(art_key): continue
            var row: Dictionary = rows[native_index_by_key[art_key]]
            if row.has("linked_keys"): continue
            var copies: Array = group.duplicate()
            copies.erase(art_key)
            row["identical_art_keys"] = copies
            row["shared_art_user_keys"] = keys.duplicate()
            row["identified_name"] = "%s combat icon (duplicate)" % names[0] if names.size() == 1 else \
                "Shared combat icon: %s" % ", ".join(names)
            row.name = row.identified_name
            row["art_basis"] = "Identical ready/action records; shared art identity"
            row["direct_script_reference_found"] = used.has(art_key)
            row["art_usage_note"] = "Direct script reference found for this archive copy." if used.has(art_key) else \
                "No direct script reference found for this archive copy."
            row["art_description"] = "%s. Identical originals: %s. Known users of this art: %s. %s" % [
                row.identified_name, ", ".join(copies), ", ".join(names), row.art_usage_note]

func _art_for(index: int) -> Dictionary:
    var row: Dictionary = rows[index]
    if int(row.get("native_index", -1)) >= 0:
        var art: Dictionary = catalog.monster_art(int(row.native_index))
        if row.has("linked_keys"):
            art.basis = "ECL LOAD MONSTER icon shared by %d creature%s" % [row.linked_keys.size(), "s" if row.linked_keys.size() != 1 else ""]
        elif row.has("identical_art_keys"):
            art.basis = row.art_basis
        var ready: Texture2D = catalog.monster_image(int(row.native_index), false)
        var strike: Texture2D = catalog.monster_image(int(row.native_index), true)
        if ready != null and strike != null:
            var width: int = maxi(ready.get_width(), strike.get_width())
            var height: int = maxi(ready.get_height(), strike.get_height())
            if width > 0 and width <= 48 and height > 0 and height <= 48:
                art["sprite_size"] = {"width_squares": 1 if width <= 24 else 2,
                    "height_squares": 1 if height <= 24 else 2}
        return art
    var source: Dictionary = draft_records.get(row.key, {}).get("source", {})
    if source.is_empty():
        var loaded: Dictionary = MonsterMetadata.load_record(row, {}, metadata_root)
        source = loaded.data.source
    return {"archive":str(source.get("art_archive", "")),
        "record":int(source.get("art_record", -1)),
        "basis":str(source.get("art_basis", "Copied monster art"))}

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
    var name_label := Label.new()
    name_label.text = "Monster / NPC name contains"
    sidebar.add_child(name_label)
    name_filter = LineEdit.new()
    name_filter.placeholder_text = "Filter by name"
    name_filter.clear_button_enabled = true
    name_filter.text_changed.connect(func(_text: String): _rebuild_list())
    sidebar.add_child(name_filter)
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
    var identity := HBoxContainer.new()
    detail.add_child(identity)
    var identity_label := Label.new()
    identity_label.text = "Name"
    identity.add_child(identity_label)
    name_edit = LineEdit.new()
    name_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    name_edit.size_flags_stretch_ratio = 1.0
    name_edit.placeholder_text = "Monster or NPC name"
    identity.add_child(name_edit)
    var rename_button := Button.new()
    rename_button.text = "Rename"
    rename_button.tooltip_text = "Save this name for the selected catalog record"
    rename_button.pressed.connect(_rename_monster)
    identity.add_child(rename_button)
    var id_label := Label.new()
    id_label.text = "Monster ID"
    identity.add_child(id_label)
    monster_id_edit = LineEdit.new()
    monster_id_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    monster_id_edit.size_flags_stretch_ratio = 2.0
    monster_id_edit.editable = false
    monster_id_edit.select_all_on_focus = true
    monster_id_edit.tooltip_text = "Select and copy the ID with the standard keyboard shortcut or context menu"
    identity.add_child(monster_id_edit)
    var copy_button := Button.new()
    copy_button.text = "Copy as new monster"
    copy_button.tooltip_text = "Copy metadata and replacement PNGs under a new name"
    copy_button.pressed.connect(_copy_monster)
    identity.add_child(copy_button)
    var columns := HBoxContainer.new()
    columns.size_flags_vertical = Control.SIZE_EXPAND_FILL
    columns.add_theme_constant_override("separation", 18)
    detail.add_child(columns)
    var preview := VBoxContainer.new()
    preview.custom_minimum_size.x = 520
    preview.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    columns.add_child(preview)
    var variant_label := Label.new()
    variant_label.text = "Weapon set"
    variant_label.add_theme_font_size_override("font_size", 20)
    preview.add_child(variant_label)
    variant_choice = OptionButton.new()
    variant_choice.custom_minimum_size.y = 42
    variant_choice.item_selected.connect(func(_index: int): _show_monster_image())
    preview.add_child(variant_choice)
    show_new_art = CheckButton.new()
    show_new_art.text = "Show new art when available"
    show_new_art.button_pressed = true
    show_new_art.toggled.connect(func(_enabled: bool): _show_monster_image())
    preview.add_child(show_new_art)
    image_frame = Control.new()
    image_frame.custom_minimum_size = Vector2(512, 416)
    image_frame.draw.connect(_draw_preview_grid)
    preview.add_child(image_frame)
    monster_image = TextureRect.new()
    monster_image.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    monster_image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    monster_image.stretch_mode = TextureRect.STRETCH_SCALE
    monster_image.mouse_filter = Control.MOUSE_FILTER_IGNORE
    image_frame.add_child(monster_image)
    preview_status = Label.new()
    preview_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    preview_status.custom_minimum_size.y = 42
    preview.add_child(preview_status)
    monster_source = Label.new()
    monster_source.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    monster_source.custom_minimum_size.y = 80
    preview.add_child(monster_source)

    var editor := VBoxContainer.new()
    editor.custom_minimum_size.x = 400
    editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    columns.add_child(editor)
    var sprite_size_label := Label.new()
    sprite_size_label.text = "Sprite size (combat squares)"
    editor.add_child(sprite_size_label)
    sprite_size_choice = OptionButton.new()
    for label in ["Unknown", "1 × 1", "1 × 2 tall", "2 × 1 wide", "2 × 2 (four squares)", "3 × 3 (nine squares)"]:
        sprite_size_choice.add_item(label)
    sprite_size_choice.tooltip_text = "Desired visual size of this sprite; Save monster data writes it to JSON."
    sprite_size_choice.item_selected.connect(func(_index: int): _refresh_monster_preview_layout())
    editor.add_child(sprite_size_choice)
    var humanoid_size_label := Label.new()
    humanoid_size_label.text = "Humanoid height"
    editor.add_child(humanoid_size_label)
    humanoid_size_choice = OptionButton.new()
    for label in ["N/A (non-humanoid)", "Small", "Normal", "Tall"]:
        humanoid_size_choice.add_item(label)
    humanoid_size_choice.tooltip_text = "Visual body-height tier for consistent art across monsters; Save monster data writes it to JSON."
    humanoid_size_choice.item_selected.connect(func(_index: int): _on_humanoid_size_changed())
    editor.add_child(humanoid_size_choice)
    var height_label := Label.new()
    height_label.text = "Small humanoid art height (feet)"
    editor.add_child(height_label)
    height_edit = SpinBox.new()
    height_edit.min_value = 2.0
    height_edit.max_value = 4.0
    height_edit.step = 0.25
    height_edit.suffix = "ft"
    height_edit.tooltip_text = "Foot-to-head art target for Small humanoids. One combat square is 5 feet tall."
    editor.add_child(height_edit)
    height_source_label = Label.new()
    height_source_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    editor.add_child(height_source_label)
    var description_heading := Label.new()
    description_heading.text = "Description for new art"
    editor.add_child(description_heading)
    description_edit = TextEdit.new()
    description_edit.custom_minimum_size.y = 165
    description_edit.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
    editor.add_child(description_edit)
    var gear_heading := Label.new()
    gear_heading.text = "Weapons, shields and gear"
    gear_heading.add_theme_font_size_override("font_size", 20)
    editor.add_child(gear_heading)
    gear_source_hint = Label.new()
    gear_source_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    editor.add_child(gear_source_hint)
    var gear_hint := Label.new()
    gear_hint.text = "Click an item's type to change it. The art selector pairs 1H weapons with shields; ranged and 2H weapons use separate art."
    gear_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    editor.add_child(gear_hint)
    var gear_scroll := ScrollContainer.new()
    gear_scroll.custom_minimum_size.y = 190
    gear_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    editor.add_child(gear_scroll)
    gear_rows = VBoxContainer.new()
    gear_rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    gear_scroll.add_child(gear_rows)
    var add_row := HBoxContainer.new()
    editor.add_child(add_row)
    add_type_choice = OptionButton.new()
    add_type_choice.tooltip_text = "An original item type fights and drops as that item in the game. Other gear only changes the art."
    add_type_choice.add_item("Other gear (art only)", -1)
    for type in catalog.item_types():
        add_type_choice.add_item(str(type.name), int(type.type))
    add_type_choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    add_row.add_child(add_type_choice)
    var add_gear := Button.new()
    add_gear.text = "Add item"
    add_gear.pressed.connect(func(): _add_gear(add_type_choice.get_selected_id()))
    add_row.add_child(add_gear)
    regeneration_flag = CheckButton.new()
    regeneration_flag.text = "Flag for regeneration"
    editor.add_child(regeneration_flag)
    save_button = Button.new()
    save_button.text = "Save monster data"
    save_button.pressed.connect(_save_monster)
    editor.add_child(save_button)
    var export_button := Button.new()
    export_button.text = "Export to game"
    export_button.tooltip_text = "Write every saved monster's name, gear and art to data/monsters"
    export_button.pressed.connect(_export_to_game)
    editor.add_child(export_button)
    metadata_status = Label.new()
    metadata_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    editor.add_child(metadata_status)
    status = Label.new()
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    detail.add_child(status)

    character_panel = preload("res://scenes/character_sprite_panel.tscn").instantiate()
    tabs.add_child(character_panel)

func _rebuild_list() -> void:
    _store_draft()
    monster_list.clear()
    visible_rows.clear()
    var query := name_filter.text.strip_edges()
    for index in range(rows.size()):
        var row: Dictionary = rows[index]
        var unused := not used.has(row.key)
        if hide_unused.button_pressed and unused: continue
        if not query.is_empty() and str(row.name).findn(query) < 0: continue
        var label := "%s%s  ·  %s" % ["[Unused] " if unused else "", row.name, row.key]
        monster_list.add_item(label)
        visible_rows.append(index)
    if visible_rows.is_empty():
        selected = -1
        monster_title.text = "No matching records"
        monster_image.texture = null
        image_frame.queue_redraw()
        preview_status.text = ""
        monster_source.text = ""
        name_edit.text = ""
        monster_id_edit.text = ""
        variant_choice.clear()
        description_edit.text = ""
        sprite_size_choice.select(0)
        humanoid_size_choice.select(0)
        height_edit.editable = false
        height_source_label.text = ""
        gear_source_hint.text = ""
        for child in gear_rows.get_children():
            gear_rows.remove_child(child)
            child.queue_free()
        regeneration_flag.set_pressed_no_signal(false)
        metadata_status.text = ""
        save_button.disabled = true
        return
    var next := visible_rows.find(selected)
    if next < 0: next = 0
    monster_list.select(next)
    _select_monster(next)

func _select_monster(list_index: int) -> void:
    if list_index < 0 or list_index >= visible_rows.size(): return
    _store_draft()
    selected = visible_rows[list_index]
    action = false
    elapsed = 0.0
    var row: Dictionary = rows[selected]
    monster_title.text = "%s%s" % [row.name, "  ·  Unused" if not used.has(row.key) else ""]
    name_edit.text = str(row.name)
    if not draft_records.has(row.key):
        var loaded: Dictionary = MonsterMetadata.load_record(row, _art_for(selected), metadata_root)
        draft_records[row.key] = loaded.data
        if not loaded.error.is_empty(): load_errors[row.key] = loaded.error
    metadata_status.text = str(load_errors.get(row.key, ""))
    var record: Dictionary = draft_records[row.key]
    row.name = record.name
    monster_title.text = "%s%s" % [row.name, "  ·  Unused" if not used.has(row.key) else ""]
    name_edit.text = record.name
    monster_id_edit.text = str(record.source.key)
    if row.has("linked_keys"):
        gear_source_hint.text = "Gear from %s" % row.linked_keys[0] if row.linked_keys.size() == 1 else \
            "Shared icon. Select a named creature to edit its gear."
    elif row.has("shared_art_user_keys"):
        gear_source_hint.text = "Shared artwork. Select a named creature to edit its gear."
    elif str(row.key).begins_with("CUSTOM:"):
        gear_source_hint.text = "Independent copied gear. Original source: %s" % row.origin_key
    elif row.equipment.is_empty():
        gear_source_hint.text = "No items recorded for this creature in the original MONnITM archive."
    else:
        gear_source_hint.text = "Original gear loaded from the matching MONnITM record."
    description_edit.text = record.description
    sprite_size_choice.select(_sprite_size_index(record.get("sprite_size", null)))
    humanoid_size_choice.select(HUMANOID_SIZES.find(str(record.get("humanoid_size", "na"))))
    _refresh_height_editor(record)
    image_frame.queue_redraw()
    regeneration_flag.set_pressed_no_signal(bool(record.flag_for_regeneration))
    for child in gear_rows.get_children():
        gear_rows.remove_child(child)
        child.queue_free()
    for item in record.equipment:
        _append_gear_row(item)
    save_button.disabled = false
    variant_choice.clear()
    _refresh_variants()

func _append_gear_row(item: Dictionary) -> void:
    var line := HBoxContainer.new()
    line.set_meta("id", str(item.id))
    line.set_meta("source_type", int(item.get("source_type", -1)))
    line.set_meta("source_hands", int(item.get("source_hands", -1)))
    line.set_meta("source_range", int(item.get("source_range", -1)))
    var kind := Button.new()
    var role := str(item.get("weapon_role", "one_hand")) if item.kind == "weapon" else str(item.kind)
    _set_gear_role(kind, role)
    kind.custom_minimum_size.x = 125
    kind.tooltip_text = "Click to change this item's type"
    kind.pressed.connect(func():
        _set_gear_role(kind, GEAR_ROLES[(GEAR_ROLES.find(str(kind.get_meta("role"))) + 1) % GEAR_ROLES.size()])
        _gear_changed()
    )
    line.add_child(kind)
    var name := LineEdit.new()
    name.text = str(item.name)
    name.placeholder_text = "Item name"
    name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    name.text_changed.connect(func(_value: String): _gear_changed())
    line.add_child(name)
    var readied := CheckButton.new()
    readied.text = "Ready"
    readied.button_pressed = bool(item.get("readied", false))
    readied.toggled.connect(func(_value: bool): _gear_changed())
    line.add_child(readied)
    var remove := Button.new()
    remove.text = "Remove"
    remove.pressed.connect(func():
        gear_rows.remove_child(line)
        line.queue_free()
        _gear_changed()
    )
    line.add_child(remove)
    gear_rows.add_child(line)

func _set_gear_role(button: Button, role: String) -> void:
    var index := GEAR_ROLES.find(role)
    if index < 0: index = GEAR_ROLES.size() - 1
    button.set_meta("role", GEAR_ROLES[index])
    button.text = ["1H weapon", "2H weapon", "Ranged", "Shield", "Gear"][index]

# Adds an item of an original type (named for it, which can be edited), or
# with type -1 a named piece of gear that only changes the art.
func _add_gear(type: int = -1) -> void:
    if selected < 0: return
    var item_id := "added_%d" % Time.get_ticks_usec()
    var kind := MonsterMetadata.source_kind(type) if type >= 0 else "gear"
    var role := MonsterMetadata.source_weapon_role(type, -1) if kind == "weapon" else ""
    var name := add_type_choice.get_item_text(add_type_choice.get_item_index(type)) if type >= 0 else ""
    _append_gear_row({"id": item_id, "source_type": type, "source_hands": -1,
        "source_range": -1, "kind": kind, "weapon_role": role, "name": name, "readied": false})
    _gear_changed()
    var line: HBoxContainer = gear_rows.get_child(gear_rows.get_child_count() - 1)
    line.get_child(1).grab_focus()

func _on_humanoid_size_changed() -> void:
    if selected >= 0 and draft_records.has(rows[selected].key):
        _refresh_height_editor(draft_records[rows[selected].key], true)
    _refresh_monster_preview_layout()

func _refresh_height_editor(record: Dictionary, preserve_value: bool = false) -> void:
    var small: bool = HUMANOID_SIZES[humanoid_size_choice.selected] == "small"
    height_edit.editable = small
    if not small:
        height_source_label.text = ""
        return
    var reference: Variant = record.get("height_reference", null)
    var low := 2.0
    var high := 4.0
    if reference is Dictionary and reference.get("min_feet") != null and reference.get("max_feet") != null:
        low = float(reference.min_feet)
        high = float(reference.max_feet)
        height_source_label.text = "Published species range: %s–%s ft. %s" % [str(reference.min_feet), str(reference.max_feet), str(reference.get("note", ""))]
        height_source_label.tooltip_text = str(reference.get("url", ""))
    elif reference is Dictionary:
        height_source_label.text = str(reference.get("note", "Art-direction estimate; no published range recorded."))
        height_source_label.tooltip_text = str(reference.get("url", ""))
    else:
        height_source_label.text = "No species-specific source range recorded."
        height_source_label.tooltip_text = ""
    height_edit.min_value = low
    height_edit.max_value = high
    var saved: Variant = height_edit.value if preserve_value else record.get("height_feet", null)
    height_edit.value = float(saved) if MonsterMetadata.valid_small_height(saved) else (low + high) * 0.5

func _store_draft() -> void:
    if selected < 0 or not draft_records.has(rows[selected].key): return
    var record: Dictionary = draft_records[rows[selected].key]
    record.description = description_edit.text
    record.sprite_size = _sprite_size_for_index(sprite_size_choice.selected)
    record.humanoid_size = HUMANOID_SIZES[humanoid_size_choice.selected]
    record.height_feet = height_edit.value if record.humanoid_size == "small" else null
    record.flag_for_regeneration = regeneration_flag.button_pressed
    var equipment: Array = []
    for line in gear_rows.get_children():
        var kind: Button = line.get_child(0)
        var name: LineEdit = line.get_child(1)
        var readied: CheckButton = line.get_child(2)
        var role: String = str(kind.get_meta("role"))
        equipment.append({
            "id": line.get_meta("id"),
            "kind": "weapon" if role in ["one_hand", "two_hand", "ranged"] else role,
            "weapon_role": role if role in ["one_hand", "two_hand", "ranged"] else "",
            "name": name.text.strip_edges(),
            "source_type": line.get_meta("source_type"),
            "source_hands": line.get_meta("source_hands"),
            "source_range": line.get_meta("source_range"),
            "readied": readied.button_pressed,
        })
    record.equipment = equipment
    MonsterMetadata.rebuild_variants(record)


func _sprite_size_for_index(index: int) -> Variant:
    match index:
        1: return {"width_squares": 1, "height_squares": 1}
        2: return {"width_squares": 1, "height_squares": 2}
        3: return {"width_squares": 2, "height_squares": 1}
        4: return {"width_squares": 2, "height_squares": 2}
        5: return {"width_squares": 3, "height_squares": 3}
    return null


func _sprite_size_index(size: Variant) -> int:
    if not size is Dictionary: return 0
    var width: int = int(size.get("width_squares", 0))
    var height: int = int(size.get("height_squares", 0))
    if width == 1 and height == 1: return 1
    if width == 1 and height == 2: return 2
    if width == 2 and height == 1: return 3
    if width == 2 and height == 2: return 4
    if width == 3 and height == 3: return 5
    return 0

func _gear_changed() -> void:
    var previous := _selected_variant()
    _store_draft()
    if not previous.is_empty():
        var key: String = rows[selected].key
        var record: Dictionary = draft_records[key]
        var remains := false
        for variant in record.variants:
            if variant.id == previous.id: remains = true
        if not remains and (FileAccess.file_exists(MonsterMetadata.art_path(key, str(previous.id), false, metadata_root)) or \
                FileAccess.file_exists(MonsterMetadata.art_path(key, str(previous.id), true, metadata_root))):
            record.preview_fallback_variant = previous.id
    _refresh_variants()

func _refresh_variants() -> void:
    if selected < 0: return
    var previous := ""
    if variant_choice.selected >= 0:
        previous = str(variant_choice.get_item_metadata(variant_choice.selected))
    variant_choice.clear()
    var record: Dictionary = draft_records[rows[selected].key]
    for variant in record.variants:
        variant_choice.add_item(str(variant.label))
        variant_choice.set_item_metadata(variant_choice.item_count - 1, variant.id)
    var next := 0
    for index in range(variant_choice.item_count):
        if variant_choice.get_item_metadata(index) == previous:
            next = index
            break
    variant_choice.select(next)
    variant_choice.disabled = variant_choice.item_count <= 1
    _show_monster_image()

func _save_monster() -> void:
    if selected < 0: return
    _store_draft()
    var record: Dictionary = draft_records[rows[selected].key]
    var error: String = MonsterMetadata.save_record(record, metadata_root)
    if error.is_empty():
        load_errors.erase(rows[selected].key)
        metadata_status.text = "Saved %s" % MonsterMetadata.json_path(rows[selected].key, metadata_root).get_file()
    else:
        metadata_status.text = error

# The game reads saved records only, so unsaved edits stay out of the export.
func _export_to_game() -> void:
    var error: String = MonsterExport.export_all(metadata_root)
    metadata_status.text = "Exported the saved monsters to data/monsters" if error.is_empty() else error

func _rename_monster() -> void:
    if selected < 0: return
    var renamed := name_edit.text.strip_edges()
    if renamed.is_empty():
        metadata_status.text = "Enter a monster name."
        return
    _store_draft()
    var row: Dictionary = rows[selected]
    var record: Dictionary = draft_records[row.key]
    var old_name: String = record.name
    record.name = renamed
    var error: String = MonsterMetadata.save_record(record, metadata_root)
    if not error.is_empty():
        record.name = old_name
        metadata_status.text = error
        return
    row.name = renamed
    monster_title.text = renamed
    _rebuild_list()
    metadata_status.text = "Renamed catalog record. Original game data was not changed."

func _copy_monster() -> void:
    if selected < 0: return
    _store_draft()
    var source: Dictionary = draft_records[rows[selected].key]
    var result: Dictionary = MonsterMetadata.copy_record(source, name_edit.text, metadata_root)
    if not result.error.is_empty():
        metadata_status.text = result.error
        return
    var copied: Dictionary = result.data
    var copied_key: String = copied.source.key
    for row in MonsterMetadata.list_custom_rows(metadata_root):
        if row.key == copied_key:
            rows.append(row)
            draft_records[copied_key] = copied
            used[copied_key] = true
            break
    name_filter.text = ""
    _rebuild_list()
    for index in range(visible_rows.size()):
        if rows[visible_rows[index]].key == copied_key:
            monster_list.select(index)
            _select_monster(index)
            break
    metadata_status.text = "Copied metadata and replacement PNGs. Original game data was not changed."

func _draw_preview_grid() -> void:
    var chosen: Variant = _sprite_size_for_index(sprite_size_choice.selected)
    if chosen is Dictionary:
        var width: int = int(chosen.width_squares)
        var height: int = int(chosen.height_squares)
        var active_top := GRID_ORIGIN + Vector2(0.0, (GRID_CELLS - height) * GRID_CELL)
        image_frame.draw_rect(Rect2(active_top, Vector2(width, height) * GRID_CELL),
            Color(0.22, 0.32, 0.37, 0.38), true)
    var end := GRID_ORIGIN + Vector2.ONE * GRID_CELLS * GRID_CELL
    for index in range(GRID_CELLS + 1):
        var x := GRID_ORIGIN.x + index * GRID_CELL
        var y := GRID_ORIGIN.y + index * GRID_CELL
        _draw_dotted_grid_line(Vector2(x, GRID_ORIGIN.y), Vector2(x, end.y))
        _draw_dotted_grid_line(Vector2(GRID_ORIGIN.x, y), Vector2(end.x, y))

func _draw_dotted_grid_line(start: Vector2, finish: Vector2) -> void:
    var length := start.distance_to(finish)
    var direction := (finish - start) / length
    for step in range(0, int(length), 12):
        image_frame.draw_line(start + direction * step,
            start + direction * minf(step + 5.0, length), Color(0.69, 0.81, 0.84, 0.82), 1.0)

func _preview_scale(tier: String, footprint: Vector2i, replacement: bool) -> float:
    if not replacement:
        return GRID_CELL / 24.0 * (0.7 if tier == "small" else 1.0)
    match tier:
        "small": return 1.0
        "normal": return 1.45
        "tall": return 2.1
        _: return float(maxi(footprint.x, footprint.y))

func _refresh_monster_preview_layout() -> void:
    image_frame.queue_redraw()
    if monster_image.texture == null:
        preview_status.text = "No image to measure."
        return
    var chosen: Variant = _sprite_size_for_index(sprite_size_choice.selected)
    var footprint := Vector2i(1, 1)
    if chosen is Dictionary:
        footprint = Vector2i(int(chosen.width_squares), int(chosen.height_squares))
    var tier: String = HUMANOID_SIZES[humanoid_size_choice.selected] if humanoid_size_choice.selected >= 0 else "na"
    var scale := _preview_scale(tier, footprint, preview_is_replacement)
    var image_size := (monster_image.texture.get_size() * scale).round()
    var center_x := GRID_ORIGIN.x + footprint.x * GRID_CELL * 0.5
    var ground_y := GRID_ORIGIN.y + GRID_CELLS * GRID_CELL
    monster_image.position = Vector2(roundf(center_x - image_size.x * 0.5),
        roundf(ground_y - preview_ready_bottom * scale))
    monster_image.size = image_size
    if not chosen is Dictionary:
        preview_status.text = "Sprite footprint unknown; choose a size to check its bounds."
        return
    var used := monster_image.texture.get_image().get_used_rect()
    var ink_rect := Rect2(monster_image.position + Vector2(used.position) * scale,
        Vector2(used.size) * scale)
    var active_top := GRID_ORIGIN + Vector2(0.0, (GRID_CELLS - footprint.y) * GRID_CELL)
    var active := Rect2(active_top, Vector2(footprint) * GRID_CELL)
    var over_left := maxf(0.0, active.position.x - ink_rect.position.x)
    var over_right := maxf(0.0, ink_rect.end.x - active.end.x)
    var over_top := maxf(0.0, active.position.y - ink_rect.position.y)
    var over_bottom := maxf(0.0, ink_rect.end.y - active.end.y)
    var overflow := maxf(maxf(over_left, over_right), maxf(over_top, over_bottom))
    if overflow > 1.0:
        preview_status.text = "Outside the selected %d × %d footprint by up to %.2f square(s)." % [
            footprint.x, footprint.y, overflow / GRID_CELL]
        preview_status.add_theme_color_override("font_color", Color(1.0, 0.74, 0.46))
    else:
        preview_status.text = "Image fits the selected %d × %d footprint." % [footprint.x, footprint.y]
        preview_status.add_theme_color_override("font_color", Color(0.72, 0.86, 0.78))

func _ready_bottom_for_image(image: Image) -> float:
    var used := image.get_used_rect()
    return float(used.end.y) if used.size.y > 0 else float(image.get_height())

func _ready_bottom_for_path(path: String, fallback: float) -> float:
    var ready_image := Image.new()
    return _ready_bottom_for_image(ready_image) if ready_image.load(path) == OK else fallback

func _selected_variant() -> Dictionary:
    if selected < 0 or variant_choice.selected < 0: return {}
    var record: Dictionary = draft_records[rows[selected].key]
    return record.variants[variant_choice.selected]

func _show_monster_image() -> void:
    if selected < 0: return
    var row: Dictionary = rows[selected]
    var art: Dictionary = _art_for(selected)
    var source_note := ""
    var ready_path := ""
    var variant := _selected_variant()
    if show_new_art.button_pressed and not variant.is_empty():
        var replacement_path: String = MonsterMetadata.art_path(str(row.key), str(variant.id), action, metadata_root)
        if FileAccess.file_exists(replacement_path):
            var image := Image.new()
            if image.load(replacement_path) == OK:
                # Art is edited outside the tool. Reload when the user chooses a
                # set or the pose changes so a replaced PNG appears immediately.
                monster_image.texture = ImageTexture.create_from_image(image)
                ready_path = MonsterMetadata.art_path(str(row.key), str(variant.id), false, metadata_root)
                source_note = "New art · %s · %s · %s" % [variant.label, "Action" if action else "Ready", replacement_path]
        if source_note.is_empty():
            var record: Dictionary = draft_records[row.key]
            var prior_id := str(record.get("preview_fallback_variant", ""))
            if not prior_id.is_empty():
                var prior_path: String = MonsterMetadata.art_path(str(row.key), prior_id, action, metadata_root)
                if FileAccess.file_exists(prior_path):
                    var prior_image := Image.new()
                    if prior_image.load(prior_path) == OK:
                        monster_image.texture = ImageTexture.create_from_image(prior_image)
                        ready_path = MonsterMetadata.art_path(str(row.key), prior_id, false, metadata_root)
                        source_note = "Previous new art · %s · regenerate for current gear · %s" % ["Action" if action else "Ready", prior_path]
    if source_note.is_empty():
        var original_index := int(row.get("native_index", -1))
        if original_index < 0:
            original_index = int(native_index_by_key.get(row.get("origin_key", ""), -1))
        monster_image.texture = catalog.monster_image(original_index, action) if original_index >= 0 else null
        if show_new_art.button_pressed:
            source_note = "Original fallback · no new %s image for %s. " % ["action" if action else "ready", variant.get("label", "default")]
    var uses: Array = row.get("script_alternatives", [])
    var uses_note := " · Other script icons: %s" % ", ".join(uses) if uses.size() > 1 else ""
    var linked: Array = row.get("linked_keys", [])
    var linked_note := " · Script-linked creatures: %s" % ", ".join(linked) if not linked.is_empty() else ""
    var shared_note := ""
    if row.has("identical_art_keys"):
        var shared_names: Array[String] = []
        for key in row.shared_art_user_keys:
            var name := str(rows[native_index_by_key[key]].name)
            if not shared_names.has(name): shared_names.append(name)
        shared_names.sort()
        shared_note = " · Identical originals: %s · Known users of identical art: %s · %s" % [
            ", ".join(row.identical_art_keys), ", ".join(shared_names), row.art_usage_note]
    var replacement_visible := not ready_path.is_empty()
    preview_is_replacement = replacement_visible
    if replacement_visible:
        preview_ready_bottom = _ready_bottom_for_path(ready_path, float(monster_image.texture.get_height()))
    else:
        var original_index := int(row.get("native_index", -1))
        if original_index < 0:
            original_index = int(native_index_by_key.get(row.get("origin_key", ""), -1))
        var ready_texture: Texture2D = catalog.monster_image(original_index, false) if original_index >= 0 else null
        preview_ready_bottom = _ready_bottom_for_image(ready_texture.get_image()) if ready_texture != null else 0.0
    _refresh_monster_preview_layout()
    if art.is_empty():
        monster_source.text = source_note if replacement_visible else source_note + "No supported ready/action CPIC binding. Source: %s" % row.key
    elif art.get("assembled", false):
        monster_source.text = source_note if replacement_visible else source_note + "%s · %s · %s" % ["Action" if action else "Ready", row.key, art.basis]
    else:
        monster_source.text = source_note if replacement_visible else source_note + "%s · %s record %d / %d · %s" % ["Action" if action else "Ready", art.archive, art.record + (128 if action else 0), art.record, art.basis]
    monster_source.text += uses_note + linked_note + shared_note

func _metadata_check() -> void:
    assert(not monster_id_edit.editable and monster_id_edit.select_all_on_focus)
    assert(is_equal_approx(monster_id_edit.size_flags_stretch_ratio,
        name_edit.size_flags_stretch_ratio * 2.0))
    assert(MonsterMetadata.valid_sprite_size({"width_squares": 3, "height_squares": 3}))
    assert(_sprite_size_index({"width_squares": 3, "height_squares": 3}) == 5)
    assert(_sprite_size_for_index(5) == {"width_squares": 3, "height_squares": 3})
    assert(rows.size() >= 170)
    assert(show_new_art.button_pressed)
    assert(MonsterMetadata.source_weapon_role(36, 1) == "one_hand")
    assert(MonsterMetadata.source_weapon_role(43, 2) == "ranged")
    assert(MonsterMetadata.source_weapon_role(38, 2) == "two_hand")
    var captain_found := false
    for index in range(rows.size()):
        var source_record: Dictionary = MonsterMetadata.new_record(rows[index], catalog.monster_art(index))
        if rows[index].key == "MON6CHA.DAX:102":
            captain_found = true
            assert(source_record.wealth.gold == 18 and source_record.wealth.platinum == 6)
            assert(source_record.variants.size() == 2)
            assert(source_record.variants[0].weapon_role == "one_hand")
            assert(not source_record.variants[0].shield_id.is_empty())
            assert(source_record.variants[1].weapon_role == "ranged")
            assert(source_record.variants[1].shield_id.is_empty())
    assert(captain_found)
    var duplicate_index: int = native_index_by_key["CPIC1.DAX:11"]
    var duplicate_row: Dictionary = rows[duplicate_index]
    var duplicate_record: Dictionary = MonsterMetadata.new_record(duplicate_row, _art_for(duplicate_index))
    duplicate_record.name = "My shared fighter art"
    assert(duplicate_record.equipment.is_empty())
    assert(not duplicate_record.source.direct_script_reference_found)
    assert(MonsterMetadata.save_record(duplicate_record, metadata_root).is_empty())
    var duplicate_loaded: Dictionary = MonsterMetadata.load_record(duplicate_row, _art_for(duplicate_index), metadata_root)
    assert(duplicate_loaded.error.is_empty())
    assert(duplicate_loaded.data.name == "My shared fighter art")
    assert(duplicate_loaded.data.source.identical_art_keys == duplicate_record.source.identical_art_keys)
    assert(duplicate_loaded.data.source.shared_art_creature_keys == duplicate_record.source.shared_art_creature_keys)
    assert(duplicate_loaded.data.source.linked_creature_keys.is_empty())
    duplicate_record.name = "Unidentified combat sprite"
    assert(MonsterMetadata.save_record(duplicate_record, metadata_root).is_empty())
    duplicate_loaded = MonsterMetadata.load_record(duplicate_row, _art_for(duplicate_index), metadata_root)
    assert(duplicate_loaded.data.name == duplicate_row.identified_name)
    var commandant_index: int = native_index_by_key["MON6CHA.DAX:50"]
    # Use distinct labels to exercise legacy-name migration regardless of the
    # local START.EXE word table's level of item-name detail.
    var commandant_row: Dictionary = rows[commandant_index].duplicate(true)
    commandant_row.equipment[0].label = "Potion Extra Healing"
    commandant_row.equipment[4].label = "Javelin of Lightning"
    var legacy: Dictionary = MonsterMetadata.new_record(commandant_row, _art_for(commandant_index))
    assert(legacy.equipment[0].name == "Potion Extra Healing")
    assert(legacy.equipment[4].name == "Javelin of Lightning")
    legacy.equipment[0].name = "Item type 71"
    legacy.equipment[4].name = "Item type 71"
    legacy.description = "COMMANDANT. Gear: %s." % MonsterMetadata._gear_summary(legacy.equipment)
    assert(MonsterMetadata.save_record(legacy, metadata_root).is_empty())
    var resolved: Dictionary = MonsterMetadata.load_record(commandant_row, _art_for(commandant_index), metadata_root)
    assert(resolved.error.is_empty())
    assert(resolved.data.equipment[0].name == "Potion Extra Healing")
    assert(resolved.data.equipment[4].name == "Javelin of Lightning")
    assert(resolved.data.description == "COMMANDANT. Gear: %s." % MonsterMetadata._gear_summary(resolved.data.equipment))
    var first: Dictionary = rows[0]
    var art: Dictionary = catalog.monster_art(0)
    var migration_row: Dictionary = first.duplicate(true)
    migration_row.equipment = [
        {"label": "Short sword", "type": 37, "hands": 1, "range": 0, "readied": true},
        {"label": "Long bow", "type": 43, "hands": 2, "range": 22, "readied": false},
        {"label": "Greatsword", "type": 38, "hands": 2, "range": 0, "readied": false},
        {"label": "Shield", "type": 59, "hands": 1, "range": 0, "readied": true},
    ]
    var older: Dictionary = MonsterMetadata.new_record(migration_row, art)
    for item in older.equipment:
        item.erase("weapon_role")
        item.erase("source_hands")
        item.erase("source_range")
    assert(MonsterMetadata.save_record(older, metadata_root).is_empty())
    var migrated: Dictionary = MonsterMetadata.load_record(migration_row, art, metadata_root)
    assert(migrated.error.is_empty())
    assert(migrated.data.equipment[0].weapon_role == "one_hand")
    assert(migrated.data.equipment[1].weapon_role == "ranged")
    assert(migrated.data.equipment[2].weapon_role == "two_hand")
    assert(migrated.data.variants.size() == 3)
    var sample: Dictionary = MonsterMetadata.new_record(first, art)
    sample.sprite_size = {"width_squares": 3, "height_squares": 3}
    sample.humanoid_size = "tall"
    sample.equipment = [
        {"id": "w1", "kind": "weapon", "weapon_role": "one_hand", "name": "Sword", "source_type": 36, "readied": true},
        {"id": "w2", "kind": "weapon", "weapon_role": "ranged", "name": "Long bow", "source_type": 43, "readied": false},
        {"id": "w3", "kind": "weapon", "weapon_role": "two_hand", "name": "Greatsword", "source_type": 38, "readied": false},
        {"id": "s1", "kind": "shield", "name": "Round shield", "source_type": 59, "readied": true},
        {"id": "s2", "kind": "shield", "name": "Kite shield", "source_type": -1, "readied": false},
        {"id": "g1", "kind": "gear", "name": "Cloak", "source_type": -1, "readied": false},
    ]
    MonsterMetadata.rebuild_variants(sample)
    assert(sample.variants.size() == 4)
    assert(sample.variants[0].id == "w1__s1" and sample.variants[1].id == "w1__s2")
    assert(sample.variants[0].shield_id == "s1" and sample.variants[1].shield_id == "s2")
    assert(sample.variants[2].id == "w2__ranged" and sample.variants[2].shield_id.is_empty())
    assert(sample.variants[3].id == "w3__two_hand" and sample.variants[3].shield_id.is_empty())
    var ranged_id: String = sample.variants[2].id
    sample.description = "Original, independently described creature."
    sample.flag_for_regeneration = true
    assert(MonsterMetadata.save_record(sample, metadata_root).is_empty())
    var loaded: Dictionary = MonsterMetadata.load_record(first, art, metadata_root)
    assert(loaded.error.is_empty())
    assert(loaded.data.description == sample.description and loaded.data.flag_for_regeneration)
    assert(_sprite_size_index(loaded.data.sprite_size) == 5)
    assert(loaded.data.humanoid_size == "tall")
    assert(loaded.data.equipment.size() == 6 and loaded.data.variants.size() == 4)
    assert(loaded.data.variants[2].id == ranged_id)
    sample.equipment = [sample.equipment[0], sample.equipment[5]]
    MonsterMetadata.rebuild_variants(sample)
    assert(sample.variants.size() == 1 and sample.variants[0].id == "w1__one_hand")
    assert(sample.variants[0].label == "Sword" and sample.variants[0].shield_id.is_empty())
    sample.equipment = [
        {"id": "s1", "kind": "shield", "name": "Round shield", "source_type": 59, "readied": true}]
    MonsterMetadata.rebuild_variants(sample)
    assert(sample.variants.size() == 1 and sample.variants[0].id == "default")
    sample.equipment = []
    MonsterMetadata.rebuild_variants(sample)
    assert(sample.variants.size() == 1 and sample.variants[0].id == "default")

    # Draft changes survive switching records; the selector follows edited gear.
    _select_monster(0)
    assert(monster_id_edit.text == str(first.key))
    description_edit.text = "Unsaved draft"
    sprite_size_choice.select(2)
    humanoid_size_choice.select(1)
    height_edit.value = 3.5
    regeneration_flag.button_pressed = true
    _select_monster(1)
    _select_monster(0)
    assert(description_edit.text == "Unsaved draft" and regeneration_flag.button_pressed)
    assert(sprite_size_choice.selected == 2)
    assert(humanoid_size_choice.selected == 1)
    save_button.pressed.emit()
    var saved_draft: Dictionary = MonsterMetadata.load_record(first, art, metadata_root)
    assert(saved_draft.error.is_empty())
    assert(saved_draft.data.description == "Unsaved draft" and saved_draft.data.flag_for_regeneration)
    assert(_sprite_size_index(saved_draft.data.sprite_size) == 2)
    assert(saved_draft.data.humanoid_size == "small")
    assert(is_equal_approx(float(saved_draft.data.height_feet), 3.5))
    _select_monster(1)
    draft_records[first.key] = loaded.data
    _select_monster(0)
    assert(variant_choice.item_count == 4)
    var bow_kind: Button = gear_rows.get_child(1).get_child(0)
    assert(bow_kind.get_meta("role") == "ranged")
    bow_kind.pressed.emit()
    assert(bow_kind.get_meta("role") == "shield")
    _set_gear_role(bow_kind, "one_hand")
    _gear_changed()
    assert(variant_choice.item_count == 5)
    _set_gear_role(bow_kind, "ranged")
    _gear_changed()
    assert(variant_choice.item_count == 4)
    # Add item takes an original type, which the game fights with and drops.
    var types: Array = catalog.item_types()
    assert(types.any(func(t): return int(t.type) == 36 and str(t.name) == "Long sword"))
    assert(not types.any(func(t): return int(t.type) == 28))
    _add_gear(36)
    _store_draft()
    var added: Dictionary = draft_records[first.key].equipment.back()
    assert(added.source_type == 36 and added.kind == "weapon" and added.weapon_role == "one_hand")
    assert(added.name == "Long sword" and MonsterExport._source_index(added) == -1)
    var added_line: Node = gear_rows.get_child(gear_rows.get_child_count() - 1)
    gear_rows.remove_child(added_line)
    added_line.free()
    _gear_changed()
    assert(variant_choice.item_count == 4)
    var equipment_before: String = JSON.stringify(draft_records[first.key].equipment)
    variant_choice.select(2)
    var image := Image.create_empty(2, 2, false, Image.FORMAT_RGBA8)
    image.fill(Color(0.8, 0.2, 0.1))
    var ready_path: String = MonsterMetadata.art_path(first.key, ranged_id, false, metadata_root)
    assert(DirAccess.make_dir_recursive_absolute(ready_path.get_base_dir()) == OK)
    assert(image.save_png(ready_path) == OK)
    var pair_path: String = MonsterMetadata.art_path(first.key, "w1__s1", false, metadata_root)
    assert(DirAccess.make_dir_recursive_absolute(pair_path.get_base_dir()) == OK)
    image.fill(Color(0.1, 0.8, 0.2))
    assert(image.save_png(pair_path) == OK)
    var pair_action_path: String = MonsterMetadata.art_path(first.key, "w1__s1", true, metadata_root)
    image.fill(Color(0.1, 0.2, 0.8))
    assert(image.save_png(pair_action_path) == OK)
    var ranged_action_path: String = MonsterMetadata.art_path(first.key, ranged_id, true, metadata_root)
    image.fill(Color(0.8, 0.8, 0.1))
    assert(image.save_png(ranged_action_path) == OK)
    variant_choice.item_selected.emit(2)
    assert(monster_source.text.begins_with("New art"))
    assert(monster_source.text.contains("Long bow"))
    var pixel: Color = monster_image.texture.get_image().get_pixel(0, 0)
    assert(pixel.r > 0.7 and pixel.g < 0.3)
    variant_choice.select(0)
    variant_choice.item_selected.emit(0)
    assert(monster_source.text.begins_with("New art"))
    assert(monster_source.text.contains("Sword + Round shield"))
    pixel = monster_image.texture.get_image().get_pixel(0, 0)
    assert(pixel.g > 0.7 and pixel.b < 0.3)
    assert(JSON.stringify(draft_records[first.key].equipment) == equipment_before)
    action = true
    _show_monster_image()
    pixel = monster_image.texture.get_image().get_pixel(0, 0)
    assert(pixel.b > 0.7 and pixel.g < 0.3)
    variant_choice.select(2)
    variant_choice.item_selected.emit(2)
    pixel = monster_image.texture.get_image().get_pixel(0, 0)
    assert(pixel.r > 0.7 and pixel.g > 0.7 and pixel.b < 0.3)
    image.fill(Color(0.7, 0.1, 0.7))
    assert(image.save_png(ranged_action_path) == OK)
    _show_monster_image()
    pixel = monster_image.texture.get_image().get_pixel(0, 0)
    assert(pixel.r > 0.6 and pixel.g < 0.3 and pixel.b > 0.6)
    assert(DirAccess.remove_absolute(ranged_action_path) == OK)
    _show_monster_image()
    assert(monster_source.text.begins_with("Original fallback"))
    assert(monster_source.text.contains("Long bow"))
    assert(JSON.stringify(draft_records[first.key].equipment) == equipment_before)
    variant_choice.select(0)
    variant_choice.item_selected.emit(0)
    var removed_shield: HBoxContainer = gear_rows.get_child(3)
    gear_rows.remove_child(removed_shield)
    removed_shield.queue_free()
    _gear_changed()
    assert(draft_records[first.key].preview_fallback_variant == "w1__s1")
    assert(monster_source.text.begins_with("Previous new art"))
    pixel = monster_image.texture.get_image().get_pixel(0, 0)
    assert(pixel.b > 0.7 and pixel.g < 0.3)
    save_button.pressed.emit()
    var edited: Dictionary = MonsterMetadata.load_record(first, art, metadata_root)
    assert(edited.error.is_empty() and edited.data.preview_fallback_variant == "w1__s1")
    name_edit.text = "Renamed test monster"
    _rename_monster()
    edited = MonsterMetadata.load_record(first, art, metadata_root)
    assert(edited.error.is_empty() and edited.data.name == "Renamed test monster")
    name_edit.text = "Copied test monster"
    _copy_monster()
    assert(str(rows[selected].key).begins_with("CUSTOM:"))
    assert(monster_id_edit.text == str(rows[selected].key))
    assert(rows[selected].name == "Copied test monster")
    assert(FileAccess.file_exists(MonsterMetadata.art_path(str(rows[selected].key), "w1__s1", true, metadata_root)))
    assert(monster_source.text.begins_with("Previous new art"))
    var reloaded: Dictionary = MonsterMetadata.load_record(rows[selected], _art_for(selected), metadata_root)
    assert(reloaded.error.is_empty() and reloaded.data.name == "Copied test monster")
    assert(reloaded.data.source.origin_key == first.key and reloaded.data.equipment.size() == 5)
    assert(_sprite_size_index(reloaded.data.sprite_size) == 5)
    assert(reloaded.data.humanoid_size == "tall")
    assert(reloaded.data.height_feet == null)
    assert(reloaded.data.wealth == first.wealth)
    var copied_key := str(rows[selected].key)
    var art_image := Image.create(128, 128, false, Image.FORMAT_RGBA8)
    art_image.fill(Color(0.2, 0.4, 0.8))
    for variant in reloaded.data.variants:
        for pose in ["ready", "action"]:
            var path := metadata_root.path_join(str(variant[pose]))
            DirAccess.make_dir_recursive_absolute(path.get_base_dir())
            assert(art_image.save_png(path) == OK)
    var unflagged: Dictionary = MonsterMetadata.load_record(rows[selected], _art_for(selected), metadata_root).data
    unflagged.flag_for_regeneration = false
    assert(MonsterMetadata.save_record(unflagged, metadata_root).is_empty())
    var exported := metadata_root.path_join("export")
    assert(MonsterExport.export_all(metadata_root, exported).is_empty())
    assert(MonsterExport.up_to_date(metadata_root, exported))
    var copy_folder := exported.path_join(MonsterMetadata.record_id(copied_key))
    var copy_export: Variant = JSON.parse_string(FileAccess.get_file_as_string(copy_folder.path_join("monster.json")))
    assert(copy_export is Dictionary and copy_export.name == "Copied test monster")
    assert(copy_export.origin_key == first.key and copy_export.equipment.size() == 5)
    assert(not str(FileAccess.get_file_as_string(copy_folder.path_join("monster.json"))).contains("dev_tools"))
    assert(copy_export.variants.size() == reloaded.data.variants.size())
    assert(FileAccess.file_exists(copy_folder.path_join(str(copy_export.variants[0].ready))))
    var flagged: Dictionary = MonsterMetadata.load_record(rows[selected], _art_for(selected), metadata_root).data
    flagged.flag_for_regeneration = true
    assert(MonsterMetadata.save_record(flagged, metadata_root).is_empty())
    assert(not MonsterExport.up_to_date(metadata_root, exported))
    assert(MonsterExport.export_all(metadata_root, exported).is_empty())
    copy_export = JSON.parse_string(FileAccess.get_file_as_string(copy_folder.path_join("monster.json")))
    assert(copy_export.variants.is_empty() and DirAccess.get_directories_at(copy_folder).is_empty())
    show_new_art.button_pressed = false
    _remove_check_tree(metadata_root)
    print("Monster metadata checks passed: JSON save/load, rename/copy with PNGs, gear-edit preview retention, weapon sets, original fallback, export to game")
    get_tree().quit()

func _remove_check_tree(path: String) -> void:
    var directory := DirAccess.open(path)
    if directory == null: return
    directory.list_dir_begin()
    while true:
        var name := directory.get_next()
        if name.is_empty(): break
        var child := path.path_join(name)
        if directory.current_is_dir():
            _remove_check_tree(child)
        else:
            DirAccess.remove_absolute(child)
    directory.list_dir_end()
    DirAccess.remove_absolute(path)

func _catalog_check() -> void:
    assert(_preview_scale("small", Vector2i.ONE, true) < _preview_scale("normal", Vector2i.ONE, true))
    assert(_preview_scale("normal", Vector2i.ONE, true) < _preview_scale("tall", Vector2i.ONE, true))
    assert(image_frame.custom_minimum_size == Vector2(512, 416))
    assert(rows.size() >= 170)
    assert(monster_list.item_count == rows.size())
    var mad_man: Dictionary = catalog.monster_art(native_index_by_key["MON2CHA.DAX:25"])
    assert(mad_man.archive == "CPIC2.DAX" and mad_man.record == 11)
    var skullcrusher: Dictionary = catalog.monster_art(native_index_by_key["MON4CHA.DAX:27"])
    assert(skullcrusher.archive == "CPIC4.DAX" and skullcrusher.record == 11)
    assert(catalog.monster_art(native_index_by_key["MON3CHA.DAX:107"]).get("assembled", false))
    assert(str(rows[native_index_by_key["CPIC4.DAX:11"]].name).contains("SKULLCRUSHER"))
    assert(not rows[native_index_by_key["MON8CHA.DAX:104"]].equipment.is_empty())
    var norris_icon_index: int = native_index_by_key["CPIC8.DAX:7"]
    var norris_icon: Dictionary = MonsterMetadata.load_record(rows[norris_icon_index], _art_for(norris_icon_index), metadata_root)
    assert(norris_icon.error.is_empty() and norris_icon.data.name.contains("NORRIS"))
    assert(norris_icon.data.equipment.size() == 3)
    assert(rows[native_index_by_key["MON2CHA.DAX:55"]].equipment[0].label == "Boulder")
    var duplicate_examples := {
        "CPIC1.DAX:11": ["CPIC2.DAX:11", "MON2CHA.DAX:25"],
        "CPIC4.DAX:14": ["CPIC5.DAX:14", "MON5CHA.DAX:88"],
        "CPIC6.DAX:4": ["CPIC1.DAX:4", "MON1CHA.DAX:4"],
        "CPIC6.DAX:12": ["CPIC5.DAX:12", "MON5CHA.DAX:45"],
        "CPIC6.DAX:13": ["CPIC5.DAX:13", "MON5CHA.DAX:90"],
    }
    for key in duplicate_examples:
        var duplicate_index: int = native_index_by_key[key]
        var row: Dictionary = rows[duplicate_index]
        assert(row.name != "Unidentified combat sprite")
        assert(row.identical_art_keys.has(duplicate_examples[key][0]))
        assert(row.shared_art_user_keys.has(duplicate_examples[key][1]))
        assert(not row.has("linked_keys") and not used.has(key))
        assert(row.equipment.is_empty())
        var art: Dictionary = _art_for(duplicate_index)
        assert("%s:%d" % [art.archive, art.record] == key)
        _select_monster(visible_rows.find(duplicate_index))
        assert(monster_list.get_item_text(visible_rows.find(duplicate_index)).begins_with("[Unused]"))
        assert(monster_source.text.contains("Known users of identical art:"))
        assert(monster_source.text.contains("No direct script reference found for this archive copy."))
        assert(gear_source_hint.text.begins_with("Shared artwork."))
    for group in catalog.identical_art_groups():
        # The same record ID in two archives can contain different images.
        assert(not (group.has("CPIC3.DAX:24") and group.has("CPIC4.DAX:24")))
    for index in range(rows.size()):
        var row: Dictionary = rows[index]
        if int(row.get("native_index", -1)) < 0: continue
        assert(str(row.name) != "Unidentified combat sprite")
        for item in row.equipment:
            assert(not str(item.label).begins_with("Item type "))
        var metadata: Dictionary = MonsterMetadata.load_record(row, _art_for(index), metadata_root)
        assert(metadata.error.is_empty())
        assert(metadata.data.has("sprite_size") and metadata.data.sprite_size != null)
        assert(MonsterMetadata.valid_sprite_size(metadata.data.sprite_size))
        assert(MonsterMetadata.valid_humanoid_size(metadata.data.get("humanoid_size", null)))
        if metadata.data.humanoid_size == "small":
            assert(MonsterMetadata.valid_small_height(metadata.data.get("height_feet", null)))
        if row.key == "MON1CHA.DAX:2": assert(metadata.data.humanoid_size == "small")
        if row.key == "MON1CHA.DAX:41": assert(metadata.data.humanoid_size == "normal")
        if row.key == "MON2CHA.DAX:55": assert(metadata.data.humanoid_size == "tall")
        if row.key == "CPIC4.DAX:37": assert(metadata.data.humanoid_size == "na")
        assert(metadata.data.wealth == row.get("wealth", {}))
        assert(str(metadata.data.description).length() >= 80)
        for item in metadata.data.equipment:
            assert(not str(item.name).begins_with("Item type "))
        assert(not str(metadata.data.description).contains("Item type "))
    var script_bound := 0
    for row in rows:
        if not str(row.key).begins_with("MON") or not used.has(row.key): continue
        if row.key == "MON3CHA.DAX:107": continue # SETUP MONSTER has no icon operand.
        assert(str(catalog.monster_art(int(row.native_index)).basis).begins_with("ECL LOAD MONSTER"))
        script_bound += 1
    assert(script_bound == 124)
    var pictured := -1
    for index in range(rows.size()):
        if not _art_for(index).is_empty():
            pictured = index
            break
    assert(pictured >= 0)
    assert(catalog.monster_image(int(rows[pictured].native_index), false) != null)
    assert(catalog.monster_image(int(rows[pictured].native_index), true) != null)
    for index in range(rows.size()):
        if str(rows[index].name).to_upper() == "MEDUSA":
            _select_monster(index)
            assert(monster_image.texture != null)
            var ready_position := monster_image.position
            var ready_size := monster_image.size
            action = true
            _show_monster_image()
            assert(monster_image.texture != null)
            assert(monster_image.position == ready_position and monster_image.size == ready_size)
            break
    _select_monster(pictured)
    assert(GRID_ORIGIN == Vector2(96.0, 16.0) and GRID_CELLS == 3)
    assert(not rows[pictured].equipment.is_empty() or rows.size() > 0)
    hide_unused.button_pressed = true
    assert(monster_list.item_count < rows.size())
    assert(monster_list.item_count > 0)
    hide_unused.button_pressed = false
    assert(monster_list.item_count == rows.size())
    var name_error := _check_name_filter()
    if not name_error.is_empty():
        push_error(name_error)
        get_tree().quit(1)
        return
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
        if _art_for(index).is_empty(): unresolved += 1
        var original_index := int(rows[index].get("native_index", -1))
        if original_index < 0:
            original_index = int(native_index_by_key.get(rows[index].get("origin_key", ""), -1))
        assert(original_index >= 0)
        assert(catalog.monster_image(original_index, false) != null)
        assert(catalog.monster_image(original_index, true) != null)
    if not MonsterExport.up_to_date():
        push_error("data/monsters is out of date: run --export-to-game")
        get_tree().quit(1)
        return
    print("Combat sprite catalog checks passed: %d rows, %d referenced, %d missing original previews, ECL NPC icons and gear, name filter, all weapon and shield previews" % [rows.size(), referenced, unresolved])
    get_tree().quit()

func _check_name_filter() -> String:
    name_filter.text = "gObLiN"
    name_filter.text_changed.emit(name_filter.text)
    if visible_rows.is_empty() or visible_rows.size() >= rows.size():
        return "Name substring filter did not narrow the monster list"
    for index in visible_rows:
        if str(rows[index].name).findn("gObLiN") < 0:
            return "Name filter included a nonmatching monster"
    hide_unused.button_pressed = true
    for index in visible_rows:
        if not used.has(rows[index].key):
            return "Name and unused filters did not combine"
    hide_unused.button_pressed = false
    name_filter.text = "no-monster-has-this-name"
    name_filter.text_changed.emit(name_filter.text)
    if not visible_rows.is_empty() or monster_image.texture != null or not monster_source.text.is_empty():
        return "No-match search left a monster preview visible"
    name_filter.text = ""
    name_filter.text_changed.emit(name_filter.text)
    if monster_list.item_count != rows.size() or selected < 0:
        return "Clearing the name filter did not restore the monster list"
    return ""
