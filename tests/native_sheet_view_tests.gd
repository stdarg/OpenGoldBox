extends SceneTree

# Existing ordinary sheets, using the earned Death Knights import or a
# separately supplied historical Native promotion diagnostic. No win is forced.
const IMPORT_ROWS := {
	"knight 1": "Knight order: Sword", "knight 2": "Knight order: Crown",
	"knight 3": "Knight order: Crown", "cleric 4": "Deity: Mishakal",
	"cleric 5": "Deity: Mishakal", "magic_user 6": "Robe order: White",
}
const SOURCE_NAME := "Native sheet source"

var source := ""
var loaded := false
var promoted := false
var non_native := false
var baseline_out := ""
var baseline_in := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--sheet-source="): source = arg.trim_prefix("--sheet-source=")
		if arg == "--sheet-loaded": loaded = true
		if arg == "--sheet-promoted": promoted = true
		if arg == "--sheet-non-native": non_native = true
		if arg.begins_with("--sheet-baseline-out="): baseline_out = arg.trim_prefix("--sheet-baseline-out=")
		if arg.begins_with("--sheet-baseline-in="): baseline_in = arg.trim_prefix("--sheet-baseline-in=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle() -> void:
	for frame in range(8): await process_frame

func press(button: Button) -> void:
	require(button != null and button.is_visible_in_tree() and not button.disabled,
		"The ordinary sheet control is available: " + (str(button.get_path()) if button else "missing"))
	button.pressed.emit()
	await settle()

func pick(list: ItemList, index: int) -> void:
	list.select(index)
	list.item_selected.emit(index)
	await settle()

func place_source() -> void:
	require(FileAccess.file_exists(source), "The explicit earned source save exists: " + source)
	var directory := ProjectSettings.globalize_path("user://saves/DKK")
	DirAccess.make_dir_recursive_absolute(directory)
	var file := directory.path_join(SOURCE_NAME.to_utf8_buffer().hex_encode() + ".ogs")
	require(DirAccess.copy_absolute(source, file) == OK, "Copy the immutable source into isolated user saves")

func reach_party() -> void:
	for frame in range(600):
		await process_frame
		if current_scene == null: continue
		if current_scene.name == "CharacterCreation":
			await settle()
			return
		var menu: Window = current_scene.get_node_or_null("GameMenu")
		if menu and menu.visible:
			await press(menu.get_node("Buttons/NewParty"))
	require(false, "The real game reaches its character/party screen")

func name_of(text: String) -> String:
	for name in IMPORT_ROWS:
		if text.contains(name + "\nLevel "): return name
	require(false, "An actual imported winner heads its sheet: " + text)
	return ""

func inspect_sheet(text: String, sheets: Dictionary) -> void:
	var name := name_of(text)
	require(not sheets.has(name), "Each actual source character is inspected once: " + name)
	require(text.contains("THAC0") and text.contains("XP"), "The existing ordinary sheet remains present")
	var expected: String = IMPORT_ROWS[name]
	if promoted and name in ["knight 2", "knight 3"]:
		expected = "Knight order: Sword"
	for label in ["Knight order:", "Robe order:", "Deity:"]:
		require(text.contains(label) == (not non_native and expected.begins_with(label)),
			name + " shows only its relevant stored Native identity: expected " + expected + "\n" + text)
	if not non_native:
		require(text.contains(expected), name + " shows its current stored identity: " + expected + "\n" + text)
	sheets[name] = text

func inspect_import(sheets: Dictionary) -> void:
	var screen := current_scene
	var party: Button = screen.get_node_or_null("Party")
	if party and party.is_visible_in_tree(): await press(party)
	await press(screen.get_node("PartyPanel/FromPool"))
	var dialog: Window = screen.get_node("FromPoolModal")
	var saves: ItemList = dialog.get_node("Margin/Rows/Lists/SaveColumn/Saves")
	var chosen := -1
	for row in range(saves.item_count):
		if saves.get_item_text(row) == SOURCE_NAME: chosen = row
	require(chosen >= 0, "Add from Death Knights offers the actual earned source")
	await pick(saves, chosen)
	var members: ItemList = dialog.get_node("Margin/Rows/Lists/MemberColumn/Members")
	for name in IMPORT_ROWS:
		var index := -1
		for row in range(members.item_count):
			if members.get_item_text(row).begins_with(name + " / "): index = row
		require(index >= 0, "The source offers its real character: " + name)
		await pick(members, index)
		await press(dialog.get_node("Margin/Rows/Buttons/Add"))
		var roster: ItemList = screen.get_node("PartyPanel/Roster")
		await pick(roster, roster.item_count - 1)
		inspect_sheet(screen.get_node("PartyPanel/Sheet").get_parsed_text(), sheets)

func inspect_loaded(sheets: Dictionary) -> void:
	await settle()
	var town: Control = current_scene.get_node_or_null("CampaignTown")
	if town and town.visible:
		for slot in range(6):
			var member: Button = town.get_node("%PartyList/Rows/Member" + str(slot))
			require(member.is_visible_in_tree() and not member.disabled, "The member row is available")
			# This existing row opens its sheet on a double-click, not selection.
			var click := InputEventMouseButton.new()
			click.button_index = MOUSE_BUTTON_LEFT
			click.double_click = true
			click.pressed = true
			member.call("_gui_input", click)
			await settle()
			var sheet: Window = current_scene.get_node("TownSheet")
			require(sheet.visible, "The ordinary exploration member sheet opens")
			inspect_sheet(sheet.get_node("Text").get_parsed_text(), sheets)
			await press(sheet.get_node("Close"))
	else:
		var roster: ItemList = current_scene.get_node("PartyPanel/Roster")
		require(roster.is_visible_in_tree() and roster.item_count >= 6, "The loaded source shows its ordinary party roster")
		for row in range(6):
			await pick(roster, row)
			inspect_sheet(current_scene.get_node("PartyPanel/Sheet").get_parsed_text(), sheets)

func run_checks() -> void:
	if StartupView.game_folder("DKK").is_empty() or (not non_native and StartupView.game_folder("DQK").is_empty()):
		print("Skipped: Death Knights or Dark Queen files are not here.")
		quit(0)
		return
	if not loaded: place_source()
	root.size = Vector2i(1920, 1080)
	change_scene_to_file("res://scenes/startup.tscn")
	await reach_party()
	var sheets := {}
	if loaded:
		await inspect_loaded(sheets)
	else:
		await inspect_import(sheets)
	require(sheets.size() == 6, "Inspect all six actual source PCs")
	if not baseline_out.is_empty():
		var file := FileAccess.open(baseline_out, FileAccess.WRITE)
		require(file != null, "Write the non-Native full-text baseline")
		file.store_string(JSON.stringify(sheets))
	if not baseline_in.is_empty():
		require(FileAccess.file_exists(baseline_in), "The explicit full-text baseline exists")
		require(JSON.parse_string(FileAccess.get_file_as_string(baseline_in)) == sheets,
			"Every non-Native sheet remains byte-for-byte identical to the clean baseline")
	print("Native sheet view passed: " + ("non-Native" if non_native else "promoted" if promoted else "imported"))
	quit(0)
