extends SceneTree

# Graphical integration using the real character pool and campaign handoff.
# Requires OPENGOLD_GAME_DIR and the game's imported portrait assets.
var portraits := ProjectSettings.globalize_path("res://bin/portraits")
var hidden: Array[String] = []

func _initialize() -> void:
    call_deferred("run_checks")

func restore_portraits() -> void:
    for path in hidden:
        DirAccess.rename_absolute(path + ".hidden", path)
    hidden.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_portraits()
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4):
        await process_frame

func run_checks() -> void:
    change_scene_to_file("res://scenes/character_creation.tscn")
    await settle()
    var creation := current_scene as Control
    require(creation.has_node("PartyPanel"), "Original character assets load")
    creation.get_node("Party").pressed.emit()
    creation.get_node("PartyPanel/Pool").pressed.emit()
    var choices: ItemList = creation.get_node("PoolModal/List")
    var selected := false
    for index in range(choices.item_count):
        choices.select(index)
        choices.item_selected.emit(index)
        if creation.get_node("PoolModal/Text").text.contains("Goliath"):
            selected = true
            break
    require(selected, "Pool supplies a Goliath")
    creation.get_node("PoolModal/Add").pressed.emit()
    creation.get_node("PoolModal/Close").pressed.emit()
    # Hide the Goliath's packaged portraits (goliath-<gender>-<class>-NN.png), but
    # keep its additional head (goliath-<gender>.png), so combat must compose it.
    for file in DirAccess.get_files_at(portraits):
        if file.begins_with("goliath-") and file.get_slice(".", 0).get_slice_count("-") > 2:
            hidden.append(portraits.path_join(file))
            require(DirAccess.rename_absolute(hidden.back(), hidden.back() + ".hidden") == OK, "Hide " + file)
    require(not hidden.is_empty(), "Goliath portraits are packaged")
    creation.get_node("PartyPanel/Combat").pressed.emit()
    await settle()
    require(creation.has_node("CampaignCombat"), "Combat opens when a Goliath portrait must be composed")
    restore_portraits()
    print("Portrait fallback checks passed: composed additional-head portrait in campaign combat")
    quit(0)
