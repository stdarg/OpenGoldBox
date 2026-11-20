extends SceneTree
# The Buck Rogers ship combat screen (docs/games/countdown.md, "Ship combat";
# scenes/ship_combat.tscn), clicked through seeded fights of the walk's team
# (opengold_countdown_campaign_tests writes it): the opening's lit orders
# once a member takes the helm, the enemy's rounds hidden until Sensors reads
# them, Target's choice of system, a crew member's orders, a system hit's
# colours, boarding at range 0, withdrawing out of range, the picture shrinking
# with the range or not, and the fight's outcome. Every state fits the
# smallest window, 1120 x 800. Run with --fixtures=<folder of
# countdown-team.save>; --screenshots=<folder> also saves the screen at
# 1920 x 1080 and 1120 x 800 (needs a window).
var fixtures := ""
var screenshots := ""
var folder := ""
var view: Control

const MAIN := "Margin/Columns/Main/"

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--fixtures="): fixtures = arg.trim_prefix("--fixtures=")
		if arg.begins_with("--screenshots="): screenshots = arg.trim_prefix("--screenshots=")
	call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
	if not ok:
		push_error(message)
		quit(1)
		assert(ok, message)

func settle() -> void:
	for frame in range(3): await process_frame

func button(path: String) -> Button:
	return view.get_node(MAIN + "Orders/" + path)

func lit(path: String) -> bool:
	var order := button(path)
	return order.visible and not order.disabled

func press(path: String) -> void:
	require(lit(path), path + " is lit to press")
	button(path).pressed.emit()
	await settle()

func answers() -> Array:
	return view.get_node(MAIN + "Choice/Answers").get_children()

func answer(text: String) -> bool:
	for choice in answers():
		if (choice as Button).text == text:
			(choice as Button).pressed.emit()
			await settle()
			return true
	return false

func question() -> String:
	return (view.get_node(MAIN + "Choice/Question") as Label).text

func log_text() -> String:
	return (view.get_node(MAIN + "Log") as RichTextLabel).get_parsed_text()

func range_shown() -> int:
	var text: String = (view.get_node(MAIN + "Scene/SceneText/Lines/Range") as Label).text
	return int(text.trim_prefix("Range: "))

func enemy_weapon_cells() -> Array:
	var cells := []
	for cell in view.get_node(MAIN + "Ships/Enemy/Margin/Rows/Weapons").get_children():
		cells.append((cell as Label).text)
	return cells

func system_row(ship: String, system: int) -> Array:
	var grid := view.get_node(MAIN + "Ships/" + ship + "/Margin/Rows/Systems")
	return [grid.get_child(system * 4 + 2), grid.get_child(system * 4 + 3)]

func over() -> bool:
	return view.check_outcome() >= 0

# The fight waits for the dodge, for a round no one can act in, or for the
# acting member's order: one step of a player wanting the enemy's decks.
func step_toward_boarding() -> void:
	if question() == "Try to dodge the enemy ship?":
		await answer("Yes")
	elif await answer("Next round"):
		pass
	elif lit("Second/Board"):
		await press("Second/Board")
	elif lit("Second/Command"):
		await press("Second/Command")
	elif lit("Second/Close"):
		await press("Second/Close")
	elif lit("First/Target"):
		await press("First/Target")
		require(await answer("Engines"), "Target offers the engines")
	elif lit("First/Fire"):
		await press("First/Fire")
	elif lit("First/Load"):
		await press("First/Load")
		require(answers().size() >= 1, "Load asks which weapon")
		(answers()[0] as Button).pressed.emit()
		await settle()
	else:
		await press("First/Quit")

# Every control of the screen lies inside the window.
func check_fit(where: String) -> void:
	var window := Rect2(Vector2.ZERO, Vector2(root.size))
	var minimum: Vector2 = view.get_combined_minimum_size()
	require(minimum.x <= window.size.x + 0.5 and minimum.y <= window.size.y + 0.5,
		"The screen fits " + str(window.size) + " " + where + ": needs " + str(minimum))
	for path in [MAIN + "Scene", MAIN + "RangeTrack", MAIN + "Ships", MAIN + "Orders", MAIN + "Choice",
			MAIN + "Status", MAIN + "Log", MAIN + "Help", "Margin/Columns/Crew"]:
		var control: Control = view.get_node(path)
		var rect := control.get_global_rect()
		require(not control.visible or window.encloses(rect.grow(-0.5)),
			path + " lies in the window " + where + ": " + str(rect))

func shoot(name: String) -> void:
	if screenshots.is_empty(): return
	await settle()
	RenderingServer.force_draw(false)
	await process_frame
	var image := root.get_texture().get_image()
	DirAccess.make_dir_recursive_absolute(screenshots)
	image.save_png(screenshots.path_join(name + "-" + str(root.size.x) + "x" + str(root.size.y) + ".png"))

func stage(seed: int, shrinks: bool, enemy_type := 1) -> void:
	change_scene_to_file("res://scenes/ship_combat.tscn")
	await settle()
	view = current_scene
	require(view.stage_check(folder, fixtures.path_join("countdown-team.save"), enemy_type, 50, 18, seed, shrinks),
		"A seeded fight of the walk's team opens")
	await settle()

func run_checks() -> void:
	folder = StartupView.game_folder("buck-rogers-countdown-to-doomsday")
	var goldbox := OS.get_environment("OPENGOLD_GOLDBOX_DIR")
	if folder.is_empty() and not goldbox.is_empty():
		folder = goldbox.path_join("buck-rogers-countdown-to-doomsday")
	if not FileAccess.file_exists(folder.path_join("SHIPS.DAX")): folder = ""
	if folder.is_empty() or not FileAccess.file_exists(fixtures.path_join("countdown-team.save")):
		print("Skipped: Countdown to Doomsday's files or the walk's team are not here.")
		quit(0)
		return
	TranslationServer.set_locale("en")
	for size in [Vector2i(1920, 1080), Vector2i(1120, 800)]:
		root.size = size
		await check_fight(size)
	await check_withdraw()
	await check_shrinking()
	print("Ship combat view checks passed")
	quit(0)

func check_fight(size: Vector2i) -> void:
	await stage(3, true)
	var where := str(size)
	# The opening: range 8, the enemy closing, its rounds unknown; no one is
	# at the helm yet, so the first member may take COMMAND.
	require(range_shown() == 8 and (view.get_node(MAIN + "Scene/SceneText/Lines/Maneuver") as Label).text == "Closing",
		"The fight opens at range 8, the enemy closing")
	require(enemy_weapon_cells().has("?"), "The enemy's rounds read ? before Sensors")
	require(lit("Second/Command") and button("First/Load").visible and not button("Second/Close").visible,
		"With no one at the helm, the first member's crew orders offer COMMAND")
	await check_fit(where + " at the opening")
	# The Rocketjock works the sensors; the next member takes the helm.
	require(lit("Second/Sensors"), "The Rocketjock may use the sensors")
	await press("Second/Sensors")
	var sensed := not enemy_weapon_cells().has("?")
	await press("Second/Command")
	# The pilot at range 8: only Close, Withdraw and Quit (and View) are lit;
	# Fire, Target, Ram and Board stay in their places, greyed out.
	for path in ["Second/Close", "Second/Withdraw", "First/Quit", "First/View"]:
		require(lit(path), path + " is lit for the pilot at range 8")
	for path in ["First/Fire", "First/Target", "Second/Ram", "Second/Board"]:
		require(button(path).visible and button(path).disabled, path + " is shown greyed out at range 8")
	require(not button("First/Load").visible and not button("Second/Sensors").visible,
		"The crew's orders step aside for the pilot's")
	require((view.get_node(MAIN + "Prompt") as Label).text.contains("is your pilot"), "The prompt names the pilot")
	await shoot("09-ship-opening")
	# View shows the ship, then Back.
	await press("First/View")
	require(question() == "View:" and await answer("Ship"), "View offers the ship")
	require(log_text().contains("tons, Armor Class"), "Viewing the ship writes its figures")
	await press("Second/Close")
	await press("First/Quit")

	# The crew: an Engineer's and others' orders, and Sensors until the
	# enemy's rounds show.
	var crew_seen := false

	var targeted := false
	var hit_seen := false
	for turn in range(400):
		if over(): break
		var prompt: String = (view.get_node(MAIN + "Prompt") as Label).text
		if prompt.begins_with("Gears:") and not crew_seen:
			crew_seen = true
			for path in ["First/Fire", "First/Target", "First/Load", "First/JuryRig", "First/Quit", "First/View",
					"Second/Command", "Second/Aid", "Second/Boost", "Second/Sensors"]:
				require(button(path).visible, path + " is among the Engineer's orders")
			require(lit("Second/Boost"), "The Engineer may boost the engines")
			require(not button("Second/Close").visible and not button("Second/Board").visible,
				"The pilot's orders step aside for a crew member's")
			await check_fit(where + " on a crew member's turn")
			await shoot("11-ship-crew-orders")
		if not sensed and lit("Second/Sensors"):
			await press("Second/Sensors")
			sensed = not enemy_weapon_cells().has("?")
			if sensed:
				require(log_text().contains("reads the enemy on the sensors"), "A Sensors success is in the log")
			continue
		if not targeted and prompt.contains("is your pilot") and lit("First/Target"):
			await press("First/Target")
			require(question() == "Target which system?" and answers().size() == 8,
				"Target asks which of the seven systems, with Back")
			await check_fit(where + " with Target's choice open")
			await shoot("10-ship-pilot-orders")
			require(await answer("Engines"), "The pilot aims at the engines")
			targeted = true
			continue
		if not hit_seen:
			for system in range(6):
				var row := system_row("Party", system)
				var value: Label = row[1]
				var parts := value.text.split(" / ")
				if int(parts[0]) < int(parts[1]):
					var filled: ColorRect = (row[0] as Control).get_node("Filled")
					var color := view.get_theme_color("score_positive" if int(parts[0]) > 0 else "score_negative", "OpenGoldPalette")
					require(filled.color == color or int(parts[0]) == 0, "A damaged system's bar takes the damaged colour")
					require(value.get_theme_color("font_color") == color, "A damaged system's value takes the damaged colour")
					hit_seen = true
					await check_fit(where + " after a system hit")
					await shoot("12-ship-system-hit")
					break
		if lit("Second/Board"):
			require(lit("Second/Ram") and range_shown() <= 1, "Board comes with Ram at range 1 or less")
			await check_fit(where + " with boarding offered")
			await shoot("13-ship-boarding")
		await step_toward_boarding()
	require(crew_seen and sensed and targeted and hit_seen, "The fight showed the crew's orders, Sensors, Target and a hit: "
		+ str([crew_seen, sensed, targeted, hit_seen]))
	require(over() and view.check_boarded() and view.check_outcome() == 0,
		"The team boards the disabled Hubble: outcome 0")
	require(log_text().contains("The team boards the"), "The boarding is in the log")
	require(not view.get_node(MAIN + "Orders").visible and await answer("Continue") and view.done(),
		"The ended fight closes with Continue")

func check_withdraw() -> void:
	root.size = Vector2i(1120, 800)
	# The Adder is slower than the Maelstrom Rider: withdrawing gains on it.
	await stage(5, true, 2)
	await press("Second/Command")
	for turn in range(200):
		if over(): break
		if question() == "Try to dodge the enemy ship?":
			await answer("Yes")
		elif await answer("Next round"):
			pass
		elif lit("Second/Withdraw"):
			await press("Second/Withdraw")
			await press("First/Quit")
		else:
			await press("First/Quit")
	require(over() and view.check_outcome() == 1 and not view.check_boarded(),
		"Withdrawing loses the enemy from the scanners: outcome 1")
	require(log_text().contains("is lost from the scanners"), "The log says the enemy is lost")

func check_shrinking() -> void:
	root.size = Vector2i(1920, 1080)
	await stage(3, true)
	var far: float = view.picture_share()
	require(abs(far - 0.4) < 0.02, "At range 8 the shrinking picture takes 4 tenths of the scene: " + str(far))
	await stage(3, false)
	require(abs(view.picture_share() - 1.0) < 0.01, "With the option off the picture fills the scene at range 8")
