extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is the party south of the Slums trolls'
# room (trolls.ogs from OPENGOLD_SLUMS_FIXTURES in opengold_expedition_tests).
# The exploration title names the area, the monster close-up says how to start
# the fight and Continue starts it, and the finished fight shows no key hints.
const SLOT = "Slums encounter fixture"
# The trolls' close-up, PIC2 record 31, holds five frames. The check gives it
# new art for the length of the run (data/art/closeups).
const TROLL_ART = "res://data/art/closeups/pic2_dax_31"
const TROLL_FRAMES = 5
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
        restore_files(); remove_troll_art(); push_error(message); quit(1); assert(ok, message)

func write_troll_art() -> void:
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(TROLL_ART))
    for n in range(TROLL_FRAMES):
        var image := Image.create(352, 422, false, Image.FORMAT_RGBA8)
        image.fill(Color(0.1, 0.6, 0.2))
        image.save_png(ProjectSettings.globalize_path(TROLL_ART.path_join("frame_%d.png" % n)))

func remove_troll_art() -> void:
    var folder := ProjectSettings.globalize_path(TROLL_ART)
    for name in DirAccess.get_files_at(folder): DirAccess.remove_absolute(folder.path_join(name))
    DirAccess.remove_absolute(folder)

# New close-up art plays in place of the original's, at its size, and the
# 3D view art setting brings back the original's at once, and the new again.
func check_close_up_art() -> void:
    var art: OptionButton = current_scene.get_node("GameMenu/Options/Interface/WallArt")
    require(town().tour_visuals()["picture"].get_width() == 352, "The trolls' close-up plays its new art")
    art.select(1); art.item_selected.emit(1); await settle(1)
    require(town().tour_visuals()["picture"].get_width() == 88, "Original 3D view art brings back the original close-up at once")
    art.select(0); art.item_selected.emit(0); await settle(1)
    require(town().tour_visuals()["picture"].get_width() == 352, "New 3D view art brings the new close-up back at once")

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
    require(first.tooltip_text.begins_with("★ ") and not second.tooltip_text.begins_with("★"), "The first member leads: " + first.tooltip_text)
    # Each row shows the roster's lines, then Armor Class and hit points, the
    # weapon in hand and the armor worn.
    var lines := second.tooltip_text.split("\n")
    require(lines.size() == 7 and lines[4].begins_with("AC ") and not lines[5].is_empty() and not lines[6].is_empty(),
        "A party row lists identity, AC and HP, weapon and armor: " + second.tooltip_text)
    var sheet: Window = current_scene.get_node("TownSheet")
    # One click chooses the member services go to; a double click opens the sheet.
    second.pressed.emit(); await settle()
    require(not sheet.visible and second.theme_type_variation == &"SelectedMemberRow"
        and first.theme_type_variation != &"SelectedMemberRow", "A click selects the member without opening the sheet")
    second.emit_signal("opened"); await settle()
    var make: Button = sheet.get_node("MakeLeader")
    require(sheet.visible and not make.disabled, "A member's sheet offers Make leader")
    # The portrait opens the sheet at the top right, beside the name, and
    # scrolls with the text: the sheet's first line is the portrait's height,
    # and the name's column comes before the portrait's (an image is a space
    # in the parsed text).
    var text: RichTextLabel = sheet.get_node("Text")
    var portrait_size := text.get_theme_constant("sheet_portrait_size", "OpenGoldMetrics")
    require(portrait_size == 300 and text.get_line_offset(1) >= portrait_size
        and text.get_parsed_text().begins_with(second.tooltip_text.split("\n")[0].trim_prefix("★ ")),
        "The sheet starts with the member's name beside the 300 px portrait on its right: " + text.get_parsed_text().left(40))
    make.pressed.emit(); await settle()
    require(make.disabled and second.tooltip_text.begins_with("★ ") and not first.tooltip_text.begins_with("★"), "Make leader moves the star: " + second.tooltip_text)
    require(roster.get_item_text(1).begins_with("★ ") and not roster.get_item_text(0).begins_with("★"), "The party roster marks the new leader")
    sheet.get_node("Close").pressed.emit(); await settle()

# Auto-equip (EQUIP-1): turned off in the Options, "Do not equip unidentified
# magic items" and each sheet's Maximize defense / Maximize damage hide; on
# again they show, and a member's choice stays theirs.
func check_auto_equip() -> void:
    var gameplay: Control = current_scene.get_node("GameMenu/Options/Gameplay")
    var auto_equip: OptionButton = gameplay.get_node("AutoEquip")
    var member: Button = town().get_node("PartyList/Rows/Member0")
    var sheet: Window = current_scene.get_node("TownSheet")
    var preference: OptionButton = sheet.get_node("GearPreference")
    auto_equip.select(1); auto_equip.item_selected.emit(1); await settle()
    require(not gameplay.get_node("UnidentifiedMagic").visible, "Off hides the unidentified magic option")
    member.emit_signal("opened"); await settle()
    require(sheet.visible and not preference.visible, "Off hides the sheet's gear preference")
    sheet.get_node("Close").pressed.emit(); await settle()
    auto_equip.select(0); auto_equip.item_selected.emit(0); await settle()
    require(gameplay.get_node("UnidentifiedMagic").visible, "On shows the unidentified magic option")
    member.emit_signal("opened"); await settle()
    require(preference.visible and preference.selected == 0, "A member starts on Maximize defense")
    preference.select(1); preference.item_selected.emit(1); await settle()
    sheet.get_node("Close").pressed.emit(); await settle()
    member.emit_signal("opened"); await settle()
    require(preference.selected == 1, "The member keeps Maximize damage")
    preference.select(0); preference.item_selected.emit(0); await settle()
    sheet.get_node("Close").pressed.emit(); await settle()

# Steps into the trolls' room and reads its story until the close-up plays.
func reach_close_up() -> void:
    await press("CampaignTown/Forward")
    for step in range(60):
        await settle(10)
        if in_combat() or (town().get_node("Continue").disabled and town().tour_visuals().has("picture")): return
        var continue_button: Button = town().get_node("Continue")
        if not continue_button.disabled: continue_button.pressed.emit()
    require(false, "The trolls' close-up plays: " + dialogue())

# Ends every party turn until the trolls win. Monster turns take real time and
# each fight has its own dice, so a long one gets minutes, not a count of frames;
# the fight plays at eight times speed, as the play-through check plays it.
func lose_the_fight() -> void:
    var combat: Control = current_scene.get_node("CampaignCombat")
    combat.set_process(true)
    Engine.time_scale = 8.0
    var deadline := Time.get_ticks_msec() + 200000
    while Time.get_ticks_msec() < deadline:
        await settle(2)
        if combat.get_node("StatusStack/Turn").text.contains(tr("Party incapacitated / defeat")):
            Engine.time_scale = 1.0
            return
        for name in ["Decline", "End"]:
            var b: Button = combat.get_node(name)
            if b.is_visible_in_tree() and not b.disabled:
                b.pressed.emit(); break
    require(false, "The trolls defeat a party that only ends its turns")

# The 3D view art setting (#274): the Slums' walls switch to the original art
# at once when the in-game menu sets Original, and back.
func check_wall_art() -> void:
    var art: OptionButton = current_scene.get_node("GameMenu/Options/Interface/WallArt")
    var fresh: PackedByteArray = town().tour_visuals()["wall_image"].get_data()
    art.select(1); art.item_selected.emit(1); await settle()
    var old: PackedByteArray = town().tour_visuals()["wall_image"].get_data()
    require(old != fresh, "Original 3D view art redraws the walls at once")
    art.select(0); art.item_selected.emit(0); await settle()
    require(town().tour_visuals()["wall_image"].get_data() == fresh, "New 3D view art comes back at once")

# The Combat art setting (#274): the trolls stand in their new art, switch to
# their original icons at once when the in-game menu sets Original, and back.
func check_combat_art() -> void:
    var combat: Node = current_scene.get_node("CampaignCombat")
    var art: OptionButton = current_scene.get_node("GameMenu/Options/Interface/CombatArt")
    require(not combat.enemy_sprite_widths().is_empty() and combat.enemy_sprite_widths().all(func(w): return w == 128), "The trolls stand in their new art: " + str(combat.enemy_sprite_widths()))
    art.select(1); art.item_selected.emit(1); await settle()
    require(combat.enemy_sprite_widths().all(func(w): return w > 0 and w < 128), "Original combat art shows the trolls' original icons at once: " + str(combat.enemy_sprite_widths()))
    art.select(0); art.item_selected.emit(0); await settle()
    require(combat.enemy_sprite_widths().all(func(w): return w == 128), "New combat art comes back at once")

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
    await check_auto_equip()
    await check_wall_art()
    write_troll_art()
    await reach_close_up()
    await check_close_up_art()
    remove_troll_art()
    for wait in range(600):
        if in_combat(): break
        await settle(1)
    require(in_combat(), "The fight opens by itself after the close-up")
    await check_combat_art()
    await lose_the_fight()
    var log: String = current_scene.get_node("CampaignCombat/LogStack/Log").get_parsed_text()
    require(not log.contains(tr("A: next action | Space: use | Z: spell slot | Enter: end turn")), "A finished fight shows no key hints")
    require(not log.begins_with("Slums encounter"), "A finished campaign fight shows no developer label")
    restore_files()
    print("Slums encounter checks passed")
    quit(0)
