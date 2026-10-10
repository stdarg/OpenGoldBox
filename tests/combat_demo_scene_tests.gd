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
    require(not combat.has_node("Title") and not combat.has_node("Subtitle"),
        "Demo uses the header-free campaign combat layout")
    require(combat.get_node("BattlefieldScroll").position.y == 16,
        "Demo battlefield reaches the top of the combat screen")
    require(is_equal_approx(combat.get_node("BattlefieldScroll").size.y, (combat.size.y - 180) * 0.85),
        "Demo uses the shorter shared battlefield layout")
    require(combat.get_node("ZoomLevel").text == "100%", "Demo shows the combat zoom level")
    require(combat.get_node("Footer").text.contains("Shift+arrow: diagonal"),
        "Demo explains the combat keyboard controls")
    require(not combat.get_node("Roster").visible and not combat.get_node("Turn").visible,
        "The upper-right text window is removed")
    require(combat.get_node("LogHeader").text.contains("Dorian Nightwind turn"),
        "Turn text moved below the battlefield")
    require(combat.get_node("Log").text.contains("The original script has requested combat."),
        "Demo uses the campaign encounter log presentation")
    require(combat.get_node("End").visible and not combat.get_node("End").disabled,
        "Active party turn has a visible End turn button")
    require(combat.get_node("End").position.y < combat.get_node("Log").position.y,
        "Turn control sits above the combat log without covering portraits")
    require(not combat.get_node("React").visible and not combat.get_node("Decline").visible,
        "Reaction choices stay hidden until a reaction is pending")
    require(combat.get_node("ZoomLevel").text == "100%", "Showcase uses 100 percent zoom")
    await RenderingServer.frame_post_draw
    var screenshot := root.get_texture().get_image()
    require(screenshot.get_pixel(1015, 415).r > screenshot.get_pixel(1140, 415).r,
        "Reachable movement squares visibly lighten in the surrounded demo")
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = Vector2(1550, 90)
    root.push_input(click)
    await RenderingServer.frame_post_draw
    var other_selected := root.get_texture().get_image()
    require(combat.selected_character_id() == 1 and combat.selected_character_cell() == Vector2i(5, 5),
        "Selecting the portrait row selects its party character")
    require(other_selected.get_pixel(1540, 65).r > screenshot.get_pixel(1540, 65).r,
        "Selected portrait row is visibly highlighted")
    require(other_selected.get_pixel(1050, 415).r < 0.25,
        "Off-turn portrait selection shows no movement highlights")
    require(combat.get_node("LogHeader").text.contains("It is not Liora Hallowgrove's turn."),
        "Off-turn selection explains why movement is unavailable")
    var off_turn_key := InputEventKey.new()
    off_turn_key.keycode = KEY_RIGHT
    off_turn_key.pressed = true
    root.push_input(off_turn_key)
    require(combat.selected_character_cell() == Vector2i(5, 5),
        "Arrow keys do not bypass the selected character's turn")
    click.position = Vector2(940, 460)
    root.push_input(click)
    await RenderingServer.frame_post_draw
    screenshot = root.get_texture().get_image()
    require(combat.selected_character_id() == 3 and combat.selected_character_cell() == Vector2i(7, 5),
        "Selecting the combat sprite selects the same active party character")
    require(screenshot.get_pixel(1015, 415).r > screenshot.get_pixel(1140, 415).r,
        "Sprite selection shows the active hero's legal movement squares")
    var blocked_key := InputEventKey.new()
    blocked_key.keycode = KEY_LEFT
    blocked_key.pressed = true
    root.push_input(blocked_key)
    require(combat.get_node("Log").text.contains("That square is occupied."),
        "Blocked movement explains why the hero cannot enter an ally's square")
    var active_key := InputEventKey.new()
    active_key.keycode = KEY_RIGHT
    active_key.pressed = true
    root.push_input(active_key)
    require(combat.get_node("LogHeader").text.contains("reaction"),
        "Arrow key starts the selected character's legal move and handles enemy reactions")
    for frame in range(180):
        if combat.selected_character_cell() == Vector2i(8, 5):
            break
        await process_frame
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
    require(combat.get_node("Log").text.contains("Dorian Nightwind -> Kobold"),
        "Arrow key submits a melee attack against the occupied enemy square")
    require(combat.get_node("Turn").text.contains("Dorian Nightwind turn") and not combat.get_node("End").disabled,
        "Attack preserves the hero's remaining turn and explicit End control")
    await RenderingServer.frame_post_draw
    var attack_screenshot := root.get_texture().get_image()
    require(attack_screenshot.save_png(ProjectSettings.globalize_path("res://../../../build/checks/combat-attack.png")) == OK,
        "Action pose screenshot saves")
    await create_timer(1.1).timeout
    require(not combat.attack_pose_active(3), "Action pose ends after one second")
    for frame in range(500):
        if combat.get_node("Log").text.contains("Kobold 7 -> Dorian Nightwind"):
            break
        var end_key := InputEventKey.new()
        end_key.keycode = KEY_ENTER
        end_key.pressed = true
        root.push_input(end_key)
        await process_frame
    require(combat.get_node("Log").text.contains("Kobold 7 -> Dorian Nightwind"),
        "Kobold on the right attacks the hero to its left")
    require(combat.sprite_facing_left(1007), "Kobold faces left toward its attack target")
    await RenderingServer.frame_post_draw
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
    var initial_movement := int(movement_pattern.search(combat.get_node("Turn").text).get_string(1))
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
    require(combat.get_node("Turn").text.contains("Move %d ft" % (initial_movement - 15)) and combat.get_node("Turn").text.contains("Action spent"),
        "Transit costs fifteen feet and preserves the spent Disengage action")
    root.push_input(active_key)
    require(combat.selected_character_cell() == Vector2i(4, 5) and combat.get_node("Log").text.contains("That square is occupied."),
        "A one-square arrow cannot voluntarily end movement on an ally")
    require(combat.get_node("End").visible and not combat.get_node("End").disabled,
        "Allied transit keeps the existing End Turn control available")
    if failed:
        quit(1)
    else:
        print("Combat demo checks passed: ", output)
        quit(0)
