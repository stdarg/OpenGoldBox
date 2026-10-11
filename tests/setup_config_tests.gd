extends SceneTree

const CONFIG := "res://settings.cfg"
var scenario := "first"
var real_path := ""
var captures := ""
var args: PackedStringArray

func _initialize() -> void:
	args = OS.get_cmdline_args() + OS.get_cmdline_user_args()
	for arg in args:
		if arg.begins_with("--setup-case="): scenario = arg.trim_prefix("--setup-case=")
		if arg.begins_with("--setup-capture="): captures = arg.trim_prefix("--setup-capture=")
	real_path = OS.get_environment("OPENGOLD_GAME_DIR")
	call_deferred("run_checks")

func require(value: bool, message: String) -> void:
	if not value:
		push_error(message)
		quit(1)
		assert(value, message)

func settle() -> void:
	for frame in range(6): await process_frame

func capture(window: Window, name: String) -> void:
	if captures.is_empty(): return
	DirAccess.make_dir_recursive_absolute(captures)
	await RenderingServer.frame_post_draw
	require(window.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Capture failed")

func read_config() -> ConfigFile:
	var config := ConfigFile.new()
	config.load(CONFIG)
	return config

func run_checks() -> void:
	require(not real_path.is_empty(), "Set OPENGOLD_GAME_DIR to the local reference installation for this test")
	OS.unset_environment("OPENGOLD_LANG")
	OS.unset_environment("OPENGOLD_GAME_DIR")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(CONFIG))
	var config := ConfigFile.new()
	if scenario not in ["first", "first-warning", "save-error"]:
		if scenario not in ["missing-path", "cancel-path"]: config.set_value("game", "path", real_path)
		if scenario != "missing-lang": config.set_value("interface", "language", "es" if scenario == "missing-path" else "en")
		config.set_value("unrelated", "keep", "retained")
		require(config.save(CONFIG) == OK, "Cannot prepare config")
	if scenario == "override":
		OS.set_environment("OPENGOLD_GAME_DIR", real_path)
		OS.set_environment("OPENGOLD_LANG", "es")
	if scenario == "reset":
		OS.set_environment("OPENGOLD_GAME_DIR", "X:/invalid-environment-path")
		OS.set_environment("OPENGOLD_LANG", "es")
	if scenario == "save-error":
		var file := FileAccess.open(CONFIG, FileAccess.WRITE)
		file.store_string("[invalid\n")
		file.close()
	var original := FileAccess.get_file_as_string(CONFIG) if FileAccess.file_exists(CONFIG) else ""
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	require(current_scene.name == "Startup", "Startup scene missing")
	var path_dialog: Window = current_scene.get_node("PathDialog")
	var language: Window = current_scene.get_node("LanguageDialog")
	if scenario == "save-error":
		require(language.visible and not path_dialog.visible, "Language must precede path even with a malformed config")
		language.get_node("Continue").pressed.emit()
		await settle()
		require(language.visible and not language.get_node("Status").text.is_empty(), "Malformed config must report a save error")
		require(FileAccess.get_file_as_string(CONFIG) == original, "Malformed config was overwritten")
		print("Setup config passed: save-error")
		quit(0)
		return
	if scenario in ["first", "first-warning", "reset"]:
		require(language.visible and not path_dialog.visible and not current_scene.get_node("ChecksumWarning").visible, "Language must precede folder setup and checksum warnings")
		require(current_scene.get_node("ImageFit/Image/Text").texture == null, "Splash began before language selection")
		if scenario == "reset":
			require(FileAccess.get_file_as_string(CONFIG) == original, "Reset erased saved choices before confirmation")
			require(TranslationServer.get_locale() == "en", "Reset language did not supersede environment override")
		await choose_spanish(language, true)
		require(path_dialog.visible and path_dialog.get_node("Continue").text == "Continuar", "Folder dialog did not use the confirmed language")
		require(path_dialog.get_node("Title").text == TranslationServer.translate("Pool of Radiance game files"), "Folder heading was translated before confirmation")
		original = FileAccess.get_file_as_string(CONFIG)
	if scenario in ["valid", "override"]:
		require(not path_dialog.visible and not language.visible, "Complete config should skip setup")
		require(current_scene.get_node("ImageFit/Image/Text").texture != null, "Splash did not start")
		require(TranslationServer.get_locale() == ("es" if scenario == "override" else "en"), "Language precedence incorrect")
		require(FileAccess.get_file_as_string(CONFIG) == original, "Override rewrote config")
	elif scenario in ["missing-lang", "cancel-lang"]:
		require(not path_dialog.visible and language.visible, "Should ask only for missing/reset language")
		if scenario == "cancel-lang":
			current_scene.tree_exiting.connect(func():
				require(FileAccess.get_file_as_string(CONFIG) == original, "Cancel changed config")
				print("Setup cancel-language passed."))
			language.get_node("Cancel").pressed.emit()
			await settle()
			require(false, "Cancel did not exit")
			return
		await choose_spanish(language)
	else:
		require(path_dialog.visible and not language.visible, "Expected folder setup after language was resolved")
		await capture(path_dialog, "game-path-" + scenario)
		if scenario == "cancel-path":
			current_scene.tree_exiting.connect(func():
				require(FileAccess.get_file_as_string(CONFIG) == original, "Cancel changed config")
				print("Setup cancel-path passed."))
			path_dialog.get_node("Cancel").pressed.emit()
			await settle()
			require(false, "Cancel did not exit")
			return
		if scenario == "first":
			path_dialog.get_node("Path").text = "X:/not-a-game-folder"
			path_dialog.get_node("Continue").pressed.emit()
			await settle()
			require(path_dialog.visible and not path_dialog.get_node("Status").text.is_empty(), "Invalid folder was accepted")
			require(path_dialog.get_node("Status").text == TranslationServer.translate("Choose an existing folder containing the Pool of Radiance game files."), "Validation error did not use the chosen language")
			var field: LineEdit = path_dialog.get_node("Path")
			field.grab_focus()
			field.caret_column = field.text.length()
			var edit := InputEventKey.new()
			edit.keycode = KEY_BACKSPACE
			edit.pressed = true
			path_dialog.push_input(edit, true)
			await settle()
			require(field.text != "X:/not-a-game-folder", "Keyboard edit did not change the path")
			require(path_dialog.get_node("Status").text.is_empty(), "Editing the path left a stale error")
			path_dialog.get_node("Continue").pressed.emit()
			await settle()
			require(not path_dialog.get_node("Status").text.is_empty(), "Retrying an invalid path must show a fresh error")
			path_dialog.get_node("Browse").pressed.emit()
			await settle()
			var browser: FileDialog = path_dialog.get_node("BrowseDialog")
			require(browser.visible and browser.file_mode == FileDialog.FILE_MODE_OPEN_DIR, "Browse must select a folder")
			browser.dir_selected.emit(real_path)
			browser.hide()
			require(path_dialog.get_node("Path").text == real_path, "Browse result was not applied")
			require(path_dialog.get_node("Status").text.is_empty(), "Browse selection left a stale error")
		var selected := real_path
		if scenario in ["mismatch", "warning-quit", "missing-file", "first-warning"]:
			selected = ProjectSettings.globalize_path("user://synthetic-files-" + scenario)
			DirAccess.make_dir_recursive_absolute(selected)
			var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://config/por-pc13-md5.json"))
			for name in manifest.files:
				var fixture := FileAccess.open(selected.path_join(name), FileAccess.WRITE)
				fixture.store_string("Synthetic test data; not original game content.")
				fixture.close()
			if scenario == "missing-file": DirAccess.remove_absolute(selected.path_join("START.EXE"))
		path_dialog.get_node("Path").text = selected
		path_dialog.get_node("Continue").pressed.emit()
		await settle()
		if scenario == "missing-file":
			require(path_dialog.visible and path_dialog.get_node("Status").text.contains("START.EXE"), "Missing required file was not reported")
			require(FileAccess.get_file_as_string(CONFIG) == original, "Invalid path was saved")
		else:
			if scenario in ["mismatch", "warning-quit", "first-warning"]:
				var warning: Window = current_scene.get_node("ChecksumWarning")
				require(warning.visible and warning.get_node("Files").text.contains("START.EXE"), "MD5 mismatch warning missing")
				require(FileAccess.get_file_as_string(CONFIG) == original, "Unaccepted mismatch path was saved")
				if scenario == "first-warning":
					require(warning.title == TranslationServer.translate("Game file warning") and warning.get_node("Continue").text == "Continuar", "Checksum warning did not use the chosen language")
				await capture(warning, "checksum-warning")
				if scenario == "warning-quit":
					current_scene.tree_exiting.connect(func():
						require(FileAccess.get_file_as_string(CONFIG) == original, "Quit changed saved path")
						print("Setup checksum-quit passed."))
					warning.get_node("Quit").pressed.emit()
					await settle()
					require(false, "Warning Quit did not exit")
					return
				warning.get_node("Continue").pressed.emit()
				await settle()
			require(read_config().get_value("game", "path") == selected, "Confirmed path was not saved")
			require(not language.visible, "Language was prompted again after folder confirmation")
			require(current_scene.get_node("ImageFit/Image/Text").texture != null, "Path confirmation did not start splash")
			if scenario in ["first", "reset", "first-warning"]:
				require(current_scene.get_node("ImageFit/Image/Text").texture.resource_path.ends_with("OpenGoldBoxEngineLettering.es.png"), "Path confirmation skipped first Spanish splash")
	if scenario not in ["first", "first-warning", "save-error"]:
		require(read_config().get_value("unrelated", "keep") == "retained", "Unrelated setting lost")
	print("Setup config passed: ", scenario)
	quit(0)

func choose_spanish(dialog: Window, expect_path := false) -> void:
	var choices: ItemList = dialog.get_node("Choices")
	choices.select(1)
	choices.item_selected.emit(1)
	require(dialog.get_node("Continue").text == "Continuar", "Language preview failed")
	choices.grab_focus()
	var enter := InputEventKey.new()
	enter.keycode = KEY_ENTER
	enter.pressed = true
	dialog.push_input(enter, true)
	await settle()
	require(read_config().get_value("interface", "language") == "es", "Spanish was not saved")
	if not expect_path:
		require(current_scene.get_node("ImageFit/Image/Text").texture.resource_path.ends_with("OpenGoldBoxEngineLettering.es.png"), "Confirmation skipped first Spanish splash")
