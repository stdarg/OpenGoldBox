extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is the party south of the Slums trolls'
# room (trolls.ogs from OPENGOLD_SLUMS_FIXTURES in opengold_expedition_tests).
# The exploration title names the area, the monster close-up says how to start
# the fight and Continue starts it, and the finished fight shows no key hints.
const SLOT = "Slums encounter fixture"
var fixture := ""
var originals := {}

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--slums-fixture="): fixture = arg.trim_prefix("--slums-fixture=")
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

func settle(frames := 5) -> void:
    for frame in range(frames): await process_frame

func press(path: String) -> void:
    var b: Button = current_scene.get_node(path)
    require(not b.disabled, "Disabled control: " + path)
    b.pressed.emit(); await settle()

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func dialogue() -> String:
    return (town().get_node("Dialogue") as RichTextLabel).get_parsed_text()

func in_combat() -> bool:
    return current_scene.has_node("CampaignCombat") and current_scene.get_node("CampaignCombat").is_visible_in_tree()

func open_slot(slot: String) -> void:
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    var index := -1
    for i in range(list.item_count):
        if list.get_item_text(i) == slot: index = i
    require(index >= 0, "Save slot listed: " + slot)
    list.select(index); list.item_selected.emit(index)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    require(not current_scene.get_node("SaveSlots").visible, "Save loaded: " + slot)

# The first member leads until another is made leader from its sheet; the
# exploration party list and the party roster both mark the leader with a star.
func check_leader() -> void:
    var first: Button = town().get_node("PartyList/Rows/Member0")
    var second: Button = town().get_node("PartyList/Rows/Member1")
    var roster: ItemList = current_scene.get_node("PartyPanel/Roster")
    require(first.text.begins_with("★ ") and not second.text.begins_with("★"), "The first member leads: " + first.text)
    second.pressed.emit(); await settle()
    var sheet: Window = current_scene.get_node("TownSheet")
    var make: Button = sheet.get_node("MakeLeader")
    require(sheet.visible and not make.disabled, "A member's sheet offers Make leader")
    make.pressed.emit(); await settle()
    require(make.disabled and second.text.begins_with("★ ") and not first.text.begins_with("★"), "Make leader moves the star: " + second.text)
    require(roster.get_item_text(1).begins_with("★ ") and not roster.get_item_text(0).begins_with("★"), "The party roster marks the new leader")
    sheet.get_node("Close").pressed.emit(); await settle()

# Steps into the trolls' room and reads its story until the close-up shows.
func reach_close_up() -> void:
    await press("CampaignTown/Forward")
    for step in range(60):
        await settle(10)
        if dialogue() == tr("Press any key to fight."): return
        var continue_button: Button = town().get_node("Continue")
        if not continue_button.disabled: continue_button.pressed.emit()
    require(false, "The trolls' close-up asks for a key: " + dialogue())

# Ends every party turn until the trolls win.
func lose_the_fight() -> void:
    var combat: Control = current_scene.get_node("CampaignCombat")
    combat.set_process(true)
    for step in range(3000):
        await settle(2)
        if combat.get_node("Turn").text.contains(tr("Party incapacitated / defeat")): return
        for name in ["Decline", "End"]:
            var b: Button = combat.get_node(name)
            if b.is_visible_in_tree() and not b.disabled:
                b.pressed.emit(); break
    require(false, "The trolls defeat a party that only ends its turns")

func run_checks() -> void:
    require(not fixture.is_empty(), "--slums-fixture required")
    var path := slot_path(SLOT)
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for suffix in ["", ".bak"]:
        originals[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    var f := FileAccess.open(path, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
    root.gui_embed_subwindows = true
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
    await press("Party")
    await press("PartyPanel/Load"); await open_slot(SLOT)
    await press("PartyPanel/ActionRow2/Explore")
    require(town().get_node("Title").text == "OPENGOLDBOX  /  Slums", "The title names the area: " + town().get_node("Title").text)
    require(not dialogue().contains("Rolf"), "Exploring the Slums does not follow Rolf: " + dialogue())
    await check_leader()
    await reach_close_up()
    var continue_button: Button = town().get_node("Continue")
    require(not continue_button.disabled, "Continue can start the fight from the close-up")
    continue_button.pressed.emit(); await settle(30)
    require(in_combat(), "Continue leaves the close-up for combat")
    await lose_the_fight()
    var log: String = current_scene.get_node("CampaignCombat/Log").get_parsed_text()
    require(not log.contains(tr("A: next action | Space: use | Z: spell slot | Enter: end turn")), "A finished fight shows no key hints")
    require(not log.begins_with("Slums encounter"), "A finished campaign fight shows no developer label")
    restore_files()
    print("Slums encounter checks passed")
    quit(0)
