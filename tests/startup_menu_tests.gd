extends SceneTree

# The startup menus (the user's design, 2026-11-14): after OpenGoldBox's
# title, the main menu's Options and Exit to desktop, and the games. Run by
# tests/settings_check.cmake; --menu-capture=DIR saves pictures of them.
# Curse's button is checked when OPENGOLD_GOLDBOX_DIR holds its files, and
# with --secret Secret of the Silver Blades' button, with --gateway Gateway
# to the Savage Frontier's, with --ua and --ua-design=FOLDER Unlimited
# Adventures' with the test design in FOLDER. A game other than Pool
# of Radiance first shows its own title pictures (TITLE.DAX), as its original
# does when it starts, then its menu.

var captures := ""
var exit_case := false
var secret_case := false
var gateway_case := false
var ua_case := false
var ua_design := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--menu-capture="): captures = arg.trim_prefix("--menu-capture=")
		if arg == "--menu-exit": exit_case = true
		if arg == "--secret": secret_case = true
		if arg == "--gateway": gateway_case = true
		if arg == "--ua": ua_case = true
		if arg.begins_with("--ua-design="): ua_design = arg.trim_prefix("--ua-design=")
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
	var menu: Window = current_scene.get_node("MainMenu")
	menu.get_texture().get_image().save_png(captures.path_join(name + "-menu.png"))
	root.get_texture().get_image().save_png(captures.path_join(name + "-screen.png"))

func capture_screen(name: String) -> void:
	if captures.is_empty(): return
	DirAccess.make_dir_recursive_absolute(captures)
	await process_frame
	RenderingServer.force_draw(false)
	await process_frame
	root.get_texture().get_image().save_png(captures.path_join(name + ".png"))

func press_key() -> void:
	var event := InputEventKey.new()
	event.keycode = KEY_SPACE
	event.pressed = true
	Input.parse_input_event(event)
	await settle()

func click() -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = true
	Input.parse_input_event(event)
	await settle()

func shown_title() -> PackedByteArray:
	var title: Control = current_scene.get_node("GameTitle")
	var picture: TextureRect = title.get_node("Picture")
	if not title.visible or picture.texture == null: return PackedByteArray()
	require(picture.texture.get_size() == Vector2(320, 200), "A title screen is the original's 320 x 200 screen")
	return picture.texture.get_image().get_data()

# The game's title, screen by screen, each new; the game's menu waits for it. The first screen leaves by itself when its seconds are up, a click
# passes the second and a key each one after. The screen at index sound_screen
# starts the game's title sound.
func check_title(game: String, screens: int, first_seconds: float, sound_screen := -1) -> void:
	var seen := []
	for screen in range(screens):
		var shown := shown_title()
		require(not shown.is_empty() and not seen.has(shown), game + "'s title screen " + str(screen + 1) + " must show")
		require(not current_scene.get_node("GameMenu").visible, "The game's menu waits for the title")
		seen.append(shown)
		require(current_scene.get_node("TitleSound").playing == (screen == sound_screen), game + "'s title sound plays with its screen " + str(sound_screen + 1) + " only")
		await capture_screen(game + "-title-" + str(screen + 1))
		if screen == 0:
			await create_timer(first_seconds - 0.5).timeout
			require(shown_title() == shown, game + "'s first screen stays its seconds")
			await create_timer(1.0).timeout
			await settle()
		elif screen == 1:
			await click()
		else:
			await press_key()
	require(not current_scene.get_node("GameTitle").visible, game + "'s title ends after " + str(screens) + " screens")
	require(current_scene.get_node("GameMenu").visible, "The game's menu follows " + game + "'s title")

# Startup to the main menu: OpenGoldBox's title, passed by a key.
func open_main_menu() -> Window:
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	require(current_scene.name == "Startup", "Startup scene missing")
	await press_key()
	return current_scene.get_node("MainMenu")

# The main menu's Pick game, then the game's button.
func pick(folder: String) -> void:
	current_scene.get_node("MainMenu/Buttons/PickGame").pressed.emit()
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/" + folder).pressed.emit()
	await settle()

# The game's menu's Create new party, which asks the rules first.
func create_new_party(original := false) -> void:
	var menu: Window = current_scene.get_node("GameMenu")
	require(menu.visible, "The game's menu must show")
	menu.get_node("Buttons/NewParty").pressed.emit()
	await answer_rules(original)

func run_checks() -> void:
	var menu: Window = await open_main_menu()
	var image: TextureRect = current_scene.get_node("ImageFit/Image")
	require(image.texture != null, "The title picture must show behind the menu")
	require(menu.visible and menu.exclusive, "The main menu must be a modal dialog")
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
	if secret_case:
		await check_secret(menu)
		return
	if gateway_case:
		await check_gateway()
		return
	if ua_case:
		await check_ua()
		return
	# Pool of Radiance's title is its new one, which a key passes; Options from
	# its menu returns to its menu, and closing the question too.
	await pick("POOLRAD")
	require(shown_title().is_empty(), "Pool of Radiance keeps its new title, not its original pictures")
	await press_key()
	var game_menu: Window = current_scene.get_node("GameMenu")
	require(game_menu.visible, "Pool of Radiance's menu follows its title")
	await capture("pool")
	game_menu.get_node("Buttons/Options").pressed.emit()
	await settle()
	require(not game_menu.visible and current_scene.get_node("Options").visible, "The game's menu steps aside for Options")
	current_scene.get_node("Options/Options/Close").pressed.emit()
	await settle()
	require(game_menu.visible, "Closing Options returns to the game's menu")
	game_menu.get_node("Buttons/NewParty").pressed.emit()
	await settle()
	current_scene.get_node("RulesDialog").close_requested.emit()
	await settle()
	require(game_menu.visible and not current_scene.get_node("GameTitle").visible, "Closing the question returns to the game's menu")
	game_menu.get_node("Buttons/MainMenu").pressed.emit()
	await settle()
	var curse := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("CURSE")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(curse.path_join("TILES.DAX")):
		print("Startup menu passed: Options and back, Pool of Radiance's title and menu (Curse's files are not here).")
		quit(0)
		return
	await pick("CURSE")
	# Curse's title: picture 1, its name over picture 2, and picture 4 with its
	# sound (sndIntro).
	await check_title("CURSE", 3, 5.0, 2)
	require(game_menu.title == "Curse of the Azure Bonds", "The menu names its game: " + game_menu.title)
	await create_new_party(true)
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
	var location: String = current_scene.get_node("%Location").text
	require(location.begins_with("Tilverton / "), "The exploration screen names Curse's first map: " + location)
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("curse-exploration.png"))
	print("Startup menu passed: Options and back, Pool of Radiance's and Curse's titles and menus, Curse's folder found, the party screen and Curse's opening.")
	quit(0)

# A fighter made on the party screen, under the original rules, taken exploring.
func explore_with_a_fighter() -> Control:
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Vala"
	name.text_changed.emit(name.text)
	await settle()
	await choose("Human")
	await press("Next")
	await choose("Fighter")
	await press("Next")
	await choose("Lawful Good")
	await press("Next")
	await press("Roll")
	for page in range(3): await press("Next")
	await press("AddParty")
	require(current_scene.get_node("PartyPanel/Roster").item_count == 1, "The fighter joins the party")
	await press("PartyPanel/ActionRow2/Explore")
	return current_scene.get_node("CampaignTown")

# Gateway to the Savage Frontier from the menu: its five title pictures, its
# title music from the second, then a party made under the original rules
# plays the opening, the night of celebration and the morning's loss, and
# explores Yartar.
func check_gateway() -> void:
	var gateway := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("GATEWAY")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(gateway.path_join("GEO2.DAX")):
		print("Skipped: Gateway's files are not here.")
		quit(0)
		return
	await pick("GATEWAY")
	require(not shown_title().is_empty(), "Gateway's title must show")
	require(not current_scene.get_node("TitleSound").playing, "Gateway's title music waits for its second picture")
	# A key on the first picture ends the whole title, as its Intro's does.
	await press_key()
	require(not current_scene.get_node("GameTitle").visible and current_scene.get_node("GameMenu").visible,
		"A key on Gateway's first picture ends its title")
	# Escape goes back to the games; Gateway again, and this time the first
	# picture leaves by itself after its 3 seconds, then a key passes one
	# picture at a time.
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	Input.parse_input_event(escape)
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/GATEWAY").pressed.emit()
	await settle()
	var first := shown_title()
	require(not first.is_empty(), "Gateway's title shows again")
	await create_timer(3.5).timeout
	await settle()
	var seen := [first]
	for screen in range(1, 5):
		var shown := shown_title()
		require(not shown.is_empty() and not seen.has(shown), "Gateway's title screen " + str(screen + 1) + " must show")
		seen.append(shown)
		require(current_scene.get_node("TitleSound").playing, "Gateway's title music plays from its second picture")
		await capture_screen("GATEWAY-title-" + str(screen + 1))
		await press_key()
	require(not current_scene.get_node("GameTitle").visible, "Gateway's title ends after five screens")

	await create_new_party(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Gateway's button must lead on to the party")
	var town: Control = await explore_with_a_fighter()
	var celebration := false
	var morning := false
	for wait in range(600):
		await process_frame
		var dialogue: String = town.find_child("Dialogue", true, false).get_parsed_text()
		if dialogue.contains("NIGHT OF CELEBRATION"): celebration = true
		if dialogue.contains("AWAKEN TO DESPAIR"): morning = true
		var continue_button: Button = town.get_node("%Continue")
		if morning and not continue_button.visible: break
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
	require(celebration and morning, "Gateway's opening must play on the exploration screen")
	require(not town.get_node("%Continue").visible, "The opening ends exploring Yartar")
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("gateway-exploration.png"))
	print("Startup menu passed: Gateway's title, a party made under the original rules, its opening and Yartar.")
	quit(0)

# Secret of the Silver Blades from the menu: its folder found, a fighter made
# under the original rules and taken exploring, and the opening vision played
# on to New Verdigris, whose name the exploration screen shows.
func check_secret(menu: Window) -> void:
	var secret := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("SECRET")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(secret.path_join("GEO1.DAX")):
		print("Skipped: Secret's files are not here.")
		quit(0)
		return
	await pick("SECRET")
	# Secret's title: four screens, which one key ends, as its original's does.
	require(not shown_title().is_empty(), "Secret's title must show")
	require(current_scene.get_node("TitleSound").playing, "Secret's title music plays over its pictures")
	await press_key()
	await settle()
	require(not current_scene.get_node("GameTitle").visible, "One key ends Secret's title")
	require(not current_scene.get_node("TitleSound").playing, "Secret's title music ends with its title")
	await create_new_party(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Secret's button must lead on to the party")
	var town: Control = await explore_with_a_fighter()
	var vision := ""
	var location := ""
	for wait in range(600):
		await process_frame
		var dialogue: String = town.find_child("Dialogue", true, false).get_parsed_text()
		if dialogue.contains("GLACIER"): vision = dialogue
		location = town.get_node("%Location").text
		if location.begins_with("New Verdigris / "): break
		var continue_button: Button = town.get_node("%Continue")
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
	require(not vision.is_empty(), "Secret's opening vision must play on the exploration screen")
	require(location.begins_with("New Verdigris / "), "The vision ends in New Verdigris, named on the screen: " + location)
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("secret-exploration.png"))
	print("Startup menu passed: Secret's folder found, a party made under the original rules, its vision and New Verdigris.")
	quit(0)

# Unlimited Adventures from the menu: the kit's folder found, its design asked
# (a folder that is not a design refused), no title, no default party, the
# original rules only, then a fighter made under the kit's rules plays the
# design's start: its welcome, the 3D view drawn, and a step.
func check_ua() -> void:
	var kit := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("UA")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(kit.path_join("CKIT.EXE")) \
			or not DirAccess.dir_exists_absolute(ua_design):
		print("Skipped: the kit's files or the test design are not here.")
		quit(0)
		return
	await pick("UA")
	var dialog: Window = current_scene.get_node("PathDialog")
	require(dialog.visible and dialog.get_node("Title").text == "Unlimited Adventures design",
		"Unlimited Adventures asks for its design once the kit's folder is found")
	var path: LineEdit = dialog.get_node("Path")
	path.text = kit
	dialog.get_node("Continue").pressed.emit()
	await settle()
	require(dialog.visible and not dialog.get_node("Status").text.is_empty(), "The kit's own folder is not a design")
	path.text = ua_design
	dialog.get_node("Continue").pressed.emit()
	await settle()
	require(not dialog.visible, "The test design is taken")
	var game_menu: Window = current_scene.get_node("GameMenu")
	require(game_menu.visible and game_menu.title == "Unlimited Adventures", "Unlimited Adventures' menu follows, with no title")
	require(game_menu.get_node("Buttons/DefaultParty").disabled, "A design has no default party yet")
	require(game_menu.get_node("Buttons/Load").disabled, "A design has no saves yet")
	game_menu.get_node("Buttons/NewParty").pressed.emit()
	await settle()
	var rules: OptionButton = current_scene.get_node("RulesDialog/Rows/Rules")
	require(rules.selected == 0 and rules.is_item_disabled(1), "A design plays by the original rules only")
	await answer_rules(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Unlimited Adventures' button must lead on to the party")
	var town: Control = await explore_with_a_fighter()
	var welcome := ""
	for wait in range(120):
		await process_frame
		var dialogue: String = town.find_child("Dialogue", true, false).get_parsed_text()
		if dialogue.contains("WELCOME TO THE TEST HALL"):
			welcome = dialogue
			break
	require(welcome.contains("WELCOME TO THE TEST HALL, Vala."), "The design's welcome names the selected member: " + welcome)
	var location: String = town.get_node("%Location").text
	require(location.begins_with("THE TEST HALL / "), "The screen names the design's area: " + location)
	var visuals: Dictionary = town.tour_visuals()
	require(visuals.has("wall_image") and (visuals["wall_image"] as Image).get_size() == Vector2i(88, 88),
		"The design's 3D view is drawn")
	for answer in range(10):
		var continue_button: Button = town.get_node("%Continue")
		if not continue_button.visible: break
		continue_button.pressed.emit()
		await settle()
	require(not town.get_node("%Continue").visible, "The welcome's words end")
	require(town.get_node("%Coordinates").text.begins_with("Party (2, 2)"), "The party stands on the design's start")
	var forward: Button = town.get_node("%Forward")
	require(not forward.disabled, "The party may step once the words end")
	forward.pressed.emit()
	await settle()
	require(town.get_node("%Coordinates").text.begins_with("Party (3, 2)"), "A step takes the party east: " + town.get_node("%Coordinates").text)
	# The hall looks the same a square on; turning north faces its wall.
	var east_view: PackedByteArray = (town.tour_visuals()["wall_image"] as Image).get_data()
	town.get_node("%Left").pressed.emit()
	await settle()
	require((town.tour_visuals()["wall_image"] as Image).get_data() != east_view, "Turning shows another view")
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("ua-exploration.png"))
	# East again, past the chest (left shut), to the kobolds at (8, 2): their
	# fight opens on the combat screen with the design's monsters.
	town.get_node("%Right").pressed.emit()
	await settle()
	for step in range(5):
		await answer_town(town, "No")
		forward.pressed.emit()
		await settle()
	await answer_town(town, "No")
	var fight: Node = null
	for wait in range(120):
		await process_frame
		fight = current_scene.get_node_or_null("CampaignCombat")
		if fight: break
	var said: String = town.find_child("Dialogue", true, false).get_parsed_text()
	require(fight != null, "The kobolds' fight opens on the combat screen: " + said)
	print("Startup menu passed: Unlimited Adventures' kit and design, a party made under the kit's rules, the design's welcome, its 3D view, a step and its kobolds' fight.")
	quit(0)

# Goes on from what the exploration screen waits for: Return to words, and
# `choice` where it offers choices.
func answer_town(town: Control, choice: String) -> void:
	for answer in range(10):
		var continue_button: Button = town.get_node("%Continue")
		if not continue_button.visible or continue_button.disabled: return
		var choices: ItemList = town.get_node("%Choices")
		if choices.visible:
			for i in range(choices.item_count):
				if choices.get_item_text(i) == choice:
					choices.select(i)
		continue_button.pressed.emit()
		await settle()
