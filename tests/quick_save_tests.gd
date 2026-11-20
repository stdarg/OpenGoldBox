extends SceneTree

# Quick save and quick load (the user's decision, 2026-11-20), played on the
# exploration screen as a player does: F9 with no quick save says so; F5
# writes the game's quick save, F9 asks before it loads it and puts back what
# changed since; the in-game menu's Quick save and Quick load do the same; and
# the load screen lists the quick save as "Quick save". Where the menu's Save
# is not allowed, neither is a quick save. Run by tests/settings_check.cmake
# with --game=<folder>, --rules=original or --rules=srd and --default-party;
# skipped without the game's files.

var folder := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--game="): folder = arg.trim_prefix("--game=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(folder + ": " + message)
		quit(1)

func settle() -> void:
	for frame in range(8): await process_frame

func town() -> Control:
	return current_scene.get_node_or_null("CampaignTown") if current_scene else null

func game_files_here() -> bool:
	if folder == "POOLRAD": return true
	var here := StartupView.game_folder(folder)
	return not here.is_empty() and (FileAccess.file_exists(here.path_join("ITEMS")) or FileAccess.file_exists(here.path_join("ITEMS.1")))

# Each game's saves have their own folder; Pool of Radiance's are where they
# always were.
func quick_save_file() -> String:
	var saves := "user://saves" if folder == "POOLRAD" else "user://saves/" + folder
	return ProjectSettings.globalize_path(saves).path_join("Quick save".to_utf8_buffer().hex_encode() + ".ogs")

func key(code: Key) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	Input.parse_input_event(event)
	await settle()

func press(path: String) -> void:
	var button: Button = current_scene.get_node(path)
	require(button.visible and not button.disabled, path + " must be there to press")
	button.pressed.emit()
	await settle()

func note() -> String:
	return town().get_node("%Movement").text

func location() -> String:
	return town().get_node("%Location").text

func quick_dialog() -> Window:
	return current_scene.get_node("QuickLoad")

# Opens the in-game menu with the exploring screen's Menu button.
func open_menu() -> Control:
	await press("CampaignTown/%Menu")
	var menu: Control = current_scene.get_node("GameMenu")
	require(menu.visible, "The Menu button must open the in-game menu")
	return menu

func reach_exploring() -> void:
	for wait in range(600):
		await process_frame
		if town() and town().visible: break
	require(town() != null and town().visible, "The game must begin exploring")

# The opening's text plays on until the game may be saved. While it may not,
# the menu's Quick save and Quick load follow its Save and Load, and F5 saves
# nothing.
func reach_saving() -> void:
	for attempt in range(400):
		var menu := await open_menu()
		var save: Button = menu.get_node("Menu/Save")
		require(menu.get_node("Menu/QuickSave").disabled == save.disabled, "Quick save must be allowed where Save is")
		require(menu.get_node("Menu/QuickLoad").disabled == menu.get_node("Menu/Load").disabled, "Quick load must be allowed where Load is")
		await press("GameMenu/Menu/Resume")
		if not save.disabled: return
		await key(KEY_F5)
		require(not FileAccess.file_exists(quick_save_file()), "F5 must not save where Save is not allowed")
		require(note() == "The game cannot be saved now.", "F5 must say the game cannot be saved now: " + note())
		var continue_button: Button = town().get_node("%Continue")
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
		else:
			# Pool of Radiance's opening walks behind Rolf for some seconds.
			await create_timer(0.25).timeout
	require(false, "The game never allowed saving")

# Loads the quick save through its confirmation, then explores again, where
# the load left the party.
func confirm_quick_load() -> void:
	var dialog := quick_dialog()
	require(dialog.visible, "Quick load must ask first")
	await press("QuickLoad/Load")
	require(not dialog.visible, "Confirming must close the question")
	require(not town().visible, "The quick save loads to the party screen, as a load does")
	await press("PartyPanel/ActionRow2/Explore")
	require(town().visible, "Explore must go back to exploring")

func turn_right() -> void:
	var before := location()
	await press("CampaignTown/%Right")
	require(location() != before, "Turning must change the view: " + location())

func run_checks() -> void:
	if not game_files_here():
		print("Skipped: " + folder + "'s files are not here.")
		quit(0)
		return
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await reach_exploring()
	await reach_saving()
	# Before any quick save, F9 says there is none.
	await key(KEY_F9)
	require(not quick_dialog().visible, "With no quick save, F9 must not ask to load")
	require(note() == "There is no quick save.", "F9 must say there is no quick save: " + note())
	# F5 saves at once; F9 asks, and Cancel keeps the game as it is.
	var saved_view := location()
	await key(KEY_F5)
	require(FileAccess.file_exists(quick_save_file()), "F5 must write the quick save")
	require(note() == "Quick saved.", "F5 must say it saved: " + note())
	await turn_right()
	var turned_view := location()
	await key(KEY_F9)
	require(quick_dialog().visible, "F9 must ask before it loads")
	await press("QuickLoad/Cancel")
	require(not quick_dialog().visible and town().visible and location() == turned_view, "Cancel must keep the game as it is")
	await key(KEY_F9)
	await confirm_quick_load()
	require(location() == saved_view, "F9 must put the party back where F5 saved: " + location() + " not " + saved_view)
	# The menu's Quick save replaces the quick save without asking, and its
	# Quick load loads it.
	await turn_right()
	var menu_view := location()
	await open_menu()
	await press("GameMenu/Menu/QuickSave")
	require(not current_scene.get_node("GameMenu").visible, "Quick save must close the menu")
	require(note() == "Quick saved.", "The menu's Quick save must say it saved: " + note())
	await turn_right()
	await open_menu()
	await press("GameMenu/Menu/QuickLoad")
	await confirm_quick_load()
	require(location() == menu_view, "The menu's Quick load must load the menu's quick save: " + location() + " not " + menu_view)
	# The load screen lists it by its name.
	await open_menu()
	await press("GameMenu/Menu/Load")
	var slots: Window = current_scene.get_node("SaveSlots")
	require(slots.visible, "The load screen must open")
	var listed := []
	var list: ItemList = slots.get_node("Slots")
	for i in range(list.item_count): listed.append(list.get_item_text(i))
	require(listed.has("Quick save") and listed.has("Quick save (previous version)"), "The load screen must list the quick save: " + str(listed))
	await press("SaveSlots/Cancel")
	print("Quick save passed: " + folder + ", F5 and F9, the menu's Quick save and Quick load, and the load screen's Quick save.")
	quit(0)
