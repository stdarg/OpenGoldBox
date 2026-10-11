extends SceneTree

# Exercise the game combat canvas through real Godot input and container layout.
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

func mouse_button(position: Vector2, button: MouseButton, pressed: bool, shift := false) -> void:
    var event := InputEventMouseButton.new()
    event.position = position
    event.button_index = button
    event.pressed = pressed
    event.shift_pressed = shift
    root.push_input(event)

func movement_key(code: Key, shift := false) -> void:
    var event := InputEventKey.new()
    event.keycode = code
    event.pressed = true
    event.shift_pressed = shift
    root.push_input(event)

func run_checks() -> void:
    var config_path := ProjectSettings.globalize_path("res://settings.cfg")
    var had_config := FileAccess.file_exists(config_path)
    var original_config := FileAccess.get_file_as_bytes(config_path) if had_config else PackedByteArray()
    if had_config:
        require(DirAccess.remove_absolute(config_path) == OK, "Isolate combat zoom test config")
    root.size = Vector2i(1920, 1080)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    var scroll: ScrollContainer = combat.get_node("BattlefieldScroll")
    var canvas: Control = scroll.get_node("Canvas")
    require(ProjectSettings.get_setting("opengold/combat_zoom") == 100, "Combat zoom defaults to 100 in project config")
    require(scroll.size.x > combat.size.x * 0.7, "Battlefield viewport fills the left side")
    require(scroll.position.y == 16, "Battlefield begins at the top of the screen")
    require(is_equal_approx(scroll.size.y, (combat.size.y - 180) * 0.85),
        "Battlefield is 15 percent shorter to make room for the combat log")
    require(combat.get_node("LogStack/Log").global_position.y > scroll.position.y + scroll.size.y,
        "Combat log occupies the space below the battlefield")
    require(not combat.has_node("Title") and not combat.has_node("Subtitle"),
        "Combat header and instruction text are removed from the shared scene")
    require(combat.get_node("ZoomLevel").text == "100%", "Zoom readout shows the initial level")
    var base_tile := maxf(scroll.size.x / 12.0, scroll.size.y / 9.0)
    require(canvas.custom_minimum_size.is_equal_approx(Vector2(12, 9) * base_tile),
        "Battlefield uses configured 100% zoom")
    require(scroll.clip_contents, "Magnified battlefield must be clipped")
    require(scroll.scroll_horizontal >= 0 and scroll.scroll_vertical >= 0, "Initial battlefield scroll stays in bounds")
    var action_before: String = combat.get_node("Prompt").text
    var action_key := InputEventKey.new()
    action_key.keycode = KEY_A
    action_key.pressed = true
    root.push_input(action_key)
    require(combat.get_node("Prompt").text != action_before,
        "Keyboard action cycling remains available after removing the action panel")
    combat.get_node("Move").pressed.emit()
    var origin: Vector2i = combat.selected_character_cell()
    movement_key(KEY_RIGHT, true)
    require(combat.selected_character_cell() == origin + Vector2i(1, 1), "Shift+Right rotates to southeast")
    movement_key(KEY_DOWN, true)
    require(combat.selected_character_cell() == origin + Vector2i(0, 2), "Shift+Down rotates to southwest")
    movement_key(KEY_LEFT, true)
    require(combat.selected_character_cell() == origin + Vector2i(-1, 1), "Shift+Left rotates to northwest")
    movement_key(KEY_UP, true)
    require(combat.selected_character_cell() == origin, "Shift+Up rotates to northeast")
    combat.get_node("Training").pressed.emit()
    origin = combat.selected_character_cell()
    movement_key(KEY_KP_1)
    require(combat.selected_character_cell() == origin + Vector2i(-1, 1), "Numpad 1 moves southwest")
    movement_key(KEY_KP_3)
    require(combat.selected_character_cell() == origin + Vector2i(0, 2), "Numpad 3 moves southeast")
    movement_key(KEY_KP_7)
    require(combat.selected_character_cell() == origin + Vector2i(-1, 1), "Numpad 7 moves northwest")
    movement_key(KEY_KP_9)
    require(combat.selected_character_cell() == origin, "Numpad 9 moves northeast")
    combat.get_node("Training").pressed.emit()
    origin = combat.selected_character_cell()
    movement_key(KEY_DELETE)
    require(combat.selected_character_cell() == origin + Vector2i(-1, 1), "Delete moves southwest")
    movement_key(KEY_PAGEDOWN)
    require(combat.selected_character_cell() == origin + Vector2i(0, 2), "Page Down moves southeast")
    movement_key(KEY_INSERT)
    require(combat.selected_character_cell() == origin + Vector2i(-1, 1), "Insert moves northwest")
    movement_key(KEY_PAGEUP)
    require(combat.selected_character_cell() == origin, "Page Up moves northeast")
    combat.get_node("ZoomIn100").pressed.emit()
    await settle()
    scroll.scroll_horizontal = 120
    scroll.scroll_vertical = 120
    await settle()
    var point := scroll.global_position + scroll.size / 2
    mouse_button(point, MOUSE_BUTTON_MIDDLE, true)
    var motion := InputEventMouseMotion.new()
    motion.position = point + Vector2(40, 30)
    motion.relative = Vector2(40, 30)
    motion.button_mask = MOUSE_BUTTON_MASK_MIDDLE
    root.push_input(motion)
    mouse_button(motion.position, MOUSE_BUTTON_MIDDLE, false)
    require(scroll.scroll_horizontal == 80 and scroll.scroll_vertical == 90, "Middle drag must pan both axes")
    await settle()
    require(scroll.scroll_horizontal == 80 and scroll.scroll_vertical == 90, "Manual pan must persist during the same turn")
    mouse_button(point, MOUSE_BUTTON_WHEEL_DOWN, true)
    mouse_button(point, MOUSE_BUTTON_WHEEL_DOWN, false)
    require(scroll.scroll_vertical > 90, "Mouse wheel must scroll vertically")
    mouse_button(point, MOUSE_BUTTON_WHEEL_DOWN, true, true)
    mouse_button(point, MOUSE_BUTTON_WHEEL_DOWN, false, true)
    require(scroll.scroll_horizontal > 80, "Shift-wheel must scroll horizontally")
    var hbar := scroll.get_h_scroll_bar()
    require(hbar.focus_mode == Control.FOCUS_ALL, "Horizontal scrollbar must support keyboard focus")
    hbar.grab_focus()
    var cell_before: Vector2i = combat.selected_character_cell()
    movement_key(KEY_RIGHT)
    require(combat.selected_character_cell() == cell_before + Vector2i(1, 0),
        "Arrow key moves selected character even when a scrollbar has focus")
    scroll.scroll_horizontal = 100000
    scroll.scroll_vertical = 100000
    await settle()
    require(scroll.scroll_horizontal <= canvas.size.x and scroll.scroll_vertical <= canvas.size.y, "Scrolling must clamp to battlefield bounds")
    var before := Vector2(scroll.scroll_horizontal, scroll.scroll_vertical)
    mouse_button(combat.get_node("StatusStack/Turn").global_position, MOUSE_BUTTON_WHEEL_UP, true)
    require(Vector2(scroll.scroll_horizontal, scroll.scroll_vertical) == before, "Wheel outside battlefield must not pan it")
    combat.get_node("ZoomOut100").pressed.emit()
    await settle()
    root.size = Vector2i(1120, 800)
    await settle()
    base_tile = maxf(scroll.size.x / 12.0, scroll.size.y / 9.0)
    require(canvas.custom_minimum_size.is_equal_approx(Vector2(12, 9) * base_tile),
        "Resize preserves configured magnification")
    combat.get_node("ZoomIn10").pressed.emit()
    await settle()
    require(canvas.custom_minimum_size.is_equal_approx(Vector2(12, 9) * base_tile * 1.1),
        "+10% button scales the shared battlefield")
    require(combat.get_node("ZoomLevel").text == "110%", "Zoom readout follows the +10% control")
    combat.get_node("ZoomIn100").pressed.emit()
    await settle()
    require(canvas.custom_minimum_size.is_equal_approx(Vector2(12, 9) * base_tile * 2.1),
        "+100% button scales the shared battlefield")
    require(combat.get_node("ZoomLevel").text == "210%", "Zoom readout follows the +100% control")
    combat.get_node("ZoomOut100").pressed.emit()
    combat.get_node("ZoomOut10").pressed.emit()
    await settle()
    require(canvas.custom_minimum_size.is_equal_approx(Vector2(12, 9) * base_tile),
        "Zoom buttons reverse to the 100% default")
    if OS.get_cmdline_user_args().has("--capture"):
        root.size = Vector2i(1920, 1080)
        await settle()
        await RenderingServer.frame_post_draw
        var path := ProjectSettings.globalize_path("res://../../../build/checks/combat-zoom.png")
        DirAccess.make_dir_recursive_absolute(path.get_base_dir())
        require(root.get_texture().get_image().save_png(path) == OK, "Capture failed")
    # Keep the real enemy controller from racing turn/resource assertions.
    combat.set_process(false)
    combat.get_node("Training").pressed.emit()
    await settle()
    origin = combat.selected_character_cell()
    var attacker: int = combat.selected_character_id()
    combat.get_node("Ranged").pressed.emit()
    var target := Vector2(9.5, 4.5) * (canvas.size.x / 12.0)
    scroll.scroll_horizontal = int(target.x - scroll.size.x / 2)
    scroll.scroll_vertical = int(target.y - scroll.size.y / 2)
    await settle()
    var target_point := canvas.get_global_transform_with_canvas() * target
    mouse_button(target_point, MOUSE_BUTTON_LEFT, true)
    mouse_button(target_point, MOUSE_BUTTON_LEFT, false)
    require(combat.attack_pose_active(attacker), "Ranged input actually performs the attack")
    require(combat.get_node("Ranged").disabled, "Attack spends the action in the real scene")
    require(not combat.get_node("End").disabled,
        "Player retains an explicit End Turn command after attacking")
    movement_key(KEY_RIGHT)
    require(combat.selected_character_cell() == origin + Vector2i(1, 0),
        "Keyboard movement works after attacking in the same turn")
    movement_key(KEY_ENTER)
    require(combat.get_node("End").disabled, "Enter explicitly hands control to the enemy")
    # Resume the production AI: a ranged attack must now be followed by its
    # own End Turn command, returning control to the player without a stall.
    combat.set_process(true)
    for frame in range(600):
        await process_frame
        if not combat.get_node("End").disabled:
            break
    require(not combat.get_node("End").disabled,
        "Enemy AI finishes its retained turn and returns control to the player")
    var config := ConfigFile.new()
    config.set_value("combat", "combat_zoom", 300)
    require(config.save(config_path) == OK, "Prepare portable combat zoom setting")
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var alternate_scroll: ScrollContainer = current_scene.get_node("BattlefieldScroll")
    var alternate_canvas: Control = alternate_scroll.get_node("Canvas")
    base_tile = maxf(alternate_scroll.size.x / 12.0, alternate_scroll.size.y / 9.0)
    require(alternate_canvas.custom_minimum_size.is_equal_approx(Vector2(12, 9) * base_tile * 3),
        "Combat reads 300% from settings.cfg")
    var log_view: RichTextLabel = current_scene.get_node("LogStack/Log")
    log_view.size.y = 60
    current_scene.get_node("Move").pressed.emit()
    await settle()
    var log_scroll := log_view.get_v_scroll_bar()
    require(log_scroll.max_value > log_scroll.page, "Combat log can scroll when text exceeds its window")
    log_view.scroll_to_line(log_view.get_line_count() - 1)
    current_scene.get_node("Move").pressed.emit()
    await settle()
    require(log_scroll.value >= log_scroll.max_value - log_scroll.page - 2,
        "Combat log follows new text while the player is at the bottom")
    log_scroll.value = 0
    current_scene.get_node("Move").pressed.emit()
    await settle()
    require(log_scroll.value == 0, "Combat log stays in place after the player scrolls up")
    if had_config:
        var file := FileAccess.open(config_path, FileAccess.WRITE)
        require(file != null, "Restore original config")
        file.store_buffer(original_config)
        file.close()
    else:
        require(DirAccess.remove_absolute(config_path) == OK, "Remove temporary config")
    print("Game combat view checks passed: zoom, input, attack then move, explicit End Turn, enemy completion")
    quit(0)
