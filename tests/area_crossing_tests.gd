extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is the campaign saved at New Phlan's west
# gate after the four orcs (OPENGOLD_GATE_FIXTURE from opengold_expedition_tests).
# Through the game's own controls this loads it, walks west into the Slums and
# back, checking the district, square and facing after each crossing, and saves.
# opengold_expedition_tests --verify-gate-crossing checks the written save.
const FIXTURE_SLOT = "Gate crossing fixture"
const CROSSED_SLOT = "Gate crossing returned"
var fixture := ""
var output := ""
var originals := {}

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--gate-fixture="): fixture = arg.trim_prefix("--gate-fixture=")
        if arg.begins_with("--gate-output="): output = arg.trim_prefix("--gate-output=")
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

func face(direction: String) -> void:
    for turn in range(4):
        if label("Location").ends_with(direction + " view"): return
        await press("CampaignTown/Right")
    require(false, "The party faces " + direction)

# Steps forward and continues through any message the step shows, until the
# party may move again.
func step_forward() -> void:
    await press("CampaignTown/Forward")
    for prompt in range(20):
        var continue_button: Button = town().get_node("Continue")
        if continue_button.disabled: return
        await press("CampaignTown/Continue")
    require(false, "The step's messages end")

func require_at(district: String, coordinates: String, direction: String) -> void:
    require(label("Location") == district + " / " + direction + " view",
        "The party is in " + district + " facing " + direction + ": " + label("Location"))
    require(label("Coordinates").begins_with("Party (" + coordinates + ")   " + direction),
        "The party stands at (" + coordinates + "): " + label("Coordinates"))

func run_checks() -> void:
    require(not fixture.is_empty() and not output.is_empty(), "--gate-fixture and --gate-output required")
    DirAccess.make_dir_recursive_absolute(output)
    for slot in [FIXTURE_SLOT, CROSSED_SLOT]:
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
    await press("PartyPanel/ActionRow2/Explore")
    require(town().visible, "Explore shows the saved town")
    require_at("New Phlan", "0, 4", "East")

    await face("West")
    await step_forward()
    require_at("Slums", "15, 4", "West")
    require(label("MapTitle").begins_with("Slums"), "The map is the Slums': " + label("MapTitle"))

    await face("East")
    await step_forward()
    require_at("New Phlan", "0, 4", "East")
    require(label("MapTitle").begins_with("New Phlan"), "The map is the town's: " + label("MapTitle"))
    await save_as(CROSSED_SLOT, "crossed.ogs")
    restore_files()
    print("Area crossing route passed")
    quit(0)
