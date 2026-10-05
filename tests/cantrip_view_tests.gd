extends SceneTree

var captures := ""
var locales := ["en", "es"]

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--cantrip-demo": locales = ["en"]
		if arg.begins_with("--poison-capture="):
			captures = arg.trim_prefix("--poison-capture=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle() -> void:
	for frame in range(6):
		await process_frame

func press(path: String) -> void:
	var button: Button = current_scene.get_node(path)
	require(not button.disabled, "Disabled control: " + path)
	button.pressed.emit()
	await settle()

func capture(name: String) -> void:
	if captures.is_empty(): return
	DirAccess.make_dir_recursive_absolute(captures)
	await RenderingServer.frame_post_draw
	require(root.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Cannot save screenshot")

func drag(from: Vector2, to: Vector2) -> void:
	var down := InputEventMouseButton.new()
	down.button_index = MOUSE_BUTTON_LEFT
	down.pressed = true
	down.position = from
	down.global_position = from
	root.push_input(down, true)
	await settle()
	var motion := InputEventMouseMotion.new()
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	motion.position = to
	motion.global_position = to
	motion.relative = to - from
	root.push_input(motion, true)
	await settle()
	var up := InputEventMouseButton.new()
	up.button_index = MOUSE_BUTTON_LEFT
	up.position = to
	up.global_position = to
	root.push_input(up, true)
	await settle()

func choose(path: String, text: String) -> void:
	var list = current_scene.get_node(path)
	var found := -1
	for i in range(list.item_count):
		if list.get_item_text(i) == text:
			found = i
			break
	require(found >= 0 and not list.is_item_disabled(found), "Missing eligible choice: " + text)
	list.select(found)
	list.item_selected.emit(found)
	await settle()

func pick(group: int, option: String, selected := true) -> void:
	var check: CheckBox = current_scene.get_node("Training/Rows/Group%d/%s" % [group, option])
	require(check.is_visible_in_tree() and not check.disabled, "Unavailable training: " + option)
	check.set_pressed(selected)
	await settle()

func keyboard(key: Key) -> void:
	for down in [true, false]:
		var event := InputEventKey.new()
		event.keycode = key
		event.pressed = down
		root.push_input(event, true)
		await settle()

func run_checks() -> void:
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	root.size = Vector2i(1120, 800)
	await settle()
	await press("Next")
	await press("Next")
	await choose("Background", "Sage")
	for attempt in range(100):
		await press("Roll")
		if current_scene.get_node("Dice3").get_parsed_text().to_int() >= 13: break
	for i in range(6):
		await drag(current_scene.get_node("Dice" + str(i)).get_global_rect().get_center(), current_scene.get_node("Score" + str(i)).get_global_rect().get_center())
	await press("Next")
	await choose("Choices", "Wizard")
	await press("Next")
	var fixed: RichTextLabel = current_scene.get_node("TrainingFixed")
	for label in ["Arcana", "History"]:
		require(fixed.get_parsed_text().contains(label), "Sage fixed training is visible: " + label)
	for locale in locales:
		TranslationServer.set_locale(locale)
		await press("Back")
		await press("Next")
		require(fixed.get_parsed_text().contains("Sage background" if locale == "en" else "Trasfondo de sabio"), "Sage source label is human-readable and localized")
		for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
			root.size = size
			await settle()
			await capture("sage-training-" + locale + "-" + str(size.x))
	TranslationServer.set_locale("en")
	await press("Back")
	await press("Next")
	root.size = Vector2i(1120, 800)
	await settle()
	await pick(0, "nature")
	await pick(0, "investigation")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Spell Choices", "Spell Choices follows Training")
	var fire: CheckBox = current_scene.get_node("SpellChoices/Rows/fire_bolt")
	var poison: CheckBox = current_scene.get_node("SpellChoices/Rows/poison_spray")
	require(not fire.button_pressed and not poison.button_pressed, "Interactive choices are explicit")
	poison.grab_focus()
	await keyboard(KEY_SPACE)
	require(poison.button_pressed and poison.has_focus(), "Keyboard chooses Poison Spray and retains focus")
	fire.set_pressed(true)
	await settle()
	require(current_scene.get_node("SpellChoices/Rows/Count").text.ends_with("(2 / 3)"), "Correct SRD entitlement and selection count")
	require(current_scene.get_node("Next").disabled and current_scene.get_node("SpellChoices/Rows/Pending").visible, "Available Wizard choices must be completed")
	var frost: CheckBox = current_scene.get_node("SpellChoices/Rows/ray_of_frost")
	frost.grab_focus()
	await keyboard(KEY_SPACE)
	require(frost.button_pressed and current_scene.get_node("Next").disabled, "Book and preparation are independent remaining choices")
	var book: CheckBox = current_scene.get_node("SpellChoices/Rows/BookChoices/spellbook_1/magic_missile")
	book.set_pressed(true); await settle()
	# The book takes six level-one spells and four of them are prepared.
	var learned := 1
	for entry in current_scene.get_node("SpellChoices/Rows/BookChoices/spellbook_1").get_children():
		if entry is CheckBox and entry.visible and not entry.button_pressed and learned < 6:
			entry.set_pressed(true); await settle(); learned += 1
	var prepared: CheckBox = current_scene.get_node("SpellChoices/Rows/BookChoices/prepared/magic_missile")
	prepared.grab_focus(); await keyboard(KEY_SPACE)
	var ready := 1
	for entry in current_scene.get_node("SpellChoices/Rows/BookChoices/prepared").get_children():
		if entry is CheckBox and entry.visible and not entry.disabled and not entry.button_pressed and ready < 4:
			entry.set_pressed(true); await settle(); ready += 1
	require(not current_scene.get_node("Next").disabled, "Keyboard completes supported independent preparation")
	book.set_pressed(false); await settle()
	require(current_scene.get_node("Next").disabled and not prepared.visible, "Removing a book entry clears dependent preparation")
	book.set_pressed(true); await settle(); prepared.set_pressed(true); await settle()
	for locale in locales:
		TranslationServer.set_locale(locale); await press("Back"); await press("Next")
		for size in [Vector2i(1120,800), Vector2i(1920,1080)]:
			root.size = size; await settle(); await capture("wizard-creation-%s-%d" % [locale,size.x])
	TranslationServer.set_locale("en"); await press("Back"); await press("Next")
	require(not current_scene.get_node("SpellChoices/Rows/Pending").visible, "No missing starting cantrip remains")
	await capture("cantrip-choices-1120")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Name", "Name follows Spell Choices")
	await press("Back")
	require(fire.button_pressed and poison.button_pressed and frost.button_pressed, "Back retains all three selections")
	await press("Back")
	await press("Next")
	require(fire.button_pressed and poison.button_pressed and frost.button_pressed, "Training navigation retains cantrips")
	await press("Next")
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Cantrip Wizard"
	name.text_changed.emit(name.text)
	await press("Next")
	await press("Next")
	await press("Modifiers")
	var sheet: String = current_scene.get_node("ModifiersModal/Text").text
	require(sheet.contains("Poison Spray") and sheet.contains("Fire Bolt") and sheet.contains("Ray of Frost"), "Created character sheet shows all three choices")
	for label in ["Arcana", "History", "Sage background", "Nature +", "Wizard class"]:
		require(current_scene.get_node("Description").get_parsed_text().contains(label), "Created character sheet retains Sage training: " + label)
	print("Cantrip creator checks passed")
	quit(0)
