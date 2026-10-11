extends SceneTree

# Play-test of Quick combat on the campaign combat screen: a member hands its
# turn to the computer with Quick, Q puts the whole party on Quick, M switches
# Quick magic and Space takes the party back. Screenshots each state. Run with
# a window and the original files, through tools/playtest.py quick.
var out := ""
var report := PackedStringArray()

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--playtest-out="): out = arg.trim_prefix("--playtest-out=")
    call_deferred("run")

func settle(frames := 6) -> void:
    for frame in range(frames): await process_frame

func capture(name: String) -> void:
    await settle()
    RenderingServer.force_draw()
    root.get_texture().get_image().save_png(out.path_join(name + ".png"))
    report.append("  [screenshot " + name + ".png] " + prompt())

func combat() -> Node:
    return current_scene

func prompt() -> String:
    return combat().get_node("Prompt").text

func turn() -> String:
    return combat().get_node("StatusStack/Turn").text.get_slice("\n", 0)

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle(3)

func shown(name: String) -> bool:
    return (combat().get_node(name) as Control).is_visible_in_tree()

# Passes initiative choices and reactions, as a player might.
func pass_prompts() -> void:
    for button in ["Keep initiative", "Decline reaction", "Keep current"]:
        for node in combat().find_children("*", "Button", true, false):
            if node.is_visible_in_tree() and not node.disabled and node.text == button:
                node.pressed.emit(); await settle(6)

# Waits until a party member the player controls has the turn.
func manual_turn() -> bool:
    for i in range(1200):
        await settle(2)
        await pass_prompts()
        if not turn().begins_with("Round"): return false
        if shown("End"): return true
    return false

func run() -> void:
    DirAccess.make_dir_recursive_absolute(out)
    root.size = Vector2i(1600, 1000)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle(20)
    combat().set_process(true)
    report.append("== Quick combat")
    if not await manual_turn():
        report.append("FAILED: no party turn"); finish(1); return
    await capture("quick-1-member-turn")
    report.append("  Quick shown: %s, Flee shown: %s" % [shown("Quick"), shown("Flee")])
    var first := turn()
    combat().get_node("Quick").pressed.emit()
    await settle(4)
    await capture("quick-2-computer-plays")
    report.append("  " + first + " -> Take control shown: %s, End shown: %s" % [shown("TakeControl"), shown("End")])
    if await manual_turn():
        await capture("quick-3-next-member")
        report.append("  next manual turn: " + turn() + "; Quick magic shown: %s" % shown("QuickMagic"))
        await key(KEY_Q)
        await key(KEY_M)
        await settle(4)
        await capture("quick-4-whole-party-magic-on")
        report.append("  after Q and M: " + turn())
        # A spellcaster's turn under Quick, with Quick magic on.
        for i in range(3000):
            await settle(1)
            if shown("TakeControl") and (turn().contains("Yoren") or turn().contains("Dorian")): break
        await capture("quick-5-caster-under-quick")
        # Space takes control while the computer plays a member.
        for i in range(3000):
            if shown("TakeControl"): break
            await settle(1)
        await key(KEY_SPACE)
        await settle(6)
        await capture("quick-6-taken-back")
        report.append("  after Space: Take control shown: %s, End shown: %s" % [shown("TakeControl"), shown("End")])
    finish(0)

func finish(code: int) -> void:
    var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    f.store_string("\n".join(report) + "\n"); f.close()
    print("\n".join(report))
    quit(code)
