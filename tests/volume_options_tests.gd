extends SceneTree

# The Options screen's two volumes, Sound effects and Music, shared by every
# game: each a slider from silent to the games' full loudness, saved in the
# player's settings. A fight's sounds follow the Sound effects slider at once,
# and the exploration screen's thunder and footsteps take it when they open.
# Requires OPENGOLD_GAME_DIR (the sounds are the original's).

# The loudness every game's sounds play at with a slider at its top.
const FULL_VOLUME := 0.125

func _initialize() -> void:
	call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
	if not ok:
		push_error(message)
		quit(1)
		assert(ok, message)

func settle() -> void:
	for frame in range(4):
		await process_frame

func add_fighter(creation: Control) -> void:
	creation.get_node("PartyPanel/Pool").pressed.emit()
	var choices: ItemList = creation.get_node("PoolModal/List")
	for index in range(choices.item_count):
		if choices.get_item_text(index).begins_with("Fighter / "):
			choices.select(index)
			choices.item_selected.emit(index)
			creation.get_node("PoolModal/Add").pressed.emit()
			creation.get_node("PoolModal/Close").pressed.emit()
			return
	require(false, "The character pool must include a Fighter")

func open_options(creation: Control) -> Node:
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	Input.parse_input_event(escape)
	await settle()
	var menu: Node = creation.get_node("GameMenu")
	require(menu.visible, "Escape opens the in-game menu")
	menu.get_node("Menu/Options").pressed.emit()
	await settle()
	return menu

func close_options(menu: Node) -> void:
	menu.get_node("Options/Close").pressed.emit()
	menu.get_node("Menu/Resume").pressed.emit()
	await settle()

func slider(menu: Node, name: String) -> HSlider:
	var found := menu.get_node_or_null("Options/Interface/Volume/" + name) as HSlider
	require(found != null, "Options must have the " + name + " slider")
	return found

func require_volume(players: Array, expected: float, when: String) -> void:
	for player: AudioStreamPlayer in players:
		require(is_equal_approx(player.volume_linear, expected),
			player.name + " plays at " + str(expected) + " " + when + ", not " + str(player.volume_linear))

func saved_volume(kind: String) -> Variant:
	var config := ConfigFile.new()
	require(config.load(OS.get_environment("OPENGOLD_SETTINGS_FILE")) == OK, "The settings must be readable")
	return config.get_value("sound", kind, null)

func run_checks() -> void:
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle()
	var creation := current_scene as Control
	require(creation.has_node("PartyPanel"), "Original character assets must load")
	creation.get_node("Party").pressed.emit()
	add_fighter(creation)
	creation.call("start_check_fight")
	await settle()
	var combat := creation.get_node_or_null("CampaignCombat")
	require(combat != null, "The check fight must start")
	var fight_sounds := ["AttackAudio", "EffectAudio", "DeathAudio"].map(func(n): return combat.get_node(n))
	require_volume(fight_sounds, FULL_VOLUME, "by default")

	# Both sliders stand at their top until moved, a percentage beside each.
	var menu: Node = await open_options(creation)
	for name in ["SoundEffects", "Music"]:
		var shown := slider(menu, name)
		require(shown.min_value == 0 and shown.max_value == 100 and shown.value == 100,
			name + " goes from 0 to 100% and starts at 100%")
		require(menu.get_node("Options/Interface/Volume/" + name + "Percent").text == "100%",
			name + " shows its percentage")

	# Moving Sound effects saves it and changes the fight's sounds at once;
	# Music is saved apart from it.
	slider(menu, "SoundEffects").value = 50
	slider(menu, "Music").value = 0
	await settle()
	require(saved_volume("effects") == 50 and saved_volume("music") == 0,
		"The volumes must be saved: effects " + str(saved_volume("effects")) + ", music " + str(saved_volume("music")))
	require(menu.get_node("Options/Interface/Volume/SoundEffectsPercent").text == "50%",
		"Sound effects shows its new percentage")
	require_volume(fight_sounds, FULL_VOLUME / 2, "at 50% Sound effects")
	slider(menu, "SoundEffects").value = 0
	await settle()
	require_volume(fight_sounds, 0.0, "at 0% Sound effects")
	slider(menu, "SoundEffects").value = 50
	await close_options(menu)

	# Options shows what was saved when it opens again.
	menu = await open_options(creation)
	require(slider(menu, "SoundEffects").value == 50 and slider(menu, "Music").value == 0,
		"Options must show the saved volumes")
	await close_options(menu)

	# The exploration screen's sounds open at the saved Sound effects volume.
	change_scene_to_file("res://scenes/rolf_tour.tscn")
	await settle()
	var town := current_scene
	require(town != null and town.has_node("OriginalSound"), "The exploration screen must open")
	require_volume([town.get_node("OriginalSound")], FULL_VOLUME / 2, "on the exploration screen")
	require_volume([town.get_node("Footstep")], db_to_linear(-12.0) / 2, "on the exploration screen")
	print("Volume options passed: two sliders saved, a fight's sounds follow at once, exploration's on opening.")
	quit(0)
