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

@onready var archive_filter: OptionButton = $Margin/Column/Filters/ArchiveFilter
@onready var view_filter: OptionButton = $Margin/Column/Filters/ViewFilter
@onready var search: LineEdit = $Margin/Column/Filters/Search
@onready var contact_grid: ItemList = $Margin/Column/Split/ContactGrid
@onready var identity: Label = $Margin/Column/Split/PreviewColumn/Identity
@onready var source: Label = $Margin/Column/Split/PreviewColumn/Source
@onready var preview: TextureRect = $Margin/Column/Split/PreviewColumn/PreviewPanel/Preview
@onready var previous: Button = $Margin/Column/Split/PreviewColumn/Actions/Previous
@onready var next: Button = $Margin/Column/Split/PreviewColumn/Actions/Next
@onready var export_button: Button = $Margin/Column/Split/PreviewColumn/Actions/Export
@onready var status: Label = $Margin/Column/Status
@onready var save_dialog: FileDialog = $SaveDialog

var catalog_entries: Array = []
var visible_indices: Array[int] = []
var thumbnail_cache: Dictionary = {}
var selected_entry := -1

func _ready() -> void:
    archive_filter.item_selected.connect(func(_index: int): _rebuild_grid())
    view_filter.item_selected.connect(func(_index: int): _rebuild_grid())
    search.text_changed.connect(func(_text: String): _rebuild_grid())
    contact_grid.item_selected.connect(_select_visible)
    previous.pressed.connect(func(): _navigate(-1))
    next.pressed.connect(func(): _navigate(1))
    export_button.pressed.connect(_show_export_dialog)
    save_dialog.file_selected.connect(_export_png)
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

    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--viewer-check="):
            call_deferred("_run_check", argument.trim_prefix("--viewer-check="))

func _load_failure(message: String) -> void:
    status.text = message
    identity.text = "Artwork unavailable"
    previous.disabled = true
    next.disabled = true
    export_button.disabled = true
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--viewer-check="):
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

func _unhandled_key_input(event: InputEvent) -> void:
    if not event is InputEventKey or not event.pressed or event.echo:
        return
    if get_viewport().gui_get_focus_owner() == search:
        return
    if event.keycode == KEY_LEFT:
        _navigate(-1)
        get_viewport().set_input_as_handled()
    elif event.keycode == KEY_RIGHT:
        _navigate(1)
        get_viewport().set_input_as_handled()

func _export_name(entry: Dictionary) -> String:
    return "%s_record-%d_appearance-%02d_%s.png" % [
        str(entry["archive"]).trim_suffix(".DAX").to_lower(),
        entry["record"], entry["appearance"],
        str(VIEW_NAMES[int(entry["view"])]).to_lower().replace(" ", "-")
    ]

func _show_export_dialog() -> void:
    if selected_entry < 0:
        return
    save_dialog.current_file = _export_name(catalog_entries[selected_entry])
    save_dialog.popup_centered_ratio(0.7)

func _export_png(path: String) -> void:
    if selected_entry < 0:
        return
    var output := path if path.get_extension().to_lower() == "png" else path + ".png"
    var image: Image = image_at(selected_entry)
    if image == null:
        status.text = "Selected artwork could not be decoded."
        return
    var result := image.save_png(output)
    status.text = "Exported %s" % output if result == OK else "PNG export failed: %s" % error_string(result)

func _run_check(output_path: String) -> void:
    if catalog_entries.size() != 2100 or visible_indices.size() != 2100:
        push_error("Expected 2,100 source perspective pieces; got %d" % catalog_entries.size())
        get_tree().quit(1)
        return
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
    _export_png(output_path)
    var saved := Image.load_from_file(output_path)
    var original: Image = image_at(selected_entry)
    if saved == null or original == null or saved.get_size() != original.get_size() or saved.get_data() != original.get_data():
        push_error("Exported PNG differs from the original decoded pixels")
        get_tree().quit(1)
        return
    print("Tile art viewer checks passed: %d pieces; navigation, waterfront search and PNG export" % catalog_entries.size())
    get_tree().quit()
