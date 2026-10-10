extends SceneTree

# Play-test of Kuto's Well in the game window: loads two campaign fixtures from
# opengold_expedition_tests (OPENGOLD_KUTO_WELL_FIXTURE beside the well and
# OPENGOLD_KUTO_NORRIS_FIXTURE beside Norris's hall), climbs down the well into
# the arrow volley, then meets Norris the Gray and starts his fight. Saves a
# screenshot at each step and a report of the dialogue. Not a CTest check; run
# with a window (not --headless) and HOME pointed at a scratch folder so the
# player's saves are untouched:
#   godot --path src/OpenGoldBox/godot --script ../../../tests/playtest_kutos_well.gd -- \
#       --kuto-well=/tmp/well.ogs --kuto-norris=/tmp/norris.ogs --playtest-out=/tmp/kuto
var well := ""
var norris := ""
var out := ""
var report := PackedStringArray()

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--kuto-well="): well = arg.trim_prefix("--kuto-well=")
        if arg.begins_with("--kuto-norris="): norris = arg.trim_prefix("--kuto-norris=")
        if arg.begins_with("--playtest-out="): out = arg.trim_prefix("--playtest-out=")
    call_deferred("run")

func fail(message: String) -> void:
    report.append("FAILED: " + message)
    finish(1)

func finish(code: int) -> void:
    var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    f.store_string("\n".join(report) + "\n"); f.close()
    print("\n".join(report))
    quit(code)

func settle(frames := 6) -> void:
    for frame in range(frames): await process_frame

func capture(name: String) -> void:
    await settle()
    # Draw now: macOS may not redraw a window that is not in front.
    RenderingServer.force_draw()
    root.get_texture().get_image().save_png(out.path_join(name + ".png"))
    report.append("[screenshot " + name + ".png]")

func press(path: String) -> void:
    var b: Button = current_scene.get_node(path)
    if b.disabled:
        fail("Disabled control: " + path)
        return
    b.pressed.emit(); await settle()

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func dialogue() -> String:
    return (town().get_node("Dialogue") as RichTextLabel).get_parsed_text()

func slot_path(slot: String) -> String:
    return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

func load_fixture(fixture: String, slot: String) -> void:
    var path := slot_path(slot)
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    var f := FileAccess.open(path, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
    var load_button := "CampaignTown/LoadGame" if current_scene.has_node("CampaignTown") and town().visible else "PartyPanel/Load"
    await press(load_button)
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    for i in range(list.item_count):
        if list.get_item_text(i) == slot:
            list.select(i); list.item_selected.emit(i)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    if not town().visible: await press("PartyPanel/Explore")
    report.append("Loaded " + slot + ": " + town().get_node("Location").text + " / " + town().get_node("Coordinates").text)

# Answers each prompt, choosing `wanted` when offered, until `until` appears in
# the dialogue or no prompt is left. Captures `shot` when `until` appears.
func play_until(until: String, wanted: String, shot: String) -> bool:
    for prompt in range(40):
        var text := dialogue()
        if not until.is_empty() and text.contains(until):
            report.append("> " + text.replace("\n", "\n  "))
            await capture(shot)
            return true
        var continue_button: Button = town().get_node("Continue")
        if continue_button.disabled:
            # A script DELAY runs on the game clock; wait it out before giving up.
            for wait in range(20):
                if not continue_button.disabled: break
                await create_timer(0.25).timeout
            if continue_button.disabled: return until.is_empty() or dialogue().contains(until)
            continue
        report.append("> " + text.replace("\n", "\n  "))
        var choices: ItemList = town().get_node("Choices")
        var selection := 0
        for i in range(choices.item_count):
            if choices.get_item_text(i) == wanted: selection = i
        if choices.item_count:
            report.append("  choose " + choices.get_item_text(selection))
            choices.select(selection)
        await press("CampaignTown/Continue")
    return false

func run() -> void:
    if well.is_empty() or norris.is_empty() or out.is_empty():
        fail("--kuto-well, --kuto-norris and --playtest-out are required"); return
    DirAccess.make_dir_recursive_absolute(out)
    root.gui_embed_subwindows = true
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
    await press("Party")

    await load_fixture(well, "Kuto play-test well")
    await capture("01-beside-the-well")
    await press("CampaignTown/Forward")
    if not await play_until("CLIMB DOWN", "", "02-climb-down"): fail("The well offers its rungs"); return
    var choices: ItemList = town().get_node("Choices")
    choices.select(0); await press("CampaignTown/Continue")
    if not await play_until("", "NO", "unused"): fail("The descent finishes"); return
    await capture("03-catacombs")
    # The secret door is south of the well's rungs.
    for turn in range(4):
        if town().get_node("Location").text.ends_with("South view"): break
        await press("CampaignTown/Right")
    await press("CampaignTown/Forward")
    if not await play_until("An arrow", "", "04-arrow-volley"): fail("The arrow volley reports"); return
    await play_until("", "", "unused")

    await load_fixture(norris, "Kuto play-test Norris")
    await capture("05-before-norris")
    await press("CampaignTown/Forward")
    if not await play_until("NORRIS", "", "06-norris-speaks"): fail("Norris confronts the party"); return
    if not await play_until("WHAT DO YOU DO?", "", "07-surrender-or-fight"): fail("Norris offers a choice"); return
    choices = town().get_node("Choices")
    for i in range(choices.item_count):
        if choices.get_item_text(i) == "FIGHT": choices.select(i)
    await press("CampaignTown/Continue")
    await settle(30)
    await capture("08-close-up")
    var key := InputEventKey.new(); key.keycode = KEY_SPACE; key.pressed = true
    root.push_input(key); key = key.duplicate(); key.pressed = false; root.push_input(key)
    await settle(30)
    if not current_scene.has_node("CampaignCombat"): fail("Norris's fight opens the combat screen"); return
    await capture("09-norris-fight")
    report.append("Kuto's Well play-test finished")
    finish(0)
