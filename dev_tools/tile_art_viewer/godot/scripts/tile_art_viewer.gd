extends WallArtCatalog

const VIEW_NAMES := [
    "Far front", "Far left", "Far right", "Middle front", "Middle left",
    "Middle right", "Near front", "Near left", "Near right", "Far extension"
]
const PHLAN_NAMES := [
    "", "Open gateway", "Gray wall", "Gray wall and door", "Ivy wall",
    "Ivy wall and door", "Light masonry A", "Light masonry and door",
    "Light masonry B", "Shopfront", "Stone facade", "City Hall",
    "Training hall", "Temple entrance", "Waterfront posts and chains",
    "Low waterfront wall and water"
]
const DIRECTIONS := ["North", "East", "South", "West"]
const DIRECTION_STEPS := [Vector2i.UP, Vector2i.RIGHT, Vector2i.DOWN, Vector2i.LEFT]

@onready var tabs: TabContainer = $Margin/Tabs
@onready var archive_filter: OptionButton = $Margin/Tabs/ArtCatalog/Filters/ArchiveFilter
@onready var view_filter: OptionButton = $Margin/Tabs/ArtCatalog/Filters/ViewFilter
@onready var search: LineEdit = $Margin/Tabs/ArtCatalog/Filters/Search
@onready var contact_grid: ItemList = $Margin/Tabs/ArtCatalog/Split/ContactGrid
@onready var identity: Label = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Identity
@onready var source: Label = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Source
@onready var preview: TextureRect = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/PreviewPanel/Preview
@onready var previous: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/Previous
@onready var next: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/Next
@onready var export_button: Button = $Margin/Tabs/ArtCatalog/Split/PreviewColumn/Actions/Export
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
var visible_indices: Array[int] = []
var thumbnail_cache: Dictionary = {}
var selected_entry := -1
var export_pending_index := -1
var catalog_maps: Array = []
var selected_map := -1
var selected_profile := 0
var map_pose := Vector2i(8, 8)
var map_facing := 0
var selected_map_entry := -1
var visible_samples: Array = []

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
    map_export.pressed.connect(_show_map_export_dialog)
    save_dialog.file_selected.connect(_export_png)
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
    archive_filter.add_item("All archives")
    var archives: Array[String] = []
    for entry in catalog_entries:
        var name: String = entry["archive"]
        if not archives.has(name):
            archives.append(name)
    for name in archives:
        archive_filter.add_item(name)
    _rebuild_grid()
    catalog_maps = maps()
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

    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--viewer-check="):
            call_deferred("_run_check", argument.trim_prefix("--viewer-check="))
        elif argument.begins_with("--viewer-capture="):
            call_deferred("_run_viewer_capture", argument.trim_prefix("--viewer-capture="))
        elif argument.begins_with("--map-check="):
            call_deferred("_run_map_check", argument.trim_prefix("--map-check="))
        elif argument.begins_with("--map-capture="):
            call_deferred("_run_map_capture", argument.trim_prefix("--map-capture="))

func _load_failure(message: String) -> void:
    status.text = message
    identity.text = "Artwork unavailable"
    previous.disabled = true
    next.disabled = true
    export_button.disabled = true
    map_export.disabled = true
    map_status.text = message
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--viewer-check=") or argument.begins_with("--viewer-capture=") or \
                argument.begins_with("--map-check=") or \
                argument.begins_with("--map-capture="):
            push_error(message)
            get_tree().quit(1)
            return

func _description(entry: Dictionary) -> String:
    if entry["archive"] == "WALLDEF3.DAX" and entry["record"] == 0:
        return PHLAN_NAMES[int(entry["appearance"])]
    return ""

func _label(entry: Dictionary) -> String:
    return "W%s:%d  #%d\n%s" % [
        str(entry["archive"]).trim_prefix("WALLDEF").trim_suffix(".DAX"),
        entry["record"], entry["appearance"], VIEW_NAMES[int(entry["view"])]
    ]

func _matches(index: int) -> bool:
    var entry: Dictionary = catalog_entries[index]
    if archive_filter.selected > 0 and entry["archive"] != archive_filter.get_item_text(archive_filter.selected):
        return false
    if view_filter.selected > 0 and int(entry["view"]) != view_filter.selected - 1:
        return false
    var query := search.text.strip_edges().to_lower()
    if query.is_empty():
        return true
    return ("%s %s %s" % [entry["archive"], _label(entry), _description(entry)]).to_lower().contains(query)

func _thumbnail(index: int) -> Texture2D:
    if thumbnail_cache.has(index):
        return thumbnail_cache[index]
    var image: Image = image_at(index)
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
        previous.disabled = true
        next.disabled = true
        export_button.disabled = true
    else:
        var visible_position := visible_indices.find(selected_entry)
        _select_visible(maxi(0, visible_position))
    status.text = "%d shown · %d pieces total · original PNG export preserves transparency" % [
        visible_indices.size(), catalog_entries.size()
    ]

func _select_visible(position: int) -> void:
    if position < 0 or position >= visible_indices.size():
        return
    selected_entry = visible_indices[position]
    contact_grid.select(position)
    contact_grid.ensure_current_is_visible()
    var entry: Dictionary = catalog_entries[selected_entry]
    var name := _description(entry)
    identity.text = "%s%s" % [name + " · " if not name.is_empty() else "",
                                VIEW_NAMES[int(entry["view"])]]
    source.text = "%s  record %d  ·  appearance %d  ·  %d × %d pixels" % [
        entry["archive"], entry["record"], entry["appearance"],
        entry["width"], entry["height"]
    ]
    var image: Image = image_at(selected_entry)
    preview.texture = ImageTexture.create_from_image(image) if image != null else null
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
    if focused is LineEdit or focused is OptionButton:
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
    save_dialog.current_file = _export_name(catalog_entries[export_pending_index])
    save_dialog.popup_centered_ratio(0.7)

func _show_map_export_dialog() -> void:
    if selected_map_entry < 0:
        return
    export_pending_index = selected_map_entry
    save_dialog.current_file = _export_name(catalog_entries[export_pending_index])
    save_dialog.popup_centered_ratio(0.7)

func _export_png(path: String) -> void:
    if export_pending_index < 0:
        return
    var output := path if path.get_extension().to_lower() == "png" else path + ".png"
    var image: Image = image_at(export_pending_index)
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
    piece_name.text = "%s · %s" % [name if not name.is_empty() else "Appearance %d" % entry["appearance"],
                                     VIEW_NAMES[int(entry["view"])]]
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
    if focused is OptionButton or focused is LineEdit:
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
