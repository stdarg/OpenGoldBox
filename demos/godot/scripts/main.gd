extends Control

const INITIAL_ARCHIVE_NAME := "CPIC1.DAX"
const INITIAL_RECORD_ID := 2
const DISPLAY_SCALE := 4
const ART_BACKGROUND := Color(0.08, 0.095, 0.12, 1.0)

var sprite_entries: Array[Dictionary] = []
var record_index := 0
var catalogs: Array = [[], []]
var positions: Array[int] = [0, 0]
var category := 0
var indexing_errors: PackedStringArray = []
var loader := DaxSpriteLoader.new()

@onready var nearest_sprite: TextureRect = %NearestSprite
@onready var xbr_sprite: TextureRect = %XbrSprite
@onready var hqx_sprite: TextureRect = %HqxSprite
@onready var status: Label = %Status
@onready var category_select: OptionButton = %CategorySelect

func _ready() -> void:
	category_select.add_item("Combat sprites")
	category_select.add_item("Encounter sprites")
	category_select.select(0)
	category_select.item_selected.connect(_select_category)
	var game_directory := GameFinder.game_folder("POOLRAD")
	var directory := DirAccess.open(game_directory)
	if directory == null:
		status.text = "Pool of Radiance's files were not found; set OPENGOLD_GAME_DIR to their folder."
		return
	var archives: Array[String] = []
	for filename in directory.get_files():
		if filename.to_upper().match("SPRIT*.DAX") or _is_combat_archive(filename):
			archives.append(filename)
	archives.sort_custom(func(a: String, b: String) -> bool: return a.naturalnocasecmp_to(b) < 0)
	var failures: PackedStringArray = []
	for archive in archives:
		var path := game_directory.path_join(archive)
		var combat := _is_combat_archive(archive)
		var target: int = 0 if combat else 1
		var ids := loader.list_record_ids(path)
		if ids.is_empty():
			failures.append("%s: %s" % [archive, loader.error_message])
		for id in ids:
			var count := loader.frame_count(path, id, combat)
			if count == 0:
				failures.append("%s record %d: no frames (%s)" % [archive, id, loader.error_message])
			for frame in range(count):
				if archive.to_upper() == INITIAL_ARCHIVE_NAME and id == INITIAL_RECORD_ID and frame == 0:
					positions[target] = catalogs[target].size()
				catalogs[target].append({"path": path, "archive": archive, "id": id, "frame": frame, "frame_count": count, "combat": combat})
	indexing_errors = failures
	record_index = positions[0]
	_select_category(0)

func _is_combat_archive(filename: String) -> bool:
	var name := filename.to_upper()
	return name.match("CPIC*.DAX") or name in ["COMSPR.DAX", "CHEAD.DAX", "CBODY.DAX"]

func _select_category(index: int) -> void:
	positions[category] = record_index
	category = index
	sprite_entries.assign(catalogs[index])
	record_index = positions[index]
	if sprite_entries.is_empty():
		for sprite in [nearest_sprite, xbr_sprite, hqx_sprite]:
			sprite.texture = null
		status.text = "No %s found in the game directory." % category_select.get_item_text(index).to_lower()
		if not indexing_errors.is_empty():
			status.text += "\n" + "\n".join(indexing_errors)
		return
	_show_record()

func _input(event: InputEvent) -> void:
	if category_select.get_popup().visible or sprite_entries.is_empty() or not event is InputEventKey:
		return
	if not event.pressed or event.echo:
		return
	var direction := 0
	if event.keycode == KEY_LEFT:
		direction = -1
	elif event.keycode == KEY_RIGHT:
		direction = 1
	if direction == 0:
		return
	get_viewport().set_input_as_handled()
	record_index = wrapi(record_index + direction, 0, sprite_entries.size())
	_show_record()

func _show_record() -> void:
	var entry := sprite_entries[record_index]
	var image := loader.load_frame(entry.path, entry.id, entry.frame, entry.combat)
	if image == null:
		for sprite in [nearest_sprite, xbr_sprite, hqx_sprite]:
			sprite.texture = null
		status.text = "%s | record %d (%d/%d) failed to load:\n%s\nUse Left / Right to try another sprite." % [entry.archive, entry.id, record_index + 1, sprite_entries.size(), loader.error_message]
		return
	# Give all filters identical opaque input and a two-pixel border for xBR's
	# neighborhood. Composite first so transparent black cannot create dark halos.
	var preview := Image.create(image.get_width() + 4, image.get_height() + 4, false, Image.FORMAT_RGBA8)
	preview.fill(ART_BACKGROUND)
	preview.blend_rect(image, Rect2i(Vector2i.ZERO, image.get_size()), Vector2i(2, 2))
	var texture := ImageTexture.create_from_image(preview)
	for sprite in [nearest_sprite, xbr_sprite, hqx_sprite]:
		sprite.texture = texture
		sprite.custom_minimum_size = Vector2(preview.get_size() * DISPLAY_SCALE)
	status.text = "%s | record %d | image %d/%d | %d/%d total | %dx%d | all at %dx\nLeft / Right: previous / next image" % [entry.archive, entry.id, entry.frame + 1, entry.frame_count, record_index + 1, sprite_entries.size(), image.get_width(), image.get_height(), DISPLAY_SCALE]
	if not indexing_errors.is_empty():
		status.text += "\nSome records could not be indexed:\n" + "\n".join(indexing_errors)
