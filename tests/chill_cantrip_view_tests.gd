extends SceneTree

var captures := ""
var locales := ["en", "es"]

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--english-only": locales = ["en"]
		if arg.begins_with("--chill-capture="):
			captures = arg.trim_prefix("--chill-capture=")
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

# A Wizard must fill every cantrip slot, write six level-one spells in the
# spellbook and prepare four of them before Name.
func complete_wizard_choices() -> void:
	for cantrip in ["fire_bolt", "ray_of_frost"]:
		current_scene.get_node("SpellChoices/Rows/" + cantrip).set_pressed(true)
		await settle()
	await check_until("SpellChoices/Rows/BookChoices/spellbook_1", 6)
	await check_until("SpellChoices/Rows/BookChoices/prepared", 4)

# Check the list's open boxes in order until `wanted` of them are checked.
func check_until(path: String, wanted: int) -> void:
	var checked := 0
	for entry in current_scene.get_node(path).get_children():
		if entry is CheckBox and entry.visible and entry.button_pressed: checked += 1
	for entry in current_scene.get_node(path).get_children():
		if checked >= wanted: return
		if entry is CheckBox and entry.visible and not entry.disabled and not entry.button_pressed:
			entry.set_pressed(true)
			await settle()
			checked += 1

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
	var blast: CheckBox = current_scene.get_node("SpellChoices/Rows/chill_touch")
	require(not blast.button_pressed, "Interactive choice is explicit")
	blast.grab_focus()
	await keyboard(KEY_SPACE)
	require(blast.button_pressed and blast.has_focus(), "Keyboard selection retains focus")
	require(current_scene.get_node("SpellChoices/Rows/Count").text.ends_with("(1 / 3)"), "Three SRD choices")
	require(current_scene.get_node("Next").disabled and current_scene.get_node("SpellChoices/Rows/Pending").visible, "Available Wizard choices must be completed")
	await complete_wizard_choices()
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Name", "Name follows choices")
	await press("Back")
	require(blast.button_pressed, "Back retains choice")
	for locale in locales:
		TranslationServer.set_locale(locale)
		await press("Back")
		await press("Next")
		require(blast.text.contains("Chill Touch" if locale == "en" else "Toque helado"), "Localized cantrip")
		for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
			root.size = size
			await settle()
			await capture("chill-choices-" + locale + "-" + str(size.x))
	TranslationServer.set_locale("en")
	await press("Next")
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Chill Wizard"
	name.text_changed.emit(name.text)
	await press("Next")
	await press("Next")
	await press("Modifiers")
	require(current_scene.get_node("ModifiersModal/Text").text.contains("Chill Touch"), "Final sheet retains selected spell")
	print("Chill creator checks passed")
	quit(0)
