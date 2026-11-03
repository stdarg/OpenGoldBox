extends Control

const CATALOG_ROOT := "res://../../combat_sprite_catalog"
const CATALOG_JSON := CATALOG_ROOT + "/json"
const BG := Color("192430")
const TEXT := Color("edf2f5")

var records: Array[Dictionary] = []
var selected: Dictionary = {}
var selected_variant := 0
var show_action := false
var search: LineEdit
var creature_list: ItemList
var heading: Label
var details: RichTextLabel
var art: TextureRect
var variant_choice: OptionButton
var pose_button: Button
var status: Label


func _ready() -> void:
	_build_ui()
	_load_records()
	_filter_records("")


func _build_ui() -> void:
	var background := ColorRect.new()
	background.color = BG
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	margin.add_theme_constant_override("margin_left", 20)
	margin.add_theme_constant_override("margin_right", 20)
	margin.add_theme_constant_override("margin_top", 20)
	margin.add_theme_constant_override("margin_bottom", 20)
	add_child(margin)
	var columns := HBoxContainer.new()
	columns.add_theme_constant_override("separation", 18)
	margin.add_child(columns)
	var left := VBoxContainer.new()
	left.custom_minimum_size.x = 350
	left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	columns.add_child(left)
	var title := Label.new()
	title.text = "Creature Stats Editor"
	title.add_theme_font_size_override("font_size", 26)
	title.add_theme_color_override("font_color", TEXT)
	left.add_child(title)
	search = LineEdit.new()
	search.placeholder_text = "Search creature, record, or gear"
	search.clear_button_enabled = true
	search.text_changed.connect(_filter_records)
	left.add_child(search)
	creature_list = ItemList.new()
	creature_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	creature_list.item_selected.connect(_select_record)
	left.add_child(creature_list)
	var right := VBoxContainer.new()
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	columns.add_child(right)
	heading = Label.new()
	heading.add_theme_font_size_override("font_size", 24)
	heading.add_theme_color_override("font_color", TEXT)
	right.add_child(heading)
	var controls := HBoxContainer.new()
	right.add_child(controls)
	variant_choice = OptionButton.new()
	variant_choice.custom_minimum_size.x = 260
	variant_choice.item_selected.connect(_select_variant)
	controls.add_child(variant_choice)
	pose_button = Button.new()
	pose_button.text = "Show action"
	pose_button.pressed.connect(_toggle_pose)
	controls.add_child(pose_button)
	art = TextureRect.new()
	art.custom_minimum_size = Vector2(420, 420)
	art.size_flags_vertical = Control.SIZE_EXPAND_FILL
	art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	art.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	right.add_child(art)
	details = RichTextLabel.new()
	details.bbcode_enabled = true
	details.custom_minimum_size.y = 175
	details.size_flags_vertical = Control.SIZE_EXPAND_FILL
	right.add_child(details)
	status = Label.new()
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status.add_theme_color_override("font_color", Color("cbd6de"))
	right.add_child(status)


func _load_records() -> void:
	var folder := DirAccess.open(CATALOG_JSON)
	if folder == null:
		status.text = "Catalog unavailable: " + ProjectSettings.globalize_path(CATALOG_JSON)
		return
	folder.list_dir_begin()
	var filename := folder.get_next()
	while filename != "":
		if not folder.current_is_dir() and filename.ends_with(".json"):
			var path := CATALOG_JSON + "/" + filename
			var value: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
			if value is Dictionary and value.has("source") and value.has("name"):
				records.append(value)
		filename = folder.get_next()
	folder.list_dir_end()
	records.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return str(a.get("name", "")) < str(b.get("name", "")))
	status.text = "%d catalog creatures loaded. Combat art and gear are available for inspection." % records.size()


func _filter_records(query: String) -> void:
	creature_list.clear()
	var needle := query.strip_edges().to_lower()
	for record in records:
		var source: Dictionary = record.get("source", {})
		var key := str(source.get("key", ""))
		var gear := ""
		for item in record.get("equipment", []):
			gear += " " + str(item.get("name", ""))
		var label := str(record.get("name", "")) + "  ·  " + key
		if needle == "" or (label + gear).to_lower().contains(needle):
			creature_list.add_item(label)
			creature_list.set_item_metadata(creature_list.item_count - 1, record)
	if creature_list.item_count > 0:
		creature_list.select(0)
		_select_record(0)
	else:
		selected = {}
		heading.text = "No matching creatures"
		details.text = ""
		art.texture = null


func _select_record(index: int) -> void:
	selected = creature_list.get_item_metadata(index)
	selected_variant = 0
	show_action = false
	pose_button.text = "Show action"
	var source: Dictionary = selected.get("source", {})
	heading.text = "%s  ·  %s" % [selected.get("name", ""), source.get("key", "")]
	variant_choice.clear()
	for variant in selected.get("variants", []):
		variant_choice.add_item(str(variant.get("label", variant.get("id", "Pose"))))
	variant_choice.disabled = variant_choice.item_count == 0
	pose_button.disabled = variant_choice.item_count == 0
	_update_art()
	var gear_lines := PackedStringArray()
	for item in selected.get("equipment", []):
		var marker := "ready" if item.get("readied", false) else "stored"
		gear_lines.append("• %s (%s)" % [item.get("name", ""), marker])
	details.text = "[b]Catalog description[/b]\n%s\n\n[b]Gear[/b]\n%s" % [
		selected.get("description", ""), "\n".join(gear_lines)]


func _select_variant(index: int) -> void:
	selected_variant = index
	_update_art()


func _toggle_pose() -> void:
	show_action = not show_action
	pose_button.text = "Show ready" if show_action else "Show action"
	_update_art()


func _update_art() -> void:
	art.texture = null
	var variants: Array = selected.get("variants", [])
	if selected_variant >= variants.size():
		return
	var variant: Dictionary = variants[selected_variant]
	var relative := str(variant.get("action" if show_action else "ready", ""))
	if relative == "":
		return
	var image := Image.new()
	if image.load(CATALOG_ROOT + "/" + relative) == OK:
		art.texture = ImageTexture.create_from_image(image)
