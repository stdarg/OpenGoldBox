extends SceneTree

# Play-test of the Slums' set encounters in the game window, played like a
# person would: loads campaign saves written by opengold_expedition_tests
# (OPENGOLD_SLUMS_FIXTURES: beside the arguing hobgoblins, beside the monster
# leaders and south of the trolls' room), walks in, answers the story and plays
# every party turn through the real controls. On each turn it cycles actions
# with A and clicks the enemies; when nothing can be attacked it walks toward
# the nearest enemy with the arrow keys and ends the turn. The report lists
# every distinct prompt and error with a screenshot of each first sight, and
# the first morale, fleeing and bandaging line of each kind with the enemies'
# hover panels at that moment. A last run presses Flee in the hobgoblins'
# fight. Not a
# CTest check; run with a window (not --headless) and a scratch HOME:
#   HOME=/tmp/slums/home OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --path src/OpenGoldBox/godot \
#       --script tests/playtest_slums.gd -- --slums-fixtures=/tmp/slums/fixtures \
#       --playtest-out=/tmp/slums/out
var fixtures := ""
var out := ""
var report := PackedStringArray()
var seen := {}
var shots := 0

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--slums-fixtures="): fixtures = arg.trim_prefix("--slums-fixtures=")
        if arg.begins_with("--playtest-out="): out = arg.trim_prefix("--playtest-out=")
    call_deferred("run")

func finish(code: int) -> void:
    var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    f.store_string("\n".join(report) + "\n"); f.close()
    print("\n".join(report))
    quit(code)

func settle(frames := 6) -> void:
    for frame in range(frames): await process_frame

func capture(name: String) -> void:
    await settle()
    RenderingServer.force_draw()
    shots += 1
    var file := "%03d-%s.png" % [shots, name]
    root.get_texture().get_image().save_png(out.path_join(file))
    report.append("  [screenshot " + file + "]")

func key(code: Key, shift := false) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down; event.shift_pressed = shift
        root.push_input(event)
    await settle(3)

func press(path: String) -> bool:
    var b: Button = current_scene.get_node(path)
    if b.disabled or not b.is_visible_in_tree():
        report.append("  CONTROL NOT AVAILABLE: " + path)
        return false
    b.pressed.emit(); await settle()
    return true

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func dialogue() -> String:
    return (town().get_node("Dialogue") as RichTextLabel).get_parsed_text()

func slot_path(slot: String) -> String:
    return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

func load_fixture(name: String) -> void:
    var slot := "Slums play-test " + name
    var path := slot_path(slot)
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    var f := FileAccess.open(path, FileAccess.WRITE)
    f.store_buffer(FileAccess.get_file_as_bytes(fixtures.path_join(name + ".ogs"))); f.close()
    var load_button := "CampaignTown/LoadGame" if current_scene.has_node("CampaignTown") and town().visible else "PartyPanel/Load"
    await press(load_button)
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    for i in range(list.item_count):
        if list.get_item_text(i) == slot:
            list.select(i); list.item_selected.emit(i)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    if not town().visible: await press("PartyPanel/Explore")
    report.append("Loaded " + name + ": " + town().get_node("Location").text + " / " + town().get_node("Coordinates").text)

func in_combat() -> bool:
    return current_scene.has_node("CampaignCombat") and current_scene.get_node("CampaignCombat").is_visible_in_tree()

# Answers the story, choosing the first of `wanted` offered, until combat opens
# or no prompt is left.
func play_story(wanted: Array) -> void:
    for step in range(60):
        if in_combat(): return
        var continue_button: Button = town().get_node("Continue")
        if continue_button.disabled:
            for wait in range(20):
                if not continue_button.disabled or in_combat(): break
                await create_timer(0.25).timeout
            if in_combat(): return
            if continue_button.disabled:
                # A monster close-up waits for a key before the fight.
                await capture("close-up")
                await key(KEY_SPACE)
                await settle(30)
                if not in_combat(): return
            continue
        report.append("  > " + dialogue().replace("\n", "\n    "))
        var choices: ItemList = town().get_node("Choices")
        var selection := 0
        for i in range(choices.item_count):
            if wanted.has(choices.get_item_text(i)):
                selection = i
                break
        if choices.item_count:
            report.append("    choose " + choices.get_item_text(selection))
            choices.select(selection)
        await press("CampaignTown/Continue")
        await settle(20)

# A won or lost fight leaves the combat screen up (a defeat under its dialog);
# its turn panel then no longer names a round.
func fighting() -> bool:
    return in_combat() and combat().get_node("Turn").text.begins_with("Round")

func combat() -> Control:
    return current_scene.get_node("CampaignCombat")

func prompt() -> String:
    return (combat().get_node("Prompt") as Label).text

func log_text() -> String:
    return (combat().get_node("Log") as RichTextLabel).get_parsed_text()

# Notes a prompt or error the first time it is seen, with a screenshot.
func note(kind: String, text: String) -> void:
    if seen.has(kind + text): return
    seen[kind + text] = true
    report.append("  " + kind + ": " + text)
    await capture(kind.to_lower())

func press_any(node: Node, texts: Array) -> bool:
    for child in node.get_children():
        if child is Button and child.is_visible_in_tree() and not child.disabled and texts.has(child.text):
            report.append("  press " + child.text)
            child.pressed.emit()
            await settle(6)
            return true
        if await press_any(child, texts): return true
    return false

func click(cell: Vector2i) -> void:
    var canvas: Control = combat().get_node("BattlefieldScroll/Canvas")
    var tile: float = combat().cell_pixels()
    var point := canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = point
        root.push_input(event, true)
    await settle(4)

# A party member acts when End turn can be pressed.
func party_turn() -> bool:
    var end: Button = combat().get_node("End")
    return end.is_visible_in_tree() and not end.disabled

# Tries each action in the A cycle that aims at a creature on every enemy.
func try_attack() -> bool:
    var first := ""
    for i in range(30):
        await key(KEY_A)
        var p := prompt()
        if p == first: return false
        if first.is_empty(): first = p
        await note("Prompt", p)
        if not p.contains("Click a highlighted creature"): continue
        for cell in combat().enemy_cells():
            var before := log_text()
            await click(cell)
            if log_text() != before:
                return true
            if not prompt().begins_with("Selected:"):
                await note("Error after " + p, prompt())
    return false

# Walks one square at a time toward the nearest enemy.
func approach() -> void:
    for step in range(8):
        var me: Vector2i = combat().selected_character_cell()
        var nearest := Vector2i(-1, -1)
        var best := 1 << 30
        for cell in combat().enemy_cells():
            var d: int = max(abs(cell.x - me.x), abs(cell.y - me.y))
            if d < best: best = d; nearest = cell
        if nearest.x < 0 or best <= 1: return
        var dx: int = sign(nearest.x - me.x)
        var dy: int = sign(nearest.y - me.y)
        var keyed: Key = KEY_RIGHT if dx > 0 else KEY_LEFT if dx < 0 else KEY_DOWN if dy > 0 else KEY_UP
        await key(keyed)
        if combat().selected_character_cell() == me:
            if not prompt().begins_with("Selected:"): await note("Error after moving", prompt())
            return

# Log lines that show morale breaking, fleeing and the dying bandaged.
const WATCHED := ["flees in panic", "surrenders", "flees the battle", "cannot get away",
    "is bandaged and stable"]

# Notes the first log line of each watched kind with a screenshot and, while
# the fight goes on, what hovering each enemy shows.
func watch_log() -> void:
    for line in log_text().split("\n"):
        for phrase in WATCHED:
            if seen.has(phrase) or not line.contains(phrase): continue
            seen[phrase] = true
            report.append("  Log: " + line)
            await capture("log")
            if in_combat(): await hover_enemies()

func hover_enemies() -> void:
    var canvas: Control = combat().get_node("BattlefieldScroll/Canvas")
    var tile: float = combat().cell_pixels()
    for cell in combat().enemy_cells():
        var event := InputEventMouseMotion.new()
        event.position = canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)
        root.push_input(event, true)
        await settle(4)
        var details: Label = combat().get_node("HoverInfo/Details")
        report.append("    hover " + str(cell) + ": " + details.text.replace("\n", " | "))

func play_fight(name: String) -> void:
    await capture(name + "-start")
    for turn in range(400):
        if not fighting(): break
        await settle(10)
        await watch_log()
        if await press_any(combat(), ["Keep initiative", "Decline reaction", "Keep current", "Continue"]):
            continue
        if not party_turn():
            var p := prompt()
            if not p.begins_with("Enemy turn") and not p.is_empty(): await note("Waiting", p)
            await create_timer(0.3).timeout
            continue
        if not await try_attack():
            await approach()
            await try_attack()
        if party_turn(): await key(KEY_ENTER)
    report.append("  fight over: " + ("still in combat" if in_combat() else "left combat"))
    await watch_log()
    await capture(name + "-end")
    if not in_combat(): await play_story([])

# The hobgoblins' fight again, pressing Flee on the party's first turn: the
# game runs every member for the edge.
func flee_run() -> void:
    report.append("== flee")
    change_scene_to_file("res://scenes/character_creation.tscn"); await settle(20)
    await press("Party")
    await load_fixture("hobgoblins")
    for steps in range(3):
        await press("CampaignTown/Forward")
        await settle(20)
        if not (town().get_node("Continue") as Button).disabled or in_combat(): break
    await play_story(["Fight", "FIGHT", "Bash", "BASH"])
    if not in_combat():
        report.append("  NO COMBAT"); return
    for turn in range(400):
        if not fighting(): break
        await settle(10)
        await watch_log()
        if await press_any(combat(), ["Keep initiative", "Decline reaction", "Keep current", "Continue", "Flee"]):
            continue
        await create_timer(0.3).timeout
    await watch_log()
    report.append("  fight over: " + ("still in combat" if in_combat() else "left combat"))
    await capture("flee-end")
    if not in_combat(): await play_story([])

func run() -> void:
    if fixtures.is_empty() or out.is_empty():
        report.append("FAILED: --slums-fixtures and --playtest-out are required"); finish(1); return
    DirAccess.make_dir_recursive_absolute(out)
    root.gui_embed_subwindows = true
    root.size = Vector2i(1600, 1000)
    TranslationServer.set_locale("en")
    for name in ["hobgoblins", "leaders", "trolls"]:
        report.append("== " + name)
        # Start each scenario afresh: a lost fight leaves its defeat dialog open.
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(20)
        await press("Party")
        await load_fixture(name)
        await capture(name + "-before")
        # Step forward until the story starts: a room may lie past its door.
        for steps in range(3):
            await press("CampaignTown/Forward")
            await settle(20)
            if not (town().get_node("Continue") as Button).disabled or in_combat(): break
        await play_story(["Fight", "FIGHT", "Bash", "BASH"])
        if not in_combat():
            report.append("  NO COMBAT"); await capture(name + "-no-combat"); continue
        await play_fight(name)
    await flee_run()
    report.append("Slums play-test finished")
    finish(0)
