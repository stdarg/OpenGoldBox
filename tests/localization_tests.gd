extends SceneTree

var captures := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--localization-capture="):
			captures = arg.trim_prefix("--localization-capture=")
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

func run_checks() -> void:
	TranslationServer.set_locale("es")
	require(TranslationServer.translate("Next") == "Siguiente", "Spanish catalog not loaded")
	var weapon_names := {"Greatclub": "Gran garrote", "Sickle": "Hoz", "Greataxe": "Gran hacha", "Lance": "Lanza de caballería", "Maul": "Maza a dos manos", "Rapier": "Estoque", "Whip": "Látigo", "Blowgun": "Cerbatana", "Hand Crossbow": "Ballesta de mano", "Heavy Crossbow": "Ballesta pesada", "Musket": "Mosquete", "Pistol": "Pistola"}
	for key in weapon_names:
		require(TranslationServer.translate(key) == weapon_names[key], "New weapon label is not translated: " + key)
	var armor_names := {"Padded Armor": "Armadura acolchada", "Studded Leather Armor": "Armadura de cuero tachonado", "Hide Armor": "Armadura de pieles", "Chain Shirt": "Camisa de malla", "Scale Mail": "Cota de escamas", "Breastplate": "Coraza", "Half Plate Armor": "Armadura de semiplacas", "Ring Mail": "Cota de anillas", "Splint Armor": "Armadura de bandas", "Plate Armor": "Armadura de placas"}
	for key in armor_names:
		require(TranslationServer.translate(key) == armor_names[key], "New armor label is not translated: " + key)
	require(TranslationServer.translate("This armor imposes Disadvantage on Dexterity (Stealth) checks.") == "Esta armadura impone desventaja en las pruebas de Destreza (Sigilo).", "Armor Stealth penalty is not translated")
	require(TranslationServer.translate_plural("{count} item", "{count} items", 1) == "{count} objeto", "Spanish singular missing")
	require(TranslationServer.translate_plural("{count} item", "{count} items", 2) == "{count} objetos", "Spanish plural missing")
	require(TranslationServer.translate("A missing original message", "por/test/dialogue") == "A missing original message", "Source fallback failed")
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	require(current_scene.get_node("PageTitle").text == "Raza y género", "Native page title is not Spanish")
	require(current_scene.get_node("Choices").get_item_text(0) == "Dracónido", "Rule choice labels are not translated")
	require(current_scene.get_node("Gender").get_item_text(0) == "Femenino", "Gender labels are not translated")
	var filter: OptionButton = current_scene.get_node("PortraitClass")
	var wizard := -1
	for i in range(1, filter.item_count):
		if filter.get_item_metadata(i) == "Wizard": wizard = i
	require(wizard > 0 and filter.get_item_text(wizard) == "Mago", "Translated portrait filter lost its stable metadata")
	filter.select(wizard)
	filter.item_selected.emit(wizard)
	await settle()
	require(current_scene.get_node("PortraitSelect").item_count > 0, "Spanish portrait filtering has no matches")
	for i in range(current_scene.get_node("PortraitSelect").item_count):
		require(current_scene.get_node("PortraitSelect").get_item_text(i).begins_with("Mago /"), "Filter matched the wrong class")
	filter.select(0)
	filter.item_selected.emit(0)
	await capture("spanish-race")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Alineamiento", "Alignment page not translated")
	await press("Next")
	require(current_scene.get_node("Ability0").text.contains("Fuerza"), "Ability name not translated")
	require(current_scene.get_node("Targets/Rows/Class4").text.contains("FUE 13 o DES 13"), "Class prerequisites not translated from structured data")
	for attempt in range(30):
		await press("Roll")
		if current_scene.get_node("Dice3").get_parsed_text().to_int() >= 13:
			break
	require(current_scene.get_node("Dice3").get_parsed_text().to_int() >= 13, "Fixture needs an eligible Wizard")
	await capture("spanish-attributes")
	for i in range(6):
		await drag(current_scene.get_node("Dice" + str(i)).get_global_rect().get_center(), current_scene.get_node("Score" + str(i)).get_global_rect().get_center())
	await press("Next")
	var choices: ItemList = current_scene.get_node("Choices")
	var eligible := -1
	for i in range(choices.item_count):
		if choices.get_item_text(i) == "Mago" and not choices.is_item_disabled(i):
			eligible = i
			break
	require(eligible >= 0, "Fixture has no eligible Wizard")
	choices.select(eligible)
	choices.item_selected.emit(eligible)
	await settle()
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Entrenamiento", "Training page not translated")
	require(current_scene.get_node("TrainingFixed").text.contains("Trasfondo"), "Training sources not translated")
	require(current_scene.get_node("Next").disabled, "Missing class skills must block Next")
	current_scene.get_node("Training/Rows/Group0/investigation").set_pressed(true)
	current_scene.get_node("Training/Rows/Group0/medicine").set_pressed(true)
	await capture("spanish-training")
	await press("Next")
	require(current_scene.get_node("PageTitle").text == "Elegir conjuros", "Cantrip step is translated")
	current_scene.get_node("SpellChoices/Rows/fire_bolt").set_pressed(true)
	await capture("spanish-cantrips")
	# A Wizard must fill every cantrip slot and prepare a spellbook spell before Name.
	for cantrip in ["ray_of_frost", "poison_spray"]:
		current_scene.get_node("SpellChoices/Rows/" + cantrip).set_pressed(true)
		await settle()
	current_scene.get_node("SpellChoices/Rows/BookChoices/spellbook_1/magic_missile").set_pressed(true)
	await settle()
	current_scene.get_node("SpellChoices/Rows/BookChoices/prepared/magic_missile").set_pressed(true)
	await settle()
	await press("Next")
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Fighter"
	name.text_changed.emit(name.text)
	require(current_scene.get_node("PreviewName").text == "Fighter", "Player name was translated as a class label")
	name.text = "Mira {level} [b]"
	name.text_changed.emit(name.text)
	await press("Next")
	await press("Next")
	var sheet: String = current_scene.get_node("Description").text
	require(sheet.contains("Mira {level} [lb]b]"), "Name braces or BBCode were interpreted")
	require(sheet.contains("Nivel 1") and sheet.contains("Dados de Golpe") and sheet.contains("Salvación"), "Character sheet not translated")
	require(sheet.contains("Entrenamiento") and sheet.contains("Acrobacias"), "Sheet training not translated")
	await capture("spanish-sheet")
	await press("Modifiers")
	var modifiers: String = current_scene.get_node("ModifiersModal/Text").text
	require(modifiers.contains("Ajustes de características") and modifiers.contains("Origen:"), "Rule explanations not translated")
	require(modifiers.contains("Truco conocido: Descarga de fuego."), "Known cantrip missing from translated Modifiers")
	require(modifiers.contains("Libro de conjuros: Proyectil mágico (aprendido en el nivel 1 de Mago)."), "Sourced book entry missing from translated Modifiers")
	await press("ModifiersModal/Close")
	await press("SavingThrows")
	require(current_scene.get_node("SavingThrowsModal/Text").text.contains("Salvación de Fuerza"), "Saving throws not translated")
	await press("SavingThrowsModal/Close")
	TranslationServer.set_locale("en")
	require(TranslationServer.translate("Heavy Crossbow") == "Heavy Crossbow" and TranslationServer.translate("Hand Crossbow") == "Hand Crossbow", "English weapon labels expose internal IDs")
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	require(current_scene.get_node("PageTitle").text == "Race & Gender", "English reload failed")
	require(current_scene.get_node("Choices").get_item_text(0) == "Dragonborn", "English rules labels failed")
	# Expanded accented rendering finds clipping and missing glyphs without a third
	# selectable player language. Leave the feature off after the screenshot.
	TranslationServer.pseudolocalization_enabled = true
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	await capture("pseudo-race")
	TranslationServer.pseudolocalization_enabled = false
	print("Localization UI passed: Spanish choices, metadata filters, structured prerequisites, literal names, full sheet, modifiers, saves, English reload and pseudo-localization.")
	quit(0)
