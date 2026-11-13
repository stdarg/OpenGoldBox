extends SceneTree

# The startup menu (the user's design, 2026-11-12): after the language, a modal
# dialog with Options, a button for each game and Exit to desktop. Run by
# tests/settings_check.cmake; --menu-capture=DIR saves a picture of it.
# Curse's button is checked when OPENGOLD_GOLDBOX_DIR holds its files.

var captures := ""
var exit_case := false

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--menu-capture="): captures = arg.trim_prefix("--menu-capture=")
		if arg == "--menu-exit": exit_case = true
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle() -> void:
	for frame in range(8): await process_frame

# The new game's first question (docs/ORIGINAL-RULES.md): the rules it plays
# by, the SRD's offered first for now; Begin starts the game with them.
func answer_rules(original := false) -> void:
	await settle()
	var dialog: Window = current_scene.get_node_or_null("RulesDialog") if current_scene else null
	if dialog and dialog.visible:
		if original:
			var rules: OptionButton = dialog.get_node("Rows/Rules")
			rules.select(0)
			rules.item_selected.emit(0)
		dialog.get_node("Rows/Begin").pressed.emit()
		await settle()
		await settle()

func choose(text: String) -> void:
	var list: ItemList = current_scene.get_node("Choices")
	for i in range(list.item_count):
		if list.get_item_text(i) == text:
			require(not list.is_item_disabled(i), "Unavailable choice: " + text)
			list.select(i)
			list.item_selected.emit(i)
			await settle()
			return
	require(false, "Missing choice: " + text)

func press(path: String) -> void:
	var button: Button = current_scene.get_node(path)
	require(button.visible and not button.disabled, "Unavailable control: " + path)
	button.pressed.emit()
	await settle()

func capture(name: String) -> void:
	if captures.is_empty(): return
	DirAccess.make_dir_recursive_absolute(captures)
	await process_frame
	RenderingServer.force_draw(false)
	await process_frame
	var menu: Window = current_scene.get_node("GameMenu")
	menu.get_texture().get_image().save_png(captures.path_join(name + "-menu.png"))
	root.get_texture().get_image().save_png(captures.path_join(name + "-screen.png"))

func run_checks() -> void:
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	require(current_scene.name == "Startup", "Startup scene missing")
	var image: TextureRect = current_scene.get_node("ImageFit/Image")
	require(image.texture != null, "The title picture must show behind the menu")
	var menu: Window = current_scene.get_node("GameMenu")
	require(menu.visible and menu.exclusive, "The startup menu must be a modal dialog")
	var buttons := menu.get_node("Buttons")
	var labels := []
	for child in buttons.get_children():
		if child is Button: labels.append(child.text)
		else:
			for game in child.get_children(): labels.append(game.text)
	require(labels == ["Options", "Pool of Radiance", "Curse of the Azure Bonds", "Exit to desktop"],
		"The menu offers Options, each game and Exit to desktop: " + str(labels))
	await capture("startup")
	if exit_case:
		current_scene.tree_exiting.connect(func(): print("Startup menu passed: Exit to desktop."))
		menu.get_node("Buttons/Exit").pressed.emit()
		await settle()
		require(false, "Exit to desktop did not quit")
		return
	# Options opens alone over the title, and closing it brings the menu back.
	menu.get_node("Buttons/Options").pressed.emit()
	await settle()
	var options: Control = current_scene.get_node("Options")
	require(options.visible and options.get_node("Options").visible and not options.get_node("Menu").visible,
		"Options must open without the in-game menu")
	require(not menu.visible, "The menu steps aside for Options")
	options.get_node("Options/Close").pressed.emit()
	await settle()
	require(menu.visible and current_scene.get_node_or_null("Options") == null, "Closing Options returns to the menu")
	var curse := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("CURSE")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(curse.path_join("TILES.DAX")):
		print("Startup menu passed: Options and back (Curse's files are not here).")
		quit(0)
		return
	menu.get_node("Buttons/Games/CURSE").pressed.emit()
	await settle()
	await answer_rules(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Curse's button must lead on to the party")
	# Curse's own creation under the original rules: a human may be a paladin
	# or ranger, and starts trained up from 25,000 experience with 300 platinum.
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Taliesin"
	name.text_changed.emit(name.text)
	await settle()
	await choose("Human")
	await press("Next")
	await choose("Paladin")
	await choose("Ranger")
	await press("Next")
	await choose("Neutral Good")
	await press("Next")
	await press("Roll")
	var rolled: String = current_scene.get_node("SwapHint").text
	require(rolled.contains("Platinum 300"), "A Curse character starts with 300 platinum: " + rolled)
	# The exploration screen plays the chosen game's campaign: Curse's opening.
	change_scene_to_file("res://scenes/rolf_tour.tscn")
	var dialogue := ""
	for wait in range(120):
		await process_frame
		var label: RichTextLabel = current_scene.find_child("Dialogue", true, false) if current_scene else null
		if label and label.get_parsed_text().contains("AWAKEN"):
			dialogue = label.get_parsed_text()
			break
	if not dialogue.contains("YOU AWAKEN IN A SMALL ROOM"):
		var shown: RichTextLabel = current_scene.find_child("Dialogue", true, false)
		print("Exploration shows: ", shown.get_parsed_text() if shown else "no dialogue", " in ", current_scene.name if current_scene else "no scene")
	require(dialogue.contains("YOU AWAKEN IN A SMALL ROOM"), "Curse's opening must play on the exploration screen")
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("curse-exploration.png"))
	print("Startup menu passed: Options and back, Curse's folder found, the party screen and Curse's opening.")
	quit(0)
