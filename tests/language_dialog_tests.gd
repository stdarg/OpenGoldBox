extends SceneTree

var args: PackedStringArray
var capture_dir := ""

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
		require(current_scene.name == "CharacterCreation", "No-flag startup should skip the language dialog and splashes")
		require(current_scene.get_node("PageTitle").text == "Raza y género", "Restored language did not reach character creation")
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
	require(current_scene.get_node("Image").texture == null and current_scene.get_node("Text").texture == null, "Splash must not start behind the language dialog")
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
	require(config.load("res://settings.cfg") == OK and config.get_value("interface", "language") == "es", "Spanish preference was not saved")
	if args.has("--splash"):
		require(current_scene.name == "Startup" and not dialog.visible, "Language selection should continue to splash")
		var background: Texture2D = current_scene.get_node("Image").texture
		require(current_scene.get_node("Text").texture.resource_path.ends_with("OpenGoldBoxEngineLettering.es.png"), "Confirmation key skipped the first Spanish splash")
		await create_timer(0.65).timeout
		await capture(root, "spanish-engine-splash")
		current_scene.get_node("Text").hide()
		await settle()
		var pixels: PackedByteArray
		if not capture_dir.is_empty():
			await RenderingServer.frame_post_draw
			pixels = root.get_texture().get_image().get_data()
		current_scene.get_node("Text").show()
		await key(root, KEY_SPACE)
		require(current_scene.get_node("Text").texture.resource_path.ends_with("OpenGoldBoxGameLettering.es.png"), "Second Spanish splash lettering is wrong")
		require(current_scene.get_node("Image").texture == background, "Language flow changed the shared background")
		await create_timer(0.65).timeout
		await capture(root, "spanish-game-splash")
		if not capture_dir.is_empty():
			current_scene.get_node("Text").hide()
			await settle()
			await RenderingServer.frame_post_draw
			require(root.get_texture().get_image().get_data() == pixels, "Spanish splash backgrounds are not pixel-identical")
		await key(root, KEY_SPACE)
	require(current_scene.name == "CharacterCreation" and current_scene.get_node("PageTitle").text == "Raza y género", "Selection did not reach Spanish character creation")
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
	require(config.save("res://settings.cfg") == OK, "Cannot prepare restart check")
	print("Language dialog passed: exact options, arrows/Enter/button, saved preference, startup ordering and localized splashes.")
	quit(0)
