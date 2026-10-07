extends SceneTree

# Play-test capture: runs the combat demo with a chosen party and, on each party
# member's first turn, saves a screenshot and lists the Bonus Action dropdown and
# the A-key action cycle. Not a CTest check; see docs/COMBAT-DEMO.md.
#   godot --path src/OpenGoldBox/godot --script tests/playtest_capture.gd -- \
#       --combat-demo --combat-demo-party=druid,warlock --combat-demo-level=4 \
#       --playtest-out=/tmp/playtest
var out := ""
var report := PackedStringArray()

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--playtest-out="): out = arg.trim_prefix("--playtest-out=")
    call_deferred("run")

func settle(frames := 6) -> void:
    for frame in range(frames): await process_frame

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle(2)

func capture(name: String) -> void:
    await settle()
    # Draw now: macOS may not redraw a window that is not in front.
    RenderingServer.force_draw()
    root.get_texture().get_image().save_png(out.path_join(name + ".png"))

func turn_text(combat: Node) -> String:
    return combat.get_node("Turn").text.get_slice("\n", 0)

func record_turn(combat: Node, index: int) -> String:
    var who := turn_text(combat)
    report.append("== " + who)
    report.append("Status: " + combat.get_node("Turn").text.replace("\n", " | "))
    var dropdown: OptionButton = combat.get_node("CunningAction")
    if dropdown.visible:
        for i in range(dropdown.item_count):
            report.append("  Bonus Action: " + dropdown.get_item_text(i) + (" (unavailable)" if dropdown.is_item_disabled(i) else ""))
    var cantrips: OptionButton = combat.get_node("Cantrip")
    if cantrips.visible:
        for i in range(cantrips.item_count):
            report.append("  Spell dropdown: " + cantrips.get_item_text(i))
    var seen := {}
    for i in range(60):
        await key(KEY_A)
        var prompt: String = combat.get_node("Prompt").text
        if seen.has(prompt): break
        seen[prompt] = true
        report.append("  A cycle: " + prompt)
    await key(KEY_ESCAPE)
    await capture("%02d-%s" % [index, who.get_slice("/ ", 1).replace(" turn", "").replace(" ", "_")])
    return who

# Presses the first visible, enabled button with one of these texts.
func press(node: Node, texts: Array) -> bool:
    for child in node.get_children():
        if child is Button and child.is_visible_in_tree() and not child.disabled and texts.has(child.text):
            child.pressed.emit()
            return true
        if press(child, texts): return true
    return false

func select_in_cycle(combat: Node, label: String) -> bool:
    for i in range(60):
        await key(KEY_A)
        if combat.get_node("Prompt").text.contains(label): return true
    return false

func run() -> void:
    DirAccess.make_dir_recursive_absolute(out)
    root.size = Vector2i(1600, 1000)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle(20)
    var combat := current_scene
    await capture("00-start")
    var done := {}
    var index := 1
    var last_turn := ""
    var reaction_captured := false
    for step in range(400):
        # Answer pop-ups: keep Initiative, decline reactions, keep Temporary HP.
        var decline: Button = combat.get_node("Decline")
        if decline.visible and not reaction_captured:
            reaction_captured = true
            report.append("  Reaction prompt: " + combat.get_node("Prompt").text.replace("\n", " | ") + " [" + combat.get_node("React").text + "]")
            await capture("%02d-reaction" % index)
            index += 1
        if press(combat, ["Keep initiative", "Decline reaction", "Keep current"]):
            report.append("  (answered a prompt)")
            await settle(10)
            continue
        var who := turn_text(combat)
        if step == 0 or who != last_turn:
            report.append("-- " + who)
            last_turn = who
        var end_button: Button = combat.get_node("End")
        if end_button.visible and not end_button.disabled and not done.has(who) and who.ends_with(" turn"):
            done[who] = true
            await record_turn(combat, index)
            index += 1
            # A Druid tries the Wolf form through the action cycle.
            if await select_in_cycle(combat, "Wild Shape: Wolf"):
                await key(KEY_SPACE)
                await capture("%02d-wild-shape" % index)
                report.append("  After Wild Shape: " + combat.get_node("Turn").text.replace("\n", " | "))
                index += 1
            end_button.pressed.emit()
        if done.size() >= 6 or combat.get_node("Turn").text.contains("Victory") or combat.get_node("Turn").text.contains("defeat"):
            break
        await settle(30)
    var file := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    file.store_string("\n".join(report))
    file.close()
    print("Play-test capture finished: ", out)
    quit(0)
