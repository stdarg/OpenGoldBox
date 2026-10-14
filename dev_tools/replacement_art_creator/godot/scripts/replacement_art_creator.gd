extends Control

const Builder = preload("res://scripts/perspective_builder.gd")
const INDEX_PATH := "../data/appearance_index.csv"
const BRIEFS_PATH := "../data/briefs.json"
const KINDS := ["solid", "door", "opening", "landmark"]

@onready var search: LineEdit = $Margin/Main/Split/ListColumn/Search
@onready var appearance_list: ItemList = $Margin/Main/Split/ListColumn/AppearanceList
@onready var coverage: Label = $Margin/Main/Split/ListColumn/Coverage
@onready var identity: Label = $Margin/Main/Split/Editor/Identity
@onready var title_edit: LineEdit = $Margin/Main/Split/Editor/Fields/TitleEdit
@onready var kind_picker: OptionButton = $Margin/Main/Split/Editor/Fields/KindPicker
@onready var brief_edit: TextEdit = $Margin/Main/Split/Editor/BriefEdit
@onready var save_brief: Button = $Margin/Main/Split/Editor/BriefActions/SaveBrief
@onready var copy_wall_prompt: Button = $Margin/Main/Split/Editor/BriefActions/CopyWallPrompt
@onready var copy_feature_prompt: Button = $Margin/Main/Split/Editor/BriefActions/CopyFeaturePrompt
@onready var import_base: Button = $Margin/Main/Split/Editor/ArtActions/ImportBase
@onready var import_feature: Button = $Margin/Main/Split/Editor/ArtActions/ImportFeature
@onready var build_views: Button = $Margin/Main/Split/Editor/ArtActions/BuildViews
@onready var export_set: Button = $Margin/Main/Split/Editor/ArtActions/ExportSet
@onready var sources: Label = $Margin/Main/Split/Editor/Sources
@onready var depth_picker: OptionButton = $Margin/Main/Split/Editor/PreviewSplit/CorridorColumn/DepthRow/DepthPicker
@onready var corridor_preview: TextureRect = $Margin/Main/Split/Editor/PreviewSplit/CorridorColumn/CorridorPanel/CorridorPreview
@onready var piece_grid: ItemList = $Margin/Main/Split/Editor/PreviewSplit/PiecesColumn/PieceGrid
@onready var piece_name: Label = $Margin/Main/Split/Editor/PreviewSplit/PiecesColumn/PieceName
@onready var piece_preview: TextureRect = $Margin/Main/Split/Editor/PreviewSplit/PiecesColumn/PiecePanel/PiecePreview
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

func _ready() -> void:
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
    var error := _load_index()
    if error.is_empty():
        error = _load_briefs()
    if not error.is_empty():
        status.text = error
        push_error(error)
        return
    _rebuild_list()
    var pilot := "WALLDEF1.DAX:1:4"
    if keys.has(pilot):
        _select_key(pilot)
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--creator-check="):
            call_deferred("_run_check", argument.trim_prefix("--creator-check="))
        elif argument.begins_with("--creator-capture="):
            call_deferred("_capture", argument.trim_prefix("--creator-capture="))

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

func _rebuild_list() -> void:
    var query := search.text.strip_edges().to_lower()
    visible_keys.clear()
    appearance_list.clear()
    for key in keys:
        var record: Dictionary = records.get(key, {})
        var title := str(record.get("title", ""))
        if not query.is_empty() and not (key + " " + title).to_lower().contains(query):
            continue
        visible_keys.append(key)
        appearance_list.add_item("%s  %s" % [key, title])
    if visible_keys.has(selected_key):
        appearance_list.select(visible_keys.find(selected_key))
    var complete := 0
    for key in keys:
        var record: Dictionary = records.get(key, {})
        if not str(record.get("base", "")).is_empty():
            complete += 1
    coverage.text = "%d / %d appearances have new art" % [complete, keys.size()]

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

func _capture(path: String) -> void:
    await RenderingServer.frame_post_draw
    var error := get_viewport().get_texture().get_image().save_png(path)
    if error != OK:
        push_error("Cannot save creator screenshot: %s" % error_string(error))
        get_tree().quit(1)
        return
    print("Replacement art creator screenshot saved: %s" % path)
    get_tree().quit()
