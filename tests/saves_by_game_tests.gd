extends SceneTree

# Saves by game (docs/GAMES.md, phase 3 step 1.7): each game keeps its saves
# in its own folder, as each original had its SAVE folder, and its save and
# load screens list only that game's. A save of another game, put among them
# by hand, starts its own game when loaded. Run by tests/settings_check.cmake;
# skipped without Curse's files in OPENGOLD_GOLDBOX_DIR.

func _initialize() -> void:
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle() -> void:
	for frame in range(8): await process_frame

func press_key() -> void:
	var event := InputEventKey.new()
	event.keycode = KEY_SPACE
	event.pressed = true
	Input.parse_input_event(event)
	await settle()

# The startup menu's button for the game, its title passed by key, and the new
# game's question answered: the original rules for Curse, the SRD's for Pool
# of Radiance.
func start(folder: String) -> void:
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	current_scene.get_node("GameMenu/Buttons/Games/" + folder).pressed.emit()
	await settle()
	for screen in range(8):
		if not current_scene.get_node("GameTitle").visible: break
		await press_key()
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

func load_save(name: String) -> void:
	var dialog := await open_saves(false)
	var slots: ItemList = dialog.get_node("Slots")
	var index := listed(dialog).find(name)
	require(index >= 0, name + " must be listed to load")
	slots.select(index)
	slots.item_selected.emit(index)
	for confirm in range(2):
		dialog.get_node("Action").pressed.emit()
		await settle()
	require(not dialog.visible, "Loading " + name + " must close the save screen: " + dialog.get_node("Status").text)

func file_of(folder: String, name: String) -> String:
	return ProjectSettings.globalize_path(folder).path_join(name.to_utf8_buffer().hex_encode() + ".ogs")

func run_checks() -> void:
	var curse := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("CURSE")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(curse.path_join("TILES.DAX")):
		print("Skipped: Curse's files are not here.")
		quit(0)
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
	print("Saves by game passed: each game's own folder and list, and a moved Curse save starting Curse.")
	quit(0)
