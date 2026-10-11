extends SceneTree

var saved_files := {}

func _initialize() -> void:
    call_deferred("run_checks")

func restore_files() -> void:
    for path in saved_files:
        if saved_files[path] == null:
            if FileAccess.file_exists(path):
                DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            if file:
                file.store_buffer(saved_files[path])
                file.close()
    saved_files.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4):
        await process_frame

func run_checks() -> void:
    for suffix in ["", ".bak"]:
        var path := ProjectSettings.globalize_path("user://checks/combat.save" + suffix)
        saved_files[path] = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    combat.set_process(false) # Keep the real enemy AI from racing UI assertions.
    var blindness: Button = combat.get_node("Blindness")
    var roster: RichTextLabel = combat.get_node("StatusStack/Roster")
    var log: RichTextLabel = combat.get_node("Log")
    require(not blindness.disabled, "Prepared fixture offers Blindness")
    require(blindness.focus_mode == Control.FOCUS_ALL, "Blindness has keyboard focus")
    require(blindness.tooltip_text.contains("level 2"), "Control explains its spell-slot cost")
    for size in [Vector2i(1120, 800), Vector2i(1280, 900)]:
        root.size = size
        await settle()
        require(blindness.get_global_rect().end.x <= size.x, "Blindness stays inside window")
        require(not blindness.get_global_rect().intersects(combat.get_node("SpellSlot").get_global_rect()), "Spell buttons do not overlap")
        require(not (combat.get_node("End").visible and combat.get_node("React").visible and combat.get_node("End").get_global_rect().intersects(combat.get_node("React").get_global_rect())), "Visible turn and reaction controls do not overlap")
    blindness.emit_signal("pressed")
    require(combat.get_node("Prompt").text.contains("Blindness"), "Selecting spell changes targeting mode")
    var canvas: Control = combat.get_node("BattlefieldScroll/Canvas")
    var scroll: ScrollContainer = combat.get_node("BattlefieldScroll")
    # Fixture target is (3,4) on the 12-column board. Use the actual transformed
    # canvas coordinates, preserving input checks at the configured combat zoom.
    var local := Vector2(3.5, 4.5) * (canvas.size.x / 12.0)
    scroll.scroll_horizontal = int(local.x - scroll.size.x / 2)
    scroll.scroll_vertical = int(local.y - scroll.size.y / 2)
    await settle()
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = canvas.get_global_transform_with_canvas() * local
    root.push_input(click)
    await settle()
    require(roster.text.contains("Blinded"), "Failed save displays condition beside monster")
    require(log.text.contains("Constitution save") and log.text.contains("DC 13"), "Log explains the saving throw")
    require(blindness.disabled, "Casting spends action and disables another cast")
    require(not combat.get_node("StatusStack/Turn").get_global_rect().intersects(roster.get_global_rect()), "Turn status does not overlap roster")
    require(combat.get_node("StatusStack/Turn").text.contains("L2 slots: 1"), "Level-two slot expenditure is visible")
    var expected_roster := roster.text
    var expected_log := log.text
    combat.get_node("Save").emit_signal("pressed")
    require(combat.get_node("Prompt").text.contains("saved"), "Active condition checkpoint saved")
    combat.get_node("Replay").emit_signal("pressed")
    require(not roster.text.contains("Blinded"), "Restart resets effects")
    combat.get_node("Load").emit_signal("pressed")
    require(roster.text == expected_roster and log.text == expected_log, "Load restores condition and its save log")
    if OS.get_cmdline_user_args().has("--capture"):
        await settle()
        await RenderingServer.frame_post_draw
        var path := ProjectSettings.globalize_path("res://../../../build/checks/conditions.png")
        DirAccess.make_dir_recursive_absolute(path.get_base_dir())
        require(root.get_texture().get_image().save_png(path) == OK, "Condition capture saved")
    restore_files()
    print("Condition view checks passed: targeting, status, save log, checkpoint, layout")
    quit(0)
