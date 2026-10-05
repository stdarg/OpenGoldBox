extends SceneTree

var captures := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--sacred-capture="):
			captures = arg.trim_prefix("--sacred-capture=")
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
		if current_scene.get_node("Dice3").get_parsed_text().to_int() >= 13 and current_scene.get_node("Dice4").get_parsed_text().to_int() >= 13: break
	for i in range(6):
		await drag(current_scene.get_node("Dice" + str(i)).get_global_rect().get_center(), current_scene.get_node("Score" + str(i)).get_global_rect().get_center())
	await press("Next")
	await choose("Choices", "Cleric")
	await press("Next")
	await pick(1, "medicine")
	await pick(1, "persuasion")
	require(current_scene.get_node("Training/Rows/Group0/Title").text == "Divine Order (0 / 1)" and current_scene.get_node("Next").disabled, "A Cleric chooses a Divine Order")
	await choose("Training/Rows/Group0/Choice", "Protector")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Spell Choices", "Spell Choices follows Training")
	var sacred: CheckBox = current_scene.get_node("SpellChoices/Rows/sacred_flame")
	require(not sacred.button_pressed, "Interactive choices are explicit")
	sacred.grab_focus()
	await keyboard(KEY_SPACE)
	require(sacred.button_pressed and sacred.has_focus(), "Keyboard chooses Sacred Flame and retains focus")
	var spare: CheckBox = current_scene.get_node("SpellChoices/Rows/spare_the_dying")
	spare.set_pressed(true)
	await settle()
	require(current_scene.get_node("SpellChoices/Rows/Count").text == "Cleric cantrips (2 / 3)", "Correct Cleric SRD entitlement and source label")
	require(current_scene.get_node("SpellChoices/Rows/Pending").visible, "Unavailable cantrips stay pending")
	var prepared := "SpellChoices/Rows/BookChoices/prepared/"
	require(current_scene.get_node(prepared + "Count").text == "Prepared spells (0 / 4)", "Four prepared spells")
	var blindness = current_scene.get_node_or_null(prepared + "blindness")
	require(blindness == null or not blindness.visible, "A level-one Cleric prepares only spells it has slots for")
	for spell in ["cure_wounds", "healing_word", "inflict_wounds", "shield_of_faith"]:
		current_scene.get_node(prepared + spell).set_pressed(true)
		await settle()
	require(current_scene.get_node(prepared + "Count").text == "Prepared spells (4 / 4)", "Every available spell is prepared")
	await capture("cleric-cantrip-choices-1120")
	TranslationServer.set_locale("es")
	await press("Back")
	await press("Next")
	require(sacred.text == "Llama sagrada" and sacred.button_pressed, "Translated choice preserves selection")
	require(current_scene.get_node("SpellChoices/Rows/Count").text == "Trucos de Clérigo (2 / 3)", "Translated Cleric entitlement label")
	await capture("cleric-cantrip-choices-es-1120")
	TranslationServer.set_locale("en")
	await press("Back")
	await press("Next")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Name", "Name follows Spell Choices")
	await press("Back")
	require(sacred.button_pressed, "Back retains selection")
	await press("Back")
	await press("Back")
	await choose("Choices", "Wizard")
	await press("Next")
	require(current_scene.get_node("Training/Rows/Group0/medicine").button_pressed, "Wizard retains compatible Medicine choice")
	await pick(0, "nature")
	await press("Next")
	require(not sacred.visible and current_scene.get_node("SpellChoices/Rows/fire_bolt").visible, "Wizard cannot select a leftover Cleric checkbox")
	current_scene.get_node("SpellChoices/Rows/fire_bolt").set_pressed(true)
	await press("Back")
	await press("Back")
	await choose("Choices", "Cleric")
	await press("Next")
	await pick(1, "medicine")
	await pick(1, "persuasion")
	await choose("Training/Rows/Group0/Choice", "Thaumaturge")
	await press("Next")
	require(current_scene.get_node("SpellChoices/Rows/Count").text == "Cleric cantrips (0 / 4)", "Thaumaturge adds a Cleric cantrip")
	require(sacred.visible and not sacred.button_pressed and not current_scene.get_node("SpellChoices/Rows/fire_bolt").visible, "Class changes clear invalid choices without silently restoring them")
	sacred.set_pressed(true)
	await settle()
	spare.set_pressed(true)
	await settle()
	for spell in ["cure_wounds", "healing_word", "inflict_wounds", "shield_of_faith"]:
		current_scene.get_node(prepared + spell).set_pressed(true)
		await settle()
	await press("Next")
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Cantrip Cleric"
	name.text_changed.emit(name.text)
	await press("Next")
	await press("Next")
	await press("Modifiers")
	var sheet: String = current_scene.get_node("ModifiersModal/Text").text
	require(sheet.contains("Sacred Flame") and sheet.contains("Pending Cleric choices: 2 cantrips, 0 prepared spells."), "Created character sheet shows choices and remaining entitlement")
	require(current_scene.get_node("Description").get_parsed_text().contains("Cleric class"), "Created sheet shows the class skill source")
	print("Cleric cantrip creator checks passed")
	quit(0)
