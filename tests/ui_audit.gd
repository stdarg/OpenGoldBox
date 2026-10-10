extends SceneTree

# Audits the game's screens for broken layout (tests/ui_audit_checks.gd) at
# three window sizes, in English and Spanish, and fails on any finding: the
# startup screen and the combat screen in the play-test fights; with the
# original files (OPENGOLD_GAME_DIR), character creation and the party; with
# Slums saves (--slums-fixtures or OPENGOLD_SLUMS_FIXTURES, written by
# opengold_expedition_tests), the town, a story choice and a campaign fight.
# --audit-out=DIR also writes each screen's control tree as JSON there;
# --capture=DIR, run in a window (not --headless), saves a screenshot of each
# screen for tools/ui_snapshots.py to compare.
#   godot --headless --path src/OpenGoldBox/godot --script $PWD/tests/ui_audit.gd -- \
#       --playtest-fixtures=build/playtest-fixtures [--slums-fixtures=DIR] [--audit-out=DIR]
const SIZES := [Vector2i(1120, 800), Vector2i(1600, 1000), Vector2i(1920, 1080)]
const LOCALES := ["en", "es"]

var checks: GDScript
var locale := "en"
var fixtures := ""
var slums := ""
var out := ""
var capture := ""
var findings := PackedStringArray()
var audited := []
var save_path := ""
var original: Variant = null

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--playtest-fixtures="): fixtures = arg.trim_prefix("--playtest-fixtures=")
        if arg.begins_with("--audit-out="): out = arg.trim_prefix("--audit-out=")
        if arg.begins_with("--capture="): capture = arg.trim_prefix("--capture=")
        if arg.begins_with("--slums-fixtures="): slums = arg.trim_prefix("--slums-fixtures=")
    if slums.is_empty(): slums = OS.get_environment("OPENGOLD_SLUMS_FIXTURES")
    checks = load(get_script().resource_path.get_base_dir().path_join("ui_audit_checks.gd"))
    call_deferred("run")

func settle(frames := 8) -> void:
    for frame in range(frames): await process_frame

# Audits what is on screen now under `label`, at the current size and locale.
# A screen opened over another (the party, the town, a campaign fight) is
# audited alone: the one beneath stays in the tree but out of sight.
func audit(label: String, screen: Node = null) -> void:
    await settle()
    if screen == null: screen = current_scene
    if not audited.has(label): audited.append(label)
    var name := "%s@%dx%d/%s" % [label, root.size.x, root.size.y, TranslationServer.get_locale()]
    for finding in checks.audit(screen, Vector2(root.size)):
        findings.append(name + ": " + finding)
    var file_name := name.replace("/", "-").replace("@", "-")
    if not out.is_empty():
        var file := FileAccess.open(out.path_join(file_name + ".json"), FileAccess.WRITE)
        file.store_string(JSON.stringify(checks.dump(screen), "  ")); file.close()
    if not capture.is_empty():
        RenderingServer.force_draw()
        var image := root.get_texture().get_image()
        if image == null:
            findings.append("--capture needs a window, not --headless"); return
        image.save_png(capture.path_join(file_name + ".png"))

# The game sets its language from the settings as a scene loads; the audit's
# language is set after.
func open(scene: String) -> void:
    change_scene_to_file(scene)
    await settle(12)
    TranslationServer.set_locale(locale)
    await settle()

func load_fixture(name: String) -> bool:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    if bytes.is_empty():
        findings.append("missing play-test fixture " + name); return false
    var file := FileAccess.open(save_path, FileAccess.WRITE); file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
    return true

func press(path: String) -> void:
    var button: Button = current_scene.get_node(path)
    if button.disabled or not button.is_visible_in_tree():
        findings.append("cannot press " + path); return
    button.pressed.emit(); await settle()

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func in_combat() -> bool:
    return current_scene.has_node("CampaignCombat") and current_scene.get_node("CampaignCombat").is_visible_in_tree()

# Loads a Slums save through Load game, as a player would.
func load_slums(name: String) -> bool:
    var slot := "UI audit " + name
    var path := ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")
    var bytes := FileAccess.get_file_as_bytes(slums.path_join(name + ".ogs"))
    if bytes.is_empty():
        findings.append("missing Slums fixture " + name); return false
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(bytes); file.close()
    await press("PartyPanel/Load")
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    for i in range(list.item_count):
        if list.get_item_text(i) == slot:
            list.select(i); list.item_selected.emit(i)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    if not town().visible: await press("PartyPanel/Explore")
    return town().visible

# The town, then the hobgoblins' story with its choices, then their fight.
func campaign() -> void:
    await open("res://scenes/character_creation.tscn")
    await press("Party")
    if not await load_slums("hobgoblins"): return
    await audit("town", town())
    for step in range(3):
        await press("CampaignTown/Forward")
        if not (town().get_node("Continue") as Button).disabled or in_combat(): break
    await audit("story", town())
    for step in range(20):
        if in_combat(): break
        var choices: ItemList = town().get_node("Choices")
        for i in range(choices.item_count):
            if choices.get_item_text(i).to_upper() in ["FIGHT", "BASH", "LUCHAR"]:
                choices.select(i)
        var continue_button: Button = town().get_node("Continue")
        if continue_button.disabled:
            for frame in range(30): await process_frame
        else:
            continue_button.pressed.emit(); await settle(20)
    if not in_combat():
        findings.append("the hobgoblins' fight did not start"); return
    var combat := current_scene.get_node("CampaignCombat")
    for frame in range(600):
        if (combat.get_node("End") as Button).is_visible_in_tree(): break
        await process_frame
    await audit("campaign-combat", combat)
    (combat.get_node("Quick") as Button).pressed.emit()
    await audit("campaign-combat-quick", combat)

func drag(from: Vector2, to: Vector2) -> void:
    var down := InputEventMouseButton.new()
    down.button_index = MOUSE_BUTTON_LEFT; down.pressed = true
    down.position = from; down.global_position = from
    root.push_input(down, true); await settle(3)
    var motion := InputEventMouseMotion.new()
    motion.button_mask = MOUSE_BUTTON_MASK_LEFT
    motion.position = to; motion.global_position = to; motion.relative = to - from
    root.push_input(motion, true); await settle(3)
    var up := InputEventMouseButton.new()
    up.button_index = MOUSE_BUTTON_LEFT
    up.position = to; up.global_position = to
    root.push_input(up, true); await settle(3)

# Each page of character creation, taking the first choice where one is needed.
func creation_pages() -> void:
    var seen := {}
    for step in range(16):
        var page: String = (current_scene.get_node("PageTitle") as Label).text
        if not seen.has(page):
            seen[page] = true
            await audit("creation-%d" % seen.size())
        # The ability page: roll, then drag each die onto its ability.
        var roll: Button = current_scene.get_node("Roll")
        if roll.is_visible_in_tree() and current_scene.get_node("Next").disabled:
            roll.pressed.emit(); await settle()
            for i in range(6):
                await drag(current_scene.get_node("Dice%d" % i).get_global_rect().get_center(),
                    current_scene.get_node("Score%d" % i).get_global_rect().get_center())
        var choices: ItemList = current_scene.get_node("Choices")
        if choices.is_visible_in_tree() and choices.item_count and not choices.is_anything_selected():
            choices.select(0); choices.item_selected.emit(0); await settle()
        var next: Button = current_scene.get_node("Next")
        if next.disabled or not next.is_visible_in_tree(): return
        next.pressed.emit(); await settle()

func level_up_lab() -> void:
    await open("res://scenes/level_up_lab.tscn")
    await audit("level-up-lab")
    var klass: OptionButton = current_scene.get_node("Class")
    var wizard := -1
    for i in range(klass.item_count):
        if str(klass.get_item_metadata(i)) == "wizard": wizard = i
    if wizard < 0:
        findings.append("level-up lab has no Wizard choice"); return
    klass.select(wizard)
    (current_scene.get_node("CurrentLevel") as OptionButton).select(2)
    await press("CreateCharacter")
    await press("OpenLevelUp")
    await audit("level-up-dialog", current_scene.get_node("LevelUp"))
    await press("LevelUp/Confirm")
    if current_scene.get_node("LevelUp/SpellChoicesPage").visible:
        await audit("level-up-spell-page", current_scene.get_node("LevelUp"))
    else:
        findings.append("Wizard level-up did not open its spell-choice page")

func screens() -> void:
    await open("res://scenes/startup.tscn"); await audit("startup")
    await level_up_lab()
    if not OS.get_environment("OPENGOLD_GAME_DIR").is_empty():
        await open("res://scenes/character_creation.tscn")
        await creation_pages()
        await open("res://scenes/character_creation.tscn")
        current_scene.get_node("Party").pressed.emit()
        await audit("party", current_scene.get_node("PartyPanel"))
    if not slums.is_empty(): await campaign()
    await open("res://scenes/combat_demo.tscn")
    current_scene.set_process(false)
    await audit("combat")
    if fixtures.is_empty(): return
    for fixture in ["gear-torch", "gear-ally", "bandage", "morale-panic", "flee"]:
        if await load_fixture(fixture): await audit("combat-" + fixture)
    # A member's turn played by the computer (Quick): Take control and Quick magic.
    if await load_fixture("gear-ally"):
        current_scene.get_node("Quick").pressed.emit()
        await audit("combat-quick")

func run() -> void:
    root.gui_embed_subwindows = true
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(save_path) if FileAccess.file_exists(save_path) else null
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    if not out.is_empty(): DirAccess.make_dir_recursive_absolute(out)
    if not capture.is_empty(): DirAccess.make_dir_recursive_absolute(capture)
    for language in LOCALES:
        locale = language
        for size in SIZES:
            root.size = size
            await screens()
    if original == null:
        if FileAccess.file_exists(save_path): DirAccess.remove_absolute(save_path)
    else:
        var restore := FileAccess.open(save_path, FileAccess.WRITE); restore.store_buffer(original); restore.close()
    for finding in findings: print("UI: " + finding)
    if findings.is_empty():
        # The screens covered, so a run that skipped some cannot pass unseen.
        print("UI audit passed: " + ", ".join(audited))
        quit(0)
    else:
        print("UI audit found %d problems" % findings.size())
        quit(1)
