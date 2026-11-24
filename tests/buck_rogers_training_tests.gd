extends SceneTree

# A Buck Rogers training's choices on the creation screen's pages
# (docs/games/countdown.md, "Training and specializations"), played through the
# real screens from the save opengold_countdown_training_fixture writes: a new
# team in Earth's orbit, the Warrior Runner at level 3 with level 4's
# experience. The team docks at Salvation, HQ's team menu trains Runner, and
# the specialization page and the skill screen follow; leaving them keeps the
# choices and the team menu shows again. Run by tests/settings_check.cmake with
# --game, --rules=original and --load-save.

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

func node(path: String) -> Node:
	var found := current_scene.get_node_or_null(path)
	require(found != null, "Missing node: " + path)
	return found

func press(button: Button) -> void:
	require(button.is_visible_in_tree() and not button.disabled, button.name + " can be pressed")
	button.pressed.emit()
	await settle()

func offered() -> Array:
	var choices: ItemList = town().get_node("%Choices")
	var texts := []
	for i in range(choices.item_count): texts.append(choices.get_item_text(i))
	return texts

func pick(index: int) -> void:
	var choices: ItemList = town().get_node("%Choices")
	if choices.visible and choices.item_count > index:
		choices.select(index)
		choices.item_selected.emit(index)
	await press(town().get_node("%Continue"))

# The first enabled + of a page's rows.
func first_raise(rows: String) -> Button:
	for child in node(rows).get_children():
		if child is Button and String(child.name).begins_with("Raise") and not child.disabled:
			return child
	return null

func run_checks() -> void:
	if StartupView.game_folder(FOLDER).is_empty():
		print("Skipped: Countdown to Doomsday's files are not here.")
		quit(0)
		return
	root.size = Vector2i(1120, 800)
	change_scene_to_file("res://scenes/startup.tscn")
	for wait in range(600):
		await process_frame
		if town() and town().visible: break
	require(town() != null and town().visible, "The save loads onto the exploration screen")
	await settle()

	# Out of Earth's orbit and back: Earth's menu offers Salvation.
	await press(town().get_node("%PadEast"))
	await press(town().get_node("%PadWest"))
	var trained := false
	for page in range(80):
		if node("Specializations").visible: break
		var texts := offered()
		var continue_button: Button = town().get_node("%Continue")
		if not continue_button.visible or continue_button.disabled:
			await settle()
			continue
		var chosen := 0
		for wanted in ["Train Runner", "HQ", "SALVATION", "FLEE"]:
			if texts.has(wanted):
				chosen = texts.find(wanted)
				trained = wanted == "Train Runner"
				break
		await pick(chosen)
		if trained: break
	await settle()
	require(trained and node("Specializations").visible and not town().visible,
		"Train opens the specialization page in the town's place: " + str(offered()))
	require((node("PageTitle") as Label).text.contains("Runner") and (node("Next") as Button).disabled,
		"The page names the trainee and waits for the specialization")
	require(not (node("Skills") as Control).visible and not (node("Steps") as Control).visible,
		"Only the training's page shows")

	var raise := first_raise("Specializations/Scroll/Rows")
	require(raise != null, "A weapon can be specialized in")
	var weapon := String(raise.name).trim_prefix("Raise")
	var level_before := (node("Specializations/Scroll/Rows/Level" + weapon) as Label).text
	await press(raise)
	require((node("Specializations/Scroll/Rows/Level" + weapon) as Label).text != level_before and
		not (node("Next") as Button).disabled and first_raise("Specializations/Scroll/Rows") == null,
		"One specialization is chosen, and no more")
	await press(node("Next"))

	require(node("Skills").visible and not node("Specializations").visible, "The skill screen follows")
	var points := (node("Skills/Points") as Label).text
	require(points.contains("40") and points.contains("20"), "The training gives 40 career and 20 general points: " + points)
	var skill := first_raise("Skills/Scroll/Rows")
	require(skill != null, "A career skill can be raised")
	var number := String(skill.name).trim_prefix("Raise")
	var skill_before := int((node("Skills/Scroll/Rows/Points" + number) as Label).text)
	await press(skill)
	require(int((node("Skills/Scroll/Rows/Points" + number) as Label).text) == skill_before + 1 and
		(node("Skills/Points") as Label).text.contains("39"), "A point is spent")

	# Points are left: leaving asks first, then the town's team menu returns.
	await press(node("Next"))
	var question: Window = node("UnspentSkills")
	require(question.visible and node("Skills").visible, "Unspent points are asked about")
	question.emit_signal("confirmed")
	question.hide()
	await settle(12)
	require(town().visible and not node("Skills").visible and not node("Specializations").visible,
		"The town shows again")
	var said: String = town().get_node("%Dialogue").get_parsed_text()
	require(said.contains("Runner") and said.contains("4") and not offered().has("Train Runner"),
		"The team menu says Runner trained: " + said + " " + str(offered()))
	print("Buck Rogers training passed: Train, the specialization page, the skill screen and the team menu again.")
	quit(0)
