extends SceneTree
# The allies Leadership leads, in a real campaign fight on the combat screen
# (docs/BUCK-ROGERS.md): Matrix Cubed's earned route saves its team one step
# east of the prison's surveillance room, facing it, with the prison gang
# behind it (opengold_matrix_cubed_earned_ending,
# OPENGOLD_MATRIX_LED_ALLIES_SAVE). The check loads that save, steps in and
# reads on to the fight. There each ally Leadership leads is the player's to
# command (and any it does not lead stays on Quick); and on a led ally's turn
# the player has its controls. Run by tests/settings_check.cmake with --game,
# --rules=original and --load-save.

const FOLDER := "buck-rogers-matrix-cubed"

func _initialize() -> void:
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle(frames := 8) -> void:
	for frame in range(frames): await process_frame

func node(path: String) -> Node:
	return current_scene.get_node_or_null(path) if current_scene else null

func town() -> Control:
	return node("CampaignTown")

func combat() -> Control:
	return node("CampaignCombat")

func fighting() -> bool:
	return combat() != null and combat().is_visible_in_tree()

func press(button: Button) -> bool:
	if button == null or not button.is_visible_in_tree() or button.disabled:
		return false
	button.pressed.emit()
	await settle()
	return true

func led_ally_acting() -> bool:
	for ally in combat().ally_controls():
		if ally["acting"] and ally["led"]: return true
	return false

func run_checks() -> void:
	if StartupView.game_folder(FOLDER).is_empty():
		print("Skipped: Matrix Cubed's files are not here.")
		quit(0)
		return
	root.size = Vector2i(1920, 1080)
	change_scene_to_file("res://scenes/startup.tscn")
	for wait in range(600):
		await process_frame
		if town() and town().visible: break
	require(town() != null and town().visible, "The save loads onto the exploration screen")
	await settle()
	await press(town().get_node("%Forward"))
	for attempt in range(40):
		if fighting(): break
		require(await press(town().get_node("%Continue")), "The surveillance room's text reads on to its fight")
	require(fighting(), "Stepping into the surveillance room begins its fight")
	await settle()
	var allies: Array = combat().ally_controls()
	var led := allies.filter(func(a): return a["led"])
	var unled := allies.filter(func(a): return not a["led"])
	print("Allies: ", allies.size(), ", led ", led.size(), ": ", led.map(func(a): return a["name"]))
	require(not led.is_empty(), "Leadership leads some of the gang: " + str(allies))
	# With the campaign's fight seed every member of this gang is led; one not
	# led would stay on Quick.
	require(led.all(func(a): return not a["quick"]), "Every led ally is the player's: " + str(led))
	require(unled.all(func(a): return a["quick"]), "Every ally not led stays on Quick: " + str(unled))
	# The team's turns are ended, and the computer plays the allies not led,
	# until a led ally's turn comes.
	var end: Button = combat().get_node("End")
	for wait in range(600):
		if led_ally_acting() and not end.disabled: break
		if not end.disabled:
			end.pressed.emit()
		await create_timer(0.05).timeout
	require(led_ally_acting(), "A led ally's turn comes")
	await settle()
	var acting: Dictionary = combat().ally_controls().filter(func(a): return a["acting"])[0]
	var computer := tr("{name} fights under computer control (Quick). Space or Take control: play the party yourself.").format({"name": acting["name"]})
	require(combat().get_node("Prompt").text != computer, "The led ally's turn is not the computer's")
	require(not combat().get_node("Move").disabled and not end.disabled,
		"The player can move the led ally or end its turn")
	print("Buck Rogers led allies passed")
	quit(0)
