extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is the party east of the Slums' four
# orcs (orcs.ogs from OPENGOLD_SLUMS_FIXTURES in opengold_expedition_tests). An
# orc carries a sword and shield and a bow, each with its own new art (#274): it
# stands in its staged set and holds the set it attacks with, its bow to shoot.
const SLOT = "Weapon set fixture"
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

# Steps into the orcs and fights them.
func start_the_fight() -> void:
    for step in range(3):
        await press("CampaignTown/Forward")
        if not (town().get_node("Continue") as Button).disabled or in_combat(): break
    for step in range(20):
        if in_combat(): return
        var choices: ItemList = town().get_node("Choices")
        for i in range(choices.item_count):
            if choices.get_item_text(i).to_upper() in ["FIGHT", "BASH"]: choices.select(i)
        var continue_button: Button = town().get_node("Continue")
        if continue_button.disabled: await settle(30)
        else:
            continue_button.pressed.emit(); await settle(20)
    require(in_combat(), "The orcs' fight starts")

# Ends the party's turns until an orc has shot, checking the set each
# enemy shows along the way.
func check_weapon_sets() -> void:
    var combat: Control = current_scene.get_node("CampaignCombat")
    var sets: Dictionary = combat.enemy_weapon_sets()
    require(not sets.is_empty() and sets.values().all(func(s): return s == "staged"), "Every orc stands in its staged set before it attacks: " + str(sets))
    combat.set_process(true)
    for step in range(3000):
        await settle(2)
        sets = combat.enemy_weapon_sets()
        if sets.values().has("ranged"): return
        require(not combat.get_node("StatusStack/Turn").text.contains(tr("Party incapacitated / defeat")), "An orc shoots before the party falls: " + str(sets))
        for name in ["Decline", "End"]:
            var b: Button = combat.get_node(name)
            if b.is_visible_in_tree() and not b.disabled:
                b.pressed.emit(); break
    require(false, "An orc holds its bow to shoot: " + str(sets))

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
    if not town().visible: await press("PartyPanel/ActionRow2/Explore")
    await start_the_fight()
    await check_weapon_sets()
    restore_files()
    print("Weapon set checks passed")
    quit(0)
