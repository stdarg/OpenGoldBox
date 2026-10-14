extends WallArtCatalog

const VIEW_NAMES := [
    "Far front", "Far left", "Far right", "Middle front", "Middle left",
    "Middle right", "Near front", "Near left", "Near right", "Far extension"
]
const METADATA_FILE := "../data/tile_art_catalog.csv"
const REPLACEMENT_PACK := "../../replacement_art_creator/replacement_pack"
const CSV_COLUMNS := [
    "index", "archive", "record", "appearance", "view", "view_name", "name", "width", "height",
    "description", "usage", "map_archive", "map_record", "map_name", "profile_index",
    "profile_label", "profile_source", "pose_x", "pose_y", "facing", "facing_name",
    "click_x", "click_y", "cell_x", "cell_y", "face", "face_name", "wall_id",
    "door_code", "event_raw"
]
const DIRECTIONS := ["North", "East", "South", "West"]
const DIRECTION_STEPS := [Vector2i.UP, Vector2i.RIGHT, Vector2i.DOWN, Vector2i.LEFT]
const REPLACEMENT_SCALE := 4
const REPLACEMENT_SIZES := [
    [8, 16], [8, 32], [8, 32], [24, 32], [16, 64],
    [16, 64], [56, 64], [16, 88], [16, 88], [8, 16]
]

@onready var tabs: TabContainer = $Margin/Tabs
@onready var archive_filter: OptionButton = $Margin/Tabs/ArtCatalog/Filters/ArchiveFilter
@onready var view_filter: OptionButton = $Margin/Tabs/ArtCatalog/Filters/ViewFilter
@onready var search: LineEdit = $Margin/Tabs/ArtCatalog/Filters/Search
@onready var catalog_show_replacement: CheckButton = $Margin/Tabs/ArtCatalog/ReplacementRow/ShowReplacement
@onready var contact_grid: ItemList = $Margin/Tabs/ArtCatalog/Split/ContactGrid
@onready var identity: Label = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Identity
@onready var source: Label = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Source
@onready var preview: TextureRect = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/PreviewPanel/Preview
@onready var description_edit: TextEdit = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/DescriptionEdit
@onready var save_description: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/SaveDescription
@onready var usage: Label = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Usage
@onready var previous: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/Navigate/Previous
@onready var next: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/Navigate/Next
@onready var export_button: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/ExportRow/Export
@onready var see_in_map: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/ExportRow/SeeInMap
@onready var status: Label = $Margin/Tabs/ArtCatalog/Status
@onready var map_picker: OptionButton = $Margin/Tabs/MapBrowser/MapToolbar/MapPicker
@onready var profile_picker: OptionButton = $Margin/Tabs/MapBrowser/MapToolbar/ProfilePicker
@onready var map_grid: Control = $Margin/Tabs/MapBrowser/MapSplit/MapPanel/MapColumn/MapCenter/MapGrid
@onready var legend_wall: Label = $Margin/Tabs/MapBrowser/MapSplit/MapPanel/MapColumn/MapLegend/LegendWall
@onready var legend_door: Label = $Margin/Tabs/MapBrowser/MapSplit/MapPanel/MapColumn/MapLegend/LegendDoor
@onready var legend_event: Label = $Margin/Tabs/MapBrowser/MapSplit/MapPanel/MapColumn/MapLegend/LegendEvent
@onready var legend_pose: Label = $Margin/Tabs/MapBrowser/MapSplit/MapPanel/MapColumn/MapLegend/LegendPose
@onready var pose_summary: Label = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/PoseSummary
@onready var facing_picker: OptionButton = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/FacingRow/FacingPicker
@onready var show_replacement: CheckButton = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/FacingRow/ShowReplacement
@onready var first_person: Control = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/FirstPersonPanel/FirstPerson
@onready var visible_pieces: OptionButton = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/VisiblePieces
@onready var picked_tile: Label = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/PickedTile
@onready var piece_preview: TextureRect = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/PieceRow/PiecePanel/PiecePreview
@onready var piece_name: Label = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/PieceRow/PieceDetails/PieceName
@onready var piece_source: Label = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/PieceRow/PieceDetails/PieceSource
@onready var piece_size: Label = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/PieceRow/PieceDetails/PieceSize
@onready var map_export: Button = $Margin/Tabs/MapBrowser/MapSplit/DetailColumn/MapExport
@onready var map_status: Label = $Margin/Tabs/MapBrowser/MapStatus
@onready var save_dialog: FileDialog = $SaveDialog

var catalog_entries: Array = []
var metadata_rows: Array = []
var draft_descriptions: Dictionary = {}
var updating_description := false
var visible_indices: Array[int] = []
var thumbnail_cache: Dictionary = {}
var selected_entry := -1
var export_pending_index := -1
var export_pending_replacement := false
var catalog_maps: Array = []
var selected_map := -1
var selected_profile := 0
var map_pose := Vector2i(8, 8)
var map_facing := 0
var selected_map_entry := -1
var visible_samples: Array = []
var replacement_sets: Dictionary = {}

func _ready() -> void:
    legend_wall.add_theme_color_override("font_color", map_grid.get_theme_color("wall", "MapGrid"))
    legend_door.add_theme_color_override("font_color", map_grid.get_theme_color("door", "MapGrid"))
    legend_event.add_theme_color_override("font_color", map_grid.get_theme_color("event", "MapGrid"))
    legend_pose.add_theme_color_override("font_color", map_grid.get_theme_color("selected", "MapGrid"))
    archive_filter.item_selected.connect(func(_index: int): _rebuild_grid())
    view_filter.item_selected.connect(func(_index: int): _rebuild_grid())
    search.text_changed.connect(func(_text: String): _rebuild_grid())
    contact_grid.item_selected.connect(_select_visible)
    previous.pressed.connect(func(): _navigate(-1))
    next.pressed.connect(func(): _navigate(1))
    export_button.pressed.connect(_show_export_dialog)
    description_edit.text_changed.connect(_on_description_changed)
    save_description.pressed.connect(_save_descriptions)
    see_in_map.pressed.connect(_see_in_map)
    map_export.pressed.connect(_show_map_export_dialog)
    save_dialog.file_selected.connect(_export_png)
    catalog_show_replacement.toggled.connect(_set_replacement_mode)
    show_replacement.toggled.connect(_set_replacement_mode)
    map_picker.item_selected.connect(_select_map)
    profile_picker.item_selected.connect(_select_profile)
    facing_picker.item_selected.connect(_select_facing)
    map_grid.cell_clicked.connect(_jump_to_cell)
    first_person.pixel_clicked.connect(_pick_map_pixel)
    visible_pieces.item_selected.connect(_select_visible_piece)
    for direction in DIRECTIONS:
        facing_picker.add_item(direction)
    view_filter.add_item("All views")
    for view_name in VIEW_NAMES:
        view_filter.add_item(view_name)

    var game_directory := OS.get_environment("OPENGOLD_GAME_DIR")
    if game_directory.is_empty():
        game_directory = str(ProjectSettings.get_setting("opengold/game_directory", ""))
    if game_directory.is_empty():
        _load_failure("Set OPENGOLD_GAME_DIR to the folder containing the original DAX files.")
        return
    var error_message := load_directory(game_directory)
    if not error_message.is_empty():
        _load_failure(error_message)
        return
    catalog_entries = entries()
    catalog_maps = maps()
    error_message = _load_metadata()
    if not error_message.is_empty():
        _load_failure(error_message)
        return
    archive_filter.add_item("All archives")
    var archives: Array[String] = []
    for entry in catalog_entries:
        var name: String = entry["archive"]
        if not archives.has(name):
            archives.append(name)
    for name in archives:
        archive_filter.add_item(name)
    _rebuild_grid()
    for map in catalog_maps:
        map_picker.add_item("%s · %s:%d" % [map["name"], map["archive"], map["record"]])
    var phlan_index := 0
    for index in catalog_maps.size():
        if catalog_maps[index]["name"] == "New Phlan":
            phlan_index = index
            break
    if not catalog_maps.is_empty():
        map_picker.select(phlan_index)
        _select_map(phlan_index)
    _load_replacement_pack(_replacement_pack_path())

    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--viewer-check="):
            call_deferred("_run_check", argument.trim_prefix("--viewer-check="))
        elif argument.begins_with("--viewer-capture="):
            call_deferred("_run_viewer_capture", argument.trim_prefix("--viewer-capture="))
        elif argument.begins_with("--map-check="):
            call_deferred("_run_map_check", argument.trim_prefix("--map-check="))
        elif argument.begins_with("--map-capture="):
            call_deferred("_run_map_capture", argument.trim_prefix("--map-capture="))
        elif argument.begins_with("--metadata-check="):
            call_deferred("_run_metadata_check", argument.trim_prefix("--metadata-check="))
        elif argument.begins_with("--replacement-check="):
            call_deferred("_run_replacement_check", argument.trim_prefix("--replacement-check="))

func _load_failure(message: String) -> void:
    status.text = message
    identity.text = "Artwork unavailable"
    previous.disabled = true
    next.disabled = true
    export_button.disabled = true
    see_in_map.disabled = true
    save_description.disabled = true
    description_edit.editable = false
    map_export.disabled = true
    map_status.text = message
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--viewer-check=") or argument.begins_with("--viewer-capture=") or \
                argument.begins_with("--map-check=") or \
                argument.begins_with("--map-capture=") or \
                argument.begins_with("--metadata-check=") or \
                argument.begins_with("--replacement-check="):
            push_error(message)
            get_tree().quit(1)
            return

func _metadata_path() -> String:
    return ProjectSettings.globalize_path("res://").path_join(METADATA_FILE).simplify_path()

func _replacement_pack_path() -> String:
    return ProjectSettings.globalize_path("res://").path_join(REPLACEMENT_PACK).simplify_path()

func _load_metadata() -> String:
    var path := _metadata_path()
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return "Cannot open tile metadata CSV: %s" % path
    var header := file.get_csv_line()
    if header.size() != CSV_COLUMNS.size():
        return "Tile metadata CSV has the wrong columns."
    for column in CSV_COLUMNS.size():
        if header[column] != CSV_COLUMNS[column]:
            return "Tile metadata CSV column %d should be %s." % [column, CSV_COLUMNS[column]]
    metadata_rows.clear()
    while file.get_position() < file.get_length():
        var fields := file.get_csv_line()
        if fields.size() != CSV_COLUMNS.size():
            return "Tile metadata CSV row %d has %d fields; expected %d." % [
                metadata_rows.size() + 2, fields.size(), CSV_COLUMNS.size()
            ]
        var row: Dictionary = {}
        for column in CSV_COLUMNS.size():
            row[CSV_COLUMNS[column]] = fields[column]
        var index := metadata_rows.size()
        if index >= catalog_entries.size():
            return "Tile metadata CSV has more rows than decoded artwork."
        var entry: Dictionary = catalog_entries[index]
        if row["index"] != str(index):
            return "Tile metadata CSV row %d has the wrong image index." % (index + 2)
        for field in ["archive", "record", "appearance", "view", "width", "height"]:
            if row[field] != str(entry[field]):
                return "Tile metadata CSV row %d disagrees with decoded %s." % [index + 2, field]
        if row["view_name"] != VIEW_NAMES[int(entry["view"])]:
            return "Tile metadata CSV row %d has the wrong view name." % (index + 2)
        if row["description"].strip_edges().is_empty():
            return "Tile metadata CSV row %d has no description." % (index + 2)
        if row["usage"] != "visible" and row["usage"] != "unused":
            return "Tile metadata CSV row %d has invalid map usage." % (index + 2)
        if row["usage"] == "visible":
            if _location_map_index(row) < 0:
                return "Tile metadata CSV row %d has no matching map/profile." % (index + 2)
            for coordinate in ["pose_x", "pose_y", "click_x", "click_y", "facing"]:
                if not str(row[coordinate]).is_valid_int():
                    return "Tile metadata CSV row %d has invalid %s." % [index + 2, coordinate]
            if int(row["pose_x"]) < 0 or int(row["pose_x"]) >= 16 or \
                    int(row["pose_y"]) < 0 or int(row["pose_y"]) >= 16 or \
                    int(row["click_x"]) < 0 or int(row["click_x"]) >= 88 or \
                    int(row["click_y"]) < 0 or int(row["click_y"]) >= 88 or \
                    int(row["facing"]) < 0 or int(row["facing"]) >= 4:
                return "Tile metadata CSV row %d has an out-of-range map location." % (index + 2)
        metadata_rows.append(row)
        entry["description"] = row["description"]
        entry["name"] = row["name"]
    if metadata_rows.size() != catalog_entries.size():
        return "Tile metadata CSV contains %d rows for %d images." % [
            metadata_rows.size(), catalog_entries.size()
        ]
    return ""

func _location_map_index(row: Dictionary) -> int:
    if not str(row["profile_index"]).is_valid_int():
        return -1
    var profile_index := int(row["profile_index"])
    for index in catalog_maps.size():
        var map: Dictionary = catalog_maps[index]
        if map["archive"] != row["map_archive"] or str(map["record"]) != row["map_record"]:
            continue
        var profiles: Array = map["profiles"]
        if profile_index < 0 or profile_index >= profiles.size():
            return -1
        if profiles[profile_index]["label"] != row["profile_label"] or \
                profiles[profile_index]["source"] != row["profile_source"]:
            return -1
        return index
    return -1

func _csv_cell(value: String) -> String:
    if value.contains(",") or value.contains("\"") or value.contains("\n"):
        return "\"%s\"" % value.replace("\"", "\"\"")
    return value

func _write_metadata_csv(path: String) -> String:
    var file := FileAccess.open(path, FileAccess.WRITE)
    if file == null:
        return "Cannot write tile metadata CSV: %s" % error_string(FileAccess.get_open_error())
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
    return "" if result == OK else "Tile metadata CSV write failed: %s" % error_string(result)

func _on_description_changed() -> void:
    if updating_description or selected_entry < 0 or metadata_rows.is_empty():
        return
    var new_text := description_edit.text
    catalog_entries[selected_entry]["description"] = new_text
    if new_text == metadata_rows[selected_entry]["description"]:
        draft_descriptions.erase(selected_entry)
    else:
        draft_descriptions[selected_entry] = new_text
    save_description.disabled = draft_descriptions.is_empty()
    var entry: Dictionary = catalog_entries[selected_entry]
    var position := visible_indices.find(selected_entry)
    if position >= 0:
        contact_grid.set_item_tooltip(position, "%s:%d · appearance %d · %s · %s" % [
            entry["archive"], entry["record"], entry["appearance"],
            VIEW_NAMES[int(entry["view"])], new_text
        ])

func _save_descriptions() -> void:
    if draft_descriptions.is_empty():
        return
    for index in draft_descriptions:
        if str(draft_descriptions[index]).strip_edges().is_empty():
            status.text = "Description for image %d cannot be empty." % index
            return
    var destination := _metadata_path()
    var temporary := destination + ".tmp"
    var error_message := _write_metadata_csv(temporary)
    if error_message.is_empty():
        var result := DirAccess.rename_absolute(temporary, destination)
        if result != OK:
            error_message = "Cannot replace tile metadata CSV: %s" % error_string(result)
    if not error_message.is_empty():
        DirAccess.remove_absolute(temporary)
        status.text = error_message
        return
    for index in draft_descriptions:
        metadata_rows[index]["description"] = draft_descriptions[index]
    draft_descriptions.clear()
    save_description.disabled = true
    status.text = "Descriptions saved to %s" % destination

func _see_in_map() -> void:
    if selected_entry < 0 or metadata_rows[selected_entry]["usage"] != "visible":
        return
    var row: Dictionary = metadata_rows[selected_entry]
    var map_index := _location_map_index(row)
    if map_index < 0:
        status.text = "This artwork's saved map location is unavailable."
        return
    var profile_index := int(row["profile_index"])
    var pose := Vector2i(int(row["pose_x"]), int(row["pose_y"]))
    var facing := int(row["facing"])
    var pixel := Vector2i(int(row["click_x"]), int(row["click_y"]))
    var hit: Dictionary = map_pick(map_index, profile_index, pose.x, pose.y,
                                    facing, pixel.x, pixel.y)
    if hit.is_empty() or int(hit["entry_index"]) != selected_entry:
        status.text = "Saved location no longer renders this exact artwork."
        return
    tabs.current_tab = 1
    map_picker.select(map_index)
    _select_map(map_index)
    profile_picker.select(profile_index)
    _select_profile(profile_index)
    map_pose = pose
    map_facing = facing
    facing_picker.select(facing)
    _refresh_map_view()
    if show_replacement.button_pressed:
        map_status.text = "Jumped to %s · (%d, %d) facing %s · new art shown; switch off Show new art to inspect the source piece." % [
            catalog_maps[map_index]["name"], pose.x, pose.y, DIRECTIONS[facing]
        ]
    else:
        _pick_map_pixel(pixel)
        map_status.text = "Jumped to %s · (%d, %d) facing %s · selected catalog image %d" % [
            catalog_maps[map_index]["name"], pose.x, pose.y, DIRECTIONS[facing], selected_entry
        ]

func _description(entry: Dictionary) -> String:
    return str(entry.get("description", ""))

func _label(entry: Dictionary) -> String:
    return "W%s:%d  #%d\n%s" % [
        str(entry["archive"]).trim_prefix("WALLDEF").trim_suffix(".DAX"),
        entry["record"], entry["appearance"], VIEW_NAMES[int(entry["view"])]
    ]

func _replacement_key(index: int) -> String:
    var entry: Dictionary = catalog_entries[index]
    return "%s:%s:%s" % [entry["archive"], entry["record"], entry["appearance"]]

func _catalog_has_replacement(index: int) -> bool:
    return catalog_show_replacement.button_pressed and replacement_sets.has(_replacement_key(index))

func _catalog_image(index: int) -> Image:
    if _catalog_has_replacement(index):
        return replacement_sets[_replacement_key(index)][int(catalog_entries[index]["view"])]
    return image_at(index)

func _matches(index: int) -> bool:
    var entry: Dictionary = catalog_entries[index]
    if archive_filter.selected > 0 and entry["archive"] != archive_filter.get_item_text(archive_filter.selected):
        return false
    if view_filter.selected > 0 and int(entry["view"]) != view_filter.selected - 1:
        return false
    var query := search.text.strip_edges().to_lower()
    if query.is_empty():
        return true
    return ("%s %s %s %s" % [entry["archive"], _label(entry), entry["name"],
                               _description(entry)]).to_lower().contains(query)

func _thumbnail(index: int) -> Texture2D:
    if thumbnail_cache.has(index):
        return thumbnail_cache[index]
    var image: Image = _catalog_image(index)
    if image == null:
        return null
    var scaled := image.duplicate() as Image
    var ratio: float = minf(80.0 / image.get_width(), 80.0 / image.get_height())
    var scaled_size := Vector2i(maxi(1, roundi(image.get_width() * ratio)),
                                 maxi(1, roundi(image.get_height() * ratio)))
    scaled.resize(scaled_size.x, scaled_size.y, Image.INTERPOLATE_NEAREST)
    var tile := Image.create(88, 88, false, Image.FORMAT_RGBA8)
    tile.fill(Color(0.13, 0.18, 0.21))
    tile.blend_rect(scaled, Rect2i(Vector2i.ZERO, scaled_size),
                    (Vector2i(88, 88) - scaled_size) / 2)
    var texture := ImageTexture.create_from_image(tile)
    thumbnail_cache[index] = texture
    return texture

func _rebuild_grid() -> void:
    contact_grid.clear()
    visible_indices.clear()
    for index in catalog_entries.size():
        if not _matches(index):
            continue
        var entry: Dictionary = catalog_entries[index]
        var item := contact_grid.add_item(_label(entry), _thumbnail(index))
        contact_grid.set_item_tooltip(item, "%s:%d · appearance %d · %s%s" % [
            entry["archive"], entry["record"], entry["appearance"],
            VIEW_NAMES[int(entry["view"])],
            " · " + _description(entry) if not _description(entry).is_empty() else ""
        ])
        visible_indices.append(index)
    if visible_indices.is_empty():
        selected_entry = -1
        preview.texture = null
        identity.text = "No matching artwork"
        source.text = ""
        updating_description = true
        description_edit.text = ""
        updating_description = false
        description_edit.editable = false
        usage.text = "No image selected."
        previous.disabled = true
        next.disabled = true
        export_button.disabled = true
        see_in_map.disabled = true
    else:
        var visible_position := visible_indices.find(selected_entry)
        _select_visible(maxi(0, visible_position))
    var mode := "original art"
    if catalog_show_replacement.button_pressed:
        mode = "%d new sets loaded; missing pieces show originals" % replacement_sets.size()
    status.text = "%d shown · %d pieces total · %s · PNG export uses displayed art" % [
        visible_indices.size(), catalog_entries.size(), mode
    ]

func _select_visible(position: int) -> void:
    if position < 0 or position >= visible_indices.size():
        return
    selected_entry = visible_indices[position]
    contact_grid.select(position)
    contact_grid.ensure_current_is_visible()
    var entry: Dictionary = catalog_entries[selected_entry]
    identity.text = "%s · %s" % [
        str(entry["name"]) if not str(entry["name"]).is_empty() else "Appearance %d" % entry["appearance"],
        VIEW_NAMES[int(entry["view"])]
    ]
    var image: Image = _catalog_image(selected_entry)
    var art_kind := "Original art"
    if _catalog_has_replacement(selected_entry):
        art_kind = "New art"
    elif catalog_show_replacement.button_pressed:
        art_kind = "Original (no replacement)"
    source.text = "%s:%d · #%d · %s · %d × %d px" % [
        entry["archive"], entry["record"], entry["appearance"], art_kind,
        image.get_width() if image != null else 0, image.get_height() if image != null else 0
    ]
    preview.texture = ImageTexture.create_from_image(image) if image != null else null
    updating_description = true
    description_edit.text = _description(entry)
    updating_description = false
    description_edit.editable = true
    var row: Dictionary = metadata_rows[selected_entry]
    if row["usage"] == "visible":
        usage.text = "Used in %s · (%s, %s) facing %s" % [
            row["map_name"], row["pose_x"], row["pose_y"], row["facing_name"]
        ]
        see_in_map.disabled = false
        see_in_map.tooltip_text = "Open this exact visible image in Map Browser"
    else:
        usage.text = "Unused in available maps/profiles; no visible map location."
        see_in_map.disabled = true
        see_in_map.tooltip_text = "This image has no visible map occurrence"
    previous.disabled = position == 0
    next.disabled = position == visible_indices.size() - 1
    export_button.disabled = image == null

func _navigate(delta: int) -> void:
    if visible_indices.is_empty():
        return
    var position := visible_indices.find(selected_entry)
    _select_visible(clampi(position + delta, 0, visible_indices.size() - 1))

func _navigate_row(direction: int) -> void:
    if visible_indices.is_empty():
        return
    var target := visible_indices.find(selected_entry) + direction * _contact_columns()
    if target >= 0 and target < visible_indices.size():
        _select_visible(target)

func _input(event: InputEvent) -> void:
    if not event is InputEventKey or not event.pressed or event.echo:
        return
    if tabs.current_tab == 1:
        _map_key(event)
    else:
        _catalog_key(event)

func _catalog_key(event: InputEventKey) -> void:
    if save_dialog.visible or visible_indices.is_empty():
        return
    var focused := get_viewport().gui_get_focus_owner()
    if focused is LineEdit or focused is TextEdit or focused is OptionButton:
        return
    if focused is TabBar and (event.keycode == KEY_LEFT or event.keycode == KEY_RIGHT):
        return
    match event.keycode:
        KEY_LEFT:
            _navigate(-1)
        KEY_RIGHT:
            _navigate(1)
        KEY_UP:
            _navigate_row(-1)
        KEY_DOWN:
            _navigate_row(1)
        _:
            return
    get_viewport().set_input_as_handled()

func _contact_columns() -> int:
    if contact_grid.item_count < 2:
        return 1
    var first_row_y := contact_grid.get_item_rect(0).position.y
    var columns := 1
    for index in range(1, contact_grid.item_count):
        if contact_grid.get_item_rect(index).position.y != first_row_y:
            break
        columns += 1
    return columns

func _export_name(entry: Dictionary) -> String:
    return "%s_record-%d_appearance-%02d_%s.png" % [
        str(entry["archive"]).trim_suffix(".DAX").to_lower(),
        entry["record"], entry["appearance"],
        str(VIEW_NAMES[int(entry["view"])]).to_lower().replace(" ", "-")
    ]

func _show_export_dialog() -> void:
    if selected_entry < 0:
        return
    export_pending_index = selected_entry
    export_pending_replacement = _catalog_has_replacement(export_pending_index)
    save_dialog.current_file = ("new-" if export_pending_replacement else "") + \
        _export_name(catalog_entries[export_pending_index])
    save_dialog.title = "Export displayed tile art as PNG"
    save_dialog.popup_centered_ratio(0.7)

func _show_map_export_dialog() -> void:
    if selected_map_entry < 0:
        return
    export_pending_index = selected_map_entry
    export_pending_replacement = false
    save_dialog.current_file = _export_name(catalog_entries[export_pending_index])
    save_dialog.title = "Export original tile art as PNG"
    save_dialog.popup_centered_ratio(0.7)

func _export_png(path: String) -> void:
    if export_pending_index < 0:
        return
    var output := path if path.get_extension().to_lower() == "png" else path + ".png"
    var image: Image = image_at(export_pending_index)
    if export_pending_replacement and replacement_sets.has(_replacement_key(export_pending_index)):
        image = replacement_sets[_replacement_key(export_pending_index)][
            int(catalog_entries[export_pending_index]["view"])]
    if image == null:
        _export_status("Selected artwork could not be decoded.")
        return
    var result := image.save_png(output)
    _export_status("Exported %s" % output if result == OK else "PNG export failed: %s" % error_string(result))

func _export_status(message: String) -> void:
    if tabs.current_tab == 1:
        map_status.text = message
    else:
        status.text = message

func _select_map(index: int) -> void:
    if index < 0 or index >= catalog_maps.size():
        return
    selected_map = index
    selected_profile = 0
    map_pose = Vector2i(8, 8)
    map_facing = 0
    facing_picker.select(map_facing)
    profile_picker.clear()
    var profiles: Array = catalog_maps[index]["profiles"]
    for profile in profiles:
        profile_picker.add_item(str(profile["label"]))
    profile_picker.select(0)
    _load_map_grid()
    _refresh_map_view()

func _select_profile(index: int) -> void:
    selected_profile = index
    _load_map_grid()
    _refresh_map_view()

func _select_facing(index: int) -> void:
    map_facing = index
    _refresh_map_view()

func _jump_to_cell(cell: Vector2i) -> void:
    map_pose = cell
    _refresh_map_view()

func _load_map_grid() -> void:
    var cells: Array = []
    for y in 16:
        for x in 16:
            cells.append(map_cell(selected_map, selected_profile, x, y, 6))
    map_grid.set_map(cells, map_pose, map_facing)

func _refresh_map_view() -> void:
    if selected_map < 0:
        return
    map_grid.set_pose(map_pose, map_facing)
    pose_summary.text = "%s · (%d, %d) facing %s" % [
        catalog_maps[selected_map]["name"], map_pose.x, map_pose.y,
        DIRECTIONS[map_facing]
    ]
    if show_replacement.button_pressed and not replacement_sets.is_empty():
        first_person.set_image(_compose_replacement_view())
    else:
        first_person.set_image(map_view(selected_map, selected_profile,
                                        map_pose.x, map_pose.y, map_facing))
    visible_samples = map_visible_pieces(selected_map, selected_profile,
                                         map_pose.x, map_pose.y, map_facing)
    visible_pieces.clear()
    visible_pieces.add_item("Choose a visible piece…")
    for sample in visible_samples:
        visible_pieces.add_item("(%d, %d) %s · %s" % [
            sample["x"], sample["y"], DIRECTIONS[int(sample["face"])],
            VIEW_NAMES[int(sample["view"])]
        ])
    visible_pieces.select(0)
    _clear_map_pick("Click a visible wall or door in the first-person view to inspect it.")
    var profile: Dictionary = catalog_maps[selected_map]["profiles"][selected_profile]
    map_status.text = "%s · %s" % [profile["label"], profile["source"]]
    if show_replacement.button_pressed:
        map_status.text += " · %d new sets loaded; missing sets show local original art. Switch to original art to inspect a clicked source piece." % replacement_sets.size()

func _load_replacement_pack(folder: String) -> void:
    var loaded: Dictionary = {}
    var rejected := 0
    var directories := DirAccess.get_directories_at(folder)
    directories.sort()
    for dirname in directories:
        var directory := folder.path_join(dirname)
        var manifest_path := directory.path_join("manifest.json")
        if not FileAccess.file_exists(manifest_path):
            continue
        var manifest_file := FileAccess.open(manifest_path, FileAccess.READ)
        if manifest_file == null:
            rejected += 1
            continue
        var manifest: Variant = JSON.parse_string(manifest_file.get_as_text())
        if not manifest is Dictionary or manifest.get("format", "") != "opengold-replacement-art-v1":
            rejected += 1
            continue
        var key := str(manifest.get("appearance_key", ""))
        var file_data: Variant = manifest.get("files", {})
        if not file_data is Dictionary:
            rejected += 1
            continue
        var files: Dictionary = file_data
        if key.is_empty() or files.size() != VIEW_NAMES.size():
            rejected += 1
            continue
        var images: Array[Image] = []
        for view in VIEW_NAMES.size():
            var filename := str(files.get(VIEW_NAMES[view], ""))
            var path := directory.path_join(filename)
            if filename.is_empty() or filename.get_file() != filename or \
                    not filename.to_lower().ends_with(".png") or not FileAccess.file_exists(path):
                break
            var image := Image.new()
            if image.load_png_from_buffer(FileAccess.get_file_as_bytes(path)) != OK:
                break
            var expected := Vector2i(REPLACEMENT_SIZES[view][0], REPLACEMENT_SIZES[view][1]) * REPLACEMENT_SCALE
            if image.get_size() != expected:
                break
            image.convert(Image.FORMAT_RGBA8)
            images.append(image)
        if images.size() != VIEW_NAMES.size():
            rejected += 1
            continue
        loaded[key] = images
    if loaded.is_empty():
        var message := "No valid replacement sets found in %s (%d rejected)." % [folder, rejected]
        map_status.text = message
        status.text = message
        return
    replacement_sets = loaded
    catalog_show_replacement.disabled = false
    show_replacement.disabled = false
    var location_hint := "New art: dev_tools/replacement_art_creator/replacement_pack (restart after changing the pack)"
    catalog_show_replacement.tooltip_text = location_hint
    show_replacement.tooltip_text = location_hint
    _set_replacement_mode(true)
    if rejected > 0:
        map_status.text += " · %d invalid sets skipped" % rejected
        status.text += " · %d invalid sets skipped" % rejected

func _set_replacement_mode(enabled: bool) -> void:
    catalog_show_replacement.set_pressed_no_signal(enabled)
    show_replacement.set_pressed_no_signal(enabled)
    thumbnail_cache.clear()
    _rebuild_grid()
    _refresh_map_view()

func _replacement_cell(depth: int, lateral: int, view: int) -> Dictionary:
    var forward: Vector2i = DIRECTION_STEPS[map_facing]
    var cell := map_pose + forward * depth + Vector2i(-forward.y, forward.x) * lateral
    if cell.x < 0 or cell.y < 0 or cell.x >= 16 or cell.y >= 16:
        return {}
    return map_cell(selected_map, selected_profile, cell.x, cell.y, view)

func _replacement_wall(depth: int, lateral: int, face: int) -> int:
    var cell := _replacement_cell(depth, lateral, 0)
    if cell.is_empty():
        return 0
    return int(cell["faces"][face]["wall_id"])

func _replacement_piece(depth: int, lateral: int, face: int, view: int) -> Image:
    var cell := _replacement_cell(depth, lateral, view)
    if cell.is_empty():
        return null
    var index := int(cell["faces"][face]["entry_index"])
    if index < 0 or index >= catalog_entries.size():
        return null
    var key := _replacement_key(index)
    if replacement_sets.has(key):
        return replacement_sets[key][view]
    # The reviewer can inspect a partially completed pack; originals are read
    # from the user's installation and never copied into the exported pack.
    var original: Image = image_at(index)
    if original == null:
        return null
    var fallback := original.duplicate() as Image
    fallback.resize(original.get_width() * REPLACEMENT_SCALE,
                    original.get_height() * REPLACEMENT_SCALE, Image.INTERPOLATE_NEAREST)
    return fallback

func _draw_replacement(canvas: Image, depth: int, lateral: int, face: int,
                       view: int, x: int, y: int) -> void:
    var piece := _replacement_piece(depth, lateral, face, view)
    if piece != null:
        canvas.blend_rect(piece, Rect2i(Vector2i.ZERO, piece.get_size()),
                          Vector2i(x, y) * REPLACEMENT_SCALE)

func _compose_replacement_view() -> Image:
    var size := 88 * REPLACEMENT_SCALE
    var canvas := Image.create(size, size, false, Image.FORMAT_RGBA8)
    var current := _replacement_cell(0, 0, 0)
    var interior := bool(current.get("interior", false))
    canvas.fill(Color8(85, 85, 85) if interior else Color8(85, 255, 255))
    canvas.fill_rect(Rect2i(0, 44 * REPLACEMENT_SCALE, size, 44 * REPLACEMENT_SCALE),
                     Color8(170, 85, 0))
    var left := (map_facing + 3) % 4
    var right := (map_facing + 1) % 4
    for lateral in range(-3, 4):
        var front := _replacement_wall(2, lateral, map_facing)
        if front > 0:
            _draw_replacement(canvas, 2, lateral, map_facing, 0, 40 + lateral * 16, 32)
        if lateral <= 0 and lateral > -3 and (
                _replacement_wall(2, lateral, left) > 0 or
                _replacement_wall(2, lateral - 1, map_facing) > 0):
            _draw_replacement(canvas, 2, lateral, map_facing, 9, 32 + lateral * 16, 32)
        if lateral > 0 and (front > 0 or _replacement_wall(2, lateral - 1, right) > 0):
            _draw_replacement(canvas, 2, lateral - 1, map_facing, 9, 32 + lateral * 16, 32)
    for lateral in range(-2, 3):
        if lateral <= 0:
            _draw_replacement(canvas, 2, lateral, left, 1,
                    32 + lateral * 16 - (8 if lateral < 0 else 0), 24)
        if lateral >= 0:
            _draw_replacement(canvas, 2, lateral, right, 2,
                    48 + lateral * 16 + (8 if lateral > 0 else 0), 24)
    for lateral in range(-2, 3):
        _draw_replacement(canvas, 1, lateral, map_facing, 3, 32 + lateral * 24, 24)
    for lateral in range(-1, 2):
        if lateral <= 0:
            _draw_replacement(canvas, 1, lateral, left, 4, 16 + lateral * 24, 8)
        if lateral >= 0:
            _draw_replacement(canvas, 1, lateral, right, 5, 56 + lateral * 24, 8)
    for lateral in range(-1, 2):
        _draw_replacement(canvas, 0, lateral, map_facing, 6, 16 + lateral * 56, 8)
    _draw_replacement(canvas, 0, 0, left, 7, 0, 0)
    _draw_replacement(canvas, 0, 0, right, 8, 72, 0)
    return canvas

func _clear_map_pick(message: String) -> void:
    selected_map_entry = -1
    picked_tile.text = message
    piece_name.text = "No piece selected"
    piece_source.text = ""
    piece_size.text = ""
    piece_preview.texture = null
    map_export.disabled = true

func _pick_map_pixel(pixel: Vector2i) -> void:
    if selected_map < 0:
        return
    if show_replacement.button_pressed:
        picked_tile.text = "Replacement preview: switch off Show new art to inspect the source mapping."
        return
    var hit: Dictionary = map_pick(selected_map, selected_profile,
                                   map_pose.x, map_pose.y, map_facing,
                                   pixel.x, pixel.y)
    if hit.is_empty():
        visible_pieces.select(0)
        _clear_map_pick("That pixel contains sky or floor, with no wall-art piece.")
        return
    _display_map_hit(hit)
    for index in visible_samples.size():
        var sample: Dictionary = visible_samples[index]
        if sample["x"] == hit["x"] and sample["y"] == hit["y"] and \
                sample["face"] == hit["face"] and sample["view"] == hit["view"]:
            visible_pieces.select(index + 1)
            break

func _select_visible_piece(index: int) -> void:
    if index <= 0 or index > visible_samples.size():
        return
    var sample: Dictionary = visible_samples[index - 1]
    _pick_map_pixel(Vector2i(sample["pixel_x"], sample["pixel_y"]))

func _display_map_hit(hit: Dictionary) -> void:
    selected_map_entry = int(hit["entry_index"])
    if selected_map_entry < 0 or selected_map_entry >= catalog_entries.size():
        _clear_map_pick("The source artwork for this pixel is unavailable.")
        return
    var entry: Dictionary = catalog_entries[selected_map_entry]
    picked_tile.text = "Map cell (%d, %d) · %s face · wall ID %d · door code %d · event 0x%02X" % [
        hit["x"], hit["y"], DIRECTIONS[int(hit["face"])],
        hit["wall_id"], hit["door_code"], hit["event_raw"]
    ]
    var name := _description(entry)
    piece_name.text = "%s · %s" % [
        entry["name"] if not str(entry["name"]).is_empty() else name,
        VIEW_NAMES[int(entry["view"])]
    ]
    piece_source.text = "%s  record %d  ·  appearance %d" % [
        entry["archive"], entry["record"], entry["appearance"]
    ]
    piece_size.text = "%d × %d pixels" % [entry["width"], entry["height"]]
    var image: Image = image_at(selected_map_entry)
    piece_preview.texture = ImageTexture.create_from_image(image) if image != null else null
    map_export.disabled = image == null

func _map_key(event: InputEventKey) -> void:
    if selected_map < 0 or save_dialog.visible:
        return
    var focused := get_viewport().gui_get_focus_owner()
    if focused is OptionButton or focused is LineEdit or focused is TextEdit:
        return
    match event.keycode:
        KEY_LEFT:
            map_facing = (map_facing + 3) % 4
        KEY_RIGHT:
            map_facing = (map_facing + 1) % 4
        KEY_DOWN:
            map_facing = (map_facing + 2) % 4
        KEY_UP:
            if not _advance_map_pose():
                map_status.text = "The map boundary blocks forward movement."
                get_viewport().set_input_as_handled()
                return
        _:
            return
    facing_picker.select(map_facing)
    _refresh_map_view()
    get_viewport().set_input_as_handled()

func _advance_map_pose() -> bool:
    var step: Vector2i = DIRECTION_STEPS[map_facing]
    var target := map_pose + step
    if target.x < 0 or target.x >= 16 or target.y < 0 or target.y >= 16:
        return false
    map_pose = target
    return true

func _run_check(output_path: String) -> void:
    _set_replacement_mode(false)
    if catalog_entries.size() != 2100 or visible_indices.size() != 2100:
        push_error("Expected 2,100 source perspective pieces; got %d" % catalog_entries.size())
        get_tree().quit(1)
        return
    await get_tree().process_frame
    var columns := _contact_columns()
    if columns != 5:
        push_error("Expected five contact-sheet columns at the default window size; got %d" % columns)
        get_tree().quit(1)
        return
    _select_visible(columns + 1)
    contact_grid.grab_focus()
    var vertical := InputEventKey.new()
    vertical.keycode = KEY_UP
    vertical.pressed = true
    Input.parse_input_event(vertical)
    await get_tree().process_frame
    if selected_entry != 1:
        push_error("Up arrow did not select the tile in the previous row")
        get_tree().quit(1)
        return
    vertical.keycode = KEY_DOWN
    Input.parse_input_event(vertical)
    await get_tree().process_frame
    if selected_entry != columns + 1:
        push_error("Down arrow did not select the tile in the next row")
        get_tree().quit(1)
        return
    _select_visible(2)
    vertical.keycode = KEY_UP
    Input.parse_input_event(vertical)
    await get_tree().process_frame
    if selected_entry != 2:
        push_error("Up arrow changed columns at the first row")
        get_tree().quit(1)
        return
    _select_visible(visible_indices.size() - 3)
    vertical.keycode = KEY_DOWN
    Input.parse_input_event(vertical)
    await get_tree().process_frame
    if selected_entry != visible_indices.size() - 3:
        push_error("Down arrow changed columns at the last row")
        get_tree().quit(1)
        return
    _select_visible(0)
    var first := selected_entry
    _navigate(1)
    if selected_entry != first + 1:
        push_error("Next did not select the next artwork")
        get_tree().quit(1)
        return
    _navigate(-1)
    if selected_entry != first:
        push_error("Previous did not restore the artwork")
        get_tree().quit(1)
        return
    search.text = "waterfront"
    _rebuild_grid()
    if visible_indices.size() != 20:
        push_error("Waterfront search did not find both ten-view appearances")
        get_tree().quit(1)
        return
    _select_visible(6) # The near-front view of the waterfront posts.
    var dock: Dictionary = catalog_entries[selected_entry]
    if dock["archive"] != "WALLDEF3.DAX" or dock["record"] != 0 or dock["appearance"] != 14 or dock["view"] != 6:
        push_error("Waterfront search selected the wrong source piece")
        get_tree().quit(1)
        return
    if FileAccess.file_exists(output_path):
        DirAccess.remove_absolute(output_path)
    export_pending_index = selected_entry
    _export_png(output_path)
    var saved := Image.load_from_file(output_path)
    var original: Image = image_at(selected_entry)
    if saved == null or original == null or saved.get_size() != original.get_size() or saved.get_data() != original.get_data():
        push_error("Exported PNG differs from the original decoded pixels")
        get_tree().quit(1)
        return
    print("Tile art viewer checks passed: %d pieces; five columns, arrow navigation, waterfront search and PNG export" % catalog_entries.size())
    get_tree().quit()

func _run_map_check(output_path: String) -> void:
    _set_replacement_mode(false)
    if catalog_maps.size() != 29:
        _map_check_fail("Expected 29 game maps, got %d" % catalog_maps.size())
        return
    var phlan := -1
    var slums := -1
    var well := -1
    var keep := -1
    for index in catalog_maps.size():
        match catalog_maps[index]["name"]:
            "New Phlan": phlan = index
            "Slums": slums = index
            "Kuto's Well": well = index
            "Lizard Men Keep": keep = index
        var image: Image = map_view(index, 0, 8, 8, 0)
        if image == null or image.get_size() != Vector2i(88, 88):
            _map_check_fail("Map %d could not render its first-person view" % index)
            return
    if phlan < 0 or slums < 0 or well < 0 or keep < 0 or \
            catalog_maps[keep]["profiles"].size() != 2:
        _map_check_fail("Named maps or Lizard Men Keep alternate artwork are missing")
        return
    tabs.current_tab = 1
    map_picker.select(keep)
    _select_map(keep)
    profile_picker.select(1)
    _select_profile(1)
    if selected_profile != 1 or first_person.source_image == null or \
            first_person.source_image.get_size() != Vector2i(88, 88):
        _map_check_fail("Alternate Lizard Men Keep artwork could not be selected")
        return
    map_picker.select(phlan)
    _select_map(phlan)
    if map_grid.cells.size() != 256:
        _map_check_fail("Top-down map does not contain 256 cells")
        return
    map_pose = Vector2i(8, 8)
    map_facing = 3
    var west: Dictionary = map_cell(phlan, 0, 8, 8, 6)["faces"][3]
    _refresh_map_view()
    map_grid.grab_focus()
    await get_tree().process_frame
    var advance := InputEventKey.new()
    advance.keycode = KEY_UP
    advance.pressed = true
    Input.parse_input_event(advance)
    await get_tree().process_frame
    if int(west["wall_id"]) == 0 or map_pose != Vector2i(7, 8):
        _map_check_fail("Forward inspection failed: wall=%s pose=%s focus=%s" % [
            west["wall_id"], map_pose, get_viewport().gui_get_focus_owner()
        ])
        return
    map_pose = Vector2i(0, 0)
    _refresh_map_view()
    Input.parse_input_event(advance)
    await get_tree().process_frame
    if map_pose != Vector2i(0, 0):
        _map_check_fail("Forward inspection crossed the map boundary")
        return
    var found := false
    for y in 16:
        for x in 16:
            for direction in 4:
                var pieces: Array = map_visible_pieces(phlan, 0, x, y, direction)
                if not pieces.is_empty():
                    map_pose = Vector2i(x, y)
                    map_facing = direction
                    found = true
                    break
            if found:
                break
        if found:
            break
    if not found:
        _map_check_fail("New Phlan has no clickable wall artwork")
        return
    _refresh_map_view()
    await get_tree().process_frame
    var sample: Dictionary = visible_samples[0]
    var pixel := Vector2i(sample["pixel_x"], sample["pixel_y"])
    var image_rect: Rect2 = first_person._image_rect()
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = image_rect.position + (Vector2(pixel) + Vector2(0.5, 0.5)) / 88.0 * image_rect.size
    first_person._gui_input(click)
    var hit: Dictionary = map_pick(phlan, 0, map_pose.x, map_pose.y,
                                    map_facing, pixel.x, pixel.y)
    if hit.is_empty() or selected_map_entry != hit["entry_index"] or \
            catalog_entries[selected_map_entry]["archive"] != "WALLDEF3.DAX" or \
            catalog_entries[selected_map_entry]["view"] != hit["view"] or \
            catalog_entries[selected_map_entry]["appearance"] != hit["wall_id"]:
        _map_check_fail("Click mismatch: selected=%d expected=%s pixel=%s rect=%s entry=%s" % [
            selected_map_entry, hit, pixel, image_rect,
            catalog_entries[selected_map_entry] if selected_map_entry >= 0 else {}
        ])
        return
    if FileAccess.file_exists(output_path):
        DirAccess.remove_absolute(output_path)
    export_pending_index = selected_map_entry
    _export_png(output_path)
    var saved := Image.load_from_file(output_path)
    var original: Image = image_at(selected_map_entry)
    if saved == null or original == null or saved.get_data() != original.get_data():
        _map_check_fail("Map-tab PNG export differs from the source piece")
        return
    var grid_rect: Rect2 = map_grid._grid_rect()
    var jump := InputEventMouseButton.new()
    jump.button_index = MOUSE_BUTTON_LEFT
    jump.pressed = true
    jump.position = grid_rect.position + Vector2(15.5, 15.5) / 16.0 * grid_rect.size
    map_grid._gui_input(jump)
    if map_pose != Vector2i(15, 15) or selected_map_entry != -1:
        _map_check_fail("Clicking the top-down map did not jump or clear the previous art pick")
        return
    var turn := InputEventKey.new()
    turn.keycode = KEY_RIGHT
    turn.pressed = true
    Input.parse_input_event(turn)
    await get_tree().process_frame
    if map_facing != 1:
        _map_check_fail("Right arrow did not turn the map view as it does in the game")
        return
    print("Map browser checks passed: 29 maps, wall traversal, alternate profile, keyboard turn, map jump, first-person picks and PNG export")
    get_tree().quit()

func _run_metadata_check(output_path: String) -> void:
    _set_replacement_mode(false)
    if metadata_rows.size() != catalog_entries.size() or metadata_rows.size() != 2100:
        _map_check_fail("Metadata CSV does not cover all 2,100 source images")
        return
    if catalog_entries[586]["name"] != "Waterfront posts and chains":
        _map_check_fail("New Phlan's known appearance names were not retained in the CSV")
        return
    var visible_count := 0
    var unused_count := 0
    for index in metadata_rows.size():
        var row: Dictionary = metadata_rows[index]
        if row["usage"] == "unused":
            unused_count += 1
            if not str(row["map_archive"]).is_empty():
                _map_check_fail("Unused image %d has a map destination" % index)
                return
            continue
        visible_count += 1
        var map_index := _location_map_index(row)
        var hit: Dictionary = map_pick(map_index, int(row["profile_index"]),
            int(row["pose_x"]), int(row["pose_y"]), int(row["facing"]),
            int(row["click_x"]), int(row["click_y"]))
        if hit.is_empty() or int(hit["entry_index"]) != index:
            _map_check_fail("CSV location does not display exact image %d" % index)
            return
    if visible_count != 1552 or unused_count != 548:
        _map_check_fail("Metadata usage counts differ: %d visible, %d unused" % [
            visible_count, unused_count
        ])
        return
    _select_visible(0)
    _see_in_map()
    if tabs.current_tab != 1 or selected_map_entry != 0:
        _map_check_fail("See in Map did not select image 0 at its saved location")
        return
    tabs.current_tab = 0
    _select_visible(1)
    if not see_in_map.disabled or not usage.text.contains("Unused"):
        _map_check_fail("Unused image does not explain its missing map destination")
        return
    var original: String = metadata_rows[1]["description"]
    var edited := original + ", with a \"test\" note"
    description_edit.text = edited
    _on_description_changed()
    if save_description.disabled:
        _map_check_fail("Editing a description did not enable Save")
        return
    var write_error := _write_metadata_csv(output_path)
    if not write_error.is_empty():
        _map_check_fail(write_error)
        return
    var saved := FileAccess.open(output_path, FileAccess.READ)
    var header := saved.get_csv_line()
    var first := saved.get_csv_line()
    var second := saved.get_csv_line()
    saved.close()
    if header.size() != CSV_COLUMNS.size() or first[9] != metadata_rows[0]["description"] or \
            second[9] != edited:
        _map_check_fail("Saved CSV did not retain metadata and escaped description text")
        return
    draft_descriptions.clear()
    catalog_entries[1]["description"] = original
    save_description.disabled = true
    print("Tile metadata checks passed: 2,100 descriptions; 1,552 exact map jumps; 548 unused images; CSV edit/save")
    get_tree().quit()

func _run_replacement_check(folder: String) -> void:
    if (replacement_sets.size() != 4 and replacement_sets.size() != 168) or \
            not show_replacement.button_pressed or not catalog_show_replacement.button_pressed:
        _map_check_fail("Bundled replacement pack did not load automatically")
        return
    var row: Dictionary = {}
    for candidate in metadata_rows:
        if candidate["archive"] == "WALLDEF1.DAX" and candidate["record"] == "1" and \
                candidate["appearance"] == "4" and candidate["usage"] == "visible":
            row = candidate
            break
    if row.is_empty():
        _map_check_fail("No map location for the pilot masonry appearance")
        return
    var catalog_index := int(row["index"])
    _select_visible(catalog_index)
    var new_image: Image = preview.texture.get_image() if preview.texture != null else null
    var original_piece: Image = image_at(catalog_index)
    if new_image == null or original_piece == null or \
            new_image.get_size() != original_piece.get_size() * REPLACEMENT_SCALE or \
            not source.text.contains("New art"):
        _map_check_fail("Art Catalog did not show the replacement piece")
        return
    var new_icon: Image = contact_grid.get_item_icon(catalog_index).get_image()
    export_pending_index = catalog_index
    export_pending_replacement = true
    var exported_path := folder.path_join("review-piece.png")
    _export_png(exported_path)
    var exported := Image.load_from_file(exported_path)
    if exported == null or exported.get_size() != new_image.get_size() or \
            exported.get_data() != new_image.get_data():
        _map_check_fail("Art Catalog did not export the displayed replacement piece")
        return
    DirAccess.remove_absolute(exported_path)
    var missing_index := -1
    for index in catalog_entries.size():
        if not replacement_sets.has(_replacement_key(index)):
            missing_index = index
            break
    if missing_index < 0:
        _map_check_fail("Replacement check requires a missing set for fallback")
        return
    _select_visible(missing_index)
    var fallback: Image = preview.texture.get_image() if preview.texture != null else null
    if fallback == null or fallback.get_data() != image_at(missing_index).get_data() or \
            not source.text.contains("Original (no replacement)"):
        _map_check_fail("Art Catalog did not show the original for a missing replacement")
        return
    catalog_show_replacement.button_pressed = false
    _select_visible(catalog_index)
    var source_icon: Image = contact_grid.get_item_icon(catalog_index).get_image()
    if show_replacement.button_pressed or preview.texture.get_image().get_data() != original_piece.get_data() or \
            source_icon.get_data() == new_icon.get_data():
        _map_check_fail("Art Catalog toggle did not restore original art in both tabs")
        return
    catalog_show_replacement.button_pressed = true
    _select_visible(catalog_index)
    _see_in_map()
    if tabs.current_tab != 1 or not map_status.text.contains("new art shown") or selected_map_entry >= 0:
        _map_check_fail("See in Map did not keep replacement mode without selecting an original piece")
        return
    var map_index := _location_map_index(row)
    var replacement: Image = first_person.source_image
    if replacement == null or replacement.get_size() != Vector2i(352, 352):
        _map_check_fail("Map Browser did not render a 352 x 352 replacement preview")
        return
    var original: Image = map_view(selected_map, selected_profile,
                                   map_pose.x, map_pose.y, map_facing)
    original.resize(352, 352, Image.INTERPOLATE_NEAREST)
    if replacement.get_data() == original.get_data():
        _map_check_fail("Replacement preview is identical to the source view")
        return
    var map_review_path := folder.path_join("review-map.png")
    if replacement.save_png(map_review_path) != OK:
        _map_check_fail("Could not save replacement map preview")
        return
    DirAccess.remove_absolute(map_review_path)
    show_replacement.button_pressed = false
    if catalog_show_replacement.button_pressed or first_person.source_image.get_size() != Vector2i(88, 88):
        _map_check_fail("Switching back did not restore original inspection")
        return
    print("Replacement review passed: %d sets loaded, catalog grid/preview/export and map toggle, original fallback restored" % replacement_sets.size())
    get_tree().quit()

func _map_check_fail(message: String) -> void:
    push_error(message)
    get_tree().quit(1)

func _run_viewer_capture(output_path: String) -> void:
    tabs.current_tab = 0
    await RenderingServer.frame_post_draw
    var result := get_viewport().get_texture().get_image().save_png(output_path)
    if result != OK:
        push_error("Art catalog screenshot failed: %s" % error_string(result))
        get_tree().quit(1)
        return
    print("Art catalog screenshot saved: %s · %d columns · %s grid" % [
        output_path, _contact_columns(), contact_grid.size
    ])
    get_tree().quit()

func _run_map_capture(output_path: String) -> void:
    tabs.current_tab = 1
    if not visible_samples.is_empty():
        var sample: Dictionary = visible_samples[0]
        _pick_map_pixel(Vector2i(sample["pixel_x"], sample["pixel_y"]))
    await RenderingServer.frame_post_draw
    var result := get_viewport().get_texture().get_image().save_png(output_path)
    if result != OK:
        push_error("Map screenshot failed: %s" % error_string(result))
        get_tree().quit(1)
        return
    print("Map browser screenshot saved: %s" % output_path)
    get_tree().quit()
