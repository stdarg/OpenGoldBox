extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is a created party outside the New Phlan
# jeweler at (8,10) (OPENGOLD_SHOP_FIXTURE from opengold_expedition_tests). The
# shop list must say which stock cannot be equipped before anything is bought.
const SLOT = "Shop disclosure fixture"
var fixture := ""
var originals := {}

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--shop-fixture="): fixture = arg.trim_prefix("--shop-fixture=")
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

func open_shop() -> void:
    await press("CampaignTown/Look")
    for prompt in range(40):
        if town().get_node("LeaveShop").visible: return
        var choices: ItemList = town().get_node("Choices")
        # As the native route does: accept the shopkeeper's offer, otherwise take the last choice.
        var selection := 0 if town().get_node("Dialogue").get_parsed_text().contains("SHOP") else choices.item_count - 1
        if choices.item_count: choices.select(selection)
        await press("CampaignTown/Continue")
    require(false, "The jeweler's shop opens")

func run_checks() -> void:
    require(not fixture.is_empty(), "--shop-fixture required")
    var path := slot_path(SLOT)
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for suffix in ["", ".bak"]:
        originals[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    var f := FileAccess.open(path, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
    root.gui_embed_subwindows = true
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
        await press("Party")
        await press("PartyPanel/Load"); await open_slot(SLOT)
        await press("PartyPanel/ActionRow2/Explore")
        require(label("Coordinates").contains("(8, 10)"), "The party stands outside the jeweler")
        await open_shop()
        var stock: ItemList = town().get_node("Choices")
        var marker := "/ cannot be equipped" if locale == "en" else "/ no se puede equipar"
        require(stock.item_count == 11, "The jeweler lists its eleven original items")
        for i in range(stock.item_count):
            require(stock.get_item_text(i).ends_with(marker), "Jewelry is marked before purchase: " + stock.get_item_text(i))
    restore_files()
    print("Shop disclosure checks passed")
    quit(0)
