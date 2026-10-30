extends WallArtCatalog

const SHOP_TYPES := [
    "Arms & Armor", "General Items", "Silver Items", "Jewelry", "Rogue's Shop",
    "Magic User's Shop",
]
const TEMPLE_SERVICES := [
    ["cure_blindness", "Cure Blindness", 1000],
    ["cure_disease", "Cure Disease", 1000],
    ["cure_light_wounds", "Cure Light Wounds", 100],
    ["cure_serious_wounds", "Cure Serious Wounds", 350],
    ["cure_critical_wounds", "Cure Critical Wounds", 600],
    ["neutralize_poison", "Neutralize Poison", 1000],
    ["raise_dead", "Raise Dead", 5500],
    ["remove_curse", "Remove Curse", 3500],
    ["stone_to_flesh", "Stone to Flesh", 2000],
    # OpenGoldBox's addition: on Easy and Normal, for a member Raise Dead
    # cannot help (TEMPLE-1).
    ["resurrection", "Resurrection", 11000],
]
# A record names its portrait as the game finds it, in its portraits folder.
# The game ships art/portraits itself; a portrait drawn for this tool is
# exported to GAME_PORTRAITS, which the game ships as its portraits' shops.
const GAME_ART := "art/portraits/"
const MANAGER_PORTRAITS := "dev_tools/shop_manager/portraits/"
const GAME_PORTRAITS := "data/art/portraits/shops"
const PORTRAIT_NOTE := "Add new PNGs to dev_tools/shop_manager/portraits. A chosen portrait is exported to the game."
# Arrows and quarrels: the game has no ammunition (AMMO-UNLIMITED), so no
# shop offers them.
const AMMUNITION_TYPES := [28, 73]
const ADDED_GEAR := [
    ["10 Torches", 1, 10, 200],
    ["Vial of Acid", 25, 1, 201],
    ["Flask of Alchemist's Fire", 50, 1, 202],
]
# Gear the game knows that no original stock holds; the picker offers it to
# any shop, and the Rogue's Shop starts with it (#271).
const PICKER_GEAR := [
    ["Poisoner's Kit", 50, 1, 212],
    ["Basic Poison", 100, 1, 213],
    ["Thieves' Tools", 25, 1, 214],
    ["Magic Lock Picks", 50, 1, 215],
    ["Boots of Elvenkind", 400, 1, 216],
    ["Cloak of Elvenkind", 400, 1, 217],
    ["Stone of Good Luck", 400, 1, 218],
    ["Wand of Secrets", 400, 3, 219],
]
# An original item sold enchanted: the armory's Leather Armor with +1, which
# no original stock holds (SECRET-1).
const PICKER_ENCHANTED := [
    {"source_archive": "ITEM3.DAX", "source_record": 53, "source_index": 9,
     "name": "Leather Armor +1", "item_type": 50, "magic_bonus": 1, "price_gp": 4000,
     "bundle_quantity": 1, "quantity": -1},
]
const SIDES := ["north", "east", "south", "west"]
const LOCATION_COUNT := 17

var manifest: Dictionary = {}
var records: Dictionary = {}
var map_indices: Dictionary = {}
var selected_id := ""
var current_map := "New Phlan"
var visible_ids: Array[String] = []
var portrait_paths: Array[String] = []
var catalog_matches: Array[Dictionary] = []
var catalog_items: Array = []
var map_cells: Array = []
var data_dir := ""
var repo_dir := ""

var status_label: Label
var map_choice: OptionButton
var map_widget: Control
var location_list: ItemList
var title_label: Label
var fixed_label: Label
var type_choice: OptionButton
var portrait_choice: OptionButton
var portrait_view: TextureRect
var portrait_note: Label
var row_box: VBoxContainer
var stock_title: Label
var action_title: Label
var add_search: LineEdit
var add_choice: OptionButton
var add_row: HBoxContainer

func _ready() -> void:
    repo_dir = ProjectSettings.globalize_path("res://../../..").simplify_path()
    # The game reads these files too (TEMPLE-1).
    data_dir = repo_dir.path_join("data/shops")
    var parsed: Variant = _read_json(data_dir.path_join("locations.json"))
    if not parsed is Dictionary:
        _fatal("Cannot read fixed shop locations")
        return
    manifest = parsed
    if int(manifest.get("version", 0)) != 1 or manifest.get("locations", []).size() != LOCATION_COUNT:
        _fatal("Unexpected fixed shop location manifest")
        return
    var game_dir := OS.get_environment("OPENGOLD_GAME_DIR")
    if game_dir.is_empty():
        game_dir = OS.get_environment("HOME").path_join("POOLRAD")
    var load_error: String = load_directory(game_dir)
    if not load_error.is_empty():
        _fatal(load_error)
        return
    load_error = load_shop_directory(game_dir)
    if not load_error.is_empty():
        _fatal(load_error)
        return
    for i in maps().size():
        var map: Dictionary = maps()[i]
        map_indices[String(map["archive"]) + ":" + str(map["record"])] = i
    for map in manifest["maps"]:
        if not map_indices.has(String(map["archive"]) + ":" + str(int(map["record"]))):
            _fatal("Missing map " + String(map["name"]))
            return
    for location in manifest["locations"]:
        var path := _record_path(String(location["id"]))
        var record: Variant = _read_json(path) if FileAccess.file_exists(path) else _default_record(location)
        if not record is Dictionary or not _valid_record(record, location):
            _fatal("Invalid shop data: " + path)
            return
        records[String(location["id"])] = record
    catalog_items = _without_ammunition(item_catalog())
    catalog_items.append_array(_authored_gear())
    catalog_items.append_array(_gear_offerings(PICKER_GEAR))
    catalog_items.append_array(PICKER_ENCHANTED.duplicate(true))
    if "--seed" in OS.get_cmdline_user_args():
        for location in manifest["locations"]:
            var id := String(location["id"])
            if not FileAccess.file_exists(_record_path(id)) and not _write_record(id):
                get_tree().quit(1)
                return
        print("Seeded %d fixed shops and temples" % LOCATION_COUNT)
        get_tree().quit()
        return
    if "--export-portraits" in OS.get_cmdline_user_args():
        var export_error := _export_portraits()
        if not export_error.is_empty():
            _fatal(export_error)
            return
        print("Exported the portraits to " + GAME_PORTRAITS)
        get_tree().quit()
        return
    if "--shop-check" in OS.get_cmdline_user_args():
        _run_check()
        return
    _build_ui()
    _scan_portraits()
    _select_map("New Phlan")
    _select_location("phlan-arms-13-08")
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--select="):
            _select_location(arg.trim_prefix("--select="))
        if arg.begins_with("--capture="):
            _capture.call_deferred(arg.trim_prefix("--capture="))

# The game's name for a portrait this tool lists by its repository path.
func _game_portrait(source: String) -> String:
    if source.begins_with(MANAGER_PORTRAITS):
        return "shops/" + source.trim_prefix(MANAGER_PORTRAITS)
    return source.trim_prefix(GAME_ART)

# The repository file a record's portrait comes from.
func _source_portrait(portrait: String) -> String:
    if portrait.is_empty():
        return ""
    if portrait.begins_with("shops/"):
        return MANAGER_PORTRAITS + portrait.trim_prefix("shops/")
    return GAME_ART + portrait

# The manager's portraits the records use, by file name.
func _used_manager_portraits() -> Dictionary:
    var used := {}
    for id in records:
        var source := _source_portrait(String(records[id]["portrait"]))
        if source.begins_with(MANAGER_PORTRAITS):
            used[source.get_file()] = repo_dir.path_join(source)
    return used

# Copies each manager portrait a record uses where the game ships it, and
# removes copies no record uses. Returns an error message, or "".
func _export_portraits() -> String:
    var target := repo_dir.path_join(GAME_PORTRAITS)
    if DirAccess.make_dir_recursive_absolute(target) != OK:
        return "Cannot create " + target
    var used := _used_manager_portraits()
    for name in used:
        var source: String = used[name]
        var destination := target.path_join(name)
        if not FileAccess.file_exists(source):
            return "Missing portrait " + source
        if FileAccess.get_md5(source) != FileAccess.get_md5(destination) and \
                DirAccess.copy_absolute(source, destination) != OK:
            return "Cannot export " + source
    for name in DirAccess.get_files_at(target):
        if name.get_extension() == "png" and not used.has(name) and \
                DirAccess.remove_absolute(target.path_join(name)) != OK:
            return "Cannot remove " + target.path_join(name)
    return ""

# Whether the game has every manager portrait the records use, as drawn.
func _portraits_exported() -> bool:
    var used := _used_manager_portraits()
    for name in used:
        var copy := repo_dir.path_join(GAME_PORTRAITS).path_join(name)
        if not FileAccess.file_exists(copy) or FileAccess.get_md5(copy) != FileAccess.get_md5(used[name]):
            return false
    return true

func _fatal(message: String) -> void:
    push_error(message)
    if DisplayServer.get_name() == "headless":
        get_tree().quit(1)
        return
    var label := Label.new()
    label.text = message
    label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    add_child(label)

func _read_json(path: String) -> Variant:
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return null
    return JSON.parse_string(file.get_as_text())

func _record_path(id: String) -> String:
    return data_dir.path_join(id + ".json")

func _fixed_location(location: Dictionary) -> Dictionary:
    var map: Dictionary = _map_meta(String(location["map"]))
    return {"map": location["map"], "archive": map["archive"],
            "record": map["record"], "markers": location["markers"]}

func _map_meta(name: String) -> Dictionary:
    for map in manifest["maps"]:
        if String(map["name"]) == name:
            return map
    return {}

func _location_meta(id: String) -> Dictionary:
    for location in manifest["locations"]:
        if String(location["id"]) == id:
            return location
    return {}

func _default_record(location: Dictionary) -> Dictionary:
    var temple := String(location["kind"]) == "temple"
    var offerings: Array = []
    if temple:
        for service in TEMPLE_SERVICES:
            offerings.append({"service_id": service[0], "name": service[1],
                              "price_gp": service[2], "offered": true})
    elif location.has("entrance"):
        # OpenGoldBox's own shop has no original list (SECRET-1).
        offerings = _gear_offerings(PICKER_GEAR)
    else:
        offerings = _without_ammunition(shop_stock(int(location["source_record"])))
        if int(location["source_record"]) == 54:
            offerings.append_array(_authored_gear())
    return {"version": 1, "id": location["id"], "name": location["name"],
            "kind": location["kind"], "fixed_location": _fixed_location(location),
            "type": "Temple" if temple else location["default_type"],
            "portrait": location["portrait"], "offerings": offerings}

func _without_ammunition(items: Array) -> Array:
    return items.filter(func(item: Dictionary) -> bool:
        return not int(item["item_type"]) in AMMUNITION_TYPES)

func _gear_offerings(gear_list: Array) -> Array:
    var result: Array = []
    for gear in gear_list:
        result.append({"source_archive": "OpenGold authored gear", "source_record": -1,
                       "source_index": -1, "name": gear[0], "item_type": gear[3],
                       "price_gp": gear[1], "bundle_quantity": gear[2], "quantity": -1})
    return result

func _authored_gear() -> Array:
    var result := _gear_offerings(ADDED_GEAR)
    for level in range(1, 9):
        result.append({"source_archive": "OpenGold authored gear", "source_record": -1,
                       "source_index": -1, "name": "Bottle of Magic Ink (Level %d)" % level,
                       "item_type": 202 + level, "price_gp": 1000 * level,
                       "bundle_quantity": 1, "quantity": -1})
    return result

func _valid_record(record: Dictionary, location: Dictionary) -> bool:
    if int(record.get("version", 0)) != 1 or String(record.get("id", "")) != String(location["id"]):
        return false
    if String(record.get("kind", "")) != String(location["kind"]):
        return false
    if record.get("fixed_location", {}) != _fixed_location(location):
        return false
    if location["kind"] == "temple":
        if record.get("type", "") != "Temple":
            return false
    elif not SHOP_TYPES.has(record.get("type", "")):
        return false
    if not record.get("offerings") is Array or not record.get("portrait") is String:
        return false
    for entry in record["offerings"]:
        if not entry is Dictionary or int(entry.get("price_gp", -1)) < 0:
            return false
        if location["kind"] == "shop" and (not entry.has("name") or int(entry.get("quantity", -2)) < -1):
            return false
        if location["kind"] == "temple" and (not entry.has("service_id") or not entry.has("offered")):
            return false
    return true

func _write_record(id: String) -> bool:
    var record: Dictionary = records[id]
    var location := _location_meta(id)
    if not _valid_record(record, location):
        _set_status("Save refused: invalid " + id, true)
        return false
    var path := _record_path(id)
    var directory := path.get_base_dir()
    var error := DirAccess.make_dir_recursive_absolute(directory)
    if error != OK:
        _set_status("Cannot create " + directory, true)
        return false
    var temporary := path + ".tmp"
    var file := FileAccess.open(temporary, FileAccess.WRITE)
    if file == null:
        _set_status("Cannot write " + temporary, true)
        return false
    file.store_string(JSON.stringify(record, "  ") + "\n")
    file.flush()
    file.close()
    error = DirAccess.rename_absolute(temporary, path)
    if error != OK:
        _set_status("Cannot replace " + path, true)
        return false
    _set_status("Saved " + id)
    return true

func _set_status(message: String, bad := false) -> void:
    if status_label != null:
        status_label.text = message
        status_label.add_theme_color_override("font_color", Color("ff9d91") if bad else Color("b4d4c4"))
    elif bad:
        push_error(message)

func _make_label(text: String) -> Label:
    var label := Label.new()
    label.text = text
    return label

func _build_ui() -> void:
    var background := ColorRect.new()
    background.color = Color("101820")
    background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    background.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(background)
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    margin.add_theme_constant_override("margin_left", 18)
    margin.add_theme_constant_override("margin_right", 18)
    margin.add_theme_constant_override("margin_top", 14)
    margin.add_theme_constant_override("margin_bottom", 14)
    add_child(margin)
    var outer := VBoxContainer.new()
    outer.add_theme_constant_override("separation", 10)
    margin.add_child(outer)
    var heading := _make_label("Shop & Temple Manager")
    heading.add_theme_font_size_override("font_size", 26)
    outer.add_child(heading)
    var subtitle := _make_label("Select a fixed location on the map, then edit its offerings, prices and portrait.")
    outer.add_child(subtitle)
    var split := HSplitContainer.new()
    split.size_flags_vertical = Control.SIZE_EXPAND_FILL
    split.split_offset = 360
    outer.add_child(split)
    var left := VBoxContainer.new()
    left.custom_minimum_size = Vector2(320, 0)
    left.add_theme_constant_override("separation", 8)
    split.add_child(left)
    var map_header := HBoxContainer.new()
    left.add_child(map_header)
    map_header.add_child(_make_label("Map"))
    map_choice = OptionButton.new()
    map_choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    for map in manifest["maps"]:
        map_choice.add_item(String(map["name"]))
    map_choice.item_selected.connect(_on_map_selected)
    map_header.add_child(map_choice)
    map_widget = Control.new()
    map_widget.set_script(load("res://scripts/shop_map.gd"))
    map_widget.custom_minimum_size = Vector2(316, 316)
    map_widget.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    map_widget.location_selected.connect(_select_location)
    left.add_child(map_widget)
    left.add_child(_make_label("■ Shop    ✚ Temple    Gold: doors    Red: secret doors    Blue: walls"))
    left.add_child(_make_label("Locations on this map"))
    location_list = ItemList.new()
    location_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
    location_list.item_selected.connect(_on_location_list_selected)
    left.add_child(location_list)
    var right := VBoxContainer.new()
    right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    right.add_theme_constant_override("separation", 10)
    split.add_child(right)
    title_label = _make_label("")
    title_label.add_theme_font_size_override("font_size", 22)
    right.add_child(title_label)
    fixed_label = _make_label("")
    right.add_child(fixed_label)
    var details := HBoxContainer.new()
    details.add_theme_constant_override("separation", 16)
    right.add_child(details)
    var fields := VBoxContainer.new()
    fields.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    details.add_child(fields)
    fields.add_child(_make_label("Type"))
    type_choice = OptionButton.new()
    type_choice.item_selected.connect(_on_type_selected)
    fields.add_child(type_choice)
    fields.add_child(_make_label("Shopkeeper / temple portrait"))
    portrait_choice = OptionButton.new()
    portrait_choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    portrait_choice.item_selected.connect(_on_portrait_selected)
    fields.add_child(portrait_choice)
    var refresh := Button.new()
    refresh.text = "Refresh portraits"
    refresh.pressed.connect(_on_refresh_portraits)
    fields.add_child(refresh)
    var export_button := Button.new()
    export_button.text = "Export portraits to game"
    export_button.pressed.connect(_on_export_portraits)
    fields.add_child(export_button)
    portrait_note = _make_label(PORTRAIT_NOTE)
    fields.add_child(portrait_note)
    portrait_view = TextureRect.new()
    portrait_view.custom_minimum_size = Vector2(150, 176)
    portrait_view.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    portrait_view.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    details.add_child(portrait_view)
    var offerings_title := _make_label("Offerings")
    offerings_title.add_theme_font_size_override("font_size", 20)
    right.add_child(offerings_title)
    add_row = HBoxContainer.new()
    right.add_child(add_row)
    add_search = LineEdit.new()
    add_search.placeholder_text = "Find an original item to add"
    add_search.custom_minimum_size.x = 215
    add_search.text_changed.connect(_filter_catalog)
    add_row.add_child(add_search)
    add_choice = OptionButton.new()
    add_choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    add_row.add_child(add_choice)
    var add_button := Button.new()
    add_button.text = "Add item"
    add_button.pressed.connect(_add_item)
    add_row.add_child(add_button)
    var columns := HBoxContainer.new()
    right.add_child(columns)
    var item_title := _make_label("Item or service")
    item_title.custom_minimum_size.x = 330
    item_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    columns.add_child(item_title)
    var price_title := _make_label("Price (gp)")
    price_title.custom_minimum_size.x = 125
    columns.add_child(price_title)
    stock_title = _make_label("Stock (-1 = ∞)")
    stock_title.custom_minimum_size.x = 145
    columns.add_child(stock_title)
    action_title = _make_label("Action")
    action_title.custom_minimum_size.x = 85
    columns.add_child(action_title)
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    right.add_child(scroll)
    row_box = VBoxContainer.new()
    row_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    row_box.add_theme_constant_override("separation", 4)
    scroll.add_child(row_box)
    status_label = _make_label("Ready")
    outer.add_child(status_label)

func _on_map_selected(index: int) -> void:
    _select_map(String(manifest["maps"][index]["name"]))
    for location in manifest["locations"]:
        if String(location["map"]) == current_map:
            _select_location(String(location["id"]))
            return

func _select_map(name: String) -> void:
    current_map = name
    var map_meta := _map_meta(name)
    var key := String(map_meta["archive"]) + ":" + str(int(map_meta["record"]))
    var index: int = map_indices[key]
    map_choice.select(0 if name == "New Phlan" else 1)
    map_cells.clear()
    for y in 16:
        for x in 16:
            map_cells.append(map_cell(index, 0, x, y, 0))
    for location in manifest["locations"]:
        if String(location["map"]) == name and location.has("entrance"):
            _draw_entrance(location)
    visible_ids.clear()
    location_list.clear()
    for location in manifest["locations"]:
        if String(location["map"]) != name:
            continue
        var id := String(location["id"])
        visible_ids.append(id)
        location_list.add_item(String(location["name"]))
    _update_map()

# The game's changes for a shop with a hidden entrance (SECRET-1): walls that
# close its square's other sides, and the secret door, which the map marks.
func _draw_entrance(location: Dictionary) -> void:
    var entrance: Dictionary = location["entrance"]
    for marker in location["markers"]:
        var x := int(marker[0])
        var y := int(marker[1])
        for side_name in entrance["walls"]:
            _set_face(x, y, SIDES.find(side_name), int(entrance["walls"][side_name]), 0, false)
        _set_face(x, y, SIDES.find(String(entrance["secret_door"])), int(entrance["door_wall"]), 2, true)

# Sets one side of a square, and the facing side of its neighbour.
func _set_face(x: int, y: int, side: int, wall: int, door: int, secret: bool) -> void:
    var steps: Array[Vector2i] = [Vector2i(0, -1), Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0)]
    var other: Vector2i = Vector2i(x, y) + steps[side]
    for face in [[x, y, side], [other.x, other.y, (side + 2) % 4]]:
        if face[0] < 0 or face[1] < 0 or face[0] >= 16 or face[1] >= 16:
            continue
        var cell: Dictionary = map_cells[face[1] * 16 + face[0]]
        cell["faces"][face[2]] = {"wall_id": wall, "door_code": door, "secret": secret}

func _update_map() -> void:
    var locations: Array = []
    for location in manifest["locations"]:
        if String(location["map"]) == current_map:
            locations.append(location)
    map_widget.set_content(map_cells, locations, selected_id)

func _on_location_list_selected(index: int) -> void:
    if index >= 0 and index < visible_ids.size():
        _select_location(visible_ids[index])

func _select_location(id: String) -> void:
    var meta := _location_meta(id)
    if meta.is_empty():
        return
    if String(meta["map"]) != current_map:
        _select_map(String(meta["map"]))
    selected_id = id
    for i in visible_ids.size():
        if visible_ids[i] == id:
            location_list.select(i)
            location_list.ensure_current_is_visible()
            break
    _update_map()
    var record: Dictionary = records[id]
    title_label.text = String(record["name"])
    var markers: Array = meta["markers"]
    var points: Array[String] = []
    for marker in markers:
        points.append("(%d,%d)" % [int(marker[0]), int(marker[1])])
    fixed_label.text = "Fixed location: %s:%d at %s" % [record["fixed_location"]["archive"],
        int(record["fixed_location"]["record"]), ", ".join(points)]
    if meta.has("entrance"):
        fixed_label.text += " · OpenGoldBox's shop: a secret, locked door on its %s side" % String(meta["entrance"]["secret_door"])
    type_choice.disabled = String(meta["kind"]) == "temple"
    type_choice.clear()
    if type_choice.disabled:
        type_choice.add_item("Temple")
        type_choice.select(0)
    else:
        for type in SHOP_TYPES:
            type_choice.add_item(type)
        type_choice.select(SHOP_TYPES.find(String(record["type"])))
    add_row.visible = not type_choice.disabled
    stock_title.text = "Offered" if type_choice.disabled else "Stock (-1 = ∞)"
    action_title.text = "" if type_choice.disabled else "Action"
    _select_portrait_value(_source_portrait(String(record["portrait"])))
    _refresh_rows()
    _filter_catalog(add_search.text)

func _on_type_selected(index: int) -> void:
    if selected_id.is_empty() or _location_meta(selected_id)["kind"] != "shop" or index >= SHOP_TYPES.size():
        return
    records[selected_id]["type"] = SHOP_TYPES[index]
    _write_record(selected_id)

func _scan_portraits() -> void:
    portrait_paths.clear()
    for relative in ["art/portraits/NPCs", "dev_tools/shop_manager/portraits"]:
        var absolute := repo_dir.path_join(relative)
        var directory := DirAccess.open(absolute)
        if directory == null:
            continue
        for name in directory.get_files():
            if name.to_lower().ends_with(".png"):
                portrait_paths.append(relative.path_join(name))
    portrait_paths.sort()

func _select_portrait_value(path: String) -> void:
    portrait_choice.clear()
    portrait_choice.add_item("No portrait")
    for candidate in portrait_paths:
        portrait_choice.add_item(candidate)
    if not path.is_empty() and not portrait_paths.has(path):
        portrait_choice.add_item("Missing: " + path)
        portrait_choice.select(portrait_choice.item_count - 1)
    else:
        portrait_choice.select(0 if path.is_empty() else portrait_paths.find(path) + 1)
    portrait_view.texture = null
    if not path.is_empty() and FileAccess.file_exists(repo_dir.path_join(path)):
        var image := Image.new()
        if image.load(repo_dir.path_join(path)) == OK:
            portrait_view.texture = ImageTexture.create_from_image(image)
    portrait_note.text = PORTRAIT_NOTE

func _on_refresh_portraits() -> void:
    _scan_portraits()
    if not selected_id.is_empty():
        _select_portrait_value(_source_portrait(String(records[selected_id]["portrait"])))

func _on_portrait_selected(index: int) -> void:
    if selected_id.is_empty() or index > portrait_paths.size():
        return
    var path := "" if index == 0 else portrait_paths[index - 1]
    records[selected_id]["portrait"] = _game_portrait(path)
    _select_portrait_value(path)
    _write_record(selected_id)
    _on_export_portraits()

# A chosen portrait is exported at once, as every other edit saves at once;
# the button exports again after a portrait's PNG is redrawn.
func _on_export_portraits() -> void:
    var export_error := _export_portraits()
    portrait_note.text = PORTRAIT_NOTE if export_error.is_empty() else export_error

func _clear_rows() -> void:
    for child in row_box.get_children():
        row_box.remove_child(child)
        child.queue_free()

func _refresh_rows() -> void:
    _clear_rows()
    if selected_id.is_empty():
        return
    var offerings: Array = records[selected_id]["offerings"]
    var temple := String(records[selected_id]["kind"]) == "temple"
    for index in offerings.size():
        var entry: Dictionary = offerings[index]
        var row := HBoxContainer.new()
        row.add_theme_constant_override("separation", 8)
        row_box.add_child(row)
        var name := _make_label(String(entry["name"]))
        name.custom_minimum_size.x = 330
        name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        name.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
        name.tooltip_text = String(entry["name"]) + (" · " + String(entry.get("source_archive", ""))
            if not temple else "")
        row.add_child(name)
        var price := SpinBox.new()
        price.custom_minimum_size.x = 125
        price.min_value = 0
        price.max_value = 1000000000
        price.step = 1
        price.value = float(entry["price_gp"])
        price.value_changed.connect(func(value: float) -> void:
            records[selected_id]["offerings"][index]["price_gp"] = int(value)
            _write_record(selected_id))
        row.add_child(price)
        if temple:
            var offered := CheckButton.new()
            offered.custom_minimum_size.x = 145
            offered.text = "Offered"
            offered.button_pressed = bool(entry["offered"])
            offered.toggled.connect(func(value: bool) -> void:
                records[selected_id]["offerings"][index]["offered"] = value
                _write_record(selected_id))
            row.add_child(offered)
            row.add_child(_make_label(""))
        else:
            var quantity := SpinBox.new()
            quantity.custom_minimum_size.x = 145
            quantity.min_value = -1
            quantity.max_value = 1000000
            quantity.step = 1
            quantity.value = float(entry["quantity"])
            quantity.value_changed.connect(func(value: float) -> void:
                records[selected_id]["offerings"][index]["quantity"] = int(value)
                _write_record(selected_id))
            row.add_child(quantity)
            var remove := Button.new()
            remove.custom_minimum_size.x = 85
            remove.text = "Remove"
            remove.pressed.connect(func() -> void:
                records[selected_id]["offerings"].remove_at(index)
                _write_record(selected_id)
                _refresh_rows())
            row.add_child(remove)

func _filter_catalog(query: String) -> void:
    if add_choice == null:
        return
    add_choice.clear()
    catalog_matches.clear()
    var match_text := query.strip_edges().to_lower()
    var seen: Dictionary = {}
    for item in catalog_items:
        var entry: Dictionary = item
        var name := String(entry["name"])
        if not match_text.is_empty() and not name.to_lower().contains(match_text):
            continue
        var key := name + "|" + str(entry["item_type"]) + "|" + str(entry["price_gp"])
        if seen.has(key):
            continue
        seen[key] = true
        catalog_matches.append(entry)
        add_choice.add_item("%s · %s gp [%s:%s]" % [name, entry["price_gp"],
            entry["source_archive"], entry["source_record"]])
        if catalog_matches.size() >= 100:
            break
    add_choice.disabled = catalog_matches.is_empty()

func _add_item() -> void:
    if selected_id.is_empty() or records[selected_id]["kind"] != "shop" or catalog_matches.is_empty():
        return
    var choice := add_choice.selected
    if choice < 0 or choice >= catalog_matches.size():
        return
    var item: Dictionary = catalog_matches[choice].duplicate(true)
    records[selected_id]["offerings"].append(item)
    _write_record(selected_id)
    _refresh_rows()

func _capture(path: String) -> void:
    if DisplayServer.get_name() == "headless":
        push_error("Screenshots require a visible Godot window")
        get_tree().quit(1)
        return
    await get_tree().process_frame
    await get_tree().process_frame
    var error := get_viewport().get_texture().get_image().save_png(path)
    if error != OK:
        push_error("Screenshot failed: " + path)
        get_tree().quit(1)
    else:
        print("Saved shop manager capture: ", path)
        get_tree().quit()

func _run_check() -> void:
    var shops := 0
    var temples := 0
    var expected := {52: 11, 53: 57, 54: 7, 55: 13}
    for record in expected:
        if shop_stock(record).size() != expected[record]:
            _fatal("Original stock mismatch ITEM3.DAX:" + str(record))
            return
    for location in manifest["locations"]:
        var id := String(location["id"])
        if location["kind"] == "shop":
            shops += 1
        else:
            temples += 1
        if not _valid_record(records[id], location):
            _fatal("Invalid location " + id)
            return
        for marker in location["markers"]:
            var map := _map_meta(String(location["map"]))
            var key := String(map["archive"]) + ":" + str(int(map["record"]))
            var cell: Dictionary = map_cell(map_indices[key], 0, int(marker[0]), int(marker[1]), 0)
            if cell.is_empty():
                _fatal("Invalid map marker for " + id)
                return
    if shops != 13 or temples != 4 or records["podol-temple-ilmater"]["offerings"].size() != 10:
        _fatal("Shop/temple inventory mismatch")
        return
    if not catalog_items.any(func(item: Dictionary) -> bool:
            return int(item["item_type"]) == 212 and int(item["price_gp"]) == 50):
        _fatal("The item picker lacks the Poisoner's Kit")
        return
    if not catalog_items.any(func(item: Dictionary) -> bool:
            return int(item["item_type"]) == 213 and int(item["price_gp"]) == 100):
        _fatal("The item picker lacks Basic Poison")
        return
    if not catalog_items.any(func(item: Dictionary) -> bool:
            return int(item["item_type"]) == 214 and int(item["price_gp"]) == 25):
        _fatal("The item picker lacks Thieves' Tools")
        return
    if not catalog_items.any(func(item: Dictionary) -> bool:
            return int(item["item_type"]) == 219 and int(item["bundle_quantity"]) == 3):
        _fatal("The item picker lacks the Wand of Secrets")
        return
    var rogue: Dictionary = records["phlan-rogue-04-13"]
    if rogue["type"] != "Rogue's Shop" or not rogue["offerings"].any(func(item: Dictionary) -> bool:
            return int(item["item_type"]) == 212 and int(item["price_gp"]) == 50):
        _fatal("The Rogue's Shop does not sell the Poisoner's Kit")
        return
    if not rogue["offerings"].any(func(item: Dictionary) -> bool:
            return int(item["item_type"]) == 50 and int(item.get("magic_bonus", 0)) == 1):
        _fatal("The Rogue's Shop does not sell Leather Armor +1")
        return
    if not _entrance_drawn():
        _fatal("The map lacks the Rogue's Shop's secret door or wall")
        return
    var saved_type: String = records["phlan-arms-13-08"]["type"]
    records["phlan-arms-13-08"]["type"] = "Temple"
    if _valid_record(records["phlan-arms-13-08"], _location_meta("phlan-arms-13-08")):
        _fatal("Shop-to-temple type change was allowed")
        return
    for type in ["Rogue's Shop", "Magic User's Shop"]:
        records["phlan-arms-13-08"]["type"] = type
        if not _valid_record(records["phlan-arms-13-08"], _location_meta("phlan-arms-13-08")):
            _fatal("A shop cannot become a " + type)
            return
    records["phlan-arms-13-08"]["type"] = saved_type
    records["podol-temple-ilmater"]["type"] = "General Items"
    if _valid_record(records["podol-temple-ilmater"], _location_meta("podol-temple-ilmater")):
        _fatal("Temple-to-shop type change was allowed")
        return
    records["podol-temple-ilmater"]["type"] = "Temple"
    var fixed: Dictionary = records["podol-temple-ilmater"]["fixed_location"].duplicate(true)
    records["podol-temple-ilmater"]["fixed_location"]["markers"] = [[2, 15]]
    if _valid_record(records["podol-temple-ilmater"], _location_meta("podol-temple-ilmater")):
        _fatal("Temple relocation was allowed")
        return
    records["podol-temple-ilmater"]["fixed_location"] = fixed
    var first_price := int(records["phlan-arms-13-08"]["offerings"][0]["price_gp"])
    records["phlan-arms-13-08"]["offerings"][0]["price_gp"] = -1
    if _valid_record(records["phlan-arms-13-08"], _location_meta("phlan-arms-13-08")):
        _fatal("Negative price was allowed")
        return
    records["phlan-arms-13-08"]["offerings"][0]["price_gp"] = first_price
    for id in records:
        var portrait := String(records[id]["portrait"])
        var source := _source_portrait(portrait)
        if _game_portrait(source) != portrait or not FileAccess.file_exists(repo_dir.path_join(source)):
            _fatal("A record names a portrait as the game cannot find it: " + id)
            return
    if not _portraits_exported():
        _fatal("The game lacks an exported portrait: run --export-portraits")
        return
    print("Shop manager checks passed: 13 shops, 4 temples, 2 maps, fixed locations, original stock, the Rogue's Shop's secret door, exported portraits")
    get_tree().quit()

# The map shows the Rogue's Shop's secret door south of (4,13) and the wall
# that closes its north side.
func _entrance_drawn() -> bool:
    var saved := map_cells.duplicate(true)
    var meta := _map_meta("New Phlan")
    var index: int = map_indices[String(meta["archive"]) + ":" + str(int(meta["record"]))]
    map_cells.clear()
    for y in 16:
        for x in 16:
            map_cells.append(map_cell(index, 0, x, y, 0))
    _draw_entrance(_location_meta("phlan-rogue-04-13"))
    var door: Dictionary = map_cells[13 * 16 + 4]["faces"][2]
    var outside: Dictionary = map_cells[14 * 16 + 4]["faces"][0]
    var wall: Dictionary = map_cells[13 * 16 + 4]["faces"][0]
    var front: Dictionary = map_cells[12 * 16 + 4]["faces"][2]
    map_cells = saved
    return door.get("secret", false) and outside.get("secret", false) and int(wall["wall_id"]) != 0 and int(front["wall_id"]) != 0
