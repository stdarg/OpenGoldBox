extends SceneTree

# The startup flow (the user's design, 2026-11-14): OpenGoldBox's title for 2
# seconds, the main menu (Continue, Pick game, Options, Exit to desktop), the
# games, Pool of Radiance's new title and its menu (Continue, Load game,
# Options, Play with default party, Create new party, Main menu, Exit to
# desktop). On a title any key or click passes it and Escape goes back; in a
# menu Escape goes back. Run using Godot against src/OpenGoldBox/godot with
# --script; --splash-capture-dir=DIR saves pictures of each step.
var shared_texture: Texture2D
var capture_dir := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--splash-capture-dir="): capture_dir = arg.trim_prefix("--splash-capture-dir=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> bool:
	if not condition:
		push_error(message)
		quit(1)
	return condition

func settle() -> void:
	for frame in range(6):
		await process_frame

func window(name: String) -> Window:
	return current_scene.get_node(name)

func labels(buttons: Node) -> Array:
	var result := []
	for child in buttons.get_children():
		if child is Button: result.append(child.text)
		else:
			for nested in child.get_children(): result.append(nested.text)
	return result

func key(code: Key, pressed := true, echo := false) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = pressed
	event.echo = echo
	Input.parse_input_event(event)
	await settle()

func click() -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = true
	Input.parse_input_event(event)
	await settle()

# Escape as a menu window receives it.
func escape_in(dialog: Window) -> void:
	var event := InputEventKey.new()
	event.keycode = KEY_ESCAPE
	event.pressed = true
	dialog.window_input.emit(event)
	await settle()

func capture(filename: String) -> void:
	if capture_dir.is_empty(): return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	require(image != null and image.save_png(capture_dir.path_join(filename)) == OK, "Startup capture failed")

# A title of lettering over the shared picture: OpenGoldBox's or Pool of
# Radiance's new one.
func is_lettering(lettering_name: String) -> bool:
	if not require(current_scene != null and current_scene.name == "Startup", "Expected the startup scene"): return false
	var image: TextureRect = current_scene.get_node("ImageFit/Image")
	if shared_texture == null:
		shared_texture = image.texture
	if not require(image.texture == shared_texture and image.texture.resource_path.ends_with("OpenGoldBoxSplashBackground.png"), "Every title uses the same picture"): return false
	var lettering: TextureRect = current_scene.get_node("ImageFit/Image/Text")
	var expected := lettering_name + (".es.png" if TranslationServer.get_locale() == "es" else ".png")
	if not require(lettering.visible and lettering.texture != null and lettering.texture.resource_path.ends_with(expected), "Expected the lettering " + expected): return false
	if not require(lettering.stretch_mode == TextureRect.STRETCH_KEEP_ASPECT_CENTERED, "Lettering must preserve its proportions"): return false
	return require(image.stretch_mode == TextureRect.STRETCH_KEEP_ASPECT_CENTERED, "The picture must preserve its aspect ratio") and require(current_scene.get_node("Background").get_theme_stylebox("panel").bg_color == Color.BLACK, "Letterbox must be black")

func no_menu_open() -> bool:
	for name in ["MainMenu", "GamePicker", "GameMenu"]:
		if window(name).visible: return false
	return true

# The lettering fades in over 0.6 seconds while the picture shows at once.
func check_fade() -> bool:
	var text: TextureRect = current_scene.get_node("ImageFit/Image/Text")
	var backdrop: TextureRect = current_scene.get_node("ImageFit/Image")
	if not require(text.self_modulate.a == 0.0, "A title starts with invisible lettering"): return false
	if not require(backdrop.self_modulate.a == 1.0 and backdrop.modulate.a == 1.0, "The picture must appear immediately"): return false
	Engine.time_scale = 1.0
	await create_timer(0.2).timeout
	Engine.time_scale = 0.0
	if not require(text.self_modulate.a > 0.1 and text.self_modulate.a < 0.9, "Lettering must gradually fade in"): return false
	Engine.time_scale = 1.0
	await create_timer(0.45).timeout
	Engine.time_scale = 0.0
	return require(is_equal_approx(text.self_modulate.a, 1.0), "Lettering must finish fading after 0.6 seconds")

func open_startup() -> void:
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()

func pick(folder: String) -> void:
	window("MainMenu").get_node("Buttons/PickGame").pressed.emit()
	await settle()
	require(window("GamePicker").visible, "Pick game opens the games")
	window("GamePicker").get_node("Buttons/Games/" + folder).pressed.emit()
	await settle()

func run_checks() -> void:
	# Freeze process delta for exact checks; check_fade and the timed checks
	# advance it.
	Engine.time_scale = 0.0
	for setting in ["viewport_width", "window_width_override"]:
		if not require(ProjectSettings.get_setting("display/window/size/" + setting) == 1920, "Game width must default to 1920"): return
	for setting in ["viewport_height", "window_height_override"]:
		if not require(ProjectSettings.get_setting("display/window/size/" + setting) == 1080, "Game height must default to 1080"): return
	await open_startup()
	# OpenGoldBox's title: its lettering fades in; a key's release, a repeat
	# and time short of 2 seconds leave it.
	if not is_lettering("OpenGoldBoxEngineLettering"): return
	if not require(no_menu_open(), "No menu shows over OpenGoldBox's title"): return
	if not await check_fade(): return
	await capture("opening-title.png")
	await key(KEY_SPACE, false)
	await key(KEY_SPACE, true, true)
	Engine.time_scale = 1.0
	await create_timer(1.0).timeout
	Engine.time_scale = 0.0
	if not is_lettering("OpenGoldBoxEngineLettering"): return
	if not require(no_menu_open(), "OpenGoldBox's title stays 2 seconds"): return
	Engine.time_scale = 1.0
	await create_timer(0.6).timeout
	await settle()
	Engine.time_scale = 0.0
	# The main menu, by itself after 2 seconds.
	var main := window("MainMenu")
	if not require(main.visible and main.exclusive, "The main menu follows OpenGoldBox's title by itself after 2 seconds"): return
	if not require(not current_scene.get_node("ImageFit/Image/Text").visible, "The title's lettering leaves with it"): return
	if not require(labels(main.get_node("Buttons")) == ["Continue", "Pick game", "Options", "Exit to desktop"], "The main menu: " + str(labels(main.get_node("Buttons")))): return
	if not require(main.get_node("Buttons/Continue").disabled, "Continue waits for a save"): return
	await capture("main-menu.png")
	# A key, a click and Escape each pass OpenGoldBox's title.
	for pass_title in ["key", "click", "escape"]:
		await open_startup()
		if not is_lettering("OpenGoldBoxEngineLettering"): return
		if pass_title == "key": await key(KEY_A)
		elif pass_title == "click": await click()
		else: await key(KEY_ESCAPE)
		if not require(window("MainMenu").visible, "A " + pass_title + " passes OpenGoldBox's title"): return
	# Pick game: a button for each game and Back; Escape goes back too.
	main = window("MainMenu")
	main.get_node("Buttons/PickGame").pressed.emit()
	await settle()
	var picker := window("GamePicker")
	if not require(picker.visible and not main.visible, "Pick game opens the games"): return
	if not require(labels(picker.get_node("Buttons")) == ["Pool of Radiance", "Curse of the Azure Bonds", "Secret of the Silver Blades", "Gateway to the Savage Frontier", "Back"], "The games: " + str(labels(picker.get_node("Buttons")))): return
	await capture("pick-game.png")
	await escape_in(picker)
	if not require(main.visible and not picker.visible, "Escape goes back from the games to the main menu"): return
	main.get_node("Buttons/PickGame").pressed.emit()
	await settle()
	picker.get_node("Buttons/Back").pressed.emit()
	await settle()
	if not require(main.visible and not picker.visible, "Back returns to the main menu"): return
	# Pool of Radiance's new title; Escape goes back to the games.
	await pick("POOLRAD")
	if not is_lettering("OpenGoldBoxGameLettering"): return
	if not require(no_menu_open(), "No menu shows over Pool of Radiance's title"): return
	if not await check_fade(): return
	await capture("pool-title.png")
	await key(KEY_ESCAPE)
	if not require(picker.visible and not current_scene.get_node("ImageFit/Image/Text").visible, "Escape goes back from the title to the games"): return
	# A click passes the title to the game's menu.
	picker.get_node("Buttons/Games/POOLRAD").pressed.emit()
	await settle()
	await click()
	var game_menu := window("GameMenu")
	if not require(game_menu.visible and game_menu.exclusive, "Pool of Radiance's menu follows its title"): return
	if not require(game_menu.title == "Pool of Radiance", "The menu names its game: " + game_menu.title): return
	if not require(labels(game_menu.get_node("Buttons")) == ["Continue", "Load game", "Options", "Play with default party", "Create new party", "Main menu", "Exit to desktop"], "The game's menu: " + str(labels(game_menu.get_node("Buttons")))): return
	if not require(game_menu.get_node("Buttons/Continue").disabled and game_menu.get_node("Buttons/Load").disabled, "Continue and Load game wait for a save"): return
	await capture("pool-menu.png")
	await escape_in(game_menu)
	if not require(picker.visible and not game_menu.visible, "Escape goes back from the game's menu to the games"): return
	# The title passes by itself after 5 seconds.
	picker.get_node("Buttons/Games/POOLRAD").pressed.emit()
	await settle()
	Engine.time_scale = 1.0
	await create_timer(5.2).timeout
	await settle()
	Engine.time_scale = 0.0
	if not require(game_menu.visible, "Pool of Radiance's title passes by itself after 5 seconds"): return
	game_menu.get_node("Buttons/MainMenu").pressed.emit()
	await settle()
	if not require(main.visible and not game_menu.visible, "Main menu returns to the main menu"): return
	# Create new party asks the rules first; Escape goes back to the menu.
	await pick("POOLRAD")
	await key(KEY_SPACE)
	game_menu.get_node("Buttons/NewParty").pressed.emit()
	await settle()
	var dialog: Window = current_scene.get_node("RulesDialog")
	var rules: OptionButton = dialog.get_node("Rows/Rules")
	if not require(dialog.visible and rules.item_count == 2 and rules.selected == 1 and not dialog.get_node("Rows/Aging").visible, "A new game asks its rules first, the SRD's offered first"): return
	rules.select(0)
	rules.item_selected.emit(0)
	await settle()
	if not require(dialog.get_node("Rows/Aging").visible and dialog.get_node("Rows/Aging").selected == 0, "Under the original rules the question asks the aging, the original's first"): return
	rules.select(1)
	rules.item_selected.emit(1)
	await escape_in(dialog)
	if not require(game_menu.visible and not dialog.visible, "Escape goes back from the rules to the game's menu"): return
	game_menu.get_node("Buttons/NewParty").pressed.emit()
	await settle()
	dialog.get_node("Rows/Begin").pressed.emit()
	await settle()
	await settle()
	if not require(current_scene != null and current_scene.name == "CharacterCreation", "Create new party opens character creation"): return
	print("Startup check passed: OpenGoldBox's title, the main menu, the games, Pool of Radiance's title and menu, keys, clicks and Escape")
	quit(0)
