extends Control

const Evidence = preload("res://scripts/art_script_evidence.gd")
const MonsterMetadata = preload("res://scripts/monster_metadata.gd")
const GEAR_ROLES := ["one_hand", "two_hand", "ranged", "shield", "gear"]

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
var monster_image: TextureRect
var monster_source: Label
var description_edit: TextEdit
var gear_rows: VBoxContainer
var gear_source_hint: Label
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

func _art_for(index: int) -> Dictionary:
    var row: Dictionary = rows[index]
    if int(row.get("native_index", -1)) >= 0:
        var art: Dictionary = catalog.monster_art(int(row.native_index))
        if row.has("linked_keys"):
            art.basis = "ECL LOAD MONSTER icon shared by %d creature%s" % [row.linked_keys.size(), "s" if row.linked_keys.size() != 1 else ""]
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
    name_edit.placeholder_text = "Monster or NPC name"
    identity.add_child(name_edit)
    var rename_button := Button.new()
    rename_button.text = "Rename"
    rename_button.tooltip_text = "Save this name for the selected catalog record"
    rename_button.pressed.connect(_rename_monster)
    identity.add_child(rename_button)
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
    preview.custom_minimum_size.x = 400
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
    var image_frame := Control.new()
    image_frame.custom_minimum_size = Vector2(360, 390)
    image_frame.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    image_frame.size_flags_vertical = Control.SIZE_EXPAND_FILL
    preview.add_child(image_frame)
    monster_image = TextureRect.new()
    monster_image.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    monster_image.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    monster_image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    monster_image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    image_frame.add_child(monster_image)
    monster_source = Label.new()
    monster_source.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    monster_source.custom_minimum_size.y = 80
    preview.add_child(monster_source)

    var editor := VBoxContainer.new()
    editor.custom_minimum_size.x = 400
    editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    columns.add_child(editor)
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
    var add_gear := Button.new()
    add_gear.text = "Add weapon or gear"
    add_gear.pressed.connect(_add_gear)
    editor.add_child(add_gear)
    regeneration_flag = CheckButton.new()
    regeneration_flag.text = "Flag for regeneration"
    editor.add_child(regeneration_flag)
    save_button = Button.new()
    save_button.text = "Save monster data"
    save_button.pressed.connect(_save_monster)
    editor.add_child(save_button)
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
        monster_image.offset_top = 0.0
        monster_source.text = ""
        name_edit.text = ""
        variant_choice.clear()
        description_edit.text = ""
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
    # The Medusa icon sits at the top edge of its source frame. Lower its
    # presentation within the existing preview area so the head has breathing room.
    monster_image.offset_top = 64.0 if str(row.name).to_upper() == "MEDUSA" else 0.0
    if not draft_records.has(row.key):
        var loaded: Dictionary = MonsterMetadata.load_record(row, _art_for(selected), metadata_root)
        draft_records[row.key] = loaded.data
        if not loaded.error.is_empty(): load_errors[row.key] = loaded.error
    metadata_status.text = str(load_errors.get(row.key, ""))
    var record: Dictionary = draft_records[row.key]
    row.name = record.name
    monster_title.text = "%s%s" % [row.name, "  ·  Unused" if not used.has(row.key) else ""]
    name_edit.text = record.name
    if row.has("linked_keys"):
        gear_source_hint.text = "Gear from %s" % row.linked_keys[0] if row.linked_keys.size() == 1 else \
            "Shared icon. Select a named creature to edit its gear."
    elif str(row.key).begins_with("CUSTOM:"):
        gear_source_hint.text = "Independent copied gear. Original source: %s" % row.origin_key
    elif row.equipment.is_empty():
        gear_source_hint.text = "No items recorded for this creature in the original MONnITM archive."
    else:
        gear_source_hint.text = "Original gear loaded from the matching MONnITM record."
    description_edit.text = record.description
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

func _add_gear() -> void:
    if selected < 0: return
    var item_id := "added_%d" % Time.get_ticks_usec()
    _append_gear_row({"id": item_id, "source_type": -1, "source_hands": -1,
        "source_range": -1, "kind": "gear", "weapon_role": "", "name": "", "readied": false})
    _gear_changed()
    var line: HBoxContainer = gear_rows.get_child(gear_rows.get_child_count() - 1)
    line.get_child(1).grab_focus()

func _store_draft() -> void:
    if selected < 0 or not draft_records.has(rows[selected].key): return
    var record: Dictionary = draft_records[rows[selected].key]
    record.description = description_edit.text
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

func _selected_variant() -> Dictionary:
    if selected < 0 or variant_choice.selected < 0: return {}
    var record: Dictionary = draft_records[rows[selected].key]
    return record.variants[variant_choice.selected]

func _show_monster_image() -> void:
    if selected < 0: return
    var row: Dictionary = rows[selected]
    var art: Dictionary = _art_for(selected)
    var source_note := ""
    var variant := _selected_variant()
    if show_new_art.button_pressed and not variant.is_empty():
        var replacement_path: String = MonsterMetadata.art_path(str(row.key), str(variant.id), action, metadata_root)
        if FileAccess.file_exists(replacement_path):
            var image := Image.new()
            if image.load(replacement_path) == OK:
                # Art is edited outside the tool. Reload when the user chooses a
                # set or the pose changes so a replaced PNG appears immediately.
                monster_image.texture = ImageTexture.create_from_image(image)
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
    var replacement_visible := source_note.begins_with("New art") or source_note.begins_with("Previous new art")
    if art.is_empty():
        monster_source.text = source_note if replacement_visible else source_note + "No supported ready/action CPIC binding. Source: %s" % row.key
    elif art.get("assembled", false):
        monster_source.text = source_note if replacement_visible else source_note + "%s · %s · %s" % ["Action" if action else "Ready", row.key, art.basis]
    else:
        monster_source.text = source_note if replacement_visible else source_note + "%s · %s record %d / %d · %s" % ["Action" if action else "Ready", art.archive, art.record + (128 if action else 0), art.record, art.basis]
    monster_source.text += uses_note + linked_note

func _metadata_check() -> void:
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
            assert(source_record.variants.size() == 2)
            assert(source_record.variants[0].weapon_role == "one_hand")
            assert(not source_record.variants[0].shield_id.is_empty())
            assert(source_record.variants[1].weapon_role == "ranged")
            assert(source_record.variants[1].shield_id.is_empty())
    assert(captain_found)
    var commandant_index: int = native_index_by_key["MON6CHA.DAX:50"]
    var commandant_row: Dictionary = rows[commandant_index]
    var legacy: Dictionary = MonsterMetadata.new_record(commandant_row, _art_for(commandant_index))
    assert(legacy.equipment[0].name == "Potion Extra Healing")
    assert(legacy.equipment[4].name == "Javelin of Lightning")
    legacy.equipment[0].name = "Item type 71"
    legacy.equipment[4].name = "Item type 71"
    legacy.description = "COMMANDANT. Gear: 2 × Item type 71, Ring of Fire Resistance, Shield +1, Plate armor +2, Long sword +2."
    assert(MonsterMetadata.save_record(legacy, metadata_root).is_empty())
    var resolved: Dictionary = MonsterMetadata.load_record(commandant_row, _art_for(commandant_index), metadata_root)
    assert(resolved.error.is_empty())
    assert(resolved.data.equipment[0].name == "Potion Extra Healing")
    assert(resolved.data.equipment[4].name == "Javelin of Lightning")
    assert(resolved.data.description == "COMMANDANT. Gear: Potion Extra Healing, Ring of Fire Resistance, Shield +1, Plate armor +2, Javelin of Lightning, Long sword +2.")
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
    description_edit.text = "Unsaved draft"
    regeneration_flag.button_pressed = true
    _select_monster(1)
    _select_monster(0)
    assert(description_edit.text == "Unsaved draft" and regeneration_flag.button_pressed)
    save_button.pressed.emit()
    var saved_draft: Dictionary = MonsterMetadata.load_record(first, art, metadata_root)
    assert(saved_draft.error.is_empty())
    assert(saved_draft.data.description == "Unsaved draft" and saved_draft.data.flag_for_regeneration)
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
    assert(rows[selected].name == "Copied test monster")
    assert(FileAccess.file_exists(MonsterMetadata.art_path(str(rows[selected].key), "w1__s1", true, metadata_root)))
    assert(monster_source.text.begins_with("Previous new art"))
    var reloaded: Dictionary = MonsterMetadata.load_record(rows[selected], _art_for(selected), metadata_root)
    assert(reloaded.error.is_empty() and reloaded.data.name == "Copied test monster")
    assert(reloaded.data.source.origin_key == first.key and reloaded.data.equipment.size() == 5)
    show_new_art.button_pressed = false
    _remove_check_tree(metadata_root)
    print("Monster metadata checks passed: JSON save/load, rename/copy with PNGs, gear-edit preview retention, weapon sets, original fallback")
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
    for index in range(rows.size()):
        var row: Dictionary = rows[index]
        if int(row.get("native_index", -1)) < 0: continue
        for item in row.equipment:
            assert(not str(item.label).begins_with("Item type "))
        var metadata: Dictionary = MonsterMetadata.load_record(row, _art_for(index), metadata_root)
        assert(metadata.error.is_empty())
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
            assert(monster_image.offset_top == 64.0)
            assert(monster_image.texture != null)
            action = true
            _show_monster_image()
            assert(monster_image.texture != null)
            break
    _select_monster(pictured)
    assert(monster_image.offset_top == 0.0)
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
