extends SceneTree

# The series' next game's party screen brings in the winning party the game
# before banked at its win (docs/SAVES.md, The winning party): the startup
# scene with --game=SEQUEL --rules=original reaches the party screen, whose
# Add from <earlier game> lists "Winning party (banked at the win)" first,
# above the earlier game's own save; the winning party offers its player
# characters and, with --npc, its permanent NPC marked "(NPC)", which joins in
# an NPC position; the ordinary save offers its player characters only. Run by
# tests/winner_bank_import.cmake with --earlier=FOLDER --bank=FILE --save=FILE
# and --button=, --title= for the earlier game's names.

const SAVE_NAME := "Earlier veterans"

var earlier := ""
var bank := ""
var save := ""
var button_text := ""
var title_text := ""
var with_npc := false

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--earlier="): earlier = arg.trim_prefix("--earlier=")
		if arg.begins_with("--bank="): bank = arg.trim_prefix("--bank=")
		if arg.begins_with("--save="): save = arg.trim_prefix("--save=")
		if arg.begins_with("--button="): button_text = arg.trim_prefix("--button=")
		if arg.begins_with("--title="): title_text = arg.trim_prefix("--title=")
		if arg == "--npc": with_npc = true
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle() -> void:
	for frame in range(8): await process_frame

func press(node: Node, path: String) -> void:
	var button: Button = node.get_node(path)
	require(button.is_visible_in_tree() and not button.disabled, "Unavailable control: " + path)
	button.pressed.emit()
	await settle()

func pick(list: ItemList, index: int) -> void:
	list.select(index)
	list.item_selected.emit(index)
	await settle()

func rows(list: ItemList) -> Array:
	var texts := []
	for i in range(list.item_count): texts.append(list.get_item_text(i))
	return texts

# The winning party and a save a player left, in the earlier game's folder.
func place_saves() -> void:
	var saves := ProjectSettings.globalize_path("user://saves")
	if earlier != "POOLRAD": saves = saves.path_join(earlier)
	DirAccess.make_dir_recursive_absolute(saves)
	require(DirAccess.copy_absolute(bank, saves.path_join("winning-party.ogs")) == OK, "Cannot place the winning party " + bank)
	var file := saves.path_join(SAVE_NAME.to_utf8_buffer().hex_encode() + ".ogs")
	require(DirAccess.copy_absolute(save, file) == OK, "Cannot place the save " + save)

func reach_creation() -> void:
	for wait in range(600):
		await process_frame
		var scene := current_scene
		if scene == null: continue
		if scene.name == "CharacterCreation": break
		var game_menu: Window = scene.get_node_or_null("GameMenu")
		if game_menu and game_menu.visible:
			game_menu.get_node("Buttons/NewParty").pressed.emit()
			await settle()
	require(current_scene != null and current_scene.name == "CharacterCreation", "The game must reach the party screen")
	await settle()

func run_checks() -> void:
	place_saves()
	change_scene_to_file("res://scenes/startup.tscn")
	await settle()
	await reach_creation()
	var screen := current_scene
	var party_button: Button = screen.get_node_or_null("Party")
	if party_button and party_button.is_visible_in_tree():
		await press(screen, "Party")
	var from_earlier: Button = screen.get_node("PartyPanel/FromPool")
	require(from_earlier.visible and from_earlier.text == button_text, "The party screen offers the earlier game's characters: " + from_earlier.text)
	await press(screen, "PartyPanel/FromPool")
	var dialog: Window = screen.get_node("FromPoolModal")
	require(dialog.visible, "The earlier game's dialog opens")
	var title: Label = dialog.get_node("Margin/Rows/Title")
	require(title.text == title_text, "The dialog names the earlier game: " + title.text)
	var saves: ItemList = dialog.get_node("Margin/Rows/Lists/SaveColumn/Saves")
	require(rows(saves) == ["Winning party (banked at the win)", SAVE_NAME], "The winning party comes first: " + str(rows(saves)))
	var members: ItemList = dialog.get_node("Margin/Rows/Lists/MemberColumn/Members")
	var status: Label = dialog.get_node("Margin/Rows/Status")
	var add: Button = dialog.get_node("Margin/Rows/Buttons/Add")

	# The ordinary save offers its player characters only.
	await pick(saves, 1)
	for row in rows(members):
		require(not row.ends_with("(NPC)"), "An ordinary save offers no NPC: " + str(rows(members)))

	await pick(saves, 0)
	var shown := rows(members)
	var expected := FileAccess.get_file_as_string(bank + ".winners").split("\n", false)
	require(not expected.is_empty() and shown.size() == expected.size(),
		"Every expected winner is offered, with no extras: " + str(shown))
	var roster: ItemList = screen.get_node("PartyPanel/Roster")
	var pcs := 0
	var npcs := 0
	for index in range(expected.size()):
		var fields := expected[index].split("\t", false)
		require(fields.size() == 2, "The source fixture names its winner and PC/NPC role")
		var npc := fields[0] == "NPC"
		pcs += 0 if npc else 1
		npcs += 1 if npc else 0
		require(shown[index].begins_with(fields[1] + " / ") and shown[index].ends_with("(NPC)") == npc,
			"The offered identity and role match the independently written source roster: " + shown[index])
		await pick(members, index)
		require(not add.disabled, "Every winning PC and permanent NPC is available: " + shown[index])
		require(status.text.begins_with("Joins in an NPC position") if npc else status.text.begins_with("Brings"),
			"Each winner uses the correct PC/NPC position: " + status.text)
		await press(dialog, "Margin/Rows/Buttons/Add")
		require(add.disabled and status.text == "Brought into the party.", "Each winner is imported once")
		require(roster.item_count == index + 1 and roster.get_item_text(index).trim_prefix("★ ") == fields[1],
			"The actual destination roster contains each imported identity")
	require(pcs == 6 and (not with_npc or npcs > 0), "All six PC positions and offered NPC positions are filled")
	if earlier == "CURSE": require(npcs == 2, "Both actual Curse companions fill the two NPC positions")
	await press(dialog, "Margin/Rows/Buttons/Close")
	require(not dialog.visible, "Close puts the dialog away")
	print("Winner bank import passed: " + earlier)
	quit(0)
