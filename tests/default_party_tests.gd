extends SceneTree

# Play with Default Party: a game begun with its default party
# (data/default_parties) under either rule set goes straight to exploring
# with the file's six members, and the game's opening plays (Curse's
# awakening, Secret's vision). Run by tests/settings_check.cmake with
# --game=<folder>, --rules=original or --rules=srd and --default-party;
# --opening=TEXT is what the opening shows. --save-as=NAME then saves the
# game, and a second run with --load-save=<that save>, as Continue starts it,
# must reach exploring with the same six.

var folder := ""
var opening := ""
var save_name := ""
var loading := false

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--game="): folder = arg.trim_prefix("--game=")
		if arg.begins_with("--opening="): opening = arg.trim_prefix("--opening=")
		if arg.begins_with("--save-as="): save_name = arg.trim_prefix("--save-as=")
		if arg.begins_with("--load-save="): loading = true
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)

func settle() -> void:
	for frame in range(8): await process_frame

func town() -> Control:
	return current_scene.get_node_or_null("CampaignTown") if current_scene else null

func party_names() -> Array:
	var names := []
	for slot in range(8):
		var row: Button = town().get_node_or_null("%PartyList/Rows/Member" + str(slot))
		if row and row.visible: names.append(row.tooltip_text.split("\n")[0].trim_prefix("★ ").strip_edges())
	return names

func game_files_here() -> bool:
	if folder == "POOLRAD": return true
	var games := OS.get_environment("OPENGOLD_GOLDBOX_DIR")
	return not games.is_empty() and FileAccess.file_exists(games.path_join(folder).path_join("ITEMS"))

# The names the game's default party file gives its six.
func file_names() -> Array:
	var text := FileAccess.get_file_as_string("res://data/default_parties/" + folder + ".json")
	var names := []
	for member in JSON.parse_string(text)["members"]: names.append(member["name"])
	return names

# Waits for the exploration screen the game began on.
func reach_exploring() -> void:
	for wait in range(600):
		await process_frame
		if town() and town().visible: break
	require(town() != null and town().visible, folder + ": the game must begin exploring, not on the creation screen: " +
		(current_scene.get_node("Status").text if current_scene and current_scene.has_node("Status") else "no scene"))

# Plays the opening on until it shows `opening`, as the player's Continue would.
func see_opening() -> String:
	var shown := ""
	for wait in range(600):
		await process_frame
		shown = town().find_child("Dialogue", true, false).get_parsed_text() + "\n" + town().get_node("%Location").text
		if shown.contains(opening): return shown
		var continue_button: Button = town().get_node("%Continue")
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
	return shown

# Saves from exploring once the opening's text allows it (the save screen
# does not open during an event), continuing the text until then.
func save_as(name: String) -> void:
	var dialog: Window = current_scene.get_node("SaveSlots")
	for attempt in range(60):
		town().emit_signal("save_requested", true)
		await settle()
		if dialog.visible: break
		var continue_button: Button = town().get_node("%Continue")
		if continue_button.visible and not continue_button.disabled:
			continue_button.pressed.emit()
			await settle()
	require(dialog.visible, "The save screen must open from exploring")
	var field: LineEdit = dialog.get_node("Name")
	field.text = name
	field.text_changed.emit(name)
	dialog.get_node("Action").pressed.emit()
	await settle()
	require(not dialog.visible, "Saving must close the save screen: " + dialog.get_node("Status").text)

func run_checks() -> void:
	if not game_files_here():
		print("Skipped: " + folder + "'s files are not here.")
		quit(0)
		return
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await reach_exploring()
	var names := party_names()
	require(names == file_names(), folder + ": the default party's six members explore: " + str(names))
	if loading:
		print("Default party passed: " + folder + " continued from its save with " + str(names) + ".")
		quit(0)
		return
	var shown := await see_opening()
	require(shown.contains(opening), folder + "'s opening must play: " + shown)
	if not save_name.is_empty():
		await save_as(save_name)
	print("Default party passed: " + folder + " began with " + str(names) + ".")
	quit(0)
