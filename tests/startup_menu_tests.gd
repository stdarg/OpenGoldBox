extends SceneTree

# The startup menus (the user's design, 2026-11-14): after OpenGoldBox's
# title, the main menu's Options and Exit to desktop, and the games. Run by
# tests/settings_check.cmake; --menu-capture=DIR saves pictures of them.
# Curse's button is checked when the game finds Curse's files, and
# with --secret Secret of the Silver Blades' button, with --gateway Gateway
# to the Savage Frontier's, with --darkness Pools of Darkness', with
# --treasure Treasures of the Savage Frontier's, with --ua and --ua-design=FOLDER Unlimited
# Adventures' with the test design in FOLDER, with --buck-rogers the Buck
# Rogers games' real button activation, options and Original creation. A game other than Pool
# of Radiance first shows its own title pictures (TITLE.DAX), as its original
# does when it starts, then its menu.

var captures := ""
var exit_case := false
var secret_case := false
var gateway_case := false
var darkness_case := false
var treasure_case := false
var ua_case := false
var buck_rogers_case := false
var dark_queen_case := false
var ua_design := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--menu-capture="): captures = arg.trim_prefix("--menu-capture=")
		if arg == "--menu-exit": exit_case = true
		if arg == "--secret": secret_case = true
		if arg == "--gateway": gateway_case = true
		if arg == "--darkness": darkness_case = true
		if arg == "--treasure": treasure_case = true
		if arg == "--ua": ua_case = true
		if arg == "--buck-rogers": buck_rogers_case = true
		if arg == "--dark-queen": dark_queen_case = true
		if arg.begins_with("--ua-design="): ua_design = arg.trim_prefix("--ua-design=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

# Games without an area-name table still name their own campaign everywhere.
func check_game_title_fallback(town: Control, title: String) -> void:
	require((town.get_node("%Location") as Label).text.begins_with(title + " / "),
		"The viewpoint names the selected game: " + title)
	require((town.get_node("%MapTitle") as Label).text == title + "    N ↑",
		"The map title names the selected game: " + title)
	require((town.get_node("%Title") as Label).text == "OPENGOLDBOX  /  " + title,
		"The exploration header names the selected game: " + title)
	require((town.get_node("%Speaker") as Label).text == title,
		"The dialogue title names the selected game: " + title)

func settle() -> void:
	for frame in range(8): await process_frame

# Options keeps the wrapped row's natural height, collapses hidden settings,
# and leaves its Close button reachable while only the page scrolls.
func check_options_layout(options: Control) -> void:
	var original_size := root.size
	for dimensions in [Vector2i(1120, 800), Vector2i(1600, 1000)]:
		root.size = dimensions
		await settle()
		var scroll: ScrollContainer = options.get_node("Options/Gameplay/Scroll")
		var page: VBoxContainer = scroll.get_node("Page")
		var choices: GridContainer = page.get_node("Choices")
		require(choices.columns == 3 and (page.get_node("PartyLimits") as GridContainer).columns == 2,
			"Options use containers for setting rows and all five party spell limits")
		require(page.size.x <= scroll.size.x + 1.0,
			"Options fit their scrolling width at " + str(dimensions))
		var preceding_bottom := choices.get_global_rect().position.y
		for row in range(0, choices.get_child_count(), 3):
			var label: Label = choices.get_child(row)
			var choice: Control = choices.get_child(row + 1)
			var note: Label = choices.get_child(row + 2)
			require(label.visible == choice.visible and note.visible == choice.visible,
				"Hidden setting rows collapse together: " + str(choice.name))
			if not choice.visible: continue
			var row_bottom := preceding_bottom
			var preceding_right := choices.get_global_rect().position.x
			for cell: Control in [label, choice, note]:
				var rect := cell.get_global_rect()
				require(rect.position.y >= preceding_bottom - 1.0 and rect.position.x >= preceding_right - 1.0
					and rect.end.x <= scroll.get_global_rect().end.x + 1.0,
					"Setting cells do not overlap or escape their row: " + str(choice.name))
				preceding_right = rect.end.x
				row_bottom = maxf(row_bottom, rect.end.y)
				if cell is Label and not (cell as Label).text.is_empty():
					require((cell as Label).get_visible_line_count() >= (cell as Label).get_line_count(),
						"Wrapped text has room for every line: " + str(cell.name))
			preceding_bottom = row_bottom
		var previous_section_bottom := page.get_global_rect().position.y
		for section: Control in page.get_children():
			if not section.visible: continue
			require(section.get_global_rect().position.y >= previous_section_bottom - 1.0,
				"Visible Options sections do not overlap: " + str(section.name))
			previous_section_bottom = section.get_global_rect().end.y
		var close: Button = options.get_node("Options/Close")
		var panel: Control = options.get_node("Options")
		require(close.is_visible_in_tree() and close.get_global_rect().position.y >= scroll.get_global_rect().end.y
			and close.get_global_rect().end.y <= panel.get_global_rect().end.y,
			"Close stays outside the scrolling page and inside the Options panel")
		scroll.scroll_vertical = 100000
		await settle()
		require(scroll.get_v_scroll_bar().value + scroll.get_v_scroll_bar().page >= page.size.y - 1.0,
			"The lowest visible settings can be reached by scrolling")
		scroll.scroll_vertical = 0
	root.size = original_size
	await settle()

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

# The enabled-game startup checks use ordinary input through the button's own window.
# Emitting pressed directly would also activate a disabled game entry.
func activate(button: Button, by_keyboard: bool) -> void:
	require(button.is_visible_in_tree(), "A real startup button is visible: " + String(button.name))
	require(not button.disabled, "A real startup button is enabled: " + String(button.name))
	var viewport := button.get_viewport()
	if by_keyboard:
		button.grab_focus()
		for down in [true, false]:
			var event := InputEventKey.new()
			event.keycode = KEY_ENTER
			event.pressed = down
			viewport.push_input(event, true)
	else:
		var point := Vector2(button.get_window().position) + button.get_global_rect().get_center()
		var motion := InputEventMouseMotion.new()
		motion.position = point
		motion.global_position = point
		root.push_input(motion, true)
		for down in [true, false]:
			var event := InputEventMouseButton.new()
			event.button_index = MOUSE_BUTTON_LEFT
			event.pressed = down
			event.position = point
			event.global_position = point
			root.push_input(event, true)
	await settle()

func pick_using_input(folder: String, by_keyboard: bool) -> void:
	await activate(current_scene.get_node("MainMenu/Buttons/PickGame"), by_keyboard)
	var picker: Window = current_scene.get_node("GamePicker")
	require(picker.visible, "Real input opens the game picker")
	await activate(picker.get_node("Buttons/Games/" + folder), by_keyboard)

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
	# Pool of Radiance, the game until another is chosen, shows its own and
	# the other games' options, not the Buck Rogers games' ship combat.
	var gameplay: Control = options.get_node("Options/Gameplay/Scroll/Page")
	require(gameplay.get_node("Choices/OriginalShops").visible and gameplay.get_node("Choices/DarknessGazes").visible
		and not gameplay.get_node("Choices/CommandEveryRound").visible and not gameplay.get_node("Choices/EmptyHelmDodges").visible
		and not gameplay.get_node("Choices/BoardingFuel").visible,
		"Another game's Options leave out the ship-combat options")
	require(not menu.visible, "The menu steps aside for Options")
	await check_options_layout(options)
	options.get_node("Options/Close").pressed.emit()
	await settle()
	require(menu.visible and current_scene.get_node_or_null("Options") == null, "Closing Options returns to the menu")
	if secret_case:
		await check_secret(menu)
		return
	if gateway_case:
		await check_gateway()
		return
	if darkness_case:
		await check_darkness()
		return
	if treasure_case:
		await check_treasure()
		return
	if ua_case:
		await check_ua()
		return
	if buck_rogers_case:
		await check_buck_rogers()
		return
	if dark_queen_case:
		await check_dark_queen()
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
	var curse := StartupView.game_folder("CURSE")
	if curse.is_empty() or not FileAccess.file_exists(curse.path_join("TILES.DAX")):
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
	var gateway := StartupView.game_folder("GATEWAY")
	if gateway.is_empty() or not FileAccess.file_exists(gateway.path_join("GEO2.DAX")):
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

# Escape from the game's menu back to the games, then the game's button again.
func pick_again(folder: String) -> void:
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	Input.parse_input_event(escape)
	await settle()
	current_scene.get_node("GamePicker/Buttons/Games/" + folder).pressed.emit()
	await settle()

# Pools of Darkness from the menu: its opening film (INTRO) first, its title
# card deaf to keys, its song playing, then a key ends the film; title
# pictures 1 and 2 stay 5 seconds and a key on either goes on to 3, which a
# key ends; the title music plays from picture 1. Then a party made under the
# original rules meets Rolf on New Phlan's docks, declines his tour and is
# walked out.
func check_darkness() -> void:
	var darkness := StartupView.game_folder("DARKNESS")
	if darkness.is_empty() or not FileAccess.file_exists(darkness.path_join("INTRO.BMA")):
		print("Skipped: Pools of Darkness' files are not here.")
		quit(0)
		return
	await pick("DARKNESS")
	var card := shown_title()
	require(not card.is_empty(), "The opening film's title card must show")
	require(current_scene.get_node("TitleSound").playing, "The film plays its song")
	await capture_screen("DARKNESS-film-1")
	await press_key()
	require(shown_title() == card, "A key does not pass the film's title card")
	# The title card holds about 3.6 seconds; on the next frame a key ends the film.
	await create_timer(4.0).timeout
	await settle()
	var frame := shown_title()
	require(not frame.is_empty() and frame != card, "The film goes on after its title card")
	await capture_screen("DARKNESS-film-2")
	await press_key()
	var first := shown_title()
	require(not first.is_empty() and first != frame, "A key ends the film on title picture 1")
	require(current_scene.get_node("TitleSound").playing, "The title music plays from picture 1")
	await capture_screen("DARKNESS-title-1")
	await create_timer(5.5).timeout
	await settle()
	var second := shown_title()
	require(not second.is_empty() and second != first, "Picture 1 gives way to 2 after 5 seconds")
	await capture_screen("DARKNESS-title-2")
	await press_key()
	var third := shown_title()
	require(not third.is_empty() and third != second, "A key on picture 2 goes on to 3")
	await capture_screen("DARKNESS-title-3")
	await press_key()
	require(not current_scene.get_node("GameTitle").visible and current_scene.get_node("GameMenu").visible,
		"A key on picture 3 ends the title")
	# Again: a key on picture 1 goes straight on to picture 3.
	await pick_again("DARKNESS")
	await create_timer(4.0).timeout
	await settle()
	await press_key()
	require(shown_title() == first, "The film ends on picture 1 again")
	await press_key()
	require(shown_title() == third, "A key on picture 1 goes on to picture 3")
	await press_key()
	require(not current_scene.get_node("GameTitle").visible, "Pools of Darkness' title ends after picture 3")

	await create_new_party(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Pools of Darkness' button must lead on to the party")
	var town: Control = await explore_with_a_fighter()
	var rolf := false
	var escorted := false
	for wait in range(600):
		await process_frame
		var dialogue: String = town.find_child("Dialogue", true, false).get_parsed_text()
		var list: ItemList = town.get_node("%Choices")
		var continue_button: Button = town.get_node("%Continue")
		if dialogue.contains("I AM ROLF, PORTMASTER") and list.is_visible_in_tree() and list.item_count == 2:
			rolf = true
			list.select(1)
			list.item_selected.emit(1)
			await settle()
			continue_button.pressed.emit()
			await settle()
			continue
		if dialogue.contains("THEN LET ME ESCORT YOU OUT"): escorted = true
		if escorted and not continue_button.visible: break
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
	require(rolf and escorted, "Rolf's offer and his escort must play on the exploration screen")
	require(not town.get_node("%Continue").visible, "The opening ends exploring New Phlan")
	check_game_title_fallback(town, "Pools of Darkness")
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("darkness-exploration.png"))
	print("Startup menu passed: Pools of Darkness' film and title, a party made under the original rules, Rolf and New Phlan.")
	quit(0)

# Treasures of the Savage Frontier from the menu: its five title pictures, a
# key passing one, its title music from the first; then a party made under
# the original rules plays the opening, the valley and the dwarves' fortress.
func check_treasure() -> void:
	var treasure := StartupView.game_folder("TREASURE")
	if treasure.is_empty() or not FileAccess.file_exists(treasure.path_join("TITLE.DAX")):
		print("Skipped: Treasures' files are not here.")
		quit(0)
		return
	await pick("TREASURE")
	var seen := []
	for screen in range(5):
		var shown := shown_title()
		require(not shown.is_empty() and not seen.has(shown), "Treasures' title screen " + str(screen + 1) + " must show")
		seen.append(shown)
		require(current_scene.get_node("TitleSound").playing, "Treasures' title music plays from its first picture")
		await capture_screen("TREASURE-title-" + str(screen + 1))
		await press_key()
	require(not current_scene.get_node("GameTitle").visible and current_scene.get_node("GameMenu").visible,
		"Treasures' title ends after five screens")

	await create_new_party(true)
	require(current_scene != null and current_scene.name == "CharacterCreation", "Treasures' button must lead on to the party")
	var town: Control = await explore_with_a_fighter()
	var valley := false
	for wait in range(600):
		await process_frame
		var dialogue: String = town.find_child("Dialogue", true, false).get_parsed_text()
		if dialogue.contains("LATE SUMMER IN THE VALLEY OF DESSARIN"): valley = true
		var continue_button: Button = town.get_node("%Continue")
		if valley and not continue_button.visible: break
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
	require(valley, "Treasures' opening must play on the exploration screen")
	require(not town.get_node("%Continue").visible, "The opening ends exploring the dwarves' fortress")
	check_game_title_fallback(town, "Treasures of the Savage Frontier")
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("treasure-exploration.png"))
	print("Startup menu passed: Treasures' title, a party made under the original rules and its opening.")
	quit(0)

# Secret of the Silver Blades from the menu: its folder found, a fighter made
# under the original rules and taken exploring, and the opening vision played
# on to New Verdigris, whose name the exploration screen shows.
func check_secret(menu: Window) -> void:
	var secret := StartupView.game_folder("SECRET")
	if secret.is_empty() or not FileAccess.file_exists(secret.path_join("GEO1.DAX")):
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

# Dark Queen's enabled entry reaches its supported Original creation and the
# existing Death Knights import dialog through both mouse and keyboard input.
func check_dark_queen() -> void:
	if StartupView.game_folder("DQK").is_empty():
		print("Skipped: Dark Queen's files are not here.")
		quit(0)
		return
	root.size = Vector2i(1120, 800)
	await settle()
	for by_keyboard in [false, true]:
		await pick_using_input("DQK", by_keyboard)
		var game_menu: Window = current_scene.get_node("GameMenu")
		for wait in range(40):
			if game_menu.visible: break
			await press_key()
		require(game_menu.visible, "Dark Queen's enabled entry reaches its game menu")
		await activate(game_menu.get_node("Buttons/NewParty"), by_keyboard)
		var rules_dialog: Window = current_scene.get_node("RulesDialog")
		require(rules_dialog.visible, "Dark Queen's New Party reaches the rules question")
		var rules: OptionButton = rules_dialog.get_node("Rows/Rules")
		require(rules.selected == 0 and not rules.is_item_disabled(0) and rules.is_item_disabled(1),
			"Dark Queen defaults to its supported Original rules and cannot choose SRD")
		await activate(rules_dialog.get_node("Rows/Begin"), by_keyboard)
		require(current_scene.name == "CharacterCreation", "Dark Queen opens character creation")
		var races: ItemList = current_scene.get_node("Choices")
		var kender := false
		for choice in range(races.item_count):
			kender = kender or races.get_item_text(choice) == "Kender"
		require(kender, "Dark Queen opens its Original Krynn creation choices")
		await activate(current_scene.get_node("Party"), by_keyboard)
		var from_earlier: Button = current_scene.get_node("PartyPanel/FromPool")
		require(from_earlier.text == "Add from Death Knights", "Dark Queen offers its earlier game's characters")
		var dialog: Window = current_scene.get_node("FromPoolModal")
		var intended_size := dialog.size
		await activate(from_earlier, by_keyboard)
		require(dialog.visible and (dialog.get_node("Margin/Rows/Title") as Label).text == "Add character from Death Knights of Krynn",
			"Dark Queen reaches the existing Death Knights import dialog")
		var close: Button = dialog.get_node("Margin/Rows/Buttons/Close")
		require(intended_size == Vector2i(900, 640) and dialog.size == intended_size,
			"The earlier-game dialog retains its existing scene size: %s -> %s" % [intended_size, dialog.size])
		require(Rect2(Vector2.ZERO, Vector2(dialog.size)).encloses(close.get_global_rect()),
			"The earlier-game dialog's Close button lies inside its input viewport")
		await activate(close, by_keyboard)
		require(not dialog.visible, "Real input closes the earlier-game dialog")
		await open_main_menu()
		await settle()
	print("Startup menu passed: Dark Queen's real mouse and keyboard buttons, Original creation and Death Knights import entry.")
	quit(0)

# Unlimited Adventures from the menu: the kit's folder found, its design asked
# (a folder that is not a design refused), no title, its default party offered, the
# original rules only, then a fighter made under the kit's rules plays the
# design's start: its welcome, the 3D view drawn, and a step.
# The ship-combat and solar-map options and Original movement (docs/CONFIGURATION.md) show while a Buck Rogers
# game is the game, each at its default, and each choice is saved under that
# game's folder. The games' buttons wait until they play, so the check
# presses them as the startup menu will.
func check_buck_rogers() -> void:
	root.size = Vector2i(1120, 800)
	await settle()
	var options := {"CommandEveryRound": ["command_every_round", 1],
		"EmptyHelmDodges": ["empty_helm_dodges", 1], "BoardingFuel": ["boarding_fuel", 0],
		"DirectFlight": ["direct_flight", 0], "CourseFlown": ["course_flown", 1],
		"OriginalMovement": ["original_movement", 0], "ShipPictureShrinks": ["ship_picture_shrinks", 0]}
	for folder in ["buck-rogers-countdown-to-doomsday", "buck-rogers-matrix-cubed"]:
		if StartupView.game_folder(folder).is_empty():
			print("Skipped: the Buck Rogers games' files are not here.")
			quit(0)
			return
		var by_keyboard: bool = folder == "buck-rogers-matrix-cubed"
		await pick_using_input(folder, by_keyboard)
		var game_menu: Window = current_scene.get_node("GameMenu")
		for wait in range(40):
			if game_menu.visible: break
			await press_key()
		require(game_menu.visible, folder + "'s menu follows its title")
		game_menu.get_node("Buttons/Options").pressed.emit()
		await settle()
		var gameplay: Control = current_scene.get_node("Options/Options/Gameplay/Scroll/Page")
		require(not gameplay.get_node("Choices/OriginalShops").visible and not gameplay.get_node("Choices/DarknessGazes").visible
			and not gameplay.get_node("Choices/DesignDifficulty").visible,
			"The other games' options step aside for the ship combat's and the solar map's")
		require(not gameplay.get_node("Choices/Challenge").is_visible_in_tree()
			and not gameplay.get_node("Abilities").is_visible_in_tree()
			and not gameplay.get_node("PartyLimits").is_visible_in_tree()
			and not (gameplay.get_node("Choices/DifficultyNote") as Label).text.contains("From Hard"),
			"Buck Rogers pregame Options hide SRD balance and party spell rows and their SRD note")
		await check_options_layout(current_scene.get_node("Options"))
		var difficulty: OptionButton = gameplay.get_node("Choices/Difficulty")
		var levels := ["Novice", "Cadet", "Trooper", "Veteran", "Ace", "Admiral"]
		require(difficulty.item_count == levels.size() and difficulty.selected == 2,
			"Buck Rogers offers six source difficulty levels with Trooper as its default")
		for level in range(levels.size()):
			require(difficulty.get_item_text(level) == levels[level], "The source difficulty is " + levels[level])
		difficulty.select(0)
		difficulty.item_selected.emit(0)
		var difficulty_config := ConfigFile.new()
		difficulty_config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
		require(difficulty_config.get_value("game", "difficulty", "") == "novice",
			"The actual Novice setting is saved without mapping it to Cadet")
		await press("Options/Options/Close")
		game_menu.get_node("Buttons/Options").pressed.emit()
		await settle()
		gameplay = current_scene.get_node("Options/Options/Gameplay/Scroll/Page")
		difficulty = gameplay.get_node("Choices/Difficulty")
		require(difficulty.selected == 0, "Reopened Options retain the actual Novice setting")
		difficulty.select(2)
		difficulty.item_selected.emit(2)
		difficulty_config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
		require(difficulty_config.get_value("game", "difficulty", "") == "normal",
			"Trooper restores the original default difficulty")
		for name in options:
			var choice: OptionButton = gameplay.get_node("Choices/" + name)
			require(choice.visible and gameplay.get_node("Choices/" + name + "Label").visible and choice.item_count == 2
				and choice.get_item_text(0) == "On" and choice.selected == options[name][1],
				name + " shows its default for " + folder)
			var other: int = 1 - options[name][1]
			choice.select(other)
			choice.item_selected.emit(other)
		var config := ConfigFile.new()
		config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
		for name in options:
			var saved: String = config.get_value(options[name][0], folder, "")
			require(saved == ("on" if options[name][1] == 1 else "off"),
				name + " is saved for " + folder + ": " + saved)
		await press("Options/Options/Close")
		game_menu.get_node("Buttons/Options").pressed.emit()
		await settle()
		gameplay = current_scene.get_node("Options/Options/Gameplay/Scroll/Page")
		for name in options:
			require((gameplay.get_node("Choices/" + name) as OptionButton).selected == 1 - options[name][1],
				name + " shows the saved choice again for " + folder)
		await press("Options/Options/Close")
		await activate(game_menu.get_node("Buttons/NewParty"), by_keyboard)
		var rules_dialog: Window = current_scene.get_node("RulesDialog")
		require(rules_dialog.visible, "New Party reaches the rules question through real input")
		var rules: OptionButton = rules_dialog.get_node("Rows/Rules")
		require(rules.selected == 0 and not rules.is_item_disabled(0) and rules.is_item_disabled(1),
			"A Buck game starts with its supported Original rules and cannot choose SRD")
		await activate(rules_dialog.get_node("Rows/Begin"), by_keyboard)
		require(current_scene.name == "CharacterCreation", folder + " reaches creation from the enabled game button")
		require((current_scene.get_node("Name") as LineEdit).max_length == 15,
			"The selected game opens native Buck character creation")
		var races: ItemList = current_scene.get_node("Choices")
		var terran := false
		for choice in range(races.item_count):
			terran = terran or races.get_item_text(choice) == "Terran"
		require(terran, "The creation screen offers the native Terran race")
		await open_main_menu()
		await settle()
	print("Startup menu passed: Buck Rogers' real mouse and keyboard buttons, Original creation, and each game's saved options.")
	quit(0)

# The in-game Options' Party area spells under the original rules
# (docs/CONFIGURATION.md, [party_spell_ai]): the five limits at their
# defaults, each saved as it changes, shown again as Options reopen, and put
# back. Options are open as it starts and as it ends.
func check_party_spell_limits(gameplay: Control) -> void:
	var limits := {"DeathLimit": ["death_limit", 5.0, 10.0], "HurtShare": ["hurt_share", 50.0, 40.0],
		"HurtChance": ["hurt_chance", 50.0, 60.0], "FightEndChance": ["fight_end_chance", 75.0, 90.0],
		"PartyWeight": ["party_weight", 2.0, 3.5]}
	require(gameplay.get_node("PartySpellsHeader").is_visible_in_tree(), "Options show the party's area spells")
	await check_options_layout(current_scene.get_node("GameMenu"))
	for row in limits:
		var spin: SpinBox = gameplay.get_node("PartyLimits/" + row)
		require(spin.is_visible_in_tree() and gameplay.get_node("PartyLimits/" + row + "Label").visible
			and not (spin.tooltip_text as String).is_empty() and spin.value == limits[row][1],
			row + " shows its default and explains itself")
		spin.value = limits[row][2]
	var config := ConfigFile.new()
	config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
	for row in limits:
		require(float(config.get_value("party_spell_ai", limits[row][0], -1.0)) == limits[row][2],
			row + " is saved: " + str(config.get_value("party_spell_ai", limits[row][0], "")))
	await press("GameMenu/Options/Close")
	await press("GameMenu/Menu/Options")
	var page: Control = current_scene.get_node("GameMenu/Options/Gameplay/Scroll/Page")
	for row in limits:
		var spin: SpinBox = page.get_node("PartyLimits/" + row)
		require(spin.value == limits[row][2], row + " shows the saved limit again")
		spin.value = limits[row][1]
	config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
	for row in limits:
		require(float(config.get_value("party_spell_ai", limits[row][0], -1.0)) == limits[row][1],
			row + " is back at its default")

func check_ua() -> void:
	var kit := StartupView.game_folder("UA")
	if kit.is_empty() or not FileAccess.file_exists(kit.path_join("CKIT.EXE")) \
			or not DirAccess.dir_exists_absolute(ua_design):
		print("Skipped: the kit's files or the test design are not here.")
		quit(0)
		return
	await pick("UA")
	var dialog: Window = current_scene.get_node("FolderQuestions/PathDialog")
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
	require(not game_menu.get_node("Buttons/DefaultParty").disabled, "A design offers its default party")
	require(game_menu.get_node("Buttons/Load").disabled, "Load game waits for a save of the design")
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
	require(town.tour_visuals().has("cells") and (town.tour_visuals()["cells"] as Array)[2 * 16 + 2] != -1,
		"The hall's overhead map shows where the party stands")
	# Camp: the kit's camp menu rests the party in the hall, where nothing
	# waits, for the day its rest menu offers.
	await press("CampaignTown/%Camp")
	require(not town.get_node("RestDialog").visible, "A design camps with the kit's own menu")
	var camp_choices: ItemList = town.get_node("%Choices")
	require(camp_choices.visible and camp_choices.item_count == 4 and camp_choices.get_item_text(0) == "Rest",
		"Camp opens the kit's camp menu")
	await answer_town(town, "Rest")
	var rested: String = town.find_child("Dialogue", true, false).get_parsed_text()
	require(rested.contains("The party rests."), "The party rests in the hall: " + rested)
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
	# Saved here, back on the startup menu, and continued: the design goes on
	# where it was saved (docs/games/ua.md, Saving and loading a game).
	var north_view: PackedByteArray = (town.tour_visuals()["wall_image"] as Image).get_data()
	await press("CampaignTown/%Menu")
	# The in-game Options: a design plays the Difficulty as Design difficulty
	# says, Both by default, and a choice is kept for Unlimited Adventures.
	await press("GameMenu/Menu/Options")
	var gameplay: Control = current_scene.get_node("GameMenu/Options/Gameplay/Scroll/Page")
	var design_difficulty: OptionButton = gameplay.get_node("Choices/DesignDifficulty")
	require(design_difficulty.is_visible_in_tree() and design_difficulty.item_count == 3
		and design_difficulty.get_item_text(0) == "The design's" and design_difficulty.selected == 2,
		"Options offer Design difficulty, Both chosen")
	require((gameplay.get_node("Choices/DifficultyNote") as Label).text.contains("Design difficulty"),
		"The Difficulty's note sends a design's player to Design difficulty")
	await capture_screen("ua-options")
	design_difficulty.select(0)
	design_difficulty.item_selected.emit(0)
	var config := ConfigFile.new()
	config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE"))
	require(config.get_value("design_difficulty", "UA", "") == "design",
		"The design's own difficulty is saved for Unlimited Adventures")
	await check_party_spell_limits(gameplay)
	await press("GameMenu/Options/Close")
	await press("GameMenu/Menu/Save")
	var slots: Window = current_scene.get_node("SaveSlots")
	require(slots.visible, "Save game opens the save slots for a design")
	slots.get_node("Name").text = "Test hall"
	await press("SaveSlots/Action")
	require(not slots.visible, "The design's game is saved: " + slots.get_node("Status").text)
	require(FileAccess.file_exists(ProjectSettings.globalize_path("user://saves/UA").path_join("Test hall".to_utf8_buffer().hex_encode() + ".ogs")),
		"A design's saves go to its game's own folder")
	var main_menu := await open_main_menu()
	require(not main_menu.get_node("Buttons/Continue").disabled, "The main menu's Continue offers the design's save")
	main_menu.get_node("Buttons/Continue").pressed.emit()
	for wait in range(4): await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", "Continue opens the design's game")
	await press("PartyPanel/ActionRow2/Explore")
	town = current_scene.get_node("CampaignTown")
	require(town.get_node("%Coordinates").text.begins_with("Party (3, 2)"),
		"The loaded party stands where it was saved: " + town.get_node("%Coordinates").text)
	require(not town.get_node("%Continue").visible, "No event runs again on loading")
	require((town.tour_visuals()["wall_image"] as Image).get_data() == north_view, "It faces as it did")
	forward = town.get_node("%Forward")
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
	# The field is the hall's walls in the kit's DUNGCOM tiles, and each kobold
	# stands in its CPIC icon.
	var widths: Array = []
	for wait in range(120):
		await process_frame
		widths = fight.enemy_sprite_widths()
		if not widths.is_empty(): break
	var field: Dictionary = fight.battlefield_visuals()
	var tiled := 0
	for texture in field.get("terrain_textures", []):
		if texture != null: tiled += 1
	require(tiled == 50 * 25, "Every square of the field has its tile: %d" % tiled)
	require(widths.size() == 3 and widths.all(func(width): return width > 0),
		"Each kobold has its combat icon: " + str(widths))
	if not captures.is_empty():
		RenderingServer.force_draw(false)
		await process_frame
		root.get_texture().get_image().save_png(captures.path_join("ua-combat.png"))
	print("Startup menu passed: Unlimited Adventures' kit and design, a party made under the kit's rules, the design's welcome, its 3D view, a step, a save continued from the main menu, and its kobolds' fight on the kit's field with their icons.")
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
