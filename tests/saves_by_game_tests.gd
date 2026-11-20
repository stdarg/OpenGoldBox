extends SceneTree

# Saves by game (docs/GAMES.md, phase 3 step 1.7): each game keeps its saves
# in its own folder, as each original had its SAVE folder, and its save and
# load screens list only that game's. A save of another game, put among them
# by hand, starts its own game when loaded. Run by tests/settings_check.cmake;
# skipped when the game finds no Curse's files.
#
# --found-load=confirm or --found-load=decline: a Curse save loaded in Pool of
# Radiance when the player never chose Curse's folder, which the search finds.

var found_load := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if arg.begins_with("--found-load="): found_load = arg.trim_prefix("--found-load=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle() -> void:
	for frame in range(8): await process_frame

# A title on screen, OpenGoldBox's, Pool of Radiance's new one or a game's own
# pictures (TITLE.DAX): a key passes each.
func pass_title() -> void:
	for screen in range(8):
		var scene := current_scene
		if scene == null or scene.name != "Startup": return
		var lettering: TextureRect = scene.get_node("ImageFit/Image/Text")
		if not scene.get_node("GameTitle").visible and not (lettering.visible and lettering.texture != null): return
		var event := InputEventKey.new()
		event.keycode = KEY_SPACE
		event.pressed = true
		Input.parse_input_event(event)
		for frame in range(6): await process_frame

# Startup's menus to the new game's question for the game (the user's design,
# 2026-11-14): OpenGoldBox's title, Pick game, the game's button, its title and
# its menu's Create new party.
func pick_game(folder := "POOLRAD") -> void:
	await pass_title()
	var menu: Window = current_scene.get_node("MainMenu")
	require(menu.visible and menu.exclusive, "The main menu must show")
	menu.get_node("Buttons/PickGame").pressed.emit()
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/" + folder).pressed.emit()
	await settle()
	await pass_title()
	var game_menu: Window = current_scene.get_node_or_null("GameMenu")
	if game_menu and game_menu.visible:
		game_menu.get_node("Buttons/NewParty").pressed.emit()
		await settle()

# The game picked on startup's menus and the new game's question answered:
# the original rules for Curse, the SRD's for Pool of Radiance.
func start(folder: String) -> void:
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await pick_game(folder)
	var dialog: Window = current_scene.get_node("RulesDialog")
	require(dialog.visible, "The new game's question follows " + folder + "'s title")
	var rules: OptionButton = dialog.get_node("Rows/Rules")
	rules.select(0 if folder == "CURSE" else 1)
	rules.item_selected.emit(rules.selected)
	dialog.get_node("Rows/Begin").pressed.emit()
	await settle()
	await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", folder + "'s button must lead on to the party")

func open_saves(saving: bool) -> Window:
	var button: Button = current_scene.get_node("PartyPanel/Save" if saving else "PartyPanel/Load")
	button.pressed.emit()
	await settle()
	var dialog: Window = current_scene.get_node("SaveSlots")
	require(dialog.visible, "The save screen must open")
	return dialog

func listed(dialog: Window) -> Array:
	var slots: ItemList = dialog.get_node("Slots")
	var names := []
	for i in range(slots.item_count): names.append(slots.get_item_text(i))
	return names

func save_as(name: String) -> void:
	var dialog := await open_saves(true)
	var field: LineEdit = dialog.get_node("Name")
	field.text = name
	field.text_changed.emit(name)
	dialog.get_node("Action").pressed.emit()
	await settle()
	require(not dialog.visible, "Saving " + name + " must close the save screen: " + dialog.get_node("Status").text)

# Presses the load screen's Load for a save, confirming it.
func press_load(name: String) -> void:
	var dialog := await open_saves(false)
	var slots: ItemList = dialog.get_node("Slots")
	var index := listed(dialog).find(name)
	require(index >= 0, name + " must be listed to load")
	slots.select(index)
	slots.item_selected.emit(index)
	for confirm in range(2):
		dialog.get_node("Action").pressed.emit()
		await settle()

func load_save(name: String) -> void:
	await press_load(name)
	var dialog: Window = current_scene.get_node("SaveSlots")
	require(not dialog.visible, "Loading " + name + " must close the save screen: " + dialog.get_node("Status").text)

func file_of(folder: String, name: String) -> String:
	return ProjectSettings.globalize_path(folder).path_join(name.to_utf8_buffer().hex_encode() + ".ogs")

func run_checks() -> void:
	var curse := StartupView.game_folder("CURSE")
	if curse.is_empty() or not FileAccess.file_exists(curse.path_join("TILES.DAX")):
		print("Skipped: Curse's files are not here.")
		quit(0)
		return
	if not found_load.is_empty():
		await check_found_load(curse)
		return
	# Pool of Radiance keeps its saves where they always were.
	await start("POOLRAD")
	await save_as("Phlan party")
	require(FileAccess.file_exists(file_of("user://saves", "Phlan party")), "Pool of Radiance saves into user://saves")
	# Curse lists none of Pool of Radiance's saves, and keeps its own apart.
	await start("CURSE")
	var dialog := await open_saves(false)
	require(listed(dialog).is_empty(), "Curse's load screen lists no Pool of Radiance save: " + str(listed(dialog)))
	dialog.get_node("Cancel").pressed.emit()
	await settle()
	await save_as("Tilverton party")
	require(FileAccess.file_exists(file_of("user://saves/CURSE", "Tilverton party")), "Curse saves into user://saves/CURSE")
	dialog = await open_saves(false)
	require(listed(dialog) == ["Tilverton party"], "Curse's load screen lists Curse's saves: " + str(listed(dialog)))
	dialog.get_node("Cancel").pressed.emit()
	await settle()
	# Pool of Radiance lists only its own.
	await start("POOLRAD")
	dialog = await open_saves(false)
	require(listed(dialog) == ["Phlan party"], "Pool of Radiance's load screen lists its own saves: " + str(listed(dialog)))
	dialog.get_node("Cancel").pressed.emit()
	await settle()
	# A Curse save put among Pool of Radiance's by hand starts Curse: the load
	# screen then lists Curse's saves, and exploring plays Curse's opening.
	require(DirAccess.copy_absolute(file_of("user://saves/CURSE", "Tilverton party"), file_of("user://saves", "Moved party")) == OK, "Cannot move the Curse save")
	await load_save("Moved party")
	dialog = await open_saves(false)
	require(listed(dialog) == ["Tilverton party"], "Loading a Curse save starts Curse: " + str(listed(dialog)))
	dialog.get_node("Cancel").pressed.emit()
	await settle()
	change_scene_to_file("res://scenes/rolf_tour.tscn")
	var opening := false
	for wait in range(120):
		await process_frame
		var label: RichTextLabel = current_scene.find_child("Dialogue", true, false) if current_scene else null
		if label and label.get_parsed_text().contains("YOU AWAKEN IN A SMALL ROOM"):
			opening = true
			break
	require(opening, "After a Curse save loads, exploring plays Curse's opening")
	await check_startup_saves()
	print("Saves by game passed: each game's own folder and list, a moved Curse save starting Curse, and startup's Continue and Load game.")
	quit(0)

# Startup's menus offer the saves (the user's design, 2026-11-14): the main
# menu's Continue, and a game's Continue and Load game, which lists its own.
func check_startup_saves() -> void:
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await pass_title()
	var main: Window = current_scene.get_node("MainMenu")
	require(main.visible and not main.get_node("Buttons/Continue").disabled, "The main menu's Continue offers the newest save")
	main.get_node("Buttons/PickGame").pressed.emit()
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/CURSE").pressed.emit()
	await settle()
	await pass_title()
	var game_menu: Window = current_scene.get_node("GameMenu")
	require(game_menu.visible and not game_menu.get_node("Buttons/Continue").disabled and not game_menu.get_node("Buttons/Load").disabled, "Curse's menu offers its saves")
	game_menu.get_node("Buttons/Load").pressed.emit()
	await settle()
	var slots: Window = current_scene.get_node("SaveSlots")
	require(slots.visible and listed(slots) == ["Tilverton party"], "Curse's Load game lists Curse's saves: " + str(listed(slots)))
	slots.get_node("Cancel").pressed.emit()
	await settle()
	require(game_menu.visible, "Closing the saves returns to the game's menu")
	game_menu.get_node("Buttons/Continue").pressed.emit()
	for wait in range(4): await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", "Continue opens the game")
	var loaded := false
	for label in current_scene.find_children("*", "Label", true, false):
		if label.visible and label.text == "Campaign loaded.": loaded = true
	require(loaded, "Continue loads the save")
	var dialog := await open_saves(false)
	require(listed(dialog) == ["Tilverton party"], "Continue plays the save's game, Curse: " + str(listed(dialog)))

func read_config() -> ConfigFile:
	var config := ConfigFile.new()
	config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
	return config

# Another game's save, loaded in play, whose game's folder the player never
# chose and the search finds: the game shows the folder found for the player to
# confirm, as startup does, before it plays from it (the user's decision,
# 2026-11-20). Use this folder saves it and loads the save; Choose another
# folder opens the folder dialog, empty, whose Cancel goes back to the load
# screen and whose folder is saved before the save loads.
func check_found_load(curse: String) -> void:
	await start("CURSE")
	await save_as("Tilverton party")
	require(DirAccess.copy_absolute(file_of("user://saves/CURSE", "Tilverton party"), file_of("user://saves", "Moved party")) == OK, "Cannot move the Curse save")
	# The player never chose Curse's folder: only the search finds it.
	var config := read_config()
	config.erase_section_key("games", "CURSE")
	require(config.save(OS.get_environment("OPENGOLD_SETTINGS_FILE")) == OK, "Cannot forget Curse's folder")
	OS.set_environment("OPENGOLD_GOLDBOX_DIR", curse)
	var expected := StartupView.game_folder("CURSE")
	require(not expected.is_empty(), "The search must find Curse's folder")
	await start("POOLRAD")
	await press_load("Moved party")
	var slots: Window = current_scene.get_node("SaveSlots")
	var found: Window = current_scene.get_node("FolderQuestions/FoundFolder")
	var path_dialog: Window = current_scene.get_node("FolderQuestions/PathDialog")
	require(not slots.visible and found.visible and not path_dialog.visible, "Loading Curse's save must show the folder found for the player to confirm")
	require(found.title == "Curse of the Azure Bonds game files" and found.get_node("Title").text == found.title, "The question names Curse: " + found.title)
	require(found.get_node("Folder").text == expected, "The question shows the folder found: " + found.get_node("Folder").text)
	require(found.get_node("Use").has_focus(), "Use this folder takes the focus")
	var saved_before := FileAccess.get_file_as_string(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
	if found_load == "decline":
		found.get_node("Choose").pressed.emit()
		await settle()
		require(not found.visible and path_dialog.visible, "Choose another folder must open the folder dialog")
		require(path_dialog.get_node("Path").text.is_empty(), "The folder dialog starts empty after declining")
		require(path_dialog.get_node("Title").text == "Curse of the Azure Bonds game files", "The folder dialog names Curse")
		require(FileAccess.get_file_as_string(OS.get_environment("OPENGOLD_SETTINGS_FILE")) == saved_before, "Declining saved a folder")
		path_dialog.get_node("Cancel").pressed.emit()
		await settle()
		require(not path_dialog.visible and slots.visible, "Cancel goes back to the load screen")
		require(listed(slots) == ["Moved party"], "Nothing loaded: Pool of Radiance's saves are still listed: " + str(listed(slots)))
		slots.get_node("Cancel").pressed.emit()
		await settle()
		await press_load("Moved party")
		require(found.visible, "Loading Curse's save again asks again")
		found.get_node("Choose").pressed.emit()
		await settle()
		path_dialog.get_node("Path").text = expected
		path_dialog.get_node("Continue").pressed.emit()
		await settle()
	else:
		found.get_node("Use").pressed.emit()
		await settle()
	require(not found.visible and not path_dialog.visible, "The questions close once Curse's folder is known")
	require(read_config().get_value("games", "CURSE", "") == expected, "Curse's confirmed folder was not saved")
	var dialog := await open_saves(false)
	require(listed(dialog) == ["Tilverton party"], "The save loaded and plays Curse: " + str(listed(dialog)))
	print("Saves by game passed: found folder ", found_load)
	quit(0)
