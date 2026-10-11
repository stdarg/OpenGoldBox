extends SceneTree

# Play-test of morale, fleeing and bandaging in the combat window: loads saves
# written by opengold_playtest_fixtures, plays them through the real controls
# (A-key cycle, clicks, End turn, Flee), screenshots each moment and reports
# what the log and the enemies' hover panels show. Not a CTest check; run with
# a window (not --headless):
#   godot --path src/OpenGoldBox/godot --script $PWD/tests/playtest_morale.gd -- \
#       --playtest-fixtures=build/playtest-fixtures --playtest-out=/tmp/morale
var fixtures := ""
var out := ""
var save_path := ""
var report := PackedStringArray()

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--playtest-fixtures="): fixtures = arg.trim_prefix("--playtest-fixtures=")
        if arg.begins_with("--playtest-out="): out = arg.trim_prefix("--playtest-out=")
    call_deferred("run")

func settle(frames := 6) -> void:
    for frame in range(frames): await process_frame

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle(3)

func capture(name: String) -> void:
    await settle()
    RenderingServer.force_draw()
    root.get_texture().get_image().save_png(out.path_join(name + ".png"))
    report.append("  [screenshot " + name + ".png]")

func combat() -> Node:
    return current_scene

func prompt() -> String:
    return combat().get_node("Prompt").text

func log_text() -> String:
    return combat().get_node("Log").get_parsed_text()

func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    var file := FileAccess.open(save_path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    combat().get_node("Load").pressed.emit()
    await settle(10)
    # Every play-test save starts on the Hero's turn. Anything else means it
    # did not load, usually a save older than the rules: stop and say so
    # rather than play the demo's own fight (tools/playtest.py writes fresh ones).
    if not combat().get_node("StatusStack/Turn").text.get_slice("\n", 0).ends_with("Hero turn"):
        report.append("FAILED: play-test save " + name + " did not load: " + combat().get_node("Prompt").text)
        var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
        f.store_string("\n".join(report) + "\n"); f.close()
        printerr("\n".join(report))
        quit(1)
        await create_timer(60).timeout

func select(label: String) -> bool:
    for i in range(60):
        await key(KEY_A)
        if prompt().contains("Selected: " + label + "."): return true
    report.append("  NOT IN A CYCLE: " + label)
    var seen := {}
    for i in range(30):
        await key(KEY_A)
        if seen.has(prompt()): break
        seen[prompt()] = true
        report.append("    cycle: " + prompt())
    return false

func cell_point(cell: Vector2i) -> Vector2:
    var canvas: Control = combat().get_node("BattlefieldScroll/Canvas")
    var tile: float = combat().cell_pixels()
    return canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)

func click(cell: Vector2i) -> void:
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = cell_point(cell)
        root.push_input(event, true)
    await settle(6)

# Hovers each enemy in turn, reporting its panel; leaves the pointer on the last.
func hover_enemies() -> void:
    for cell in combat().enemy_cells():
        var event := InputEventMouseMotion.new()
        event.position = cell_point(cell)
        root.push_input(event, true)
        await settle(4)
        var details: Label = combat().get_node("HoverInfo/Details")
        report.append("    hover " + str(cell) + ": " + details.text.replace("\n", " | "))

# The standalone demo hides End turn; Enter and End's signal still end a turn.
func hero_turn() -> bool:
    return combat().get_node("StatusStack/Turn").text.get_slice("\n", 0).ends_with("Hero turn")

func fight_over() -> bool:
    return not combat().get_node("StatusStack/Turn").text.begins_with("Round")

# Waits until the hero acts again or the fight ends, declining reactions and
# ending the turns of other party members.
func wait_for_hero() -> void:
    for i in range(600):
        await settle(5)
        var decline: Button = combat().get_node("Decline")
        if decline.visible and not decline.disabled:
            decline.pressed.emit()
        if hero_turn() or fight_over():
            return
        if combat().get_node("StatusStack/Turn").text.get_slice("\n", 0).ends_with("Ally turn"):
            combat().get_node("End").pressed.emit()

# Reports each watched line the log gained since `before`.
func report_new_lines(before: String) -> void:
    for line in log_text().substr(before.length()).split("\n"):
        for phrase in ["flees in panic", "surrenders", "flees the battle", "cannot get away",
                "is bandaged and stable", "Victory."]:
            if line.contains(phrase): report.append("  log: " + line)

# True when the log gained a watched line since `before`.
func watched_since(before: String) -> bool:
    var added := log_text().substr(before.length())
    for phrase in ["flees in panic", "surrenders", "flees the battle", "cannot get away"]:
        if added.contains(phrase): return true
    return false

# Attacks whatever enemy the Melee attack highlights, one click per enemy square.
func attack() -> void:
    var me: Vector2i = combat().selected_character_cell()
    for cell in combat().enemy_cells():
        if max(abs(cell.x - me.x), abs(cell.y - me.y)) > 1: continue
        if not await select("Melee attack"): return
        var before := log_text()
        await click(cell)
        if log_text() != before: return

# Fights the Kobolds turn by turn and watches the others break when one is hurt.
func morale(name: String) -> void:
    report.append("== " + name)
    await load_fixture(name)
    await capture(name + "-1-start")
    for round in range(8):
        if fight_over(): break
        var before := log_text()
        await attack()
        if not fight_over():
            combat().get_node("End").pressed.emit()
            await wait_for_hero()
        report_new_lines(before)
        if watched_since(before):
            await hover_enemies()
            await capture(name + "-2-round-%d" % (round + 1))
    report.append("  log: " + log_text().replace("\n", "\n  log: "))
    await capture(name + "-3-end")
    report.append("  turn panel: " + combat().get_node("StatusStack/Turn").text.replace("\n", " | "))

func bandage() -> void:
    report.append("== bandage")
    await load_fixture("bandage")
    await capture("bandage-1-start")
    var before := log_text()
    for round in range(8):
        if fight_over(): break
        await attack()
        if not fight_over():
            combat().get_node("End").pressed.emit()
            await wait_for_hero()
    await settle(20)
    report_new_lines(before)
    await capture("bandage-2-end")
    report.append("  turn panel: " + combat().get_node("StatusStack/Turn").text.replace("\n", " | "))

func flee() -> void:
    report.append("== flee button")
    await load_fixture("flee")
    await capture("flee-1-start")
    var button: Button = combat().get_node("Flee")
    if not button.visible or button.disabled:
        report.append("  FLEE NOT OFFERED"); return
    var before := log_text()
    button.pressed.emit()
    await settle(10)
    await capture("flee-2-pressed")
    report.append("  prompt: " + prompt())
    for i in range(1200):
        await settle(5)
        if fight_over(): break
    await settle(30)
    report_new_lines(before)
    report.append("  log: " + log_text().replace("\n", "\n  log: "))
    await capture("flee-3-end")
    report.append("  turn panel: " + combat().get_node("StatusStack/Turn").text.replace("\n", " | "))

func run() -> void:
    DirAccess.make_dir_recursive_absolute(out)
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    var original := FileAccess.get_file_as_bytes(save_path) if FileAccess.file_exists(save_path) else PackedByteArray()
    root.size = Vector2i(1600, 1000)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle(20)
    combat().set_process(true)
    await morale("morale-panic")
    await morale("morale-surrender")
    await bandage()
    await flee()
    if original.is_empty():
        DirAccess.remove_absolute(save_path)
    else:
        var restore := FileAccess.open(save_path, FileAccess.WRITE)
        restore.store_buffer(original); restore.close()
    var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    f.store_string("\n".join(report) + "\n"); f.close()
    print("\n".join(report))
    quit(0)
