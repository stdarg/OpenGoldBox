extends SceneTree

# Play-test of aimed and targeted actions: loads each save written by
# opengold_playtest_fixtures, drives it through the real controls (A-key cycle,
# Space, clicks, the Bonus Action dropdown), screenshots each step and reports
# what the combat log shows. Not a CTest check; see docs/COMBAT-DEMO.md.
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
    report.append("  NOT IN A CYCLE: " + label + " | " + combat().get_node("StatusStack/Turn").text.replace("\n", " | "))
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

# Clicks squares until the log shows `text`: a target may have moved.
func click_until(text: String) -> void:
    for y in range(8):
        for x in range(14):
            if log_text().contains(text): return
            await click(Vector2i(x, y))

func use_bonus(label: String) -> bool:
    var dropdown: OptionButton = combat().get_node("CunningAction")
    for i in range(dropdown.item_count):
        if dropdown.get_item_text(i) == label:
            dropdown.select(i); dropdown.item_selected.emit(i)
            await settle()
            combat().get_node("UseCunningAction").pressed.emit()
            await settle()
            return true
    report.append("  NOT IN DROPDOWN: " + label)
    return false

# Ends the hero's turn and waits until it acts again, declining reactions.
func next_turn() -> void:
    combat().get_node("End").pressed.emit()
    for i in range(400):
        await settle(5)
        var decline: Button = combat().get_node("Decline")
        if decline.visible and not decline.disabled:
            decline.pressed.emit()
        var turn: String = combat().get_node("StatusStack/Turn").text.get_slice("\n", 0)
        var end: Button = combat().get_node("End")
        if not end.visible or end.disabled:
            continue
        if turn.ends_with("Hero turn"):
            return
        # Another party member, such as the Ally, ends its turn too.
        if i % 40 == 0: report.append("    waiting: " + turn + " | prompt: " + prompt())
        end.pressed.emit()

func expect(scenario: String, text: String) -> void:
    report.append(("  ok: " if log_text().contains(text) else "  MISSING in log: ") + text)

func run() -> void:
    DirAccess.make_dir_recursive_absolute(out)
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    var original := FileAccess.get_file_as_bytes(save_path) if FileAccess.file_exists(save_path) else PackedByteArray()
    root.size = Vector2i(1600, 1000)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle(20)
    combat().set_process(true)

    report.append("== Moonbeam")
    await load_fixture("moonbeam")
    if await select("Moonbeam"):
        await key(KEY_SPACE)
        await capture("moonbeam-1-aim")
        report.append("  aiming prompt: " + prompt())
        await key(KEY_SPACE)
        await capture("moonbeam-2-cast")
        expect("moonbeam", "casts Moonbeam")
        expect("moonbeam", "Radiant damage from the Moonbeam")
        await next_turn()
        if await select("Move Moonbeam"):
            await click(Vector2i(9, 3))
            await capture("moonbeam-3-moved")
            expect("moonbeam", "moves the Moonbeam")

    report.append("== Spike Growth")
    await load_fixture("spike-growth")
    if await select("Spike Growth"):
        await key(KEY_SPACE)
        await key(KEY_SPACE)
        await capture("spike-growth-1-cast")
        expect("spikes", "casts Spike Growth")
        # The Vanguards shoot from range rather than walk the spikes; the
        # native druid tests cover the piercing damage.

    report.append("== Land's Aid")
    await load_fixture("lands-aid")
    if await select("Land's Aid"):
        await key(KEY_SPACE)
        await capture("lands-aid-1-aim")
        await key(KEY_SPACE)
        await capture("lands-aid-2-cast")
        expect("lands", "uses Land's Aid")
        expect("lands", "Necrotic damage from the thorns")

    report.append("== Produce Flame")
    await load_fixture("produce-flame")
    if await select("Produce Flame"):
        await key(KEY_SPACE)
        expect("flame", "gains Produce Flame")
        if await select("Hurl flame (Produce Flame)"):
            await click(Vector2i(6, 1))
            await capture("produce-flame-hurl")
            expect("flame", "hurls Produce Flame")

    report.append("== Flame Blade")
    await load_fixture("flame-blade")
    if await select("Flame Blade"):
        await key(KEY_SPACE)
        expect("blade", "gains Flame Blade")
        if await select("Flame Blade attack"):
            await click(Vector2i(2, 1))
            await capture("flame-blade-attack")
            expect("blade", "Hero -> Enemy")

    report.append("== Heat Metal")
    await load_fixture("heat-metal")
    if await select("Heat Metal"):
        await click(Vector2i(4, 1))
        await capture("heat-metal-1-cast")
        expect("heat", "Fire damage from the hot metal")
        await next_turn()
        if await select("Heat Metal again"):
            await click_until("heats the metal again")
            await capture("heat-metal-2-again")
            expect("heat", "heats the metal again")

    report.append("== Bardic Inspiration")
    await load_fixture("bardic-inspiration")
    if await use_bonus("Bardic Inspiration"):
        await click(Vector2i(1, 4))
        await capture("bardic-inspiration")
        expect("bard", "inspires Ally")

    if original.is_empty():
        DirAccess.remove_absolute(save_path)
    else:
        var restore := FileAccess.open(save_path, FileAccess.WRITE)
        restore.store_buffer(original); restore.close()
    var file := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    file.store_string("\n".join(report))
    file.close()
    print("Play-test actions finished: ", out)
    quit(0)
