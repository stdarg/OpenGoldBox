extends WallArtCatalog

const Builder = preload("res://scripts/perspective_builder.gd")
const INDEX_PATH := "../data/appearance_index.csv"
const BRIEFS_PATH := "../data/briefs.json"
const METADATA_PATH := "../../tile_art_viewer/data/tile_art_catalog.csv"
const KINDS := ["solid", "door", "opening", "landmark", "facade", "rect_opening", "cave_opening"]
const CSV_COLUMNS := [
    "index", "archive", "record", "appearance", "view", "view_name", "name", "width", "height",
    "description", "usage", "map_archive", "map_record", "map_name", "profile_index",
    "profile_label", "profile_source", "pose_x", "pose_y", "facing", "facing_name",
    "click_x", "click_y", "cell_x", "cell_y", "face", "face_name", "wall_id",
    "door_code", "event_raw"
]

@onready var tabs: TabContainer = $Margin/Main/Split/Tabs
@onready var help: Label = $Margin/Main/Help
@onready var usage_filter: OptionButton = $Margin/Main/Split/ListColumn/UsageFilter
@onready var search: LineEdit = $Margin/Main/Split/ListColumn/Search
@onready var appearance_list: ItemList = $Margin/Main/Split/ListColumn/AppearanceList
@onready var coverage: Label = $Margin/Main/Split/ListColumn/Coverage
@onready var identity: Label = $Margin/Main/Split/Tabs/Create/Identity
@onready var appearance_usage: Label = $Margin/Main/Split/Tabs/Create/AppearanceUsage
@onready var title_edit: LineEdit = $Margin/Main/Split/Tabs/Create/Fields/TitleEdit
@onready var kind_picker: OptionButton = $Margin/Main/Split/Tabs/Create/Fields/KindPicker
@onready var brief_edit: TextEdit = $Margin/Main/Split/Tabs/Create/BriefEdit
@onready var save_brief: Button = $Margin/Main/Split/Tabs/Create/BriefActions/SaveBrief
@onready var copy_wall_prompt: Button = $Margin/Main/Split/Tabs/Create/BriefActions/CopyWallPrompt
@onready var copy_feature_prompt: Button = $Margin/Main/Split/Tabs/Create/BriefActions/CopyFeaturePrompt
@onready var import_base: Button = $Margin/Main/Split/Tabs/Create/ArtActions/ImportBase
@onready var import_feature: Button = $Margin/Main/Split/Tabs/Create/ArtActions/ImportFeature
@onready var build_views: Button = $Margin/Main/Split/Tabs/Create/ArtActions/BuildViews
@onready var export_set: Button = $Margin/Main/Split/Tabs/Create/ArtActions/ExportSet
@onready var sources: Label = $Margin/Main/Split/Tabs/Create/Sources
@onready var depth_picker: OptionButton = $Margin/Main/Split/Tabs/Create/PreviewSplit/CorridorColumn/DepthRow/DepthPicker
@onready var corridor_preview: TextureRect = $Margin/Main/Split/Tabs/Create/PreviewSplit/CorridorColumn/CorridorPanel/CorridorPreview
@onready var piece_grid: ItemList = $Margin/Main/Split/Tabs/Create/PreviewSplit/PiecesColumn/PieceGrid
@onready var piece_name: Label = $Margin/Main/Split/Tabs/Create/PreviewSplit/PiecesColumn/PieceName
@onready var piece_preview: TextureRect = $Margin/Main/Split/Tabs/Create/PreviewSplit/PiecesColumn/PiecePanel/PiecePreview
@onready var reference_status: Label = $Margin/Main/Split/Tabs/Reference/ReferenceStatus
@onready var reference_grid: ItemList = $Margin/Main/Split/Tabs/Reference/ReferenceSplit/ReferencePieces/ReferenceGrid
@onready var reference_identity: Label = $Margin/Main/Split/Tabs/Reference/ReferenceSplit/ReferencePreviewColumn/ReferenceIdentity
@onready var reference_usage: Label = $Margin/Main/Split/Tabs/Reference/ReferenceSplit/ReferencePreviewColumn/ReferenceUsage
@onready var reference_preview: TextureRect = $Margin/Main/Split/Tabs/Reference/ReferenceSplit/ReferencePreviewColumn/ReferencePanel/ReferencePreview
@onready var reference_description: TextEdit = $Margin/Main/Split/Tabs/Reference/ReferenceSplit/ReferencePreviewColumn/ReferenceDescription
@onready var save_reference_description: Button = $Margin/Main/Split/Tabs/Reference/ReferenceSplit/ReferencePreviewColumn/SaveReferenceDescription
@onready var status: Label = $Margin/Main/Status
@onready var base_dialog: FileDialog = $BaseDialog
@onready var feature_dialog: FileDialog = $FeatureDialog
@onready var export_dialog: FileDialog = $ExportDialog

var keys: Array[String] = []
var visible_keys: Array[String] = []
var records: Dictionary = {}
var selected_key := ""
var pieces: Array[Image] = []
var dirty := false
var source_entries: Array = []
var metadata_rows: Array = []
var used_views_by_key: Dictionary = {}
var usage_ready := false
var source_indices: Dictionary = {}
var draft_descriptions: Dictionary = {}
var metadata_digest := ""
var metadata_path_override := ""
var reference_ready := false
var reference_selected_index := -1
var reference_selected_view := 6
var updating_reference_description := false

func _ready() -> void:
    usage_filter.add_item("All appearances")
    usage_filter.add_item("Used")
    usage_filter.add_item("Unused")
    usage_filter.disabled = true
    usage_filter.item_selected.connect(_apply_usage_filter)
    for kind in KINDS:
        kind_picker.add_item(kind.capitalize())
    for depth in ["Far", "Middle", "Near"]:
        depth_picker.add_item(depth)
    depth_picker.select(2)
    search.text_changed.connect(func(_value: String): _rebuild_list())
    appearance_list.item_selected.connect(_select_visible)
    save_brief.pressed.connect(_save_briefs)
    copy_wall_prompt.pressed.connect(func(): _copy_prompt(false))
    copy_feature_prompt.pressed.connect(func(): _copy_prompt(true))
    kind_picker.item_selected.connect(func(_index: int): _update_feature_button())
    import_base.pressed.connect(func(): base_dialog.popup_centered_ratio())
    import_feature.pressed.connect(func(): feature_dialog.popup_centered_ratio())
    build_views.pressed.connect(_build_selected)
    export_set.pressed.connect(func(): export_dialog.popup_centered_ratio())
    base_dialog.file_selected.connect(func(path: String): _import_art(path, "base"))
    feature_dialog.file_selected.connect(func(path: String): _import_art(path, "feature"))
    export_dialog.dir_selected.connect(_export_selected)
    depth_picker.item_selected.connect(func(_index: int): _show_corridor())
    piece_grid.item_selected.connect(_show_piece)
    tabs.tab_changed.connect(_on_tab_changed)
    reference_grid.item_selected.connect(_show_reference_piece)
    reference_description.text_changed.connect(_on_reference_description_changed)
    save_reference_description.pressed.connect(_save_reference_descriptions)
    var error := _load_index()
    if error.is_empty():
        error = _load_briefs()
    if not error.is_empty():
        status.text = error
        push_error(error)
        return
    _load_reference_catalog()
    _rebuild_list()
    var pilot := "WALLDEF1.DAX:1:4"
    if keys.has(pilot):
        _select_key(pilot)
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--creator-check="):
            call_deferred("_run_check", argument.trim_prefix("--creator-check="))
        elif argument.begins_with("--export-used="):
            call_deferred("_export_used", argument.trim_prefix("--export-used="))
        elif argument.begins_with("--creator-capture="):
            call_deferred("_capture", argument.trim_prefix("--creator-capture="))
        elif argument.begins_with("--reference-check="):
            call_deferred("_run_reference_check", argument.trim_prefix("--reference-check="))
        elif argument == "--usage-check":
            call_deferred("_run_usage_check")
        elif argument.begins_with("--reference-capture="):
            call_deferred("_capture_reference", argument.trim_prefix("--reference-capture="))

func _on_tab_changed(index: int) -> void:
    status.visible = index == 0
    help.text = (
        "Review original perspective pieces and correct their Art Catalog descriptions."
        if index == 1 else
        "Create independent art from a brief. Import new PNGs, build ten aligned views, inspect a synthetic corridor, then export the set."
    )

func _data_path(relative: String) -> String:
    return ProjectSettings.globalize_path("res://").path_join(relative).simplify_path()

func _load_index() -> String:
    var file := FileAccess.open(_data_path(INDEX_PATH), FileAccess.READ)
    if file == null:
        return "Cannot read technical appearance index."
    if file.get_csv_line() != PackedStringArray(["archive", "record", "appearance"]):
        return "Technical appearance index has unexpected columns."
    while file.get_position() < file.get_length():
        var row := file.get_csv_line()
        if row.size() != 3 or not row[1].is_valid_int() or not row[2].is_valid_int():
            return "Technical appearance index contains an invalid row."
        var key := "%s:%s:%s" % [row[0], row[1], row[2]]
        if keys.has(key):
            return "Technical appearance index contains a duplicate key."
        keys.append(key)
    if keys.size() != 210:
        return "Expected 210 technical appearance keys; found %d." % keys.size()
    return ""

func _load_briefs() -> String:
    var file := FileAccess.open(_data_path(BRIEFS_PATH), FileAccess.READ)
    if file == null:
        return "Cannot read independent creative briefs."
    var parsed: Variant = JSON.parse_string(file.get_as_text())
    if not parsed is Dictionary:
        return "Independent creative briefs must be a JSON object."
    records = parsed
    for key in records:
        if not keys.has(str(key)) or not records[key] is Dictionary:
            return "Briefs contain an unknown appearance key."
    return ""

func _metadata_path() -> String:
    return metadata_path_override if not metadata_path_override.is_empty() else _data_path(METADATA_PATH)

func _load_reference_catalog() -> void:
    reference_description.editable = false
    save_reference_description.disabled = true
    var error := _load_reference_metadata()
    if not error.is_empty():
        reference_status.text = error
        return
    var game_directory := OS.get_environment("OPENGOLD_GAME_DIR")
    if game_directory.is_empty():
        game_directory = str(ProjectSettings.get_setting("opengold/game_directory", ""))
    if game_directory.is_empty():
        reference_status.text = "Set OPENGOLD_GAME_DIR to your original game files to view and edit source descriptions."
        return
    error = load_directory(game_directory)
    if not error.is_empty():
        reference_status.text = "Cannot load original art: %s" % error
        return
    source_entries = entries()
    if source_entries.size() != metadata_rows.size():
        reference_status.text = "Art Catalog CSV and decoded original art have different piece counts."
        return
    for index in source_entries.size():
        var entry: Dictionary = source_entries[index]
        var row: Dictionary = metadata_rows[index]
        for field in ["archive", "record", "appearance", "view", "width", "height"]:
            if row[field] != str(entry[field]):
                reference_status.text = "Art Catalog CSV row %d disagrees with original %s." % [index + 2, field]
                return
        if row["view_name"] != Builder.VIEW_NAMES[int(entry["view"])]:
            reference_status.text = "Art Catalog CSV row %d has the wrong view name." % (index + 2)
            return
        var key := "%s:%s:%s" % [entry["archive"], entry["record"], entry["appearance"]]
        if not keys.has(key):
            reference_status.text = "Decoded art has an unknown appearance: %s" % key
            return
        var group: Array = source_indices.get(key, [])
        if group.is_empty():
            group.resize(10)
            group.fill(-1)
        var view := int(entry["view"])
        if view < 0 or view >= 10 or group[view] != -1:
            reference_status.text = "Decoded art has duplicate or invalid views for %s." % key
            return
        group[view] = index
        source_indices[key] = group
    if source_indices.size() != keys.size():
        reference_status.text = "Decoded art does not cover all technical appearance IDs."
        return
    for key in keys:
        if (source_indices[key] as Array).has(-1):
            reference_status.text = "Decoded art lacks a perspective piece for %s." % key
            return
    reference_ready = true
    reference_status.text = "Original art loaded from %s. Select a piece to review its description." % game_directory

func _load_reference_metadata() -> String:
    usage_ready = false
    var path := _metadata_path()
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return "Cannot read Art Catalog CSV: %s" % path
    if file.get_csv_line() != PackedStringArray(CSV_COLUMNS):
        return "Art Catalog CSV has unexpected columns."
    metadata_rows.clear()
    used_views_by_key.clear()
    var piece_counts: Dictionary = {}
    while file.get_position() < file.get_length():
        var fields := file.get_csv_line()
        if fields.size() != CSV_COLUMNS.size():
            return "Art Catalog CSV row %d has the wrong number of fields." % (metadata_rows.size() + 2)
        var row: Dictionary = {}
        for column in CSV_COLUMNS.size():
            row[CSV_COLUMNS[column]] = fields[column]
        if row["index"] != str(metadata_rows.size()) or row["description"].strip_edges().is_empty():
            return "Art Catalog CSV row %d has an invalid index or empty description." % (metadata_rows.size() + 2)
        var key := "%s:%s:%s" % [row["archive"], row["record"], row["appearance"]]
        if not keys.has(key) or (row["usage"] != "visible" and row["usage"] != "unused"):
            return "Art Catalog CSV row %d has an unknown appearance or usage." % (metadata_rows.size() + 2)
        piece_counts[key] = int(piece_counts.get(key, 0)) + 1
        if row["usage"] == "visible":
            used_views_by_key[key] = int(used_views_by_key.get(key, 0)) + 1
        metadata_rows.append(row)
    if metadata_rows.size() != keys.size() * 10:
        return "Art Catalog CSV has %d pieces; expected %d." % [metadata_rows.size(), keys.size() * 10]
    for key in keys:
        if int(piece_counts.get(key, 0)) != 10:
            return "Art Catalog CSV needs ten perspective pieces for %s." % key
        if not used_views_by_key.has(key):
            used_views_by_key[key] = 0
    var unused := 0
    for key in keys:
        if int(used_views_by_key[key]) == 0:
            unused += 1
    usage_filter.set_item_text(1, "Used (%d)" % (keys.size() - unused))
    usage_filter.set_item_text(2, "Unused (%d)" % unused)
    usage_filter.disabled = false
    usage_ready = true
    metadata_digest = FileAccess.get_sha256(path)
    return ""

func _show_reference_key(key: String) -> void:
    reference_grid.clear()
    reference_preview.texture = null
    reference_selected_index = -1
    reference_identity.text = key
    reference_usage.text = ""
    updating_reference_description = true
    reference_description.text = ""
    updating_reference_description = false
    reference_description.editable = false
    if not reference_ready or not source_indices.has(key):
        return
    var group: Array = source_indices[key]
    for view in group.size():
        var image: Image = image_at(group[view])
        if image == null:
            reference_status.text = "Could not read original piece %d for %s." % [view, key]
            return
        var row: Dictionary = metadata_rows[group[view]]
        var unused: bool = row["usage"] == "unused"
        var label := "%s%s" % ["Unused · " if unused else "", Builder.VIEW_NAMES[view]]
        reference_grid.add_item(label, _reference_thumbnail(image))
        reference_grid.set_item_tooltip(view, "%s · %s" % [
            Builder.VIEW_NAMES[view],
            "Unused in available maps/profiles" if unused else "Visible in an available map/profile"
        ])
    reference_grid.select(reference_selected_view)
    _show_reference_piece(reference_selected_view)

func _reference_thumbnail(image: Image) -> ImageTexture:
    var thumbnail := Image.create(80, 80, false, Image.FORMAT_RGBA8)
    thumbnail.fill(Color.TRANSPARENT)
    var scale := minf(80.0 / image.get_width(), 80.0 / image.get_height())
    var fitted := image.duplicate() as Image
    fitted.resize(maxi(1, roundi(image.get_width() * scale)),
                  maxi(1, roundi(image.get_height() * scale)), Image.INTERPOLATE_NEAREST)
    thumbnail.blend_rect(fitted, Rect2i(Vector2i.ZERO, fitted.get_size()),
                         (Vector2i(80, 80) - fitted.get_size()) / 2)
    return ImageTexture.create_from_image(thumbnail)

func _show_reference_piece(view: int) -> void:
    if not reference_ready or selected_key.is_empty() or not source_indices.has(selected_key):
        return
    if view < 0 or view >= 10:
        return
    reference_selected_view = view
    reference_selected_index = source_indices[selected_key][view]
    var image: Image = image_at(reference_selected_index)
    var row: Dictionary = metadata_rows[reference_selected_index]
    reference_preview.texture = ImageTexture.create_from_image(image)
    reference_identity.text = "%s · %s · %s × %s px" % [
        selected_key, row["view_name"], row["width"], row["height"]
    ]
    reference_usage.text = "%s · %s" % [
        "Seen in %s" % row["map_name"] if row["usage"] == "visible" else "Unused in available maps/profiles",
        row["name"] if not row["name"].is_empty() else "Original source piece"
    ]
    updating_reference_description = true
    reference_description.text = str(draft_descriptions.get(reference_selected_index, row["description"]))
    updating_reference_description = false
    reference_description.editable = true
    save_reference_description.disabled = draft_descriptions.is_empty()

func _on_reference_description_changed() -> void:
    if updating_reference_description or reference_selected_index < 0:
        return
    var original := str(metadata_rows[reference_selected_index]["description"])
    if reference_description.text == original:
        draft_descriptions.erase(reference_selected_index)
    else:
        draft_descriptions[reference_selected_index] = reference_description.text
    save_reference_description.disabled = draft_descriptions.is_empty()
    reference_status.text = "%d unsaved description edit(s)." % draft_descriptions.size()

func _csv_cell(value: String) -> String:
    if value.contains(",") or value.contains("\"") or value.contains("\n"):
        return "\"%s\"" % value.replace("\"", "\"\"")
    return value

func _write_reference_metadata(path: String) -> String:
    var file := FileAccess.open(path, FileAccess.WRITE)
    if file == null:
        return "Cannot write Art Catalog CSV: %s" % error_string(FileAccess.get_open_error())
    file.store_line(",".join(CSV_COLUMNS))
    for index in metadata_rows.size():
        var row: Dictionary = metadata_rows[index]
        var fields: PackedStringArray = []
        for column in CSV_COLUMNS:
            var value := str(draft_descriptions.get(index, row[column])) if column == "description" else str(row[column])
            fields.append(_csv_cell(value))
        file.store_line(",".join(fields))
    file.flush()
    var result := file.get_error()
    file.close()
    return "" if result == OK else "Art Catalog CSV write failed: %s" % error_string(result)

func _save_reference_descriptions() -> void:
    if draft_descriptions.is_empty():
        return
    for index in draft_descriptions:
        if str(draft_descriptions[index]).strip_edges().is_empty():
            reference_status.text = "Description for piece %d cannot be empty." % index
            return
    var destination := _metadata_path()
    if FileAccess.get_sha256(destination) != metadata_digest:
        reference_status.text = "Art Catalog CSV changed outside this window. Reopen the creator before saving."
        return
    var temporary := destination + ".tmp"
    var error := _write_reference_metadata(temporary)
    if error.is_empty():
        var result := DirAccess.rename_absolute(temporary, destination)
        if result != OK:
            error = "Cannot replace Art Catalog CSV: %s" % error_string(result)
    if not error.is_empty():
        DirAccess.remove_absolute(temporary)
        reference_status.text = error
        return
    for index in draft_descriptions:
        metadata_rows[index]["description"] = draft_descriptions[index]
    draft_descriptions.clear()
    metadata_digest = FileAccess.get_sha256(destination)
    save_reference_description.disabled = true
    reference_status.text = "Descriptions saved to %s" % destination

func _rebuild_list() -> void:
    var query := search.text.strip_edges().to_lower()
    visible_keys.clear()
    appearance_list.clear()
    for key in keys:
        var record: Dictionary = records.get(key, {})
        var title := str(record.get("title", ""))
        var used := int(used_views_by_key.get(key, -1)) if usage_ready else -1
        if usage_filter.selected == 1 and used <= 0:
            continue
        if usage_filter.selected == 2 and used != 0:
            continue
        if not query.is_empty() and not (key + " " + title).to_lower().contains(query):
            continue
        visible_keys.append(key)
        var marker := "UNKNOWN" if used < 0 else "UNUSED" if used == 0 else "" if used == 10 else "%d/10 used" % used
        var label := "%s  %s" % [key, title]
        if not marker.is_empty():
            label = "%s · %s" % [marker, label]
        appearance_list.add_item(label)
        appearance_list.set_item_tooltip(appearance_list.get_item_count() - 1,
                "%s\n%s\n%s" % [key, title, _usage_summary(key)])
    if visible_keys.has(selected_key):
        appearance_list.select(visible_keys.find(selected_key))
    var complete := 0
    var unused_count := 0
    for key in keys:
        var record: Dictionary = records.get(key, {})
        if not str(record.get("base", "")).is_empty():
            complete += 1
        if usage_ready and int(used_views_by_key.get(key, -1)) == 0:
            unused_count += 1
    coverage.text = "%d / %d have new art · %d unused in available maps/profiles" % [
        complete, keys.size(), unused_count
    ] if usage_ready else "%d / %d appearances have new art · usage unavailable" % [
        complete, keys.size()
    ]

func _apply_usage_filter(_index: int) -> void:
    _rebuild_list()
    if not visible_keys.is_empty() and not visible_keys.has(selected_key):
        _select_key(visible_keys[0])

func _usage_summary(key: String) -> String:
    var used := int(used_views_by_key.get(key, -1)) if usage_ready else -1
    if used < 0:
        return "Map usage unavailable; check the Art Catalog CSV."
    if used == 0:
        return "UNUSED in available maps/profiles: none of its ten pieces has a visible map location."
    if used == 10:
        return "USED in available maps/profiles: all ten perspective pieces have visible map locations."
    return "PARTLY USED in available maps/profiles: %d of ten perspective pieces have visible map locations." % used

func _select_visible(index: int) -> void:
    if index < 0 or index >= visible_keys.size():
        return
    _select_key(visible_keys[index])

func _select_key(key: String) -> void:
    if not selected_key.is_empty():
        _store_form()
    selected_key = key
    var record: Dictionary = records.get(key, {})
    identity.text = key
    appearance_usage.text = _usage_summary(key)
    title_edit.text = str(record.get("title", ""))
    brief_edit.text = str(record.get("brief", ""))
    kind_picker.select(maxi(0, KINDS.find(str(record.get("kind", "solid")))))
    sources.text = "Wall: %s   Feature: %s" % [
        str(record.get("base", "none")), str(record.get("feature", "none"))
    ]
    for control in [save_brief, copy_wall_prompt, import_base, import_feature, build_views, export_set]:
        control.disabled = false
    _update_feature_button()
    if visible_keys.has(key):
        appearance_list.select(visible_keys.find(key))
    _build_selected()
    _show_reference_key(key)

func _store_form() -> void:
    if selected_key.is_empty():
        return
    var record: Dictionary = records.get(selected_key, {})
    record["title"] = title_edit.text.strip_edges()
    record["brief"] = brief_edit.text.strip_edges()
    record["kind"] = KINDS[kind_picker.selected]
    record["base"] = str(record.get("base", ""))
    record["feature"] = str(record.get("feature", ""))
    records[selected_key] = record
    dirty = true

func _save_briefs() -> void:
    _store_form()
    var path := _data_path(BRIEFS_PATH)
    var temp := path + ".tmp"
    var file := FileAccess.open(temp, FileAccess.WRITE)
    if file == null:
        status.text = "Cannot write briefs: %s" % error_string(FileAccess.get_open_error())
        return
    file.store_string(JSON.stringify(records, "  ") + "\n")
    file.flush()
    file.close()
    var error := DirAccess.rename_absolute(temp, path)
    if error != OK:
        status.text = "Cannot replace briefs: %s" % error_string(error)
        return
    dirty = false
    status.text = "Saved independent briefs."
    _rebuild_list()

func _update_feature_button() -> void:
    copy_feature_prompt.disabled = selected_key.is_empty() or KINDS[kind_picker.selected] == "solid"

func _copy_prompt(feature: bool) -> void:
    _store_form()
    var record: Dictionary = records[selected_key]
    var prompt := "Create brand-new original pixel art. %s\n" % str(record["brief"])
    prompt += "Use crisp deliberate pixel clusters, rich hand-shaded materials, and a straight-on orthographic view. "
    prompt += "This is for a 352 x 352 fantasy first-person view. No text, logos, franchise motifs, or source-game imagery. "
    if feature:
        prompt += "Create only the %s feature, isolated on an actual transparent square background. No surrounding wall, floor, or sky." % str(record["kind"])
    else:
        prompt += "Create only a plain square backing wall material that fills the canvas. Omit the door, opening, landmark, or other feature. Make left and right edges compatible."
    DisplayServer.clipboard_set(prompt)
    status.text = "Copied a source-independent %s prompt for Codex imagegen." % ("feature" if feature else "wall")

func _import_art(path: String, field: String) -> void:
    if selected_key.is_empty():
        return
    var candidate := Image.new()
    var load_error := candidate.load_png_from_buffer(FileAccess.get_file_as_bytes(path))
    if load_error != OK or candidate.is_empty():
        status.text = "The selected file is not a readable PNG image."
        return
    var basename := selected_key.replace(".", "_").replace(":", "_")
    var destination := "res://art/%s-%s.png" % [basename, field]
    var bytes := FileAccess.get_file_as_bytes(path)
    var file := FileAccess.open(ProjectSettings.globalize_path(destination), FileAccess.WRITE)
    if file == null:
        status.text = "Cannot copy art into the creator project."
        return
    file.store_buffer(bytes)
    file.close()
    _store_form()
    records[selected_key][field] = destination
    _save_briefs()
    _select_key(selected_key)
    status.text = "Imported new %s artwork. Build and review the ten views." % field

func _build_selected() -> void:
    pieces.clear()
    piece_grid.clear()
    piece_preview.texture = null
    corridor_preview.texture = null
    if selected_key.is_empty():
        return
    _store_form()
    var record: Dictionary = records[selected_key]
    var base := str(record.get("base", ""))
    if base.is_empty():
        status.text = "Import new wall artwork for this appearance."
        return
    pieces = Builder.build(base, str(record.get("feature", "")), str(record.get("kind", "solid")))
    if pieces.size() != 10:
        status.text = "Could not build all ten views. Check the imported PNG files."
        return
    for view in pieces.size():
        var thumbnail := pieces[view].duplicate() as Image
        thumbnail.resize(64, 64, Image.INTERPOLATE_NEAREST)
        piece_grid.add_item(Builder.VIEW_NAMES[view], ImageTexture.create_from_image(thumbnail))
    piece_grid.select(6)
    _show_piece(6)
    _show_corridor()
    status.text = "Built ten new views at 4x the original slot dimensions."

func _show_piece(index: int) -> void:
    if index < 0 or index >= pieces.size():
        return
    piece_name.text = "%s · %d × %d px" % [
        Builder.VIEW_NAMES[index], pieces[index].get_width(), pieces[index].get_height()
    ]
    piece_preview.texture = ImageTexture.create_from_image(pieces[index])

func _show_corridor() -> void:
    if pieces.size() != 10:
        return
    corridor_preview.texture = ImageTexture.create_from_image(Builder.preview(pieces, depth_picker.selected))

func _export_selected(folder: String) -> void:
    _store_form()
    if pieces.size() != 10 or selected_key.is_empty():
        status.text = "Build a complete ten-view set before export."
        return
    var error := _export_set(folder, selected_key, pieces)
    status.text = "Exported %s to %s" % [selected_key, folder] if error.is_empty() else error

func _export_set(folder: String, key: String, images: Array[Image]) -> String:
    if images.size() != 10:
        return "An appearance must have ten pieces."
    var stem := key.replace(".", "_").replace(":", "_")
    var directory := folder.path_join(stem)
    var version := 2
    while DirAccess.dir_exists_absolute(directory):
        directory = folder.path_join("%s-%04d" % [stem, version])
        version += 1
    var error := DirAccess.make_dir_recursive_absolute(directory)
    if error != OK:
        return "Cannot create export folder: %s" % error_string(error)
    var files: Dictionary = {}
    for view in images.size():
        var filename: String = str(Builder.VIEW_NAMES[view]).to_lower().replace(" ", "_") + ".png"
        error = images[view].save_png(directory.path_join(filename))
        if error != OK:
            return "Cannot export %s: %s" % [filename, error_string(error)]
        files[Builder.VIEW_NAMES[view]] = filename
    var record: Dictionary = records.get(key, {})
    var manifest := {
        "format": "opengold-replacement-art-v1",
        "appearance_key": key,
        "canvas_width": Builder.CANVAS,
        "canvas_height": Builder.CANVAS,
        "title": record.get("title", ""),
        "kind": record.get("kind", "solid"),
        "brief": record.get("brief", ""),
        "files": files
    }
    var file := FileAccess.open(directory.path_join("manifest.json"), FileAccess.WRITE)
    if file == null:
        return "Cannot write export manifest."
    file.store_string(JSON.stringify(manifest, "  ") + "\n")
    return ""

func _export_used(folder: String) -> void:
    if not usage_ready:
        push_error("Cannot export used art without the Art Catalog usage data.")
        get_tree().quit(1)
        return
    var count := 0
    for key in keys:
        if int(used_views_by_key.get(key, 0)) == 0:
            continue
        var record: Dictionary = records.get(key, {})
        if record.is_empty():
            push_error("Used appearance lacks a brief: %s" % key)
            get_tree().quit(1)
            return
        var images: Array[Image] = Builder.build(str(record.get("base", "")),
                str(record.get("feature", "")), str(record.get("kind", "solid")))
        if images.size() != 10:
            push_error("Used appearance lacks ten views: %s" % key)
            get_tree().quit(1)
            return
        var error := _export_set(folder, key, images)
        if not error.is_empty():
            push_error(error)
            get_tree().quit(1)
            return
        count += 1
    if count != 168:
        push_error("Expected 168 used appearances, exported %d." % count)
        get_tree().quit(1)
        return
    print("Exported %d used appearance sets (%d perspective PNGs)." % [count, count * 10])
    get_tree().quit()

func _run_check(output_folder: String) -> void:
    var pilot_keys := ["WALLDEF1.DAX:1:4", "WALLDEF1.DAX:1:5",
                       "WALLDEF1.DAX:1:1", "WALLDEF1.DAX:5:3"]
    for key in pilot_keys:
        var record: Dictionary = records.get(key, {})
        var images: Array[Image] = Builder.build(str(record.get("base", "")),
                str(record.get("feature", "")), str(record.get("kind", "solid")))
        if images.size() != 10:
            push_error("Pilot appearance lacks ten views: %s" % key)
            get_tree().quit(1)
            return
        for view in images.size():
            var expected := Vector2i(Builder.VIEW_SIZES[view][0], Builder.VIEW_SIZES[view][1]) * 4
            if images[view].get_size() != expected:
                push_error("Wrong perspective size: %s view %d" % [key, view])
                get_tree().quit(1)
                return
        var error := _export_set(output_folder, key, images)
        if not error.is_empty():
            push_error(error)
            get_tree().quit(1)
            return
        var preview_error: Error = Builder.preview(images, 2).save_png(output_folder.path_join(
                key.replace(".", "_").replace(":", "_") + "-corridor.png"))
        if preview_error != OK:
            push_error("Could not save pilot corridor preview.")
            get_tree().quit(1)
            return
    var opening: Array[Image] = Builder.build(
            str(records["WALLDEF1.DAX:1:1"]["base"]),
            str(records["WALLDEF1.DAX:1:1"]["feature"]), "opening")
    if opening[6].get_pixel(112, 180).a > 0.1:
        push_error("The pilot passage must reveal the corridor behind it.")
        get_tree().quit(1)
        return
    print("Replacement art creator check passed: four independent pilot sets, 40 PNGs.")
    get_tree().quit()

func _run_reference_check(output_folder: String) -> void:
    if not reference_ready or source_entries.size() != 2100 or source_indices.size() != 210:
        push_error("Reference tab did not load 210 original appearances and their metadata.")
        get_tree().quit(1)
        return
    var key := "WALLDEF1.DAX:1:4"
    _select_key(key)
    _show_reference_piece(6)
    var index := reference_selected_index
    var image: Image = image_at(index)
    if image == null or image.get_size() != Vector2i(56, 64):
        push_error("Reference tab did not display the selected original near-front piece.")
        get_tree().quit(1)
        return
    var source_path := _metadata_path()
    var source_digest := FileAccess.get_sha256(source_path)
    var error := DirAccess.make_dir_recursive_absolute(output_folder)
    var copy_path := output_folder.path_join("tile_art_catalog.csv")
    if error == OK:
        error = DirAccess.copy_absolute(source_path, copy_path)
    if error != OK:
        push_error("Could not create an isolated metadata copy for the reference check.")
        get_tree().quit(1)
        return
    metadata_path_override = copy_path
    metadata_digest = FileAccess.get_sha256(copy_path)
    var revised := str(metadata_rows[index]["description"]) + " [reference check]"
    reference_description.text = revised
    _on_reference_description_changed()
    if draft_descriptions.get(index, "") != revised or save_reference_description.disabled:
        push_error("Reference description edit was not staged.")
        get_tree().quit(1)
        return
    _save_reference_descriptions()
    if not draft_descriptions.is_empty() or FileAccess.get_sha256(source_path) != source_digest:
        push_error("Reference description save changed the source CSV or left a pending edit.")
        get_tree().quit(1)
        return
    var saved := FileAccess.open(copy_path, FileAccess.READ)
    saved.get_csv_line()
    for row_index in index + 1:
        var fields := saved.get_csv_line()
        if row_index == index and fields[9] != revised:
            push_error("Reference description did not persist in the Art Catalog CSV format.")
            get_tree().quit(1)
            return
    saved.close()
    reference_description.text = revised + " again"
    _on_reference_description_changed()
    var external_edit := FileAccess.open(copy_path, FileAccess.READ_WRITE)
    external_edit.seek_end()
    external_edit.store_line("")
    external_edit.close()
    _save_reference_descriptions()
    if draft_descriptions.is_empty() or not reference_status.text.contains("changed outside"):
        push_error("Reference save did not protect an externally changed Art Catalog CSV.")
        get_tree().quit(1)
        return
    print("Reference check passed: 210 original appearances, ten pieces each, CSV save and external-change protection.")
    get_tree().quit()

func _run_usage_check() -> void:
    if not reference_ready or not usage_ready or used_views_by_key.size() != 210:
        push_error("Map usage and original reference art did not load.")
        get_tree().quit(1)
        return
    var unused := 0
    var partial := 0
    var full := 0
    var partial_key := ""
    for key in keys:
        var count := int(used_views_by_key[key])
        if count == 0:
            unused += 1
        elif count == 10:
            full += 1
        else:
            partial += 1
            partial_key = key
    if unused != 42 or partial != 37 or full != 131:
        push_error("Unexpected appearance usage distribution: %d unused, %d partial, %d full." % [
            unused, partial, full
        ])
        get_tree().quit(1)
        return
    usage_filter.select(2)
    _apply_usage_filter(2)
    if visible_keys.size() != unused or selected_key != visible_keys[0] or \
            not appearance_usage.text.begins_with("UNUSED"):
        push_error("Unused appearance filter or selection label failed.")
        get_tree().quit(1)
        return
    for view in 10:
        if not reference_grid.get_item_text(view).begins_with("Unused"):
            push_error("An unused appearance has an unmarked source piece.")
            get_tree().quit(1)
            return
    usage_filter.select(1)
    _apply_usage_filter(1)
    _select_key(partial_key)
    if visible_keys.size() != partial + full or not appearance_usage.text.begins_with("PARTLY USED"):
        push_error("Used appearance filter or partial-use label failed.")
        get_tree().quit(1)
        return
    var saw_unused_piece := false
    for view in 10:
        if reference_grid.get_item_text(view).begins_with("Unused"):
            saw_unused_piece = true
    if not saw_unused_piece:
        push_error("An unused perspective piece was not marked in a partly used appearance.")
        get_tree().quit(1)
        return
    usage_filter.select(0)
    _apply_usage_filter(0)
    if visible_keys.size() != keys.size():
        push_error("All-appearances filter did not restore the catalog.")
        get_tree().quit(1)
        return
    print("Usage check passed: 42 unused, 37 partly used, 131 fully used appearances; list filters and piece labels.")
    get_tree().quit()

func _capture(path: String) -> void:
    await RenderingServer.frame_post_draw
    var error := get_viewport().get_texture().get_image().save_png(path)
    if error != OK:
        push_error("Cannot save creator screenshot: %s" % error_string(error))
        get_tree().quit(1)
        return
    print("Replacement art creator screenshot saved: %s" % path)
    get_tree().quit()

func _capture_reference(path: String) -> void:
    tabs.current_tab = 1
    await get_tree().process_frame
    await _capture(path)
