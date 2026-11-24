extends SceneTree

# Countdown to Doomsday's solar map on the exploration screen
# (docs/games/countdown.md, "The solar map"), played through the real screen
# from the save opengold_countdown_solar_map_fixture writes: a new team out of
# Earth's orbit. The pad and the arrow keys fly the rocket at once; a step
# spends the script's fuel and moves the game clock; the pointer over Earth
# names it; flying back onto Earth brings its script's menu; Change opens the
# Options, where Fly at once off asks for Move first and Done to stop, and
# Course flown on marks the course; the team's cards stay; and every control
# fits the smallest window, 1120 by 800, and 1920 by 1080. Run by
# tests/settings_check.cmake with --game, --rules=original and --load-save.

const FOLDER := "buck-rogers-countdown-to-doomsday"

func _initialize() -> void:
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle(frames := 8) -> void:
	for frame in range(frames): await process_frame

func town() -> Control:
	return current_scene.get_node_or_null("CampaignTown") if current_scene else null

func label(name: String) -> String:
	return (town().get_node("%" + name) as Label).text

func press(button: Button) -> void:
	require(button.is_visible_in_tree() and not button.disabled, button.name + " can be pressed")
	button.pressed.emit()
	await settle()

func key(code: Key) -> void:
	for pressed in [true, false]:
		var event := InputEventKey.new()
		event.keycode = code
		event.pressed = pressed
		Input.parse_input_event(event)
	await settle()

# Plays on after a step until the team can fly again: prompts are continued
# and a ship met en route is fled from, as a player may.
func go_on() -> void:
	for attempt in range(30):
		var choices: ItemList = town().get_node("%Choices")
		var continue_button: Button = town().get_node("%Continue")
		if not continue_button.visible or continue_button.disabled:
			return
		var offered := []
		for i in range(choices.item_count): offered.append(choices.get_item_text(i))
		if not choices.visible or not offered.has("FLEE"):
			await press(continue_button)
			continue
		var flee := offered.find("FLEE")
		choices.select(flee)
		choices.item_selected.emit(flee)
		await press(continue_button)

func fly(button: String) -> void:
	await press(town().get_node("%" + button))
	await go_on()

func fly_key(code: Key) -> void:
	await key(code)
	await go_on()

func rocket() -> String:
	return label("Coordinates")

# The center of a map cell on screen, as the map is drawn: square and centered
# in the 3D view's frame, 22 cells a side.
func cell_center(cell: Vector2) -> Vector2:
	var frame: Control = town().get_node("%SceneBounds")
	var bounds := frame.get_global_rect()
	var side := minf(bounds.size.x, bounds.size.y)
	var origin := bounds.position + (bounds.size - Vector2(side, side)) * 0.5
	return origin + (cell + Vector2.ONE * 0.5) * side / 22.0

func point_at(cell: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = cell_center(cell)
	event.global_position = event.position
	Input.parse_input_event(event)
	await settle()

func solar() -> Dictionary:
	return town().tour_visuals().get("solar", {})

# The in-game menu's Options, opened by the solar map's Change, set `option`
# to On or Off and closed again, back to the map.
func set_option(option: String, on: bool) -> void:
	await press(town().get_node("%RocketChange"))
	var menu: Control = current_scene.get_node("GameMenu")
	var gameplay: Control = menu.get_node("Options/Gameplay/Scroll/Page")
	require(menu.visible and menu.get_node("Options").visible, "Change opens the Options")
	var choice: OptionButton = gameplay.get_node("Choices/" + option)
	require(choice.visible, option + " shows on the Options while Countdown plays")
	var index := 0 if on else 1
	choice.select(index)
	choice.item_selected.emit(index)
	await press(menu.get_node("Options/Close"))
	await press(menu.get_node("Menu/Resume"))
	require(not menu.visible and town().visible, "The menu closes back to the solar map")

# With OPENGOLD_SOLAR_MAP_SHOTS naming a folder, a windowed run saves the
# screen there at each state, to compare with the design's mockups.
func shot(name: String) -> void:
	var folder := OS.get_environment("OPENGOLD_SOLAR_MAP_SHOTS")
	if folder.is_empty() or DisplayServer.get_name() == "headless":
		return
	await process_frame
	RenderingServer.force_draw(false)
	root.get_texture().get_image().save_png(folder.path_join("%s-%dx%d.png" % [name, root.size.x, root.size.y]))

# Every visible control of the screen lies inside the window.
func check_fits(size: Vector2i) -> void:
	root.size = size
	await settle(12)
	var window := Rect2(Vector2.ZERO, Vector2(size))
	var outside := []
	for node in town().find_children("*", "Control", true, false):
		var control := node as Control
		if not control.is_visible_in_tree() or control.size.x < 2 or control.size.y < 2:
			continue
		if control.get_parent() is ScrollContainer or control.get_parent().get_parent() is ScrollContainer:
			continue
		var rect := control.get_global_rect().grow(-1)
		if not window.encloses(rect):
			outside.append(String(control.get_path()).get_file())
	require(outside.is_empty(), "The solar map fits %s: %s" % [size, outside])
	await shot("fits")

func run_checks() -> void:
	if StartupView.game_folder(FOLDER).is_empty():
		print("Skipped: Countdown to Doomsday's files are not here.")
		quit(0)
		return
	root.size = Vector2i(1920, 1080)
	change_scene_to_file("res://scenes/startup.tscn")
	for wait in range(600):
		await process_frame
		if town() and town().visible: break
	# Until the Buck Rogers creation screen comes (#312), the party screen
	# cannot start a Buck Rogers game, and so cannot load its save.
	var status: String = current_scene.get_node("Status").text if current_scene and current_scene.has_node("Status") else ""
	if town() == null and status.contains("no character choices"):
		print("Skipped: the party screen cannot start a Buck Rogers game yet: " + status)
		quit(0)
		return
	require(town() != null and town().visible, "The save loads onto the exploration screen: " +
		(current_scene.get_node("Status").text if current_scene and current_scene.has_node("Status") else str(current_scene)))
	await settle()
	require(town().get_node("%SolarFlight").visible and not town().get_node("%Forward").is_visible_in_tree(),
		"The solar map's pad takes the 3D view's turns")
	require(label("Title").contains("Solar map") and label("MapTitle") == "En route", "The title names the solar map: " + label("Title"))
	require(rocket().begins_with("Rocket (9, 8)"), "The rocket starts on Earth's cell: " + rocket())
	require(not town().get_node("%RocketMove").visible, "Fly at once needs no Move")
	var cards := 0
	for slot in range(8):
		var card: Control = town().get_node("%PartyList/Rows/Member" + str(slot))
		if card.visible: cards += 1
	require(cards == 6, "The team's cards stay on the solar map: " + str(cards))
	var fuel := label("FuelText")
	var time := label("SolarTime")
	require(fuel == "450 / 450" or fuel.ends_with("/ 450"), "The fuel always shows: " + fuel)

	# The pad flies east, a key south: each step spends fuel and three days.
	await fly("PadEast")
	require(rocket() == "Rocket (10, 8)   East", "The pad flies a cell east: " + rocket())
	require(label("FuelText") != fuel and label("SolarTime") != time,
		"A step spends fuel and time: " + label("FuelText") + ", " + label("SolarTime"))
	await fly_key(KEY_DOWN)
	require(rocket() == "Rocket (10, 9)   South", "The down arrow flies a cell south: " + rocket())

	# The pointer over Earth's cell names it; elsewhere nothing is named.
	await point_at(Vector2(9, 8))
	require(solar().get("hover_name", "") == "Earth", "The pointer over Earth names it: " + str(solar().get("hover_name", "")))
	await point_at(Vector2(0, 21))
	require(not solar().has("hover_name"), "Empty space names nothing")

	# A step back onto Earth: its script's menu, then Exit to fly on.
	await key(KEY_HOME)
	var choices: ItemList = town().get_node("%Choices")
	var offered := []
	for i in range(choices.item_count): offered.append(choices.get_item_text(i))
	require(rocket() == "Rocket (9, 8)   Northwest" and offered == ["SALVATION", "TYCHO", "EXIT"],
		"At Earth the script offers Salvation and Tycho: " + rocket() + " " + str(offered))
	require(label("MapTitle") == "Earth" and solar().get("picture_name", "") == "EARTH", "Earth's picture is named")
	await shot("earth")
	await key(KEY_RIGHT)
	require(rocket().begins_with("Rocket (9, 8)"), "The pad and keys wait while the body's menu asks")
	choices.select(2)
	choices.item_selected.emit(2)
	await press(town().get_node("%Continue"))
	await go_on()
	require(not town().get_node("%PadEast").disabled, "Exit leaves Earth's orbit, free to fly")
	await check_fits(Vector2i(1120, 800))

	# Fly at once off: the original's Move mode, Move first and Done to stop.
	await set_option("DirectFlight", false)
	var move: Button = town().get_node("%RocketMove")
	require(move.visible and move.text.begins_with("Move"), "Fly at once off shows Move")
	require(town().get_node("%PadEast").disabled, "The pad waits for Move")
	var before := rocket()
	await key(KEY_RIGHT)
	require(rocket() == before, "A key does not fly before Move: " + rocket())
	await press(move)
	require(move.text.begins_with("Done") and not town().get_node("%PadEast").disabled, "Move turns to Done and frees the pad")
	await fly("PadEast")
	require(rocket().begins_with("Rocket (10, 8)"), "In Move mode the pad flies: " + rocket())
	await key(KEY_M)
	require(move.text.begins_with("Move") and town().get_node("%PadEast").disabled, "M is Done: the flight stops")

	# Course flown on: the cells flown show as dots.
	require((solar()["course"] as PackedVector2Array).is_empty(), "No course shows by default")
	await set_option("CourseFlown", true)
	await set_option("DirectFlight", true)
	await fly("PadSouth")
	require(not (solar()["course"] as PackedVector2Array).is_empty(), "Course flown on marks the cells flown")
	await point_at(Vector2(9, 8))
	await shot("course")
	require((solar()["orbits"] as PackedVector2Array).size() > 40, "The five orbits show as dots")

	# Rocket shows the ship's systems in the dialogue.
	await press(town().get_node("%RocketView"))
	var said: String = town().get_node("%Dialogue").get_parsed_text()
	require(label("Speaker") == "Maelstrom Rider" and said.contains("Hull: 600 of 600") and said.contains("Fuel"),
		"Rocket shows the ship: " + label("Speaker") + " " + said)
	await check_fits(Vector2i(1920, 1080))
	print("Solar map passed: flown by the pad and the keys, both flight settings, Earth's menu, the pointer's name and the course.")
	quit(0)
