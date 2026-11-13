extends SceneTree

# Making a character under the original rules on the real creation screen, as
# a player clicks it: the original's pages in its order, a class its race
# allows, an alignment its class allows, the roll kept or rolled again, and the
# sheet with the original's numbers. Run with --original-rules.

var captures := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-to="):
			captures = arg.trim_prefix("--capture-to=")
	call_deferred("run_checks")

# Saves the screen when run by hand with --capture-to=<folder>.
func capture(name: String) -> void:
	if captures.is_empty(): return
	DirAccess.make_dir_recursive_absolute(captures)
	await RenderingServer.frame_post_draw
	require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Cannot save screenshot")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle() -> void:
	for frame in range(6):
		await process_frame

func press(path: String) -> void:
	var button: Button = current_scene.get_node(path)
	require(button.visible and not button.disabled, "Unavailable control: " + path)
	button.pressed.emit()
	await settle()

func item(text: String) -> int:
	var list: ItemList = current_scene.get_node("Choices")
	for i in range(list.item_count):
		if list.get_item_text(i) == text:
			return i
	return -1

func choose(text: String) -> void:
	var list: ItemList = current_scene.get_node("Choices")
	var found := item(text)
	require(found >= 0 and not list.is_item_disabled(found), "Missing choice: " + text)
	list.select(found)
	list.item_selected.emit(found)
	await settle()

func page_title() -> String:
	return current_scene.get_node("PageTitle").text

func run_checks() -> void:
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	root.size = Vector2i(1120, 800)
	await settle()
	var steps: String = current_scene.get_node("Steps").text
	require(steps.find("2. Class") >= 0 and steps.find("3. Alignment") >= 0 and steps.find("4. Attributes") >= 0 and steps.find("Training") < 0, "The original's pages in its order: " + steps)
	var choices: ItemList = current_scene.get_node("Choices")
	require(choices.item_count == 6 and item("Half-Elf") >= 0, "The original's six races")
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Tordek"
	name.text_changed.emit(name.text)
	await settle()
	await choose("Dwarf")
	await press("Next")
	require(page_title() == "Class", "The class follows the race")
	await capture("class")
	require(choices.is_item_disabled(item("Magic-User")) and not choices.is_item_disabled(item("Fighter/Thief")), "A dwarf may not be a magic-user, but may be a fighter/thief")
	await choose("Fighter/Thief")
	await press("Next")
	require(page_title() == "Alignment", "The alignment follows the class")
	require(choices.is_item_disabled(item("Lawful Good")) and choices.is_item_disabled(item("Chaotic Good")), "A thief is never lawful or chaotic good")
	await choose("Chaotic Neutral")
	await press("Next")
	require(page_title() == "Attributes", "The roll follows the alignment")
	var next: Button = current_scene.get_node("Next")
	require(next.disabled, "The character must be rolled first")
	require(not current_scene.get_node("Background").visible and not current_scene.get_node("Dice0").visible, "No background or dice to assign")
	await press("Roll")
	var strength: Button = current_scene.get_node("Score0")
	require(not strength.text.is_empty(), "The roll shows the scores")
	var rolled: String = current_scene.get_node("SwapHint").text
	require(rolled.contains("Hit Points") and rolled.contains("Gold"), "The roll shows its Hit Points, age and gold: " + rolled)
	require(current_scene.get_node("Roll").text == "Roll again", "The character can be rolled again")
	await press("Roll")
	await capture("roll")
	await press("Next")
	require(page_title() == "Combat appearance", "No training or spells: the icon follows the roll, not " + page_title())
	await press("Next")
	await press("Next")
	await capture("sheet")
	var sheet: String = current_scene.get_node("Description").get_parsed_text()
	for shown in ["Tordek", "Fighter/Thief", "THAC0", "Saving throws", "Breath weapon", "Thief skills", "Age"]:
		require(sheet.contains(shown), "The sheet shows the original's " + shown + ":\n" + sheet)
	await press("AddParty")
	var roster: ItemList = current_scene.get_node("PartyPanel/Roster")
	require(roster.item_count == 1, "The character joins the party")
	print("Original creation view passed")
	quit(0)
