extends SceneTree

# Creation in the Buck Rogers games on the real screen
# (docs/CHARACTER-CREATION.md, "The Buck Rogers games"): the startup scene
# with --game=FOLDER and --rules=original reaches the creation screen, and a
# character is made page by page with the mouse, in the smallest window,
# 1120 x 800.
#
# Countdown to Doomsday (--game=buck-rogers-countdown-to-doomsday): a Desert
# Runner Warrior, its race's careers grayed, its seven abilities with the
# race's adjustments and the career's minimums, its weapon specialization, a
# career skill raised to its cap of 30, Jury Rig locked until Repair
# Electrical and Repair Mechanical reach 10, unspent points asked about, a
# body icon, and the sheet.
#
# Matrix Cubed (--game=buck-rogers-matrix-cubed): an Engineer at level 7
# with its preset skills, a point moved from Notice to Jury Rig.
#
# --shots=DIR saves each page in a windowed run; --size=WxH sets another
# window size.

var folder := ""
var shots := ""
var window_size := Vector2i(1120, 800)

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--game="): folder = arg.trim_prefix("--game=")
		if arg.begins_with("--shots="): shots = arg.trim_prefix("--shots=")
		if arg.begins_with("--size="):
			var parts := arg.trim_prefix("--size=").split("x")
			window_size = Vector2i(int(parts[0]), int(parts[1]))
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle(frames := 6) -> void:
	for frame in range(frames): await process_frame

func shot(name: String) -> void:
	if shots.is_empty(): return
	DirAccess.make_dir_recursive_absolute(shots)
	await settle()
	RenderingServer.force_draw(false)
	await process_frame
	root.get_texture().get_image().save_png(shots.path_join(name + ".png"))

func node(path: String) -> Node:
	var found := current_scene.get_node_or_null(path)
	require(found != null, "Missing node: " + path)
	return found

func text_of(path: String) -> String:
	return (node(path) as Label).text

func press(path: String) -> void:
	var button: Button = node(path)
	var status: Label = current_scene.get_node_or_null("Status")
	require(button.is_visible_in_tree() and not button.disabled, "Unavailable control: " + path +
		(" (" + status.text + ")" if status else ""))
	button.pressed.emit()
	await settle()

func choose(text: String) -> void:
	var list: ItemList = node("Choices")
	for i in range(list.item_count):
		if list.get_item_text(i) == TranslationServer.translate(text):
			require(not list.is_item_disabled(i), "Disabled choice: " + text)
			list.select(i)
			list.item_selected.emit(i)
			await settle()
			return
	require(false, "Missing choice: " + text)

func disabled_choice(text: String) -> bool:
	var list: ItemList = node("Choices")
	for i in range(list.item_count):
		if list.get_item_text(i) == TranslationServer.translate(text):
			return list.is_item_disabled(i)
	require(false, "Missing choice: " + text)
	return false

# Every control of the page inside the window: nothing cut off at 1120 x 800.
func fits(page: String) -> void:
	var window := Rect2(Vector2.ZERO, Vector2(root.size))
	for control in current_scene.find_children("*", "Control", true, false):
		var c := control as Control
		if not c.is_visible_in_tree() or c.get_parent() is ScrollContainer: continue
		if c.get_global_rect().size == Vector2.ZERO: continue
		var inside := false
		var parent := c.get_parent()
		while parent != null:
			if parent is ScrollContainer: inside = true
			parent = parent.get_parent()
		if inside: continue
		require(window.encloses(c.get_global_rect().grow(-1)),
			page + ": " + str(c.get_path()) + " leaves the window: " + str(c.get_global_rect()))

func game_files_here() -> bool:
	var here := StartupView.game_folder(folder)
	return not here.is_empty() and FileAccess.file_exists(here.path_join("CHARS.DAX"))

func reach_creation() -> void:
	for wait in range(600):
		await process_frame
		var scene := current_scene
		if scene == null: continue
		if scene.name == "CharacterCreation": break
		var game_menu: Window = scene.get_node_or_null("GameMenu")
		if game_menu and game_menu.visible:
			game_menu.get_node("Buttons/NewParty").pressed.emit()
			await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", folder + " must reach the creation screen")
	await settle()

func name_and_race(name: String, race: String) -> void:
	var field: LineEdit = node("Name")
	require(field.max_length == 15, "A Buck Rogers name holds 15 letters")
	field.text = name
	field.text_changed.emit(name)
	await settle()
	await choose(race)
	require(not node("Description").get_parsed_text().is_empty(), "The race is described")
	await shot("01-race")
	fits("Name, race and gender")
	await press("Next")

func roll() -> void:
	await press("BuckRogersRoll/RollRow/Roll")
	for ability in range(7):
		require(text_of("BuckRogersRoll/Table/Score%d" % ability).is_valid_int(), "Ability %d rolled" % ability)
	require(text_of("BuckRogersRoll/Table/Ability6") == TranslationServer.translate("Tech"), "The seventh ability is Tech")
	await shot("03-roll")
	fits("The roll")

func countdown() -> void:
	await name_and_race("Mira Kade", "Desert Runner")
	require(disabled_choice("Medic") and disabled_choice("Rogue"), "A Desert Runner can't be a Medic or a Rogue")
	await choose("Warrior")
	require(node("Description").get_parsed_text().contains(TranslationServer.translate("Notice")), "The career lists its skills")
	await shot("02-career")
	fits("Career")
	await press("Next")
	await roll()
	require(text_of("BuckRogersRoll/Table/Race0") == "+2" and text_of("BuckRogersRoll/Table/Minimum0") == "10",
		"Strength shows the Desert Runner's +2 and the Warrior's 10")
	require(int(text_of("BuckRogersRoll/Table/Score0")) >= 10, "Strength reaches the Warrior's minimum")
	require(text_of("BuckRogersRoll/RollRow/Rolled").contains("Level 2"), "The roll makes a level-2 character")
	await press("Next")

	# A Warrior's weapon specialization (SpecializationMenu): one in Countdown.
	require(node("Specializations").visible and (node("Next") as Button).disabled, "The Warrior's page waits for a choice")
	var raise: Button = null
	for child in node("Specializations/Scroll/Rows").get_children():
		if child.name.begins_with("Raise") and not child.disabled:
			raise = child
			break
	require(raise != null, "A weapon can be chosen")
	var weapon := String(raise.name).trim_prefix("Raise")
	await press("Specializations/Scroll/Rows/Raise" + weapon)
	require(text_of("Specializations/Scroll/Rows/Level" + weapon) == "+1", "The weapon is specialized")
	var specialized := text_of("Specializations/Scroll/Rows/Weapon" + weapon)
	await shot("02b-specialization")
	fits("Specialization")
	await press("Next")

	# The skill screen: a career skill to its cap of 30.
	require(node("Skills").visible, "The skill screen follows")
	for point in range(40):
		if (node("Skills/Scroll/Rows/Raise83") as Button).disabled: break
		await press("Skills/Scroll/Rows/Raise83")
	require(text_of("Skills/Scroll/Rows/Points83") == "30" and (node("Skills/Scroll/Rows/Raise83") as Button).disabled
		and text_of("Skills/Scroll/Rows/Note83") == TranslationServer.translate("capped at {points}").format({"points": 30}),
		"Notice stops at 30, its + disabled and the reason beside it")
	# Jury Rig, locked until Repair Electrical and Repair Mechanical have 10.
	await press("Skills/Groups/Tab1")
	require(text_of("Skills/Scroll/Rows/Note6") == TranslationServer.translate("locked") and (node("Skills/Scroll/Rows/Raise6") as Button).disabled,
		"Jury Rig is locked")
	await press("Skills/Scroll/Rows/Skill6")
	require(text_of("Skills/Needs").contains(TranslationServer.translate("Repair Electrical")), "The screen says what Jury Rig needs: " + text_of("Skills/Needs"))
	await shot("04-skills")
	fits("Skills")
	for point in range(10):
		await press("Skills/Scroll/Rows/Raise1")
		await press("Skills/Scroll/Rows/Raise2")
	require(text_of("Skills/Scroll/Rows/Note6").is_empty() and not (node("Skills/Scroll/Rows/Raise6") as Button).disabled,
		"Jury Rig unlocks once both reach 10")
	await press("Skills/Scroll/Rows/Raise6")
	require(text_of("Skills/Scroll/Rows/Points6") == "1", "Jury Rig takes a point")
	await press("Skills/Scroll/Rows/Lower1")
	require(text_of("Skills/Scroll/Rows/Points6") == "0", "Lowering Repair Electrical below 10 takes Jury Rig's point back")
	await press("Skills/Scroll/Rows/Raise1")
	await press("Skills/Scroll/Rows/Raise6")
	# Points left: leaving asks first.
	await press("Next")
	var question: Window = node("UnspentSkills")
	require(question.visible and node("Skills").visible, "Unspent points are asked about")
	question.emit_signal("confirmed")
	question.hide()
	await settle()

	# The body icon: the 36 of CHARS.DAX.
	require(node("BodyIcons").visible, "The body icon page follows the skills")
	await press("BodyIcons/Grid/Icon5")
	require((node("BodyIcons/Grid/Icon5") as Button).button_pressed and (node("BodyIcons/Preview/Ready") as TextureRect).texture != null,
		"The chosen icon shows ready and in action")
	await shot("05-icon")
	fits("Body icon")
	await press("Next")
	var sheet: String = node("Description").get_parsed_text()
	for expected in ["Mira Kade", "Desert Runner", "Warrior", TranslationServer.translate("Tech"), TranslationServer.translate("Jury Rig"), specialized + " +1"]:
		require(sheet.contains(expected), "The sheet shows " + expected + ":\n" + sheet)
	await shot("06-sheet")
	fits("Sheet")
	await press("AddParty")
	require((node("PartyPanel/Roster") as ItemList).item_count == 1, "The character joins the party")
	await shot("07-party")
	print("Buck Rogers creation passed: Countdown.")

func matrix_cubed() -> void:
	await name_and_race("Rook", "Terran")
	await choose("Engineer")
	await press("Next")
	await roll()
	require(text_of("BuckRogersRoll/RollRow/Rolled").contains("Level 7"), "Matrix Cubed starts at level 7")
	await press("Next")
	require(node("Skills").visible, "An Engineer goes from the roll to the skills")
	require(text_of("Skills/Scroll/Rows/Points6") == "20" and (node("Skills/Scroll/Rows/Raise6") as Button).disabled,
		"Jury Rig starts at its preset 20, with no points to spend")
	require(text_of("Skills/Needs").contains(TranslationServer.translate("First Aid")), "The presets outside the career's group are listed")
	await shot("04b-skills-matrix")
	fits("Matrix Cubed skills")
	await press("Skills/Scroll/Rows/Lower83")
	require(text_of("Skills/Scroll/Rows/Points83") == "24", "Notice gives a point")
	await press("Skills/Scroll/Rows/Raise6")
	require(text_of("Skills/Scroll/Rows/Points6") == "21" and (node("Skills/Scroll/Rows/Raise6") as Button).disabled,
		"The point moves to Jury Rig")
	await press("Next")
	await press("Next")
	var sheet: String = node("Description").get_parsed_text()
	require(sheet.contains("Level 7") and sheet.contains(TranslationServer.translate("Jury Rig")), "The sheet shows the level and the skills:\n" + sheet)
	await press("AddParty")
	print("Buck Rogers creation passed: Matrix Cubed.")

func run_checks() -> void:
	if not game_files_here():
		print("Skipped: " + folder + "'s files are not here.")
		quit(0)
		return
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await reach_creation()
	root.size = window_size
	await settle()
	if folder == "buck-rogers-matrix-cubed":
		await matrix_cubed()
	else:
		await countdown()
	quit(0)
