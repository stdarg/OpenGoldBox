extends SceneTree
# A Buck Rogers ground fight on the combat screen (docs/BUCK-ROGERS.md), with
# Countdown to Doomsday's content: one of the game's chaff grenades is readied
# from Items by its name, Launch or throw aims it at a square chosen with the
# keyboard or the mouse, the chaff shows where it lies and the grenade is
# spent. Run by tests/settings_check.cmake with --buck-fixtures=<folder of
# chaff.save and chaff-name.txt> (opengold_countdown_campaign_tests).
var fixtures := ""

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--buck-fixtures="): fixtures = arg.trim_prefix("--buck-fixtures=")
    call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4): await process_frame

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()

func click_cell(cell: Vector2) -> void:
    var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
    var tile := canvas.get_combined_minimum_size().x / 12.0
    var position := canvas.get_global_transform_with_canvas() * ((cell + Vector2(0.5, 0.5)) * tile)
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = position
        root.push_input(event, true)
    await settle()

# The fixtures are Countdown to Doomsday's ground fights (GoldBoxGame 10).
func load_fixture(name: String) -> void:
    var path := fixtures.path_join(name)
    require(FileAccess.file_exists(path), "The fixture " + name + " exists")
    require(current_scene.load_original_check(path, 10), "The fixture " + name + " loads")
    await settle()

func item_index(label: String) -> int:
    var items: OptionButton = current_scene.get_node("ItemAction")
    for i in range(items.item_count):
        if items.get_item_text(i) == label: return i
    return -1

# The squares the battlefield draws of one kind.
func overlay_cells(kind: String) -> Array:
    var cells := []
    for overlay in current_scene.battlefield_visuals()["overlays"]:
        if overlay["kind"] == kind: cells.append(overlay["cell"])
    return cells

func log_text() -> String:
    return current_scene.get_node("LogStack/Log").get_parsed_text()

func use(label: String) -> void:
    var items: OptionButton = current_scene.get_node("ItemAction")
    var index := item_index(label)
    require(index >= 0, label + " is listed under Items: " + str(range(items.item_count).map(func(i): return items.get_item_text(i))))
    items.select(index); items.item_selected.emit(index); await settle()
    current_scene.get_node("UseItemAction").pressed.emit(); await settle()

# Open and choose Items with real keys, then Tab to Use and activate it.
# Combat shortcuts must leave these focused controls their keyboard input.
func keyboard_use(label: String, open_key: Key, use_key: Key) -> void:
    var items: OptionButton = current_scene.get_node("ItemAction")
    var index := item_index(label)
    require(index >= 0, "The keyboard can choose " + label)
    var cell: Vector2i = current_scene.selected_character_cell()
    var before := log_text()
    items.grab_focus()
    await key(open_key)
    require(items.get_popup().visible, "The keyboard opens Items for " + label)
    await key(KEY_HOME)
    for step in range(index): await key(KEY_DOWN)
    await key(KEY_ENTER)
    require(items.selected == index and not items.get_popup().visible,
        "The keyboard chooses " + label)
    require(current_scene.selected_character_cell() == cell and log_text() == before,
        "Choosing an item does not move the actor or use an action")
    await key(KEY_TAB)
    var action: Button = current_scene.get_node("UseItemAction")
    require(action.has_focus() and not action.disabled, "Tab reaches Use for " + label)
    await key(use_key)

func check_launch(locale: String, by_mouse: bool, grenade: String) -> void:
    await load_fixture("chaff.save")
    var combat := current_scene
    var launch := tr("Launch or throw")
    require(item_index(launch) < 0, "A grenade not readied cannot be thrown: " + locale)
    var ready := tr("Ready {item}").format({"item": grenade})
    if by_mouse:
        await use(ready)
    else:
        await keyboard_use(ready, KEY_ENTER, KEY_SPACE)
    require(item_index(tr("Put away {item}").format({"item": grenade})) >= 0,
        "The grenade is readied, and can be put away: " + locale)
    require(overlay_cells("obscured").is_empty(), "No chaff before the throw: " + locale)
    if by_mouse:
        await use(launch)
    else:
        await keyboard_use(launch, KEY_SPACE, KEY_ENTER)
    require(combat.get_node("Prompt").text == tr("Selected: Launch or throw. Click a highlighted square, or Arrows: square | Space: launch | Escape: cancel"),
        "The prompt asks for a square: " + locale)
    if not by_mouse:
        var items: OptionButton = combat.get_node("ItemAction")
        var before := log_text()
        items.grab_focus()
        await key(KEY_ENTER)
        require(items.get_popup().visible and log_text() == before and overlay_cells("obscured").is_empty(),
            "Enter opens focused Items instead of launching at the chosen square")
        await key(KEY_ESCAPE)
        items.release_focus()
    require(overlay_cells("target_focus") == [Vector2i(4, 4)],
        "The chosen square starts on the trooper: " + locale)
    require(overlay_cells("target").size() > 4 and not overlay_cells("target").has(Vector2i(1, 4)),
        "Every square offered is highlighted, never the thrower's own: " + locale)
    await key(KEY_LEFT)
    require(overlay_cells("target_focus") == [Vector2i(3, 4)], "Left moves the chosen square: " + locale)
    await key(KEY_ESCAPE)
    require(overlay_cells("target").is_empty() and item_index(launch) >= 0,
        "Escape cancels without throwing: " + locale)
    if by_mouse:
        await use(launch)
    else:
        await keyboard_use(launch, KEY_ENTER, KEY_SPACE)
    var thrown := tr("{name} throws a grenade.").format({"name": "BUCK"})
    if by_mouse:
        await click_cell(Vector2(4, 4))
    else:
        await key(KEY_LEFT)
        await key(KEY_RIGHT)
        require(overlay_cells("target_focus") == [Vector2i(4, 4)], "Right moves it back: " + locale)
        await key(KEY_SPACE)
    require(log_text().contains(thrown), "The grenade is thrown: " + locale + " " + log_text())
    var chaff := overlay_cells("obscured")
    require(not chaff.is_empty() and chaff.all(func(c): return abs(c.x - 4) <= 1 and abs(c.y - 4) <= 1),
        "The chaff shows around the square it landed on: " + locale + " " + str(chaff))
    # The trooper's turn goes by; on the thrower's next turn the grenade is gone.
    combat.set_process(true)
    var end: Button = combat.get_node("End")
    for wait in range(150):
        if not end.disabled: break
        await create_timer(0.1).timeout
    combat.set_process(false)
    await settle()
    require(not end.disabled, "The thrower's turn comes round again: " + locale)
    require(item_index(launch) < 0, "The spent grenade is no longer offered: " + locale)
    require(log_text().count(thrown) == 1, "It was thrown once: " + locale)
    require(item_index(tr("Ready {item}").format({"item": grenade})) < 0 and
        item_index(tr("Put away {item}").format({"item": grenade})) < 0,
        "The thrower no longer carries the grenade: " + locale)

# Jetpack takes off; in flight the landing squares show at once, and the
# arrows and Space, or a click, land.
func check_jetpack(locale: String, by_mouse: bool) -> void:
    await load_fixture("jetpack.save")
    var combat := current_scene
    await use(tr("Jetpack"))
    require(log_text().contains(tr("{name} takes off.").format({"name": "BUCK"})), "The Medic takes off: " + locale)
    require(combat.get_node("Prompt").text == tr("Flying: Land. Click a highlighted square, or Arrows: square | Space: land"),
        "In flight the prompt asks where to land: " + locale)
    var focus: Array = overlay_cells("target_focus")
    require(focus.size() == 1 and max(abs(focus[0].x - 2), abs(focus[0].y - 2)) <= 1,
        "The landing starts by the flyer: " + locale + " " + str(focus))
    require(overlay_cells("target").size() > 20, "The squares it can land on are highlighted: " + locale)
    require(item_index(tr("Land")) >= 0, "Items offers Land: " + locale)
    var landed: Vector2i = Vector2i(6, 4)
    if by_mouse:
        await click_cell(Vector2(landed))
    else:
        landed = focus[0] + Vector2i(1, 0)
        await key(KEY_RIGHT)
        require(overlay_cells("target_focus") == [landed], "Right moves the landing square: " + locale)
        await key(KEY_SPACE)
    require(combat.selected_character_cell() == landed, "The Medic lands there: " + locale + " " + str(combat.selected_character_cell()))
    require(overlay_cells("target_focus").is_empty() and item_index(tr("Land")) < 0, "Landed, it is no longer flying: " + locale)

func run_checks() -> void:
    if not FileAccess.file_exists(fixtures.path_join("chaff.save")):
        print("Skipped: Countdown to Doomsday's fixture was not written (its files are not here).")
        quit(0)
        return
    var grenade := FileAccess.get_file_as_string(fixtures.path_join("chaff-name.txt"))
    require(not grenade.is_empty() and grenade != "?", "The grenade's name is known")
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        current_scene.set_process(false)
        root.size = Vector2i(1120, 800)
        await settle()
        await check_launch(locale, locale == "es", grenade)
        await check_jetpack(locale, locale == "es")
    TranslationServer.set_locale("en")
    print("Buck Rogers combat view checks passed")
    quit(0)
