extends SceneTree

# Graphical integration using the real character pool, campaign handoff and renderer.
# Requires OPENGOLD_GAME_DIR and the game's imported portrait assets.
func _initialize() -> void:
    call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4):
        await process_frame

func run_checks() -> void:
    root.size = Vector2i(1920, 1080)
    # This window takes the keyboard focus when it opens, so keys and clicks meant
    # for another app would reach it. Only the check's own pushed input drives it.
    DisplayServer.window_set_input_event_callback(func(_event: InputEvent) -> void: pass)
    change_scene_to_file("res://scenes/character_creation.tscn")
    await settle()
    var creation := current_scene as Control
    require(creation.has_node("PartyPanel"), "Original character assets load")
    creation.get_node("Party").pressed.emit()
    creation.get_node("PartyPanel/Pool").pressed.emit()
    var choices: ItemList = creation.get_node("PoolModal/List")
    var selected := false
    for index in range(choices.item_count):
        if not choices.get_item_text(index).begins_with("Fighter / "):
            continue
        choices.select(index)
        choices.item_selected.emit(index)
        if creation.get_node("PoolModal/Text").text.contains("Goliath"):
            selected = true
            break
    require(selected, "Pool supplies a Goliath fighter")
    var original: Image = creation.get_node("PoolModal/Ready").texture.get_image()
    var colors: Array[Color] = []
    for y in range(original.get_height()):
        for x in range(original.get_width()):
            var color := original.get_pixel(x, y)
            if color.a > 0.99 and not colors.has(color):
                colors.append(color)
    creation.get_node("PoolModal/Add").pressed.emit()
    creation.get_node("PoolModal/Close").pressed.emit()
    creation.get_node("PartyPanel/Combat").pressed.emit()
    await settle()
    var combat: Control = creation.get_node("CampaignCombat")
    combat.set_process(false)
    var scroll: ScrollContainer = combat.get_node("BattlefieldScroll")
    scroll.scroll_horizontal = 0
    scroll.scroll_vertical = 0
    await settle()
    await RenderingServer.frame_post_draw
    var rendered := root.get_texture().get_image()
    var canvas: Control = scroll.get_node("Canvas")
    var tile := canvas.size.x / 12.0
    var origin := canvas.global_position
    # The first party member's logical training cell is (1, 1). Its artwork
    # must reach y=0.75 squares; the former 90% centered icon stayed below y=1.
    var overhang_pixels := 0
    for y in range(ceili(origin.y + tile * 0.75), floori(origin.y + tile)):
        for x in range(ceili(origin.x + tile), floori(origin.x + tile * 2)):
            if colors.has(rendered.get_pixel(x, y)):
                overhang_pixels += 1
    require(overhang_pixels > 20, "Actual Goliath artwork renders into the upper square")
    var foot_pixels := 0
    var bottom := floori(origin.y + tile * 2) - 1
    for x in range(ceili(origin.x + tile), floori(origin.x + tile * 2)):
        if colors.has(rendered.get_pixel(x, bottom)):
            foot_pixels += 1
    require(foot_pixels > 0, "Visible feet reach the bottom of the occupied square")
    if OS.get_cmdline_user_args().has("--capture"):
        var screenshots := root.get_node("Screenshots")
        screenshots.request_capture()
        var result: Array = await screenshots.capture_completed
        require(result[1].is_empty(), "Goliath game screenshot saved")
    print("Goliath game rendering checks passed: real campaign species, visible overhang and bottom alignment")
    quit(0)
