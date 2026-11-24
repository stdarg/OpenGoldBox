extends SceneTree
# The running game banks its winning party at an actual win (docs/SAVES.md,
# The winning party), through the exploration screen's refresh, the host and
# opengold::WinnerCapture. Matrix Cubed's earned route saves its team one
# step before the square that wins the game, facing it
# (opengold_matrix_cubed_earned_ending, OPENGOLD_MATRIX_LAST_STEP_SAVE). The
# check loads that save, steps forward and reads the closing scene on to the
# win; Matrix Cubed wins while idle, so the bank holds its world at once.
# The bank must be in the game's save folder and Load game must list it as
# "Winning party". --bank-out=FILE copies it there for
# opengold_winner_bank_matrix_capture_reload, which reloads it byte for byte.
# Run by tests/settings_check.cmake with --game, --rules=original and
# --load-save.

const FOLDER := "buck-rogers-matrix-cubed"

var bank_out := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank-out="): bank_out = arg.trim_prefix("--bank-out=")
	call_deferred("run_checks")

func require(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		quit(1)
		assert(condition, message)

func settle(frames := 8) -> void:
	for frame in range(frames): await process_frame

func node(path: String) -> Node:
	return current_scene.get_node_or_null(path) if current_scene else null

func town() -> Control:
	return node("CampaignTown")

func press(button: Button) -> bool:
	if button == null or not button.is_visible_in_tree() or button.disabled:
		return false
	button.pressed.emit()
	await settle()
	return true

func run_checks() -> void:
	if StartupView.game_folder(FOLDER).is_empty():
		print("Skipped: Matrix Cubed's files are not here.")
		quit(0)
		return
	var bank := ProjectSettings.globalize_path("user://saves").path_join(FOLDER).path_join("winning-party.ogs")
	root.size = Vector2i(1920, 1080)
	change_scene_to_file("res://scenes/startup.tscn")
	for wait in range(600):
		await process_frame
		if town() and town().visible: break
	require(town() != null and town().visible, "The save loads onto the exploration screen")
	await settle()
	require(not FileAccess.file_exists(bank), "No winning party is banked before the win")
	await press(town().get_node("%Forward"))
	# The closing scene's pages pause between them; Continue takes each one.
	var start := Time.get_ticks_msec()
	while Time.get_ticks_msec() - start < 120000:
		if FileAccess.file_exists(bank): break
		var go_on: Button = town().get_node("%Continue")
		if go_on.is_visible_in_tree() and not go_on.disabled:
			await press(go_on)
		else:
			await process_frame
	require(FileAccess.file_exists(bank), "The win banks the winning party in the game's save folder")
	await settle()
	# Load game lists it first, under its own name.
	town().emit_signal("save_requested", false)
	await settle()
	var slots: Window = current_scene.get_node("SaveSlots")
	require(slots.visible, "Load game opens after the win")
	var list: ItemList = slots.find_child("Slots", true, false)
	require(list.item_count > 0 and list.get_item_text(0) == "Winning party",
		"Load game lists the bank as Winning party: " + (list.get_item_text(0) if list.item_count > 0 else "none"))
	if not bank_out.is_empty():
		require(DirAccess.copy_absolute(bank, bank_out) == OK, "Cannot copy the bank to " + bank_out)
	print("Winner bank capture passed: " + FOLDER)
	quit(0)
