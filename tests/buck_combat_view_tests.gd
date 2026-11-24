extends SceneTree
# A Buck Rogers ground fight on the combat screen (docs/BUCK-ROGERS.md):
# Launch or throw aims a readied chaff grenade at a square chosen with the
# keyboard or the mouse, the chaff shows where it lies and the grenade is
# spent; a companion Leadership leads is the player's on its turn. Run with
# --buck-fixtures=<folder of chaff.save and led_companion.save>.
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

func check_launch(locale: String, by_mouse: bool) -> void:
    await load_fixture("chaff.save")
    var combat := current_scene
    var launch := tr("Launch or throw")
    var index := item_index(launch)
    require(index >= 0, "The readied grenade offers Launch or throw: " + locale)
    require(overlay_cells("obscured").is_empty(), "No chaff before the throw: " + locale)
    var items: OptionButton = combat.get_node("ItemAction")
    items.select(index); items.item_selected.emit(index); await settle()
    combat.get_node("UseItemAction").pressed.emit(); await settle()
    require(combat.get_node("Prompt").text == tr("Selected: Launch or throw. Click a highlighted square, or Arrows: square | Space: launch | Escape: cancel"),
        "The prompt asks for a square: " + locale)
    require(overlay_cells("target_focus") == [Vector2i(4, 4)],
        "The chosen square starts on the trooper: " + locale)
    require(overlay_cells("target").size() > 4 and not overlay_cells("target").has(Vector2i(1, 4)),
        "Every square offered is highlighted, never the thrower's own: " + locale)
    await key(KEY_LEFT)
    require(overlay_cells("target_focus") == [Vector2i(3, 4)], "Left moves the chosen square: " + locale)
    await key(KEY_ESCAPE)
    require(overlay_cells("target").is_empty() and item_index(launch) >= 0,
        "Escape cancels without throwing: " + locale)
    combat.get_node("UseItemAction").pressed.emit(); await settle()
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

func check_led_companion(locale: String) -> void:
    await load_fixture("led_companion.save")
    var combat := current_scene
    var name := "RAM TROOPER 2000"
    var computer := tr("{name} fights under computer control (Quick). Space or Take control: play the party yourself.").format({"name": name})
    require(log_text().contains(tr("{leader} leads {name}.").format({"leader": "BUCK", "name": name})),
        "Leadership leads the companion: " + locale)
    require(combat.get_node("Prompt").text != computer, "The led companion is not on Quick: " + locale)
    require(not combat.get_node("Move").disabled and not combat.get_node("End").disabled,
        "The player commands the led companion: " + locale)
    require(combat.selected_character_id() == 2000, "The led companion is selected for its turn: " + locale)

func run_checks() -> void:
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        current_scene.set_process(false)
        root.size = Vector2i(1120, 800)
        await settle()
        await check_launch(locale, locale == "es")
        await check_led_companion(locale)
    TranslationServer.set_locale("en")
    print("Buck Rogers combat view checks passed")
    quit(0)
