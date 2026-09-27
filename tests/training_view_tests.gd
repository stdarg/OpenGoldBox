extends SceneTree

var captures := ""
var locales := ["en", "es"]

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--training-demo": locales = ["en"]
		if arg.begins_with("--training-capture="):
			captures = arg.trim_prefix("--training-capture=")
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
		Input.parse_input_event(event)
		await settle()

func background_captures(background: String, translated: String) -> void:
	for locale in locales:
		TranslationServer.set_locale(locale)
		await press("Back")
		await press("Next")
		var fixed: String = current_scene.get_node("TrainingFixed").text
		require(fixed.contains(background + " background" if locale == "en" else translated), "Missing translated fixed background source")
		for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
			root.size = size
			await settle()
			await capture(background.to_lower() + "-" + locale + "-" + str(size.x))
			if background == "Soldier":
				var gaming_box: VBoxContainer = current_scene.get_node("Training/Rows/Group4")
				require(gaming_box.get_node("Title").text == ("Soldier Gaming Set (1 / 1)" if locale == "en" else "Juego de soldado (1 / 1)"), "Translated Gaming Set heading")
				current_scene.get_node("Training").scroll_vertical = int(gaming_box.position.y)
				await settle()
				await capture("gaming-" + locale + "-" + str(size.x))
	TranslationServer.set_locale("en")
	root.size = Vector2i(1120, 800)
	await press("Back")
	await press("Next")

func all_class_skill_controls() -> void:
	for klass in ["Barbarian", "Bard", "Cleric", "Druid", "Fighter", "Monk", "Paladin", "Ranger", "Sorcerer", "Warlock", "Wizard"]:
		await choose("Choices", klass)
		await press("Next")
		if klass == "Druid":
			require(current_scene.get_node("TrainingFixed").text.contains("Herbalism Kit (Druid class)"), "Druid Training shows fixed tool source")
		var group := 2 if klass == "Fighter" else 1
		var count := 3 if klass in ["Bard", "Ranger"] else 2
		var box: VBoxContainer = current_scene.get_node("Training/Rows/Group%d" % group)
		var checks: Array[CheckBox] = []
		for child in box.get_children():
			if child is CheckBox and child.visible: checks.append(child)
		require(checks.size() >= count and box.get_node("Title").text == klass + " skills (0 / " + str(count) + ")", "Fresh class skill group: " + klass)
		checks[0].grab_focus()
		await keyboard(KEY_SPACE)
		require(checks[0].button_pressed and checks[0].has_focus(), "Class skill keyboard selection retains focus: " + klass)
		for i in range(1, count):
			checks[i].set_pressed(true)
			await settle()
		require(box.get_node("Title").text.ends_with("(%d / %d)" % [count, count]) and checks[count].disabled, "Class skill count and selection limit: " + klass)
		await press("Back")
		await press("Next")
		require(checks[0].button_pressed and checks[count - 1].button_pressed, "Back preserves class skills: " + klass)
		await pick(0, "elvish")
		await pick(0, "dwarvish")
		if klass == "Bard":
			require(current_scene.get_node("Next").disabled, "Bard instruments remain required")
			var instrument: CheckBox = current_scene.get_node("Training/Rows/Group2/flute")
			instrument.grab_focus()
			await keyboard(KEY_SPACE)
			require(instrument.button_pressed and instrument.has_focus(), "Instrument supports keyboard and retains focus")
			await pick(2, "lute")
			await pick(2, "viol")
			require(current_scene.get_node("Training/Rows/Group2/horn").disabled, "Fourth instrument disabled")
			await press("Back")
			await press("Next")
			require(instrument.button_pressed, "Back preserves instrument choices")
		if klass == "Monk":
			require(current_scene.get_node("Next").disabled, "Monk tool remains required")
			var tool: CheckBox = current_scene.get_node("Training/Rows/Group2/smiths_tools")
			tool.grab_focus()
			await keyboard(KEY_SPACE)
			require(tool.button_pressed and tool.has_focus(), "Monk tool keyboard focus")
			require(current_scene.get_node("Training/Rows/Group2/flute").disabled, "Second Monk tool disabled")
			await press("Back")
			await press("Next")
			require(tool.button_pressed, "Back preserves Monk tool")
		if klass == "Fighter": await choose("Training/Rows/Group1/Choice", "Defense")
		await complete_mastery(klass)
		require(not current_scene.get_node("Next").disabled, "Every class can finish all supported Training choices: " + klass)
		await press("Next")
		require(current_scene.get_node("PageTitle").text == ("Spell Choices" if klass in ["Cleric", "Sorcerer", "Warlock", "Wizard"] else "Name"), "Completed Training reaches the next creation step: " + klass)
		if klass in ["Bard", "Monk", "Druid"]:
			var bard_name: LineEdit = current_scene.get_node("Name")
			bard_name.text = "Instrument Bard"
			bard_name.text_changed.emit(bard_name.text)
			await press("Next")
			await press("Next")
			var bard_sheet: String = current_scene.get_node("Description").get_parsed_text()
			for instrument_name in (["Flute", "Lute", "Viol"] if klass == "Bard" else ["Smith's Tools"] if klass == "Monk" else ["Herbalism Kit"]):
				require(bard_sheet.contains(instrument_name + " (" + klass + " class)"), "Bard sheet shows selected instrument and source")
			await press("Back")
			await press("Back")
		await press("Back")
		await pick(0, "elvish", false)
		await pick(0, "dwarvish", false)
		if klass in ["Bard", "Monk", "Druid", "Wizard", "Barbarian", "Fighter", "Paladin", "Ranger", "Rogue"]:
			for locale in locales:
				TranslationServer.set_locale(locale)
				await press("Back")
				await press("Next")
				require(box.get_node("Title").text.begins_with(klass + " skills" if locale == "en" else "Habilidades de"), "Class skill heading is translated")
				await complete_mastery(klass)
				if klass == "Druid":
					require(current_scene.get_node("TrainingFixed").text.contains("Herbalism Kit (Druid class)" if locale == "en" else "Útiles de herboristería (Clase de druida)"), "Translated Druid tool and source")
				for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
					root.size = size
					await settle()
					current_scene.get_node("Training").scroll_vertical = int(box.position.y)
					await settle()
					await capture("skills-" + klass.to_lower() + "-" + locale + "-" + str(size.x))
					if klass in ["Bard", "Monk"]:
						current_scene.get_node("Training").scroll_vertical = int(current_scene.get_node("Training/Rows/Group2").position.y)
						await settle()
						await capture(klass.to_lower() + "-tools-" + locale + "-" + str(size.x))
						if klass == "Monk":
							current_scene.get_node("Training").scroll_vertical = 100000
							await settle()
							await capture("monk-tools-bottom-" + locale + "-" + str(size.x))
			TranslationServer.set_locale("en")
			root.size = Vector2i(1120, 800)
		for check in checks:
			if check.button_pressed: check.set_pressed(false)
		await settle()
		await press("Back")

func complete_mastery(klass: String) -> void:
	if not klass in ["Barbarian", "Fighter", "Paladin", "Ranger", "Rogue"]: return
	var box: VBoxContainer
	for candidate in current_scene.get_node("Training/Rows").get_children():
		if candidate.visible and candidate.has_node("dagger") and candidate.get_node("dagger").visible: box = candidate
	require(box != null, "Eligible class has a Weapon Mastery group: " + klass)
	var count := 3 if klass == "Fighter" else 2
	var picked := 0
	for child in box.get_children():
		if child is CheckBox and child.visible and child.button_pressed: picked += 1
	for child in box.get_children():
		if child is CheckBox and child.visible and not child.disabled and not child.button_pressed and picked < count:
			child.grab_focus(); await keyboard(KEY_SPACE)
			require(child.button_pressed and child.has_focus(), "Mastery supports keyboard selection with retained focus")
			picked += 1
	require(box.get_node("Title").text.ends_with("(%d / %d)" % [count, count]), "Mastery count is complete")
	for child in box.get_children():
		if child is CheckBox and child.visible and not child.button_pressed: require(child.disabled, "Mastery selection cannot exceed entitlement")
	for size in [Vector2i(1120,800), Vector2i(1920,1080)]:
		root.size = size; await settle(); box.get_node("dagger").grab_focus(); await settle()
		await capture("mastery-" + klass.to_lower() + "-" + TranslationServer.get_locale() + "-" + str(size.x))

func run_checks() -> void:
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	root.size = Vector2i(1120, 800)
	await settle()
	await press("Next")
	await press("Next")
	await choose("Background", "Criminal")
	var qualified := false
	for attempt in range(2000):
		current_scene.get_node("Roll").pressed.emit()
		qualified = true
		for ability in [0, 1, 3, 4, 5]:
			qualified = qualified and current_scene.get_node("Dice" + str(ability)).get_parsed_text().to_int() >= 13
		if qualified: break
	require(qualified, "Fixture needs all twelve classes' primary abilities")
	await settle()
	for i in range(6):
		await drag(current_scene.get_node("Dice" + str(i)).get_global_rect().get_center(), current_scene.get_node("Score" + str(i)).get_global_rect().get_center())
	await press("Next")
	await all_class_skill_controls()
	await choose("Choices", "Rogue")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Training", "Training must follow Class")
	require(current_scene.get_node("Next").disabled, "Incomplete training enables Next")
	require(current_scene.get_node("TrainingFixed").text.contains("Criminal background") and current_scene.get_node("TrainingFixed").text.contains("Rogue class"), "Fixed source grants are missing")
	var first: CheckBox = current_scene.get_node("Training/Rows/Group0/elvish")
	first.grab_focus()
	await keyboard(KEY_SPACE)
	require(first.button_pressed and first.has_focus(), "Keyboard selection lost its choice or focus")
	await keyboard(KEY_TAB)
	require(current_scene.get_node("Training/Rows/Group0/giant").has_focus(), "Tab navigation did not move to the next checkbox")
	await pick(0, "dwarvish")
	require(current_scene.get_node("Training/Rows/Group0/giant").disabled and current_scene.get_node("Training/Rows/Group0/Title").text.ends_with("(2 / 2)"), "Language limit or selection count missing")
	await capture("training-starting-languages")
	for skill in ["acrobatics", "investigation", "perception", "persuasion"]:
		await pick(1, skill)
	await pick(2, "stealth")
	await pick(2, "perception")
	await pick(3, "undercommon")
	await complete_mastery("Rogue")
	require(not current_scene.get_node("Next").disabled, "Complete Rogue training cannot continue")
	var last: CheckBox = current_scene.get_node("Training/Rows/Group3/undercommon")
	last.grab_focus()
	await settle()
	require(current_scene.get_node("Training").scroll_vertical > 0, "Focus did not scroll to the last choice")
	await capture("training-additional-language")
	await pick(1, "perception", false)
	require(not current_scene.get_node("Training/Rows/Group2/perception").visible and current_scene.get_node("Next").disabled, "Removing proficiency leaves illegal Expertise")
	await pick(1, "perception")
	require(not current_scene.get_node("Training/Rows/Group2/perception").button_pressed, "Cleared Expertise was silently restored")
	await pick(2, "perception")
	await press("Next")
	await press("Back")
	require(current_scene.get_node("Training/Rows/Group2/perception").button_pressed, "Back loses choices")
	await press("Back")
	await press("Back")
	await choose("Background", "Sage")
	await press("Next")
	await press("Next")
	require(not current_scene.get_node("Training/Rows/Group2/stealth").visible and current_scene.get_node("Training/Rows/Group2/perception").button_pressed, "Background change clears valid Expertise or keeps invalid Expertise")
	await press("Back")
	await press("Back")
	await choose("Background", "Acolyte")
	await press("Next")
	await press("Next")
	require(current_scene.get_node("TrainingFixed").text.contains("Insight") and current_scene.get_node("TrainingFixed").text.contains("Religion") and current_scene.get_node("TrainingFixed").text.contains("Calligrapher"), "Acolyte fixed skills/tool missing")
	await pick(2, "religion")
	require(not current_scene.get_node("Next").disabled, "Background Religion cannot fulfill Rogue Expertise")
	await background_captures("Acolyte", "Trasfondo de acólito")
	await press("Back")
	await press("Back")
	await choose("Background", "Soldier")
	await press("Next")
	await press("Next")
	require(current_scene.get_node("TrainingFixed").text.contains("Athletics") and current_scene.get_node("TrainingFixed").text.contains("Intimidation"), "Soldier fixed skills missing")
	require(not current_scene.get_node("Training/Rows/Group2/religion").visible and current_scene.get_node("Training/Rows/Group2/perception").button_pressed, "Background change must drop lost Religion Expertise and retain Perception")
	await pick(2, "athletics")
	require(current_scene.get_node("Next").disabled, "Soldier Gaming Set remains required")
	var gaming: CheckBox = current_scene.get_node("Training/Rows/Group4/dice")
	gaming.grab_focus()
	await keyboard(KEY_SPACE)
	require(gaming.button_pressed and gaming.has_focus() and not current_scene.get_node("Next").disabled, "Gaming Set keyboard choice completes Training")
	require(current_scene.get_node("Training/Rows/Group4/dragonchess").disabled, "Second Gaming Set disabled")
	await press("Back")
	await press("Next")
	require(gaming.button_pressed, "Back preserves Gaming Set")
	await background_captures("Soldier", "Trasfondo de soldado")
	await press("Back")
	await choose("Choices", "Fighter")
	await press("Next")
	await complete_mastery("Fighter")
	var style: OptionButton = current_scene.get_node("Training/Rows/Group1/Choice")
	require(current_scene.get_node("Training/Rows/Group3/dice").button_pressed, "Class change preserves Soldier Gaming Set")
	require(style.is_visible_in_tree() and style.selected == 0 and current_scene.get_node("Next").disabled, "Fighter must choose a starting style")
	require(current_scene.get_node("Training/Rows/Group1").get_index() == 0 and not current_scene.get_node("Training/Rows/Group1/acrobatics").visible, "Style must appear above languages without Rogue controls")
	require(current_scene.get_node("Training/Rows/Group2/acrobatics").button_pressed and current_scene.get_node("Training/Rows/Group2/persuasion").button_pressed, "Fighter keeps compatible Rogue skill selections up to its limit")
	style.grab_focus()
	await keyboard(KEY_SPACE)
	require(style.get_popup().visible, "Keyboard must open the style dropdown")
	await keyboard(KEY_DOWN)
	await keyboard(KEY_ENTER)
	require(style.selected == 2 and style.has_focus() and not current_scene.get_node("Next").disabled, "Keyboard Archery choice must complete training and retain focus")
	await press("Next")
	await press("Back")
	require(style.selected == 2, "Back must preserve the chosen style")
	await choose("Training/Rows/Group1/Choice", "Great Weapon Fighting")
	require(style.selected == 3 and not current_scene.get_node("Next").disabled, "GWF completes starting style choice")
	await choose("Training/Rows/Group1/Choice", "Defense")
	require(style.selected == 1 and not current_scene.get_node("Next").disabled, "Changing style must replace its selection")
	for locale in locales:
		TranslationServer.set_locale(locale)
		await press("Back")
		await press("Next")
		require(style.selected == 1 and style.get_item_text(1) == ("Defense" if locale == "en" else "Defensa"), "Translated dropdown must preserve Defense")
		for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
			root.size = size
			await settle()
			current_scene.get_node("Training").scroll_vertical = 0
			await settle()
			await capture("style-" + locale + "-" + str(size.x))
	TranslationServer.set_locale("en")
	root.size = Vector2i(1120, 800)
	await press("Back")
	await press("Next")
	await press("Back")
	await choose("Choices", "Rogue")
	await press("Next")
	require(not style.visible and current_scene.get_node("Next").disabled and current_scene.get_node("Training/Rows/Group1/acrobatics").button_pressed, "Returning to Rogue keeps valid skills and requires its missing choices")
	for skill in ["acrobatics", "investigation", "perception", "persuasion"]:
		await pick(1, skill)
	await pick(2, "perception")
	await pick(2, "investigation")
	await pick(3, "undercommon")
	await complete_mastery("Rogue")
	current_scene.get_node("Training/Rows/Group2/perception").grab_focus()
	await settle()
	await capture("training-rogue-expertise")
	await press("Next")
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Trained Rogue"
	name.text_changed.emit(name.text)
	await press("Next")
	await press("Next")
	var sheet: String = current_scene.get_node("Description").get_parsed_text()
	require(sheet.contains("Supported training choices complete") and sheet.contains("Perception +") and sheet.contains("Rogue Expertise") and sheet.contains("Undercommon") and sheet.contains("Soldier background") and sheet.contains("Dice (Soldier background)") and sheet.contains("Athletics +") and sheet.contains("Intimidation +"), "Sheet lacks bonuses, sources or languages")
	await press("AddParty")
	require(current_scene.get_node("PartyPanel/Sheet").get_parsed_text().contains("Supported training choices complete"), "Adding to party lost training")
	print("Training UI checks passed: all twelve class skill lists, keyboard, limits, dependent Expertise, Back, background/class changes, sheet and party.")
	quit(0)
