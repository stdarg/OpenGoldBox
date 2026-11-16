extends SceneTree

# Pools of Darkness's characters from Secret of the Silver Blades, on the real
# party screen: a Pools of Darkness game under the original rules picks a
# Secret save, sees its character, and brings it in with its items (its
# bundle left behind) and the save's platinum, gems and jewelry, as its
# LoadChar does. Run by tests/darkness_from_secret.cmake with
# --secret-save=<a Secret save under the original rules>.

const SAVE_NAME := "Secret veterans"

var secret_save := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--secret-save="): secret_save = arg.trim_prefix("--secret-save=")
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

# Startup's menus to the new game's question for the rules: OpenGoldBox's
# title, Pick game, Pools of Darkness's button, its title and its menu's
# Create new party.
func pick_darkness() -> void:
	await pass_title()
	var menu: Window = current_scene.get_node("MainMenu")
	require(menu.visible and menu.exclusive, "The main menu must show")
	menu.get_node("Buttons/PickGame").pressed.emit()
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/DARKNESS").pressed.emit()
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

# The save a player left in Secret, among that game's own saves.
func place_secret_save() -> void:
	var saves := ProjectSettings.globalize_path("user://saves/SECRET")
	DirAccess.make_dir_recursive_absolute(saves)
	var file := saves.path_join(SAVE_NAME.to_utf8_buffer().hex_encode() + ".ogs")
	require(DirAccess.copy_absolute(secret_save, file) == OK, "Cannot place the Secret save " + secret_save)

func run_checks() -> void:
	place_secret_save()
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await pick_darkness()
	await answer_original_rules()
	require(current_scene != null and current_scene.name == "CharacterCreation", "Pools of Darkness's button must lead on to the party")
	var screen := current_scene
	await press(screen, "Party")
	var from_secret: Button = screen.get_node("PartyPanel/FromPool")
	require(from_secret.text == "Add from Secret", "The party screen offers Secret's characters: " + from_secret.text)
	await press(screen, "PartyPanel/FromPool")
	var dialog: Window = screen.get_node("FromPoolModal")
	require(dialog.visible, "Add from Secret opens its dialog")
	var title: Label = dialog.get_node("Margin/Rows/Title")
	require(title.text == "Add character from Secret of the Silver Blades", "The dialog names Secret: " + title.text)
	var saves: ItemList = dialog.get_node("Margin/Rows/Lists/SaveColumn/Saves")
	require(rows(saves) == [SAVE_NAME], "The dialog lists the Secret save: " + str(rows(saves)))
	await pick(saves, 0)
	var members: ItemList = dialog.get_node("Margin/Rows/Lists/MemberColumn/Members")
	var shown := rows(members)
	require(shown.size() == 1 and shown[0].begins_with("Veteran / "),
		"The save's character shows with its class and level: " + str(shown))
	await pick(members, 0)
	var status: Label = dialog.get_node("Margin/Rows/Status")
	require(status.text.contains("share of the party's platinum"), "Its money comes along: " + status.text)
	await press(dialog, "Margin/Rows/Buttons/Add")
	var add: Button = dialog.get_node("Margin/Rows/Buttons/Add")
	require(add.disabled and status.text == "Brought into the party.", "A character is brought in once: " + status.text)

	# The party screen shows the veteran as Pools of Darkness took it in: its
	# dagger, boots and cloak, not the bundle, and the Secret save's money in
	# the party's purse.
	var roster: ItemList = screen.get_node("PartyPanel/Roster")
	require(roster.item_count == 1, "The veteran joins the party")
	var sheet: String = screen.get_node("PartyPanel/Sheet").get_parsed_text()
	require(sheet.contains("Veteran"), "The veteran's sheet shows its name:\n" + sheet)
	var items := rows(screen.get_node("PartyPanel/Inventory"))
	require(items.size() == 3, "The veteran brings its dagger, boots and cloak: " + str(items))
	# 1,234 platinum are worth 6,170 gold; Pools of Darkness's own 200 would be 1,000.
	require(sheet.contains("Party gold 6170"), "The save's 1,234 platinum comes along:\n" + sheet)
	await press(dialog, "Margin/Rows/Buttons/Close")
	require(not dialog.visible, "Close puts the dialog away")
	print("Pools of Darkness from Secret passed: the save's character, brought in with its items and its money.")
	quit(0)
