extends SceneTree

# Play-test of the gear added against trolls (GEAR-1): loads saves written by
# opengold_playtest_fixtures (gear-torch, gear-flasks, gear-bow) and drives the
# Torch attack, thrown Oil, Alchemist's Fire and Acid, Take off shield and Shoot
# through the real controls (A-key cycle, Space, clicks). With --campaign and the
# demo's troll and gear flags, it instead plays the real campaign screen and
# checks that a burning troll shows Burning when hovered. Screenshots each step
# and reports what the combat log shows. Not a CTest check; see
# docs/COMBAT-DEMO.md.
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
    if not combat().get_node("Turn").text.get_slice("\n", 0).ends_with("Hero turn"):
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
    report.append("  NOT IN A CYCLE: " + label + " | " + combat().get_node("Turn").text.replace("\n", " | "))
    report.append("    log: " + log_text().right(900).replace("\n", "\n    log: "))
    var seen := {}
    for i in range(60):
        await key(KEY_A)
        if seen.has(prompt()): break
        seen[prompt()] = true
        report.append("    cycle: " + prompt())
    return false

func click(cell: Vector2i) -> void:
    var canvas: Control = combat().get_node("BattlefieldScroll/Canvas")
    var tile: float = combat().cell_pixels()
    var point := canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = point
        root.push_input(event, true)
    await settle(6)

# Picks a gear action from the Items row and presses Use, as a player would.
func use_item(label: String) -> bool:
    var items: OptionButton = combat().get_node("ItemAction")
    if not items.visible: return false
    for i in range(items.item_count):
        if items.get_item_text(i).begins_with(label):
            items.select(i); items.item_selected.emit(i); await settle(3)
            combat().get_node("UseItemAction").pressed.emit(); await settle(3)
            return prompt().contains("Selected: " + label + ".")
    return false

# Clicks squares until the log shows `text`: a target may have moved.
func click_until(text: String) -> void:
    for y in range(8):
        for x in range(14):
            if log_text().contains(text): return
            await click(Vector2i(x, y))

# Ends the hero's turn and waits until it acts again, declining reactions.
func next_turn() -> void:
    combat().get_node("End").pressed.emit()
    for i in range(400):
        await settle(5)
        var decline: Button = combat().get_node("Decline")
        if decline.visible and not decline.disabled:
            decline.pressed.emit()
        var turn: String = combat().get_node("Turn").text.get_slice("\n", 0)
        var end: Button = combat().get_node("End")
        if not end.visible or end.disabled:
            continue
        if turn.ends_with("Hero turn"):
            return
        # Another party member, such as the Ally, ends its turn too.
        if i % 40 == 0: report.append("    waiting: " + turn + " | prompt: " + prompt())
        end.pressed.emit()

func expect(scenario: String, text: String) -> void:
    if log_text().contains(text):
        report.append("  ok: " + text)
        return
    report.append("  MISSING in log: " + text)
    report.append("    log: " + log_text().right(500).replace("\n", "\n    log: "))

# Lists every action the A key cycles through on the hero's turn.
func list_cycle() -> void:
    var seen := {}
    for i in range(60):
        await key(KEY_A)
        if seen.has(prompt()): break
        seen[prompt()] = true
        report.append("  A cycle: " + prompt())

# Presses the first visible, enabled button with one of `texts`.
func press(node: Node, texts: Array) -> bool:
    for child in node.get_children():
        if child is Button and child.is_visible_in_tree() and not child.disabled and texts.has(child.text):
            child.pressed.emit()
            return true
        if press(child, texts): return true
    return false

# Moves the mouse over a square, as a player hovering for its details.
func hover(cell: Vector2i) -> void:
    var canvas: Control = combat().get_node("BattlefieldScroll/Canvas")
    var tile: float = combat().cell_pixels()
    var event := InputEventMouseMotion.new()
    event.position = canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)
    root.push_input(event, true)
    await settle(4)

# On the real campaign screen (--combat-demo with trolls and gear): the first
# character who can throws Alchemist's Fire at the troll, and hovering over it
# should show it Burning.
func campaign_phase() -> void:
    report.append("== Campaign screen: Alchemist's Fire, then hover")
    for turn in range(12):
        await settle(10)
        # Keep initiative and decline reactions, as a player might.
        while press(combat(), ["Keep initiative", "Decline reaction", "Keep current"]):
            await settle(10)
        if await use_item("Throw Alchemist's Fire"):
            await click_until("throws Alchemist's Fire")
            # The sweep can miss a target that moved; if so, try again next turn.
            if log_text().contains("throws Alchemist's Fire"):
                await capture("campaign-1-fire")
                expect("campaign", "throws Alchemist's Fire")
                report.append("  burning: " + str(log_text().contains("starts burning")))
                var seen := {}
                for y in range(12):
                    for x in range(12):
                        await hover(Vector2i(x, y))
                        var info: Label = combat().get_node("HoverInfo/Details")
                        if combat().get_node("HoverInfo").visible and not seen.has(info.text):
                            seen[info.text] = true
                            report.append("  hover (%d,%d): %s" % [x, y, info.text.replace("\n", " | ")])
                            if info.text.contains("Burning"):
                                await capture("campaign-2-hover")
                if seen.is_empty():
                    report.append("  NO HOVER PANEL APPEARED")
                return
        combat().get_node("End").pressed.emit()
    report.append("  NO CHARACTER COULD THROW ALCHEMIST'S FIRE")

func run() -> void:
    DirAccess.make_dir_recursive_absolute(out)
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    var original := FileAccess.get_file_as_bytes(save_path) if FileAccess.file_exists(save_path) else PackedByteArray()
    root.size = Vector2i(1600, 1000)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle(20)
    combat().set_process(true)
    if OS.get_cmdline_user_args().has("--campaign"):
        await campaign_phase()
        finish_report(original)
        return

    report.append("== Torch beside the troll")
    await load_fixture("gear-torch")
    await capture("gear-1-start")
    await list_cycle()
    if await select("Torch attack"):
        await click(Vector2i(2, 1))
        await capture("gear-2-torch")
        expect("torch", "Hero -> Troll")

    report.append("== Oil, then Alchemist's Fire")
    await load_fixture("gear-flasks")
    if await select("Throw Oil"):
        await click(Vector2i(4, 1))
        await capture("gear-3-oil")
        expect("oil", "throws Oil at Troll")
        report.append("  turn: " + combat().get_node("Turn").text.replace("\n", " | "))
        await next_turn()
        if await select("Throw Alchemist's Fire"):
            await click_until("throws Alchemist's Fire")
            await capture("gear-4-fire")
            expect("fire", "throws Alchemist's Fire at Troll")
            await next_turn()
            await capture("gear-5-burning")
            expect("fire", "Troll burns for")

    report.append("== Acid")
    await load_fixture("gear-flasks")
    if await select("Throw Acid"):
        await click(Vector2i(4, 1))
        await capture("gear-6-acid")
        expect("acid", "throws Acid at Troll")

    report.append("== Take off shield, then shoot")
    await load_fixture("gear-bow")
    if await select("Take off shield"):
        await key(KEY_SPACE)
        await capture("gear-7-shield-off")
        expect("shield", "takes off a shield to use a bow")
        await next_turn()
        if await select("Shoot"):
            await click_until("Hero -> Troll")
            await capture("gear-8-shoot")
            expect("shoot", "Hero -> Troll")

    finish_report(original)

# Restores the player's training save and writes the report.
func finish_report(original: PackedByteArray) -> void:
    if original.is_empty():
        if FileAccess.file_exists(save_path): DirAccess.remove_absolute(save_path)
    else:
        var file := FileAccess.open(save_path, FileAccess.WRITE)
        file.store_buffer(original); file.close()
    var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    f.store_string("\n".join(report) + "\n"); f.close()
    print("\n".join(report))
    quit(0)
