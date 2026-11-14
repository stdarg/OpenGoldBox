extends SceneTree

# The startup menu (the user's design, 2026-11-12): after the language, a modal
# dialog with Options, a button for each game and Exit to desktop. Run by
# tests/settings_check.cmake; --menu-capture=DIR saves a picture of it.
# Curse's button is checked when OPENGOLD_GOLDBOX_DIR holds its files, and
# with --secret Secret of the Silver Blades' button. A game's button first
# shows the game's own title pictures (TITLE.DAX), as its original does when
# it starts.

var captures := ""
var exit_case := false
var secret_case := false

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--menu-capture="): captures = arg.trim_prefix("--menu-capture=")
		if arg == "--menu-exit": exit_case = true
		if arg == "--secret": secret_case = true
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

# The game's title, screen by screen, each new; the new game's question waits
# for it. The first screen leaves by itself when its seconds are up, a click
# passes the second and a key each one after. The screen at index sound_screen
# starts the game's title sound.
func check_title(game: String, screens: int, first_seconds: float, sound_screen := -1) -> void:
	var seen := []
	for screen in range(screens):
		var shown := shown_title()
		require(not shown.is_empty() and not seen.has(shown), game + "'s title screen " + str(screen + 1) + " must show")
		require(not current_scene.get_node("RulesDialog").visible, "The new game's question waits for the title")
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
	require(current_scene.get_node("RulesDialog").visible, "The new game's question follows " + game + "'s title")

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
	require(labels == ["Options", "Pool of Radiance", "Curse of the Azure Bonds", "Secret of the Silver Blades", "Exit to desktop"],
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
	if secret_case:
		await check_secret(menu)
		return
	# Pool of Radiance's title: its two pictures, 5 seconds each; closing the
	# question then returns to the menu.
	menu.get_node("Buttons/Games/POOLRAD").pressed.emit()
	await settle()
	await check_title("POOLRAD", 2, 5.0)
	current_scene.get_node("RulesDialog").close_requested.emit()
	await settle()
	require(menu.visible and not current_scene.get_node("GameTitle").visible, "Closing the question returns to the menu")
	var curse := OS.get_environment("OPENGOLD_GOLDBOX_DIR").path_join("CURSE")
	if OS.get_environment("OPENGOLD_GOLDBOX_DIR").is_empty() or not FileAccess.file_exists(curse.path_join("TILES.DAX")):
		print("Startup menu passed: Options and back, Pool of Radiance's title (Curse's files are not here).")
		quit(0)
		return
	menu.get_node("Buttons/Games/CURSE").pressed.emit()
	await settle()
	# Curse's title: picture 1, its name over picture 2, and picture 4 with its
	# sound (sndIntro).
	await check_title("CURSE", 3, 5.0, 2)
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
	var location: String = current_scene.get_node("%Location").text
	require(location.begins_with("Tilverton / "), "The exploration screen names Curse's first map: " + location)
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("curse-exploration.png"))
	print("Startup menu passed: Options and back, Pool of Radiance's and Curse's titles, Curse's folder found, the party screen and Curse's opening.")
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
	menu.get_node("Buttons/Games/SECRET").pressed.emit()
	await settle()
	# Secret's title: four screens, which one key ends, as its original's does.
	require(not shown_title().is_empty(), "Secret's title must show")
	require(current_scene.get_node("TitleSound").playing, "Secret's title music plays over its pictures")
	await press_key()
	await settle()
	require(not current_scene.get_node("GameTitle").visible, "One key ends Secret's title")
	require(not current_scene.get_node("TitleSound").playing, "Secret's title music ends with its title")
	await answer_rules(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Secret's button must lead on to the party")
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
	var town: Control = current_scene.get_node("CampaignTown")
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
