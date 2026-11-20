extends Control
## Top-down GEO inspection. Marker numbers are event data, not ECL addresses.
const MapNames = preload("res://scripts/por_map_names.gd")

var maps: Array = []
var current := 0
var selected := Vector2i.ZERO
var grid_origin := Vector2(32, 132)
var cell_size := 30.0
var picker := OptionButton.new()
var info := Label.new()
var details := Label.new()
var events := ItemList.new()
var show_events := CheckButton.new()
var show_walls := CheckButton.new()
var event_cells: Array[int] = []
var status := Label.new()
var side := VBoxContainer.new()

func _ready() -> void:
	get_window().min_size = Vector2i(1100, 720)
	RenderingServer.set_default_clear_color(Color("101722"))
	var title := Label.new()
	title.text = "OpenGoldBox  /  Map inspector"
	title.add_theme_font_size_override("font_size", 26)
	title.position = Vector2(32, 18)
	add_child(title)
	var bar := HBoxContainer.new()
	bar.position = Vector2(32, 64)
	bar.add_theme_constant_override("separation", 14)
	add_child(bar)
	picker.custom_minimum_size.x = 460
	picker.fit_to_longest_item = false
	picker.clip_text = true
	bar.add_child(picker)
	for direction in [-1, 1]:
		var button := Button.new()
		button.text = "Previous" if direction == -1 else "Next"
		button.pressed.connect(func(): _select_map(wrapi(current + direction, 0, maxi(1, maps.size()))))
		bar.add_child(button)
	show_events.text = "Event markers"
	show_events.button_pressed = true
	show_events.toggled.connect(func(_value): queue_redraw())
	bar.add_child(show_events)
	show_walls.text = "Walls / doors"
	show_walls.button_pressed = true
	show_walls.toggled.connect(func(_value): queue_redraw())
	bar.add_child(show_walls)
	picker.item_selected.connect(_select_map)
	add_child(side)
	side.add_theme_constant_override("separation", 8)
	side.add_theme_font_size_override("font_size", 15)
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	side.add_child(info)
	details.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	side.add_child(details)
	var legend := Label.new()
	legend.text = "Blue: wall IDs   •   Gold: door codes\nPurple: nonzero event byte   •   Teal: selection\nEdges are inset to preserve each cell's direction."
	legend.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	side.add_child(legend)
	var label := Label.new()
	label.text = "Event locations — click to inspect"
	side.add_child(label)
	events.custom_minimum_size.y = 100
	events.size_flags_vertical = Control.SIZE_EXPAND_FILL
	events.item_selected.connect(_event_selected)
	side.add_child(events)
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status.text = "Click a cell or use arrow keys to inspect. This cursor is not player movement.\nMarkers show GEO data; scripts are not executed. Event 0 may still participate in area logic."
	side.add_child(status)
	side.move_child(status, side.get_child_count() - 3)
	resized.connect(_layout)
	_layout()
	_load_maps()

func _layout() -> void:
	cell_size = maxf(12, minf((size.y - 180) / 16, (size.x * 0.56 - 48) / 16))
	side.position = Vector2(grid_origin.x + cell_size * 16 + 40, 125)
	side.size = Vector2(maxf(200, size.x - side.position.x - 32), maxf(200, size.y - 150))
	queue_redraw()

func _load_maps() -> void:
	var game_directory := GameFinder.game_folder("POOLRAD")
	var executable := ProjectSettings.globalize_path("res://../../build/opengold_maps.exe" if OS.get_name() == "Windows" else "res://../../build/opengold_maps")
	if not FileAccess.file_exists(executable):
		info.text = "Map loader is missing. Run build.cmd from the repository root, then reopen this demo."
		return
	var output: Array = []
	var result := OS.execute(executable, [game_directory], output, true)
	if result != 0:
		info.text = "Could not load maps. Pool of Radiance's files were not found; set OPENGOLD_GAME_DIR to their folder.\n\n" + "\n".join(output)
		return
	var parsed = JSON.parse_string("".join(output))
	if not parsed is Dictionary or parsed.get("version") != 1 or not parsed.get("maps") is Array:
		info.text = "Map loader returned an invalid catalog. Rebuild opengold_maps."
		return
	maps = parsed.maps
	for map in maps:
		var caption := "%s — %s:%d" % [MapNames.location(map.archive, int(map.id)), map.archive, map.id]
		picker.add_item(caption)
		picker.set_item_tooltip(picker.item_count - 1, caption + "\n" + MapNames.SOURCE)
	if maps.is_empty():
		info.text = "No map records were found."
		return
	_select_map(0)

func _select_map(index: int) -> void:
	if maps.is_empty(): return
	current = index
	picker.select(index)
	selected = Vector2i.ZERO
	events.clear()
	event_cells.clear()
	var map: Dictionary = maps[current]
	var mismatches := 0
	for i in range(256):
		var c: Dictionary = map.cells[i]
		var raw := int(c.event)
		if raw != 0:
			event_cells.append(i)
			events.add_item("(%2d, %2d)   event %3d%s   [0x%02X]" % [i % 16, i / 16, raw & 127, "*" if raw & 128 else "", raw])
		if i % 16 < 15:
			var east: Dictionary = map.cells[i+1]
			if c.walls[1] != east.walls[3] or c.doors[1] != east.doors[3]: mismatches += 1
		if i < 240:
			var south: Dictionary = map.cells[i+16]
			if c.walls[2] != south.walls[0] or c.doors[2] != south.doors[0]: mismatches += 1
	info.text = "%s · record %d\n16 × 16 cells · %d decoded bytes\n%d nonzero event cells · %d opposing-edge differences" % [map.archive, map.id, map.bytes, event_cells.size(), mismatches]
	_update_selection()
	info.text = MapNames.location(map.archive, int(map.id)) + "\n" + info.text
	info.tooltip_text = MapNames.SOURCE
	picker.tooltip_text = "%s — %s:%d\n%s" % [MapNames.location(map.archive, int(map.id)), map.archive, map.id, MapNames.SOURCE]
	call_deferred("_layout")

func _event_selected(index: int) -> void:
	var i := event_cells[index]
	selected = Vector2i(i % 16, i / 16)
	_update_selection()

func _update_selection() -> void:
	var c: Dictionary = maps[current].cells[selected.y * 16 + selected.x]
	var raw := int(c.event)
	details.text = "CELL (%d, %d)\nWalls N / E / S / W: %d / %d / %d / %d\nDoors N / E / S / W: %d / %d / %d / %d\nEvent: %d   Raw: 0x%02X   High bit: %s\nECL handler: unresolved" % [selected.x, selected.y, c.walls[0], c.walls[1], c.walls[2], c.walls[3], c.doors[0], c.doors[1], c.doors[2], c.doors[3], raw & 127, raw, "set (*)" if raw & 128 else "clear"]
	queue_redraw()

func _gui_input(event: InputEvent) -> void:
	if maps.is_empty(): return
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		var point: Vector2 = (event.position - grid_origin) / cell_size
		if point.x >= 0 and point.y >= 0 and point.x < 16 and point.y < 16:
			selected = Vector2i(point.floor())
			_update_selection()
			accept_event()

func _unhandled_key_input(event: InputEvent) -> void:
	if maps.is_empty() or not event.is_pressed(): return
	var delta := Vector2i.ZERO
	match event.keycode:
		KEY_LEFT: delta.x = -1
		KEY_RIGHT: delta.x = 1
		KEY_UP: delta.y = -1
		KEY_DOWN: delta.y = 1
		_: return
	selected = (selected + delta).clamp(Vector2i.ZERO, Vector2i(15,15))
	_update_selection()
	get_viewport().set_input_as_handled()

func _draw() -> void:
	var font := ThemeDB.fallback_font
	for n in range(16):
		draw_string(font, grid_origin + Vector2(n * cell_size + 6, -10), str(n), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color("92a6c0"))
		draw_string(font, grid_origin + Vector2(-23, n * cell_size + cell_size * 0.65), str(n), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color("92a6c0"))
	for i in range(256):
		var p := grid_origin + Vector2(i % 16, i / 16) * cell_size
		var rect := Rect2(p, Vector2.ONE * cell_size)
		draw_rect(rect, Color("192535"))
		draw_rect(rect, Color("2a3a4e"), false, 1)
		if maps.is_empty(): continue
		var c: Dictionary = maps[current].cells[i]
		if show_events.button_pressed and int(c.event) != 0:
			draw_rect(rect.grow(-5), Color("553777"))
			var text := str(int(c.event) & 127) + ("*" if int(c.event) & 128 else "")
			draw_string(font, p + Vector2(5, cell_size * 0.65), text, HORIZONTAL_ALIGNMENT_LEFT, cell_size - 6, int(clampf(cell_size * 0.37, 8, 13)), Color("efdfff"))
		if show_walls.button_pressed:
			var corners := [p + Vector2(3,3), p + Vector2(cell_size-3,3), p + Vector2(cell_size-3,cell_size-3), p + Vector2(3,cell_size-3)]
			for d in range(4):
				var a: Vector2 = corners[d]
				var b: Vector2 = corners[(d+1)%4]
				if c.walls[d] != 0: draw_line(a, b, Color("8ebfea"), 2)
				if c.doors[d] != 0: draw_line(a.lerp(b,0.3), a.lerp(b,0.7), Color("f6c76b"), 4)
	if not maps.is_empty():
		draw_rect(Rect2(grid_origin + Vector2(selected) * cell_size, Vector2.ONE * cell_size).grow(-1), Color("65edd1"), false, 2)
