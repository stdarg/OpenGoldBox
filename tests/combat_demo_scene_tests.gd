extends SceneTree

var failed := false

func _initialize() -> void:
    call_deferred("check_demo")

func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message)
        failed = true

# The demo's Criminal heroes hold Alert, so each may swap Initiative before the
# first turn; keeping it lets Dorian Nightwind act first, as these checks expect.
func keep_initiative(combat: Node) -> void:
    var dialog: Window = combat.get_node("InitiativeChoice")
    while dialog.visible:
        dialog.get_node("Keep").pressed.emit()
        for frame in range(4):
            await process_frame


func check_sprite_geometry(canvas: Control) -> void:
    var image := Image.create(24, 24, false, Image.FORMAT_RGBA8)
    var texture := ImageTexture.create_from_image(image)
    for visible in [Rect2(2, 1, 20, 22), Rect2(1, 3, 22, 19), Rect2(6, 0, 12, 24)]:
        for tile in [2.4, 24.0, 60.0, 72.0, 240.0]:
            var occupied := Rect2(Vector2(4.0, 5.0) * tile, Vector2.ONE * tile)
            var goliath: Rect2 = canvas.call("_sprite_rect", texture, visible, occupied, tile,
                true)
            var rendered := Rect2(goliath.position + visible.position * goliath.size / 24.0,
                visible.size * goliath.size / 24.0)
            require(rendered.size.is_equal_approx(Vector2(tile, 1.25 * tile)),
                "Goliath visible art spans one cell by 1.25 cells")
            require(is_equal_approx(rendered.position.x, occupied.position.x) and
                is_equal_approx(rendered.end.y, occupied.end.y),
                "Goliath visible feet align with the occupied cell")
            var normal: Rect2 = canvas.call("_sprite_rect", texture, visible, occupied, tile,
                false)
            var normal_art := Rect2(normal.position + visible.position * normal.size / 24.0,
                visible.size * normal.size / 24.0)
            require(normal_art.get_center().is_equal_approx(occupied.get_center()),
                "Normal visible art stays centered despite transparent padding")
            require(is_equal_approx(maxf(normal_art.size.x, normal_art.size.y), 0.9 * tile),
                "Normal visible art fills 90 percent of its cell")
    require(canvas.call("_sprite_rect", texture, Rect2(), Rect2(0, 0, 24, 24), 24.0, true) ==
        Rect2(), "Empty visible art does not divide by zero")
    # Tall original art (a troll's 24 by 48) fills the two squares it stands
    # on, one above the other (FOOT-1).
    var tall_texture := ImageTexture.create_from_image(Image.create(24, 48, false,
        Image.FORMAT_RGBA8))
    for tile in [24.0, 72.0]:
        var squares := Rect2(Vector2(4.0, 5.0) * tile, Vector2(1.0, 2.0) * tile)
        var tall: Rect2 = canvas.call("_sprite_rect", tall_texture, Rect2(0, 0, 24, 48), squares,
            tile, false)
        require(tall.get_center().is_equal_approx(squares.get_center()) and
            is_equal_approx(tall.size.y, 0.9 * 2.0 * tile),
            "Tall art fills 90 percent of its two squares, centered on them")
    # New monster art (#274) stands as the Combat Sprite Catalog previews it:
    # 1.45 art squares for a Normal humanoid, centered, feet on the cell's edge.
    var art := ImageTexture.create_from_image(Image.create(128, 128, false, Image.FORMAT_RGBA8))
    for tile in [24.0, 72.0]:
        var occupied := Rect2(Vector2(4.0, 5.0) * tile, Vector2.ONE * tile)
        var planted: Rect2 = canvas.call("_sprite_rect", art, Rect2(0, 0, 128, 128), occupied,
            tile, false, 1.45, 120.0)
        require(is_equal_approx(planted.size.x, 1.45 * tile) and
            is_equal_approx(planted.get_center().x, occupied.get_center().x),
            "New art is drawn at its fixed scale, centered on its cell")
        require(is_equal_approx(planted.position.y + 120.0 * planted.size.y / 128.0, occupied.end.y),
            "New art's feet stand on the cell's bottom edge")
        # A creature filling two squares one above the other stands on the
        # lower one, at the catalog's scale for its footprint.
        var squares := Rect2(Vector2(4.0, 5.0) * tile, Vector2(1.0, 2.0) * tile)
        var troll: Rect2 = canvas.call("_sprite_rect", art, Rect2(0, 0, 128, 128), squares,
            tile, false, 2.0, 120.0)
        require(is_equal_approx(troll.size.x, 2.0 * tile) and
            is_equal_approx(troll.get_center().x, squares.get_center().x) and
            is_equal_approx(troll.position.y + 120.0 * troll.size.y / 128.0, squares.end.y),
            "New art of a tall creature stands on its lower square's bottom edge")

func check_demo() -> void:
    root.size = Vector2i(1920, 1080)
    # This window takes the keyboard focus when it opens, so keys and clicks meant
    # for another app would reach it. Only the check's own pushed input drives it.
    DisplayServer.window_set_input_event_callback(func(_event: InputEvent) -> void: pass)
    # A fresh checkout has no build/checks folder for the screenshots yet.
    var checks := ProjectSettings.globalize_path("res://../../../build/checks")
    DirAccess.make_dir_recursive_absolute(checks)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    for frame in range(8):
        await process_frame
    var combat := current_scene
    await keep_initiative(combat)
    check_sprite_geometry(combat.get_node("BattlefieldScroll/Canvas"))
    require(not combat.has_node("Title") and not combat.has_node("Subtitle"),
        "Demo uses the header-free campaign combat layout")
    require(combat.get_node("BattlefieldScroll").position.y == 16,
        "Demo battlefield reaches the top of the combat screen")
    require(is_equal_approx(combat.get_node("BattlefieldScroll").size.y, (combat.size.y - 180) * 0.85),
        "Demo uses the shorter shared battlefield layout")
    require(combat.get_node("ZoomLevel").text == "100%", "Demo shows the combat zoom level")
    var party_rows: Control = combat.get_node("PartyRowsBounds")
    require(party_rows.visible and party_rows.call("slot_at_root", party_rows.position + Vector2(4, 4)) == 0,
        "Party rows remain visible and the first row is selectable")
    require(party_rows.call("slot_at_root", party_rows.position + Vector2(4, party_rows.size.y - 4)) == 7,
        "The last party row remains selectable")
    require(combat.get_node("Footer").text.contains("Shift+arrow: diagonal"),
        "Demo explains the combat keyboard controls")
    require(not combat.get_node("StatusStack/Roster").visible and not combat.get_node("StatusStack/Turn").visible,
        "The upper-right text window is removed")
    require(combat.get_node("LogStack/LogHeader").text.contains("Dorian Nightwind turn"),
        "Turn text moved below the battlefield")
    require(combat.get_node("LogStack/Log").text.contains("The original script has requested combat."),
        "Demo uses the campaign encounter log presentation")
    require(combat.get_node("End").visible and not combat.get_node("End").disabled,
        "Active party turn has a visible End turn button")
    require(combat.get_node("End").position.y < combat.get_node("LogStack/Log").global_position.y,
        "Turn control sits above the combat log without covering portraits")
    require(not combat.get_node("React").visible and not combat.get_node("Decline").visible,
        "Reaction choices stay hidden until a reaction is pending")
    require(combat.get_node("ZoomLevel").text == "100%", "Showcase uses 100 percent zoom")
    # A hidden or minimized window skips drawing, so frame_post_draw may never
    # come; draw explicitly after one processed frame.
    await process_frame
    RenderingServer.force_draw(false)
    var screenshot := root.get_texture().get_image()
    require(screenshot.get_pixel(1015, 415).r > screenshot.get_pixel(1140, 415).r,
        "Reachable movement squares visibly lighten in the surrounded demo")
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = Vector2(1550, 90)
    root.push_input(click)
    await process_frame
    RenderingServer.force_draw(false)
    var other_selected := root.get_texture().get_image()
    require(combat.selected_character_id() == 1 and combat.selected_character_cell() == Vector2i(5, 5),
        "Selecting the portrait row selects its party character")
    require(other_selected.get_pixel(1540, 65).r > screenshot.get_pixel(1540, 65).r,
        "Selected portrait row is visibly highlighted")
    require(other_selected.get_pixel(1050, 415).r < 0.25,
        "Off-turn portrait selection shows no movement highlights")
    require(combat.get_node("LogStack/LogHeader").text.contains("It is not Liora Hallowgrove's turn."),
        "Off-turn selection explains why movement is unavailable")
    var off_turn_key := InputEventKey.new()
    off_turn_key.keycode = KEY_RIGHT
    off_turn_key.pressed = true
    root.push_input(off_turn_key)
    require(combat.selected_character_cell() == Vector2i(5, 5),
        "Arrow keys do not bypass the selected character's turn")
    click.position = Vector2(940, 460)
    root.push_input(click)
    await process_frame
    RenderingServer.force_draw(false)
    screenshot = root.get_texture().get_image()
    require(combat.selected_character_id() == 3 and combat.selected_character_cell() == Vector2i(7, 5),
        "Selecting the combat sprite selects the same active party character")
    require(screenshot.get_pixel(1015, 415).r > screenshot.get_pixel(1140, 415).r,
        "Sprite selection shows the active hero's legal movement squares")
    var blocked_key := InputEventKey.new()
    blocked_key.keycode = KEY_LEFT
    blocked_key.pressed = true
    root.push_input(blocked_key)
    require(combat.get_node("LogStack/Log").text.contains("That square is occupied."),
        "Blocked movement explains why the hero cannot enter an ally's square")
    var active_key := InputEventKey.new()
    active_key.keycode = KEY_RIGHT
    active_key.pressed = true
    root.push_input(active_key)
    require(combat.get_node("LogStack/LogHeader").text.contains("reaction"),
        "Arrow key starts the selected character's legal move and handles enemy reactions")
    for frame in range(180):
        if combat.selected_character_cell() == Vector2i(8, 5):
            break
        await create_timer(0.02).timeout
    require(combat.selected_character_cell() == Vector2i(8, 5),
        "Arrow key moves the selected character on their turn")
    require(combat.get_node("EffectAudio").playing,
        "Moving to a new square plays the original footstep sound")
    require(is_equal_approx(combat.get_node("EffectAudio").volume_linear, 0.125),
        "Movement effects use one-eighth volume")
    require(is_equal_approx(combat.get_node("DeathAudio").volume_linear, 0.125),
        "Death effects use one-eighth volume")
    root.push_input(active_key)
    require(combat.selected_character_cell() == Vector2i(8, 5),
        "Arrow key attacks the adjacent enemy without moving into its square")
    require(combat.attack_pose_active(3), "Attack starts the hero action pose")
    require(not combat.sprite_facing_left(3), "Hero faces right when attacking the Kobold to the right")
    require(combat.get_node("AttackAudio").playing, "Attack plays the original attack sound")
    require(is_equal_approx(combat.get_node("AttackAudio").volume_linear, 0.125),
        "Attack effects use one-eighth volume")
    require(combat.get_node("LogStack/Log").text.contains("Dorian Nightwind -> Kobold"),
        "Arrow key submits a melee attack against the occupied enemy square")
    require(combat.get_node("StatusStack/Turn").text.contains("Dorian Nightwind turn") and not combat.get_node("End").disabled,
        "Attack preserves the hero's remaining turn and explicit End control")
    await process_frame
    RenderingServer.force_draw(false)
    var attack_screenshot := root.get_texture().get_image()
    require(attack_screenshot.save_png(ProjectSettings.globalize_path("res://../../../build/checks/combat-attack.png")) == OK,
        "Action pose screenshot saves")
    await create_timer(1.1).timeout
    require(not combat.attack_pose_active(3), "Action pose ends after one second")
    for frame in range(500):
        if combat.get_node("LogStack/Log").text.contains("Kobold 7 -> Dorian Nightwind"):
            break
        # Dorian's Stone's Endurance asks when a Kobold hits; it is declined.
        var decline: Button = combat.get_node("Decline")
        if decline.visible and not decline.disabled:
            decline.pressed.emit()
            await create_timer(0.02).timeout
            continue
        var end_key := InputEventKey.new()
        end_key.keycode = KEY_ENTER
        end_key.pressed = true
        root.push_input(end_key)
        await create_timer(0.02).timeout
    require(combat.get_node("LogStack/Log").text.contains("Kobold 7 -> Dorian Nightwind"),
        "Kobold on the right attacks the hero to its left")
    require(combat.sprite_facing_left(1007), "Kobold faces left toward its attack target")
    await process_frame
    RenderingServer.force_draw(false)
    require(root.get_texture().get_image().save_png(ProjectSettings.globalize_path("res://../../../build/checks/combat-left-facing.png")) == OK,
        "Left-facing attack screenshot saves")
    var output := ProjectSettings.globalize_path("res://../../../build/checks/combat-demo.png")
    require(screenshot.save_png(output) == OK, "Showcase screenshot saves")
    # Use the normal campaign formation to cross two allies via a free target.
    change_scene_to_file("res://scenes/combat_demo.tscn")
    for frame in range(8):
        await process_frame
    combat = current_scene
    await keep_initiative(combat)
    require(combat.selected_character_id() == 3, "Reset demo begins with the Cleric's normal turn")
    var movement_pattern := RegEx.new()
    movement_pattern.compile("Move ([0-9]+) ft")
    var initial_movement := int(movement_pattern.search(combat.get_node("StatusStack/Turn").text).get_string(1))
    combat.get_node("Disengage").pressed.emit()
    combat.get_node("Move").pressed.emit()
    var canvas: Control = combat.get_node("BattlefieldScroll/Canvas")
    var transit_scroll: ScrollContainer = combat.get_node("BattlefieldScroll")
    var destination := Vector2(4.5, 5.5) * (canvas.size.x / 12.0)
    transit_scroll.scroll_horizontal = int(destination.x - transit_scroll.size.x / 2)
    transit_scroll.scroll_vertical = int(destination.y - transit_scroll.size.y / 2)
    for frame in range(4):
        await process_frame
    click.position = canvas.get_global_transform_with_canvas() * destination
    root.push_input(click)
    require(combat.selected_character_cell() == Vector2i(4, 5),
        "Clicking a free square moves the active character through two allied spaces")
    require(combat.get_node("StatusStack/Turn").text.contains("Move %d ft" % (initial_movement - 15)) and combat.get_node("StatusStack/Turn").text.contains("Action spent"),
        "Transit costs fifteen feet and preserves the spent Disengage action")
    root.push_input(active_key)
    require(combat.selected_character_cell() == Vector2i(4, 5) and combat.get_node("LogStack/Log").text.contains("That square is occupied."),
        "A one-square arrow cannot voluntarily end movement on an ally")
    require(combat.get_node("End").visible and not combat.get_node("End").disabled,
        "Allied transit keeps the existing End Turn control available")
    if failed:
        quit(1)
    else:
        print("Combat demo checks passed: ", output)
        quit(0)
