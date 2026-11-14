extends SceneTree

# Curse's Add Character to Party, From Where: Pool, on the real party screen:
# a Curse game under the original rules picks a Pool of Radiance save, sees its
# characters with their classes and levels, and brings one in with its levels
# and items, while the party's six PC positions last. Run by
# tests/curse_from_pool.cmake with --pool-save=<a Pool of Radiance save under
# the original rules>; --pool-game plays Pool of Radiance instead, whose party
# screen does not offer it.

const SAVE_NAME := "Pool veterans"

var pool_save := ""
var pool_game := false

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--pool-save="): pool_save = arg.trim_prefix("--pool-save=")
		if arg == "--pool-game": pool_game = true
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle() -> void:
	for frame in range(8): await process_frame

func press(node: Node, path: String) -> void:
	var button: Button = node.get_node(path)
	require(button.visible and not button.disabled, "Unavailable control: " + path)
	button.pressed.emit()
	await settle()

func pick(list: ItemList, index: int) -> void:
	list.select(index)
	list.item_selected.emit(index)
	await settle()

func rows(list: ItemList) -> Array:
	var texts := []
	for i in range(list.item_count): texts.append(list.get_item_text(i))
	return texts

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

# The new game's first question: the rules it plays by; the original's here.
func answer_original_rules() -> void:
	await settle()
	await pass_title()
	var dialog: Window = current_scene.get_node_or_null("RulesDialog") if current_scene else null
	if dialog and dialog.visible:
		var rules: OptionButton = dialog.get_node("Rows/Rules")
		rules.select(0)
		rules.item_selected.emit(0)
		dialog.get_node("Rows/Begin").pressed.emit()
		await settle()
		await settle()

# The save a player left in Pool of Radiance, among the game's own saves.
func place_pool_save() -> void:
	var saves := ProjectSettings.globalize_path("user://saves")
	DirAccess.make_dir_recursive_absolute(saves)
	var file := saves.path_join(SAVE_NAME.to_utf8_buffer().hex_encode() + ".ogs")
	require(DirAccess.copy_absolute(pool_save, file) == OK, "Cannot place the Pool of Radiance save " + pool_save)

func run_checks() -> void:
	place_pool_save()
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await pick_game("POOLRAD" if pool_game else "CURSE")
	await answer_original_rules()
	require(current_scene != null and current_scene.name == "CharacterCreation", "The game's button must lead on to the party")
	var screen := current_scene
	await press(screen, "Party")
	var from_pool: Button = screen.get_node("PartyPanel/FromPool")
	if pool_game:
		require(not from_pool.visible, "Pool of Radiance's party screen brings no one from Pool of Radiance")
		print("Curse from Pool passed: Pool of Radiance's party screen does not offer it.")
		quit(0)
		return
	await press(screen, "PartyPanel/FromPool")
	var dialog: Window = screen.get_node("FromPoolModal")
	require(dialog.visible, "Add from Pool of Radiance opens its dialog")
	var saves: ItemList = dialog.get_node("Margin/Rows/Lists/SaveColumn/Saves")
	require(rows(saves) == [SAVE_NAME], "The dialog lists the Pool of Radiance save: " + str(rows(saves)))
	await pick(saves, 0)
	var members: ItemList = dialog.get_node("Margin/Rows/Lists/MemberColumn/Members")
	var shown := rows(members)
	require(shown.size() == 7 and shown.slice(0, 2) == ["Gareth / Fighter / Level 2", "Elmira / Cleric / Level 1"],
		"The save's seven characters show with their classes and levels: " + str(shown))
	await pick(members, 0)
	await press(dialog, "Margin/Rows/Buttons/Add")
	var add: Button = dialog.get_node("Margin/Rows/Buttons/Add")
	var status: Label = dialog.get_node("Margin/Rows/Status")
	require(add.disabled and status.text == "Brought into the party.", "A character is brought in once: " + status.text)

	# The party screen shows the fighter as Curse took it in: its levels and
	# experience, and the long sword it had readied, readied still.
	var roster: ItemList = screen.get_node("PartyPanel/Roster")
	require(roster.item_count == 1, "The fighter joins the party")
	var sheet: String = screen.get_node("PartyPanel/Sheet").get_parsed_text()
	for text in ["Gareth", "Level 2", "Fighter", "XP 2001"]:
		require(sheet.contains(text), "The fighter's sheet shows " + text + ":\n" + sheet)
	var items := rows(screen.get_node("PartyPanel/Inventory"))
	require(items.size() == 1 and items[0].contains(" / ") and items[0].to_lower().contains("sword"),
		"The fighter brings its long sword, readied: " + str(items))

	# Five more fill the party's six PC positions; then the cleric cannot come.
	for index in range(2, 7):
		await pick(members, index)
		await press(dialog, "Margin/Rows/Buttons/Add")
	require(roster.item_count == 6, "The party holds six characters: " + str(roster.item_count))
	await pick(members, 1)
	require(add.disabled and status.text == "All six PC positions are occupied.",
		"No one comes when the six PC positions are taken: " + status.text)
	await press(dialog, "Margin/Rows/Buttons/Close")
	require(not dialog.visible, "Close puts the dialog away")
	print("Curse from Pool passed: the save's characters, their levels, the fighter brought in with its sword, and a full party.")
	quit(0)
