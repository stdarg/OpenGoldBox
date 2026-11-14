extends SceneTree

var args: PackedStringArray
var capture_dir := ""
# The game's settings.cfg: the check's own when ctest names one, else the project's.
var settings_path := OS.get_environment("OPENGOLD_SETTINGS_FILE") if OS.has_environment("OPENGOLD_SETTINGS_FILE") else ProjectSettings.globalize_path("res://settings.cfg")

func _initialize() -> void:
	args = OS.get_cmdline_args() + OS.get_cmdline_user_args()
	for arg in args:
		if arg.begins_with("--language-capture="):
			capture_dir = arg.trim_prefix("--language-capture=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle() -> void:
	for frame in range(6): await process_frame

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

# The new game's first question (docs/ORIGINAL-RULES.md): the rules it plays
# by, the SRD's offered first for now; Begin starts the game with them.
func answer_rules() -> void:
	await settle()
	await pass_title()
	var dialog: Window = current_scene.get_node_or_null("RulesDialog") if current_scene else null
	if dialog and dialog.visible:
		dialog.get_node("Rows/Begin").pressed.emit()
		await settle()
		await settle()

func key(viewport: Viewport, code: Key, ctrl := false) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	event.ctrl_pressed = ctrl
	viewport.push_input(event, true)
	await settle()
	# Confirming the language can open character creation, freeing the dialog.
	if not is_instance_valid(viewport): return
	var release := InputEventKey.new()
	release.keycode = code
	release.ctrl_pressed = ctrl
	viewport.push_input(release, true)
	await settle()

func capture(viewport: Viewport, name: String) -> void:
	if capture_dir.is_empty(): return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	await RenderingServer.frame_post_draw
	require(viewport.get_texture().get_image().save_png(capture_dir.path_join(name + ".png")) == OK, "Cannot capture language dialog")

func run_checks() -> void:
	if args.has("--language-restore"):
		require(TranslationServer.get_locale() == "es", "Saved Spanish preference did not survive process restart")
		change_scene_to_file("res://scenes/startup.tscn")
		await settle()
		await pick_game()
		await answer_rules()
		require(current_scene.name == "CharacterCreation", "A saved language skips the language dialog")
		require(current_scene.get_node("PageTitle").text == "Nombre, raza y género", "Restored language did not reach character creation")
		print("Language restore passed across process restart.")
		quit(0)
		return
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	require(current_scene.name == "Startup", "--reset-lang must pause startup")
	var dialog: Window = current_scene.get_node("LanguageDialog")
	var choices: ItemList = dialog.get_node("Choices")
	require(dialog.visible and dialog.exclusive, "Language dialog must be visible and modal")
	require(choices.item_count == 2, "Only English and Spanish should be offered")
	require(choices.get_item_text(0) == "English" and choices.get_item_text(1) == "Español", "Native language names are incorrect")
	var initial_locale := TranslationServer.get_locale()
	require(choices.get_selected_items() == PackedInt32Array([1 if initial_locale == "es" else 0]), "Current locale must be selected")
	choices.select(0)
	choices.item_selected.emit(0)
	require(dialog.title == "Language" and dialog.get_node("Title").text == "Choose language" and dialog.get_node("Continue").text == "Continue", "English dialog preview is incorrect")
	require(current_scene.get_node("ImageFit/Image").texture != null, "The title picture must show behind the language dialog")
	require(current_scene.get_node("ImageFit/Image/Text").texture == null, "Splash must not start behind the language dialog")
	await capture(dialog, "language-dialog")
	if args.has("--language-close"):
		current_scene.tree_exiting.connect(func(): print("Language dialog close: scene teardown passed."))
		dialog.close_requested.emit()
		await settle()
		require(false, "Language dialog close did not exit")
		return
	if args.has("--language-ctrl-x"):
		current_scene.tree_exiting.connect(func(): print("Language dialog Ctrl+X: scene teardown passed."))
		var event := InputEventKey.new()
		event.keycode = KEY_X
		event.ctrl_pressed = true
		event.pressed = true
		# Native subwindow input reaches this signal; push_input alone bypasses it.
		dialog.window_input.emit(event)
		await settle()
		require(false, "Language dialog Ctrl+X did not exit")
		return
	await key(dialog, KEY_DOWN)
	require(choices.get_selected_items() == PackedInt32Array([1]), "Arrow key did not select Spanish")
	require(dialog.title == "Idioma" and dialog.get_node("Title").text == "Elige idioma" and dialog.get_node("Continue").text == "Continuar", "Highlighting Spanish did not translate the dialog")
	require(TranslationServer.get_locale() == initial_locale, "Preview changed the active language before confirmation")
	await capture(dialog, "language-dialog-spanish")
	await key(dialog, KEY_ENTER)
	require(TranslationServer.get_locale() == "es", "Enter did not activate Spanish")
	var config := ConfigFile.new()
	require(config.load(settings_path) == OK and config.get_value("interface", "language") == "es", "Spanish preference was not saved")
	await pick_game()
	await answer_rules()
	require(current_scene.name == "CharacterCreation" and current_scene.get_node("PageTitle").text == "Nombre, raza y género", "Selection did not reach Spanish character creation")
	# Reopen the actual startup dialog, select English using the controls, and use
	# its button path. Keep Spanish saved for the separate restart check.
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	dialog = current_scene.get_node("LanguageDialog")
	choices = dialog.get_node("Choices")
	require(choices.get_selected_items() == PackedInt32Array([1]), "Reopened dialog did not select the active locale")
	choices.select(0)
	choices.item_selected.emit(0)
	require(dialog.get_node("Title").text == "Choose language" and dialog.get_node("Continue").text == "Continue", "Selecting English did not restore the dialog text")
	dialog.get_node("Continue").pressed.emit()
	await settle()
	require(TranslationServer.get_locale() == "en", "Continue button did not select English")
	config.set_value("interface", "language", "es")
	require(config.save(settings_path) == OK, "Cannot prepare restart check")
	print("Language dialog passed: exact options, arrows/Enter/button, saved preference, startup ordering and localized splashes.")
	quit(0)
