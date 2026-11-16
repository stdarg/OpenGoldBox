extends SceneTree

# Modify Character on the real screens under the original rules
# (docs/ORIGINAL-RULES.md, Modify Character): a character made page by page
# joins the party, and the party screen's Modify character changes its scores
# and Hit Points by the keyboard and the mouse, within its limits; Escape
# leaves it as it was, Enter keeps the changes. From Curse on the name changes
# too, Treasures' magic-user chooses its spell book again, and Unlimited
# Adventures also offers Modify on the roll.
#
# Pool of Radiance opens the creation scene with --original-rules. Another
# game starts from the startup scene with --game=FOLDER and --rules=original
# (Unlimited Adventures with --ua-design=FOLDER too), run by
# tests/settings_check.cmake; --rules=srd checks that the SRD's party screen
# offers no Modify. --shots=DIR saves the screens in a windowed run.

var folder := ""
var srd := false
var ua_design := ""
var shots := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--game="): folder = arg.trim_prefix("--game=")
		if arg == "--rules=srd": srd = true
		if arg.begins_with("--ua-design="): ua_design = arg.trim_prefix("--ua-design=")
		if arg.begins_with("--shots="): shots = arg.trim_prefix("--shots=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle(frames := 6) -> void:
	for frame in range(frames): await process_frame

func shot(name: String) -> void:
	if shots.is_empty(): return
	DirAccess.make_dir_recursive_absolute(shots)
	await settle()
	RenderingServer.force_draw(false)
	await process_frame
	root.get_texture().get_image().save_png(shots.path_join(name + ".png"))

func press(node: Node, path: String) -> void:
	var button: Button = node.get_node(path)
	var status: Label = current_scene.get_node_or_null("Status")
	var page: Label = current_scene.get_node_or_null("PageTitle")
	require(button.is_visible_in_tree() and not button.disabled, "Unavailable control: " + path +
		(" on " + page.text if page else "") + (" (" + status.text + ")" if status else ""))
	button.pressed.emit()
	await settle()

func key(code: Key) -> void:
	for down in [true, false]:
		var event := InputEventKey.new()
		event.keycode = code
		event.pressed = down
		Input.parse_input_event(event)
		await process_frame
	await settle()

func choose(text: String) -> void:
	var list: ItemList = current_scene.get_node("Choices")
	for i in range(list.item_count):
		if list.get_item_text(i) == text or list.get_item_text(i) == TranslationServer.translate(text):
			require(not list.is_item_disabled(i), "Disabled choice: " + text)
			list.select(i)
			list.item_selected.emit(i)
			await settle()
			return
	require(false, "Missing choice: " + text)

func game_files_here() -> bool:
	if folder.is_empty() or folder == "POOLRAD": return true
	var games := OS.get_environment("OPENGOLD_GOLDBOX_DIR")
	var game_file := "CKIT.EXE" if folder == "UA" else "CBODY.DAX"
	if folder == "UA" and not DirAccess.dir_exists_absolute(ua_design): return false
	return not games.is_empty() and FileAccess.file_exists(games.path_join(folder).path_join(game_file))

# The startup scene with --game and --rules goes to the creation screen;
# Unlimited Adventures asks for its design first.
func reach_creation() -> void:
	for wait in range(600):
		await process_frame
		var scene := current_scene
		if scene == null: continue
		if scene.name == "CharacterCreation": break
		var dialog: Window = scene.get_node_or_null("PathDialog")
		if dialog and dialog.visible and not ua_design.is_empty():
			dialog.get_node("Path").text = ua_design
			dialog.get_node("Continue").pressed.emit()
			await settle()
			continue
		var game_menu: Window = scene.get_node_or_null("GameMenu")
		if game_menu and game_menu.visible:
			game_menu.get_node("Buttons/NewParty").pressed.emit()
			await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", folder + " must reach the creation screen")
	await settle()

func modify_dialog() -> Window:
	return current_scene.get_node("ModifyModal")

func value(row: int) -> String:
	return (modify_dialog().get_node("Margin/Rows/Grid/Value" + str(row)) as Label).text

func row_text(row: int) -> String:
	return (modify_dialog().get_node("Margin/Rows/Grid/Row" + str(row)) as Button).text

func button(part: String, row: int) -> Button:
	return modify_dialog().get_node("Margin/Rows/Grid/" + part + str(row))

# Ticks the first boxes of each spell group until its count is met, as a
# player fills Treasures' page of choices.
# A page may build its boxes again after each tick, so each is found anew.
func fill_choices(container: Node) -> void:
	for tick in range(100):
		var next: CheckBox = null
		for box in container.find_children("*", "CheckBox", true, false):
			var check := box as CheckBox
			if check.is_visible_in_tree() and not check.disabled and not check.button_pressed:
				next = check
				break
		if next == null: return
		next.set_pressed(true)
		await settle(2)

# Name, race, class, alignment, the roll (and on the kit's roll its Modify),
# any page of choices, the look and the sheet, then Add to party.
func make_character(class_name_shown: String) -> void:
	var name: LineEdit = current_scene.get_node("Name")
	name.text = "Tordek"
	name.text_changed.emit(name.text)
	await settle()
	await choose("Human")
	await press(current_scene, "Next")
	await choose(class_name_shown)
	await press(current_scene, "Next")
	await choose("Neutral Good" if class_name_shown != "Paladin" else "Lawful Good")
	await press(current_scene, "Next")
	await press(current_scene, "Roll")
	var on_roll: Button = current_scene.get_node("ModifyRoll")
	require(on_roll.visible == (folder == "UA"), "Only the kit's roll offers Modify")
	if folder == "UA":
		await modify_on_the_roll()
	for page in range(6):
		if current_scene.get_node("AddParty").is_visible_in_tree(): break
		if current_scene.get_node("Training").is_visible_in_tree():
			await fill_choices(current_scene.get_node("Training"))
		await press(current_scene, "Next")
	await press(current_scene, "AddParty")
	require(current_scene.get_node("PartyPanel/Roster").item_count == 1, "The character joins the party")

# The kit's roll: Modify there shows no name, and Keep changes the roll.
func modify_on_the_roll() -> void:
	var dexterity: Button = current_scene.get_node("Score1")
	var before := dexterity.text
	await shot("roll-with-modify")
	await press(current_scene, "ModifyRoll")
	var dialog := modify_dialog()
	require(dialog.visible and not dialog.get_node("Margin/Rows/NameRow").visible, "The roll's Modify shows no name")
	await shot("modify-on-roll")
	if not button("Lower", 1).disabled:
		await press(dialog, "Margin/Rows/Grid/Lower1")
		await press(dialog, "Margin/Rows/Buttons/Keep")
		require(not dialog.visible and dexterity.text == str(int(before) - 1),
			"Keep lowers the roll's Dexterity: " + before + " to " + dexterity.text)
	else:
		await press(dialog, "Margin/Rows/Buttons/Exit")

func run_checks() -> void:
	if not game_files_here():
		print("Skipped: " + folder + "'s files are not here.")
		quit(0)
		return
	if folder.is_empty():
		change_scene_to_file("res://scenes/character_creation.tscn")
		await settle()
	else:
		change_scene_to_file("res://scenes/startup.tscn")
		await settle()
		await reach_creation()
	root.size = Vector2i(1120, 800)
	await settle()
	var screen := current_scene
	var modify: Button = screen.get_node("PartyPanel/ActionRow1/Modify")
	if srd:
		await press(screen, "Party")
		require(not modify.visible, "The SRD's party screen offers no Modify")
		print("Modify view passed: the SRD offers none.")
		quit(0)
		return
	await make_character("Magic-User" if folder == "TREASURE" else "Fighter")
	require(modify.visible and not modify.disabled, "A new member can be modified")
	await press(screen, "PartyPanel/ActionRow1/Modify")
	var dialog := modify_dialog()
	require(dialog.visible, "Modify character opens its window")
	var renames := not folder.is_empty() and folder != "POOLRAD"
	require(dialog.get_node("Margin/Rows/NameRow").visible == renames, "The name changes from Curse on")
	require((dialog.get_node("Margin/Rows/Title") as Label).text == TranslationServer.translate("Modify {name}").format({"name": "Tordek"}), "The window names the character")
	require(row_text(0).begins_with("> " + TranslationServer.translate("Strength")), "Strength's row is chosen first: " + row_text(0))
	var treasures := folder == "TREASURE"
	require(dialog.get_node("Margin/Rows/Spells").visible == treasures, "Only Treasures' magic-user chooses its spell book")
	await shot("modify-" + (folder if not folder.is_empty() else "POOLRAD"))
	var strength := value(0)
	var hit_points := int(value(6))

	# The keyboard: Down to the Hit Points, Right to their highest, Left a step.
	for step in range(6): await key(KEY_DOWN)
	require(row_text(6).begins_with("> " + TranslationServer.translate("Hit Points")) and not row_text(0).begins_with(">"), "Down moves to the Hit Points")
	for step in range(200):
		if button("Raise", 6).disabled: break
		await key(KEY_RIGHT)
	var highest := int(value(6))
	require(button("Raise", 6).disabled and highest >= hit_points, "Right raises the Hit Points to their highest")
	await key(KEY_LEFT)
	require(int(value(6)) == highest - 1, "Left lowers them a point")
	await key(KEY_UP)
	require(row_text(5).begins_with("> " + TranslationServer.translate("Charisma")), "Up moves back a row")

	# Escape leaves the character as it was.
	await key(KEY_ESCAPE)
	require(not dialog.visible, "Escape closes the window")
	await press(screen, "PartyPanel/ActionRow1/Modify")
	require(value(0) == strength and int(value(6)) == hit_points, "Escape keeps nothing")

	# The mouse: Strength down to the fighter's 9 (a magic-user's race's 3).
	for step in range(220):
		if button("Lower", 0).disabled: break
		await press(dialog, "Margin/Rows/Grid/Lower0")
	var lowest := "3" if treasures else "9"
	require(value(0) == lowest, "Strength stops at its class's minimum: " + value(0))
	if renames:
		var field: LineEdit = dialog.get_node("Margin/Rows/NameRow/Name")
		field.text = ""
		field.text_changed.emit("")
		await settle()
		require((dialog.get_node("Margin/Rows/Buttons/Keep") as Button).disabled, "A name is needed to keep")
		field.text = "Tarn"
		field.text_changed.emit("Tarn")
		await settle()
	if treasures:
		var heading: Label = dialog.get_node("Margin/Rows/Spells/List/Heading0")
		require(heading.text.begins_with(TranslationServer.translate("Magic-user spells of level 1") + " ("), "The spell book's first level: " + heading.text)
		var first: CheckBox = null
		for box in dialog.get_node("Margin/Rows/Spells/List").get_children():
			if box is CheckBox and box.button_pressed:
				first = box
				break
		require(first != null, "The spells chosen at creation are ticked")
		first.button_pressed = false
		await settle()
		require((dialog.get_node("Margin/Rows/Buttons/Keep") as Button).disabled, "Keep waits for the spell book")
		await fill_choices(dialog.get_node("Margin/Rows/Spells/List"))
	await shot("modify-changed-" + (folder if not folder.is_empty() else "POOLRAD"))
	await key(KEY_ENTER)
	require(not dialog.visible, "Enter keeps the changes: " + (dialog.get_node("Margin/Rows/Status") as Label).text)
	var roster: ItemList = screen.get_node("PartyPanel/Roster")
	require(roster.get_item_text(0).contains("Tarn" if renames else "Tordek"), "The roster shows the name: " + roster.get_item_text(0))
	await press(screen, "PartyPanel/ActionRow1/Modify")
	require(value(0) == lowest, "The member kept its Strength")
	await press(dialog, "Margin/Rows/Buttons/Exit")
	print("Modify view passed: " + (folder if not folder.is_empty() else "POOLRAD") + ".")
	quit(0)
