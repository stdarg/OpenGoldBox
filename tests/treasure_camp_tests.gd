extends SceneTree

# Treasures' camp on the real town screen (camp.pas, CampMainMenu): the camp
# dialog names the crystals the party holds and offers Fix and, while the
# party holds Amanitas's crown, Crown. Crown tells the luck; Fix heals the
# wounded member as the casters' slots allow. Run by tests/treasure_camp.cmake
# with --camp-save=<a Treasures save of a party in the fortress, a member
# wounded, the crown and the red and blue crystals held>.

const SAVE_NAME := "Fortress camp"

var camp_save := ""
var hp_pattern := RegEx.new()

func _initialize() -> void:
	hp_pattern.compile("HP (-?\\d+)/(\\d+)")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--camp-save="): camp_save = arg.trim_prefix("--camp-save=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle(frames := 8) -> void:
	for frame in range(frames): await process_frame

func node(path: String) -> Node:
	return current_scene.get_node_or_null(path) if current_scene else null

func town() -> Node:
	return node("CampaignTown")

func press(path: String) -> void:
	var button: Button = node(path)
	require(button != null and button.is_visible_in_tree() and not button.disabled, "Unavailable control: " + path)
	button.pressed.emit()
	await settle()

func dialogue() -> String:
	return town().get_node("%Dialogue").get_parsed_text().strip_edges()

func exploring() -> bool:
	return not town().get_node("%Forward").disabled

func party_hp() -> int:
	var total := 0
	for slot in range(8):
		var row: Button = town().get_node_or_null("%PartyList/Rows/Member" + str(slot))
		if row == null or not row.visible: continue
		var found := hp_pattern.search(row.tooltip_text)
		if found: total += int(found.get_string(1))
	return total

# A title on screen, OpenGoldBox's or the game's own pictures: a key passes each.
func pass_title() -> void:
	for screen in range(10):
		var scene := current_scene
		if scene == null or scene.name != "Startup": return
		var lettering: TextureRect = scene.get_node("ImageFit/Image/Text")
		if not scene.get_node("GameTitle").visible and not (lettering.visible and lettering.texture != null): return
		var event := InputEventKey.new()
		event.keycode = KEY_SPACE
		event.pressed = true
		Input.parse_input_event(event)
		for frame in range(6): await process_frame

# Startup's menus to Treasures' saved game: OpenGoldBox's title, Pick game,
# Treasures' button, its title and its menu's Continue.
func continue_treasures() -> void:
	await pass_title()
	var menu: Window = current_scene.get_node("MainMenu")
	require(menu.visible, "The main menu must show")
	menu.get_node("Buttons/PickGame").pressed.emit()
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/TREASURE").pressed.emit()
	await settle()
	await pass_title()
	var game_menu: Window = current_scene.get_node("GameMenu")
	require(game_menu.visible and not game_menu.get_node("Buttons/Continue").disabled, "Treasures' menu offers its save")
	game_menu.get_node("Buttons/Continue").pressed.emit()
	for wait in range(4): await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", "Continue opens the game")
	var roster: ItemList = node("PartyPanel/Roster")
	require(roster.item_count == 6, "The saved party loads")
	if not node("PartyPanel").is_visible_in_tree(): await press("Party")
	await press("PartyPanel/ActionRow2/Explore")
	for wait in range(60):
		if town() != null and exploring(): break
		await settle(4)
	require(town() != null and exploring(), "The party explores the fortress")

func place_save() -> void:
	var saves := ProjectSettings.globalize_path("user://saves/TREASURE")
	DirAccess.make_dir_recursive_absolute(saves)
	var file := saves.path_join(SAVE_NAME.to_utf8_buffer().hex_encode() + ".ogs")
	require(DirAccess.copy_absolute(camp_save, file) == OK, "Cannot place the Treasures save " + camp_save)

# Plays on after a camp: each key the session waits for, until the party
# explores again.
func play_on() -> void:
	for wait in range(80):
		if exploring(): return
		var go_on: Button = town().get_node("%Continue")
		if go_on.is_visible_in_tree() and not go_on.disabled:
			go_on.pressed.emit()
		await settle(4)

func run_checks() -> void:
	place_save()
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await continue_treasures()

	await press("CampaignTown/%Camp")
	var dialog: Window = town().get_node("RestDialog")
	require(dialog.visible, "Camp opens the camp dialog")
	for button in ["Start", "Fix", "Crown", "Finish"]:
		var shown: Button = dialog.get_node(button)
		require(shown.visible and not shown.disabled, "The camp dialog offers " + button)
	var info: String = dialog.get_node("Info").get_parsed_text()
	require(info.contains("The party has acquired the...") and info.contains("Red crystal") and info.contains("Blue crystal") and not info.contains("Green crystal"),
		"The camp names the crystals held: " + info)
	await press("CampaignTown/RestDialog/Crown")
	require(not dialog.visible, "Crown puts the dialog away")
	var reading := dialogue()
	require(reading.length() > 40 and reading == reading.to_upper(), "Crown tells Amanitas's luck reading: " + reading)

	var hurt := party_hp()
	await press("CampaignTown/%Camp")
	require(dialog.visible, "Camp opens the camp dialog again")
	await press("CampaignTown/RestDialog/Fix")
	await play_on()
	require(exploring(), "The party explores after the Fix: " + dialogue())
	require(party_hp() > hurt, "The Fix heals the wounded member: " + str(hurt) + " -> " + str(party_hp()) + "; " + dialogue())
	print("Treasures camp passed: the crystals named, Crown tells the luck, Fix heals the party.")
	quit(0)
