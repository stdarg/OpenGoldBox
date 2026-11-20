extends Control

const CATALOG := "res://../../data/art/combat-body-looks.tsv"
const REVIEW_BODY_COUNT := 35
const DAGGER_SOURCES := {33: 7, 34: 24}

const OPTIONS := "res://../../data/art/combat-weapon-options.tsv"

var catalog_path := CATALOG
var options_path := OPTIONS
var deleted: Array[String] = []
var tabs: TabContainer
var report_lists: Array[VBoxContainer] = []
var report_filters: Array[LineEdit] = []
var report_counts: Array[Label] = []
var loaded := false
var body_id := 0
var assignments: Array = []
var looks: Array = []
var body_data: PackedByteArray
var head_data: PackedByteArray
var loader := DaxSpriteLoader.new()
var number: Label
var assignment: Label
var status: Label
var filter_box: LineEdit
var list: VBoxContainer
var summary: RichTextLabel
var previews: Array[TextureRect] = []

func _ready() -> void:
    _build_ui()
    if not _load_catalog(): return
    var folder := GameFinder.game_folder("POOLRAD")
    if folder.is_empty():
        _fail("Pool of Radiance's files were not found; set OPENGOLD_GAME_DIR to their folder.")
        return
    body_data = FileAccess.get_file_as_bytes(folder.path_join("CBODY.DAX"))
    head_data = FileAccess.get_file_as_bytes(folder.path_join("CHEAD.DAX"))
    if body_data.is_empty() or head_data.is_empty():
        _fail("Cannot read CBODY.DAX or CHEAD.DAX in " + folder)
        return
    _refresh()
    status.text = "Changes save immediately to " + ProjectSettings.globalize_path(catalog_path)

func _build_ui() -> void:
    var background := ColorRect.new()
    background.color = Color("17232d")
    background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    add_child(background)
    var panel := MarginContainer.new()
    panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    panel.add_theme_constant_override("margin_left", 24)
    panel.add_theme_constant_override("margin_right", 24)
    panel.add_theme_constant_override("margin_top", 18)
    panel.add_theme_constant_override("margin_bottom", 18)
    add_child(panel)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 12)
    panel.add_child(column)
    var title := Label.new()
    title.text = "Combat body assignments"
    title.add_theme_font_size_override("font_size", 28)
    column.add_child(title)
    var outer := column
    tabs = TabContainer.new()
    tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
    outer.add_child(tabs)
    column = VBoxContainer.new()
    column.name = "Review & Assign"
    column.add_theme_constant_override("separation", 8)
    tabs.add_child(column)
    var nav := HBoxContainer.new()
    column.add_child(nav)
    var previous := Button.new()
    previous.text = "◀ Previous body"
    previous.pressed.connect(func(): _step(-1))
    nav.add_child(previous)
    number = Label.new()
    number.custom_minimum_size.x = 180
    number.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    nav.add_child(number)
    var next := Button.new()
    next.text = "Next body ▶"
    next.pressed.connect(func(): _step(1))
    nav.add_child(next)
    assignment = Label.new()
    column.add_child(assignment)
    var content := HBoxContainer.new()
    content.size_flags_vertical = Control.SIZE_EXPAND_FILL
    content.add_theme_constant_override("separation", 24)
    column.add_child(content)
    var preview_grid := GridContainer.new()
    preview_grid.columns = 2
    preview_grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    content.add_child(preview_grid)
    for label_text in ["Short · Ready", "Tall · Ready", "Short · Action", "Tall · Action"]:
        var cell := VBoxContainer.new()
        cell.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        preview_grid.add_child(cell)
        var label := Label.new()
        label.text = label_text
        cell.add_child(label)
        var texture := TextureRect.new()
        texture.custom_minimum_size = Vector2(180, 180)
        texture.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        texture.size_flags_vertical = Control.SIZE_EXPAND_FILL
        texture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        texture.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
        texture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
        cell.add_child(texture)
        previews.append(texture)
    var selection := VBoxContainer.new()
    selection.custom_minimum_size.x = 410
    selection.size_flags_vertical = Control.SIZE_EXPAND_FILL
    content.add_child(selection)
    var list_label := Label.new()
    list_label.text = "Equipment combinations"
    selection.add_child(list_label)
    filter_box = LineEdit.new()
    filter_box.placeholder_text = "Filter looks (e.g. mace or shield)"
    filter_box.text_changed.connect(func(_value): _fill_list())
    selection.add_child(filter_box)
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    selection.add_child(scroll)
    list = VBoxContainer.new()
    list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    scroll.add_child(list)
    var summary_label := Label.new()
    summary_label.text = "All assignments for this body (including filtered out)"
    selection.add_child(summary_label)
    summary = RichTextLabel.new()
    summary.custom_minimum_size.y = 110
    selection.add_child(summary)
    for tab_name in ["Unassigned", "Multiple Assignments", "Manage Combinations"]:
        var page := VBoxContainer.new()
        page.name = tab_name
        page.add_theme_constant_override("separation", 10)
        tabs.add_child(page)
        var explanation := Label.new()
        explanation.text = {
            "Unassigned": "Combinations with no body assigned.",
            "Multiple Assignments": "Combinations assigned to several bodies. Select a body to review it.",
            "Manage Combinations": "Delete invalid combinations from every list and all bodies. Changes save immediately."
        }[tab_name]
        page.add_child(explanation)
        var search := LineEdit.new()
        search.placeholder_text = "Filter combinations"
        search.text_changed.connect(func(_text): _refresh_reports())
        page.add_child(search)
        report_filters.append(search)
        var count := Label.new()
        page.add_child(count)
        report_counts.append(count)
        var report_scroll := ScrollContainer.new()
        report_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
        report_scroll.follow_focus = true
        page.add_child(report_scroll)
        var rows := VBoxContainer.new()
        rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        report_scroll.add_child(rows)
        report_lists.append(rows)
    status = Label.new()
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    outer.add_child(status)

func _load_catalog() -> bool:
    if not FileAccess.file_exists(options_path):
        _fail("Missing look options: " + ProjectSettings.globalize_path(options_path))
        return false
    var ids := {"unreviewed": true}
    for line in FileAccess.get_file_as_string(options_path).split("\n"):
        if line.is_empty() or line.begins_with("#"): continue
        var fields := line.strip_edges().split("\t")
        if fields.size() != 4 or not fields[2].is_valid_int() or fields[0].is_empty() or fields[1].is_empty() or fields[3] != "ordinary":
            _fail("Invalid look option: " + line)
            return false
        var id: String = fields[0]
        if ids.has(id) or ids.has(id + "_shield"):
            _fail("Duplicate look option: " + id)
            return false
        ids[id] = true
        ids[id + "_shield"] = true
        looks.append([id, fields[1]])
        looks.append([id + "_shield", fields[1] + " & Shield"])
    if not FileAccess.file_exists(catalog_path):
        _fail("Missing catalog: " + ProjectSettings.globalize_path(catalog_path))
        return false
    assignments.resize(REVIEW_BODY_COUNT)
    for id in range(REVIEW_BODY_COUNT): assignments[id] = []
    var seen := {}
    deleted.clear()
    for line in FileAccess.get_file_as_string(catalog_path).split("\n"):
        if line.is_empty() or line.begins_with("#"): continue
        var fields := line.strip_edges().split("\t")
        if fields.size() == 2 and fields[0] == "deleted":
            if not _known(fields[1]) or deleted.has(fields[1]):
                _fail("Invalid deleted combination: " + line)
                return false
            deleted.append(fields[1])
            continue
        if fields.size() != 2 or not fields[0].is_valid_int():
            _fail("Invalid catalog row: " + line)
            return false
        var id := int(fields[0])
        if id < 0 or id >= REVIEW_BODY_COUNT or seen.has(id):
            _fail("Invalid catalog assignment: " + line)
            return false
        seen[id] = true
        var values: Array[String] = []
        if fields[1] != "unreviewed":
            for value in fields[1].split(","):
                var key: String = value
                if key.begins_with("silver_"): key = "type_" + key.substr(7)
                if not _known(key):
                    _fail("Unknown assignment: " + key)
                    return false
                if not values.has(key): values.append(key)
        values.sort()
        assignments[id] = values
    if not range(33).all(func(id): return seen.has(id)):
        _fail("Catalog must contain all 33 body IDs.")
        return false
    for values in assignments:
        for key in deleted: values.erase(key)
    loaded = true
    return true

func _known(key: String) -> bool:
    for look in looks:
        if look[0] == key: return true
    return false

func _name_for(key: String) -> String:
    for look in looks:
        if look[0] == key: return look[1]
    return key

func _step(amount: int) -> void:
    if not loaded: return
    body_id = posmod(body_id + amount, REVIEW_BODY_COUNT)
    _refresh()

func _refresh() -> void:
    number.text = "Body %d / 34" % body_id
    status.text = "Changes save immediately to " + ProjectSettings.globalize_path(catalog_path)
    _show_assignments()
    for variant in range(4):
        var size_offset: int = 0 if variant % 2 == 0 else 64
        var pose_offset: int = 0 if variant < 2 else 128
        var bank: int = size_offset + pose_offset
        var source_id: int = DAGGER_SOURCES.get(body_id, 21 if body_id == 32 else body_id)
        var body := loader._extract_record(body_data, source_id + bank)
        var head := loader._extract_record(head_data, size_offset + pose_offset)
        if body.size() < 305 or head.size() < 17:
            _fail("Cannot decode original body or head: " + loader.error_message)
            return
        if body_id == 32: body = _without_wand(body, bank)
        if DAGGER_SOURCES.has(body_id): body = _with_dagger(body, bank)
        var image := _compose(head, body)
        previews[variant].texture = ImageTexture.create_from_image(image)
    _fill_list()
    _refresh_reports()

# Edit a copy of the locally decoded sword body. No pixels are saved.
func _with_dagger(source: PackedByteArray, bank: int) -> PackedByteArray:
    var body := source.duplicate()
    var action := bank >= 128
    var tall := bank % 128 == 64
    var tip_y := (17 if tall else 18) if action else (4 if tall else 6)
    for y in range(24):
        for x in range(24):
            var p := y * 24 + x
            var offset := 17 + (p >> 1)
            var value := int(body[offset])
            var color := value >> 4 if p % 2 == 0 else value & 15
            if color != 7 and color != 15: continue
            # Keep the half nearest the grip, tapering its last row to one pixel.
            if (action and y > tip_y) or (not action and y < tip_y):
                color = 0
            elif y == tip_y:
                color = 15 if x == (19 if action else 4) else 0
            body[offset] = (value & 15) | (color << 4) if p % 2 == 0 else (value & 240) | color
    return body

func _without_wand(source: PackedByteArray, bank: int) -> PackedByteArray:
    var body := source.duplicate()
    var spans := {
        0: [[15, 14, 15], [16, 15, 17], [17, 17, 19], [18, 19, 20]],
        64: [[13, 14, 15], [14, 15, 17], [15, 17, 19], [16, 19, 20]],
        128: [[15, 15, 21]],
        192: [[13, 15, 21]],
    }
    for span in spans[bank]:
        for x in range(span[1], span[2] + 1):
            var pixel: int = span[0] * 24 + x
            var offset: int = 17 + (pixel >> 1)
            var value: int = body[offset]
            var index: int = value >> 4 if pixel % 2 == 0 else value & 15
            if index == 8:
                body[offset] = value & 15 if pixel % 2 == 0 else value & 240
    return body

func _compose(head: PackedByteArray, body: PackedByteArray) -> Image:
    var pixels := PackedByteArray()
    pixels.resize(576)
    var head_count: int = int(head[0]) * 24
    for p in range(576):
        var source: int = body[17 + (p >> 1)]
        var index: int = source >> 4 if p % 2 == 0 else source & 15
        if p < head_count:
            source = head[17 + (p >> 1)]
            var overlay: int = source >> 4 if p % 2 == 0 else source & 15
            if overlay != 0: index = overlay
        pixels[p] = index
    var image := Image.create(24, 24, false, Image.FORMAT_RGBA8)
    for p in range(576):
        var index: int = pixels[p]
        var color: Color = DaxSpriteLoader.EGA_PALETTE[index]
        if index == 8: color = Color.BLACK
        if index == 13: color = Color("ff55ff")
        if index == 0: color.a = 0
        image.set_pixel(p % 24, p / 24, color)
    return image

func _show_assignments() -> void:
    var names := PackedStringArray()
    for key in assignments[body_id]: names.append(_name_for(key))
    assignment.text = "Unreviewed - no assignments" if names.is_empty() else "%d assigned combinations" % names.size()
    summary.text = "Unreviewed" if names.is_empty() else "\n".join(names)

func _fill_list() -> void:
    for child in list.get_children():
        list.remove_child(child)
        child.queue_free()
    if not loaded: return
    var query := filter_box.text.strip_edges().to_lower()
    for i in range(looks.size()):
        if deleted.has(looks[i][0]): continue
        if not query.is_empty() and not str(looks[i][1]).to_lower().contains(query): continue
        var checkbox := CheckBox.new()
        checkbox.text = looks[i][1]
        checkbox.button_pressed = assignments[body_id].has(looks[i][0])
        checkbox.toggled.connect(func(checked): _toggle(looks[i][0], checked))
        list.add_child(checkbox)

func _toggle(key: String, checked: bool) -> void:
    if not loaded or not _known(key) or deleted.has(key): return
    var next: Array = assignments.duplicate(true)
    if checked and not next[body_id].has(key): next[body_id].append(key)
    if not checked: next[body_id].erase(key)
    next[body_id].sort()
    if not _save(next, deleted):
        _fill_list()
        return
    assignments = next
    _show_assignments()
    _refresh_reports()

func _save(next: Array, next_deleted: Array[String]) -> bool:
    var content := "# v3: body ID and combination IDs; deleted rows remove invalid combinations.\n"
    for key in next_deleted: content += "deleted\t%s\n" % key
    for id in range(REVIEW_BODY_COUNT):
        content += "%d\t%s\n" % [id, "unreviewed" if next[id].is_empty() else ",".join(next[id])]
    var temporary := catalog_path + ".tmp"
    var file := FileAccess.open(temporary, FileAccess.WRITE)
    if file == null:
        _fail("Save failed: " + error_string(FileAccess.get_open_error()))
        return false
    file.store_string(content)
    file.flush()
    var write_error := file.get_error()
    file.close()
    if write_error != OK:
        _fail("Save failed: " + error_string(write_error))
        return false
    var error := DirAccess.rename_absolute(ProjectSettings.globalize_path(temporary), ProjectSettings.globalize_path(catalog_path))
    if error != OK:
        _fail("Save failed: " + error_string(error))
        return false
    status.text = "Saved " + ProjectSettings.globalize_path(catalog_path) + ". Rebuild the game to package this edit."
    return true

func _bodies_for(key: String) -> Array[int]:
    var bodies: Array[int] = []
    for id in range(assignments.size()):
        if assignments[id].has(key): bodies.append(id)
    return bodies

func _review_body(id: int) -> void:
    body_id = id
    tabs.current_tab = 0
    _refresh()

func _refresh_reports() -> void:
    if not loaded: return
    for report in range(3):
        var rows := report_lists[report]
        for child in rows.get_children():
            rows.remove_child(child)
            child.queue_free()
        var query := report_filters[report].text.strip_edges().to_lower()
        var total := 0
        var visible := 0
        for look in looks:
            var key: String = look[0]
            if deleted.has(key): continue
            var bodies := _bodies_for(key)
            if report == 0 and not bodies.is_empty(): continue
            if report == 1 and bodies.size() < 2: continue
            total += 1
            if not query.is_empty() and not str(look[1]).to_lower().contains(query): continue
            visible += 1
            var row := HBoxContainer.new()
            row.set_meta("combination", key)
            rows.add_child(row)
            var label := Label.new()
            label.text = look[1]
            label.custom_minimum_size.x = 310
            row.add_child(label)
            if report == 1:
                var body_buttons := HFlowContainer.new()
                body_buttons.size_flags_horizontal = Control.SIZE_EXPAND_FILL
                row.add_child(body_buttons)
                for id in bodies:
                    var button := Button.new()
                    button.text = "Review body %d" % id
                    button.pressed.connect(func(): _review_body(id))
                    body_buttons.add_child(button)
            elif report == 2:
                var usage := Label.new()
                usage.text = "1 body" if bodies.size() == 1 else "%d bodies" % bodies.size()
                usage.size_flags_horizontal = Control.SIZE_EXPAND_FILL
                row.add_child(usage)
                var button := Button.new()
                button.text = "Delete combination"
                button.pressed.connect(func(): _delete_combination(key))
                row.add_child(button)
                row.move_child(button, 0)
                row.add_theme_constant_override("separation", 12)
        report_counts[report].text = "%d of %d combinations" % [visible, total]
        if visible == 0:
            var empty := Label.new()
            empty.text = "No matching combinations." if total > 0 else "No combinations in this list."
            rows.add_child(empty)

func _delete_combination(key: String) -> void:
    if not loaded or not _known(key) or deleted.has(key): return
    var next: Array = assignments.duplicate(true)
    for values in next: values.erase(key)
    var next_deleted: Array[String] = deleted.duplicate()
    next_deleted.append(key)
    next_deleted.sort()
    if not _save(next, next_deleted): return
    assignments = next
    deleted = next_deleted
    _show_assignments()
    _fill_list()
    _refresh_reports()

func _fail(message: String) -> void:
    status.text = message
    push_error(message)
