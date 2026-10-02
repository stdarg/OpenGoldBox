extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is the campaign saved at Ohlo's door with
# the potion (OPENGOLD_OHLO_FIXTURE from opengold_expedition_tests). Through the
# game's own controls this loads it, hands in the potion, saves, loads that save,
# revisits Ohlo and saves again. opengold_expedition_tests --verify-hand-in checks
# both written saves.
const FIXTURE_SLOT = "Ohlo route fixture"
const HANDED_IN_SLOT = "Ohlo route handed in"
const REVISITED_SLOT = "Ohlo route revisited"
var fixture := ""
var output := ""
var originals := {}

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--ohlo-fixture="): fixture = arg.trim_prefix("--ohlo-fixture=")
        if arg.begins_with("--ohlo-output="): output = arg.trim_prefix("--ohlo-output=")
    call_deferred("run_checks")

func slot_path(slot: String) -> String:
    return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var f := FileAccess.open(path, FileAccess.WRITE); f.store_buffer(originals[path]); f.close()
    originals.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files(); push_error(message); quit(1); assert(ok, message)

func settle() -> void:
    for frame in range(5): await process_frame

func press(path: String) -> void:
    var b: Button = current_scene.get_node(path)
    require(not b.disabled, "Disabled control: " + path)
    b.pressed.emit(); await settle()

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func label(name: String) -> String:
    return town().get_node(name).text

func open_slot(slot: String) -> void:
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    var index := -1
    for i in range(list.item_count):
        if list.get_item_text(i) == slot: index = i
    require(index >= 0, "Save slot listed: " + slot)
    list.select(index); list.item_selected.emit(index)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    require(not current_scene.get_node("SaveSlots").visible, "Save loaded: " + slot)

# Saves through the town's Save game button and copies the file for the native check.
func save_as(slot: String, copy: String) -> void:
    await press("CampaignTown/SaveGame")
    var saves: Window = current_scene.get_node("SaveSlots")
    require(saves.visible, "Save game opens the save slots")
    saves.get_node("Name").text = slot
    await press("SaveSlots/Action")
    require(not saves.visible, "The campaign is saved: " + slot)
    var bytes := FileAccess.get_file_as_bytes(slot_path(slot))
    require(not bytes.is_empty(), "The save file exists: " + slot)
    var f := FileAccess.open(output.path_join(copy), FileAccess.WRITE); f.store_buffer(bytes); f.close()

func explore() -> void:
    await press("PartyPanel/Explore")
    require(town().visible, "Explore shows the saved district")
    require(label("Location").begins_with("Slums / ") and label("Speaker") == "Slums",
        "The loaded campaign is in the Slums: " + label("Location"))
    require(label("Coordinates").begins_with("Party (14, 10)"),
        "The loaded party stands at Ohlo's door: " + label("Coordinates"))

func face_west() -> void:
    for turn in range(4):
        if label("Location").ends_with("West view"): return
        await press("CampaignTown/Right")
    require(false, "The party faces west")

# Steps west into Ohlo's room, bashing his relocked door, and answers each prompt
# with the wanted choice when offered. A failed Bash leaves the party where it
# stood; the visit ends once the party has moved or turned. Returns every choice
# the game showed.
func visit_ohlo(wanted: String) -> Array:
    await face_west()
    var outside := label("Coordinates")
    var shown := []
    for prompt in range(80):
        var continue_button: Button = town().get_node("Continue")
        if continue_button.disabled:
            if label("Coordinates") != outside: return shown
            await press("CampaignTown/Forward")
            continue
        var choices: ItemList = town().get_node("Choices")
        var selection := 0
        for i in range(choices.item_count):
            var text := choices.get_item_text(i)
            shown.append(text)
            if text == "Bash" or text == wanted: selection = i
        if choices.item_count: choices.select(selection)
        await press("CampaignTown/Continue")
    require(false, "Ohlo's room event finished")
    return shown

func run_checks() -> void:
    require(not fixture.is_empty() and not output.is_empty(), "--ohlo-fixture and --ohlo-output required")
    DirAccess.make_dir_recursive_absolute(output)
    for slot in [FIXTURE_SLOT, HANDED_IN_SLOT, REVISITED_SLOT]:
        var path := slot_path(slot)
        DirAccess.make_dir_recursive_absolute(path.get_base_dir())
        for suffix in ["", ".bak"]:
            originals[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
            if FileAccess.file_exists(path + suffix): DirAccess.remove_absolute(path + suffix)
    var f := FileAccess.open(slot_path(FIXTURE_SLOT), FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
    root.gui_embed_subwindows = true
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
    await press("Party")
    await press("PartyPanel/Load"); await open_slot(FIXTURE_SLOT)
    await explore()

    var shown := await visit_ohlo("GIVE")
    require(shown.has("GIVE"), "Ohlo asks for the potion")
    await save_as(HANDED_IN_SLOT, "handed-in.ogs")

    await press("CampaignTown/LoadGame"); await open_slot(HANDED_IN_SLOT)
    await explore()
    shown = await visit_ohlo("GIVE")
    require(not shown.has("GIVE"), "After the reload Ohlo does not ask for the potion again")
    await save_as(REVISITED_SLOT, "revisited.ogs")
    restore_files()
    print("Ohlo save route passed")
    quit(0)
