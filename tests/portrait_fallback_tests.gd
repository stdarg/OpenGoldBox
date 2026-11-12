extends SceneTree

# Graphical integration using the real character pool and campaign handoff,
# with the new art folder chosen on the Options screen as a player does.
# Requires OPENGOLD_GAME_DIR, and OPENGOLD_NEW_ART_DIR naming the repository's
# new art (the check clears it so the Options screen's choice counts).
var real_art := OS.get_environment("OPENGOLD_NEW_ART_DIR")
var broken_art := ProjectSettings.globalize_path("user://broken-new-art")
var not_art := ProjectSettings.globalize_path("user://not-new-art")

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

# A new art folder whose Goliath portraits are listed but cannot be read.
func make_broken_art() -> void:
    DirAccess.make_dir_recursive_absolute(broken_art.path_join("portraits"))
    DirAccess.make_dir_recursive_absolute(not_art)
    var manifest := FileAccess.open(broken_art.path_join("new-art-pack.json"), FileAccess.WRITE)
    manifest.store_string(FileAccess.get_file_as_string(real_art.path_join("new-art-pack.json")))
    manifest.close()
    var catalog: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(real_art.path_join("portraits/portraits.json")))
    var goliaths := {}
    for file in catalog:
        if String(file).begins_with("goliath-"):
            goliaths[file] = catalog[file]
            var junk := FileAccess.open(broken_art.path_join("portraits").path_join(file), FileAccess.WRITE)
            junk.store_string("not a png")
            junk.close()
    require(not goliaths.is_empty(), "The new art has Goliath portraits")
    var listed := FileAccess.open(broken_art.path_join("portraits/portraits.json"), FileAccess.WRITE)
    listed.store_string(JSON.stringify(goliaths))
    listed.close()

# Opens the game menu's Options as Escape and Options do, and picks `folder`
# in its New art folder browser.
func choose_new_art(creation: Control, folder: String) -> void:
    var escape := InputEventKey.new()
    escape.keycode = KEY_ESCAPE
    escape.pressed = true
    Input.parse_input_event(escape)
    await settle()
    creation.get_node("GameMenu/Menu/Options").pressed.emit()
    await settle()
    var choose: Button = creation.get_node("GameMenu/Options/Interface/NewArtChoose")
    require(not choose.disabled, "The new art folder can be chosen on the party screen")
    creation.get_node("GameMenu/Options/Interface/NewArtDialog").dir_selected.emit(folder)
    await settle()
    creation.get_node("GameMenu/Options/Close").pressed.emit()
    creation.get_node("GameMenu/Menu/Resume").pressed.emit()
    await settle()

func shown_portrait(creation: Control) -> String:
    var shown: Texture2D = creation.get_node("PartyPanel/Portrait").texture
    return "none" if shown == null else shown.resource_name

func run_checks() -> void:
    require(not real_art.is_empty(), "OPENGOLD_NEW_ART_DIR names the new art")
    make_broken_art()
    OS.set_environment("OPENGOLD_NEW_ART_DIR", "")
    change_scene_to_file("res://scenes/character_creation.tscn")
    await settle()
    var creation := current_scene as Control
    require(creation.has_node("PartyPanel"), "Original character assets load")
    await choose_new_art(creation, broken_art)
    var folder: Label = creation.get_node("GameMenu/Options/Interface/NewArtFolder")
    require(folder.text == broken_art, "Options shows the chosen new art folder: " + folder.text)
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
    # A Goliath portrait that cannot load shows the original head and body,
    # composed with the Goliath's own head, on the party panel and in a fight.
    creation.get_node("PartyPanel/Roster").item_selected.emit(creation.get_node("PartyPanel/Roster").item_count - 1)
    await settle()
    require(shown_portrait(creation).begins_with("original-head-"),
            "A portrait that cannot load shows the original head and body: " + shown_portrait(creation))
    creation.call("start_check_fight")
    await settle()
    require(creation.has_node("CampaignCombat"), "Combat opens when a Goliath portrait must be composed")
    creation.get_node("CampaignCombat").queue_free()
    await settle()
    # A folder that is not the new art is refused with its reason.
    await choose_new_art(creation, not_art)
    var note: Label = creation.get_node("GameMenu/Options/Interface/NewArtNote")
    require(folder.text == broken_art and note.text.contains("new-art-pack.json"),
            "A folder that is not the new art is refused: " + folder.text + " / " + note.text)
    # The real new art shows the new portrait at once.
    await choose_new_art(creation, real_art)
    require(shown_portrait(creation).ends_with(".png"),
            "Choosing the new art folder shows its portrait at once: " + shown_portrait(creation))
    # The Options screen's Portraits: Original shows every character's original
    # portrait at once.
    var option: OptionButton = creation.get_node("GameMenu/Options/Interface/PortraitArt")
    option.select(1)
    option.item_selected.emit(1)
    await settle()
    var original_name := shown_portrait(creation)
    option.select(0)
    option.item_selected.emit(0)
    await settle()
    require(original_name.begins_with("original-head-"), "Portraits: Original shows the original portrait: " + original_name)
    print("Portrait fallback checks passed: original portraits for art that cannot load, the new art folder chosen in Options")
    quit(0)
