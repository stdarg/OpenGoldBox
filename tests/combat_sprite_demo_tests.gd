extends SceneTree

var fixture_dir := ""
var original_game_dir := OS.get_environment("OPENGOLD_GAME_DIR")

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

# Wholly authored indexed pixels exercise real DAX decoding without original art.
func picture(width: int, height: int, frames: int, seed: int, shape := 0) -> PackedByteArray:
    var record := PackedByteArray()
    record.resize(17)
    record.encode_u16(0, height)
    record.encode_u16(2, width / 8)
    record[8] = frames
    for frame in range(frames):
        for y in range(height):
            for x in range(0, width, 2):
                var pair := 0
                for dx in range(2):
                    var pixel := (x + dx + y + seed + frame) % 15 + 1
                    if shape > 0:
                        var inset := 7 if shape == 1 else 2
                        if x + dx < inset or x + dx >= width - inset or y < (3 if shape == 1 else 0):
                            pixel = 0
                    pair = (pair << 4) | pixel
                record.append(pair)
    return record

func write_archive(name: String, records: Dictionary) -> void:
    var header := PackedByteArray()
    header.resize(2 + records.size() * 9)
    header.encode_u16(0, records.size() * 9)
    var payload := PackedByteArray()
    var entry := 2
    for id in records:
        var raw: PackedByteArray = records[id]
        var encoded := PackedByteArray()
        for offset in range(0, raw.size(), 128):
            var length := mini(128, raw.size() - offset)
            encoded.append(length - 1)
            encoded.append_array(raw.slice(offset, offset + length))
        header[entry] = id
        header.encode_u32(entry + 1, payload.size())
        header.encode_u16(entry + 5, raw.size())
        header.encode_u16(entry + 7, encoded.size())
        payload.append_array(encoded)
        entry += 9
    var file := FileAccess.open(fixture_dir.path_join(name), FileAccess.WRITE)
    require(file != null, "Fixture archive opens")
    file.store_buffer(header)
    file.store_buffer(payload)
    file.close()

func create_fixtures() -> void:
    fixture_dir = ProjectSettings.globalize_path("res://../../build/sprite-fixtures/%s" % OS.get_process_id())
    require(DirAccess.make_dir_recursive_absolute(fixture_dir) == OK, "Fixture directory created")
    for disk in range(1, 9):
        write_archive("HEAD%d.DAX" % disk, {1: picture(88, 40, 1, 0)})
        write_archive("BODY%d.DAX" % disk, {1: picture(88, 48, 1, 0)})
    for part in ["CHEAD", "CBODY"]:
        var records := {}
        for bank in [0, 64, 128, 192]:
            for id in range(14 if part == "CHEAD" else 32):
                records[bank + id] = picture(24, 8 if part == "CHEAD" else 24, 1,
                    id + (2 if bank >= 128 else 0), 2 if bank % 128 == 64 else 1)
        write_archive(part + ".DAX", records)
    write_archive("DUNGCOM.DAX", {1: picture(24, 24, 25, 0)})
    var monsters := {}
    for id in [0, 2, 4, 26, 31]:
        for pose in [0, 128]:
            monsters[id + pose] = picture(48 if id == 26 else 24, 48 if id == 31 else 24, 1, id + pose, 2)
    write_archive("CPIC2.DAX", monsters)
    # The file only Pool of Radiance has, so the game finder takes this folder for its.
    write_archive("SQRPACI.DAX", {})
    OS.set_environment("OPENGOLD_GAME_DIR", fixture_dir)

func cleanup() -> void:
    OS.set_environment("OPENGOLD_GAME_DIR", original_game_dir)
    if not fixture_dir.is_empty():
        for name in DirAccess.get_files_at(fixture_dir):
            DirAccess.remove_absolute(fixture_dir.path_join(name))
        DirAccess.remove_absolute(fixture_dir)

func press(demo: Control, name: String) -> void:
    var button: Button = demo.get_node(name)
    require(not button.disabled, "Button is enabled: " + name)
    button.pressed.emit()

func textures(canvas: Control) -> Array:
    var result := []
    for sprite in canvas.get_children():
        result.append(sprite.texture.get_image().get_data())
    return result

func visible_rect(sprite: TextureRect) -> Rect2:
    var source := sprite.texture.get_image().get_used_rect()
    var scale := sprite.size / sprite.texture.get_size()
    return Rect2(sprite.position + Vector2(source.position) * scale, Vector2(source.size) * scale)

func check_goliaths(canvas: Control, tile: float) -> void:
    for index in range(2):
        var sprite: TextureRect = canvas.get_node("GoliathStretched" if index == 0 else "GoliathProportional")
        var visible := visible_rect(sprite)
        var source := sprite.texture.get_image().get_used_rect().size
        require(is_equal_approx(visible.size.y, tile * 1.25), "Goliath visible height is 1.25 squares")
        require(is_equal_approx(visible.position.y, tile * 12.75), "Top reaches exactly 25% into the upper square")
        require(is_equal_approx(visible.end.y, tile * 14), "Visible feet rest on the bottom-square baseline")
        require(is_equal_approx(visible.get_center().x, (28.5 + 3 * index) * tile), "Both Goliaths are horizontally centered")
        if index == 0:
            require(is_equal_approx(visible.size.x, tile), "Stretched Goliath fills exactly one square's width")
        else:
            require(is_equal_approx(visible.size.x / visible.size.y, float(source.x) / source.y), "Proportional Goliath preserves visible aspect ratio")
            require(is_equal_approx(sprite.size.x / sprite.size.y, sprite.texture.get_size().x / sprite.texture.get_size().y), "Proportional artwork is uniformly scaled")
            if float(source.x) / source.y > 0.8:
                require(visible.size.x > tile, "Wide proportional artwork spills outside the column")

func capture() -> void:
    if OS.get_cmdline_user_args().has("--capture"):
        var screenshots := current_scene
        var key := InputEventKey.new()
        key.keycode = KEY_S
        key.ctrl_pressed = true
        key.pressed = true
        Input.parse_input_event(key)
        var result: Array = await screenshots.capture_completed
        require(result[1].is_empty(), "Demo screenshot saved")
        require(FileAccess.file_exists(result[0]), "Ctrl+S writes a PNG")
        key.pressed = false
        Input.parse_input_event(key)

func run_checks() -> void:
    if not OS.get_cmdline_user_args().has("--installed"):
        create_fixtures()
    root.size = Vector2i(1920, 1080)
    change_scene_to_file("res://scenes/combat_sprite_demo.tscn")
    await settle()
    var demo := current_scene as Control
    var scroll: ScrollContainer = demo.get_node("BattlefieldScroll")
    var canvas: Control = scroll.get_node("Canvas")
    require(not demo.get_node("Status").text.begins_with("Cannot load"), demo.get_node("Status").text)
    require(canvas.get_child_count() == 9, "Four player comparisons and exactly five monsters")
    require(demo.get_node("Zoom").text == "Zoom 250%", "Start at 250%")
    var normal: TextureRect = canvas.get_node("NormalPlayer")
    var small: TextureRect = canvas.get_node("SmallPlayer")
    var stretched: TextureRect = canvas.get_node("GoliathStretched")
    var proportional: TextureRect = canvas.get_node("GoliathProportional")
    require(normal.size == Vector2(60, 60), "Normal player fills one source square")
    check_goliaths(canvas, 60)
    require(small.texture.get_image().get_data() != normal.texture.get_image().get_data(), "Short player uses its own original art bank")
    for goliath in [stretched, proportional]:
        require(normal.texture.get_image().get_data() == goliath.texture.get_image().get_data(), "Both Goliaths preserve the customized tall artwork")
    demo.set_process(false)
    var before := textures(canvas)
    press(demo, "Color0_1")
    press(demo, "Palette0")
    var after := textures(canvas)
    for i in range(4):
        require(before[i] != after[i], "Recoloring updates every player comparison")
        for alpha in range(3, before[i].size(), 4):
            require(before[i][alpha] == after[i][alpha], "Black recoloring preserves transparency")
    for i in range(4, 9):
        require(before[i] == after[i], "Player customization preserves monster art")
    press(demo, "HeadPrevious")
    require(demo.get_node("Head").text == "Head 14 / 14", "Head selection wraps")
    press(demo, "HeadNext")
    press(demo, "BodyNext")
    require(demo.get_node("Body").text == "Weapon 6 / 35", "Weapon selection updates")
    check_goliaths(canvas, 60)
    press(demo, "BodyPrevious")
    check_goliaths(canvas, 60)
    demo.get_node("Plus10").grab_focus()
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = KEY_SPACE
        event.pressed = down
        Input.parse_input_event(event)
        await process_frame
    require(normal.size.is_equal_approx(Vector2(62.4, 62.4)), "10 percentage point zoom")
    press(demo, "Plus100")
    require(normal.size.is_equal_approx(Vector2(86.4, 86.4)), "100 percentage point zoom")
    check_goliaths(canvas, 86.4)
    press(demo, "Minus100")
    press(demo, "Minus10")
    require(normal.size == Vector2(60, 60), "Zoom steps reverse exactly")
    for i in range(3):
        press(demo, "Minus100")
    require(demo.get_node("Zoom").text == "Zoom 10%" and demo.get_node("Minus10").disabled, "Zoom clamps safely at 10%")
    check_goliaths(canvas, 2.4)
    for i in range(10):
        press(demo, "Plus100")
    require(demo.get_node("Zoom").text == "Zoom 1000%" and demo.get_node("Plus100").disabled, "Zoom clamps at 1000%")
    check_goliaths(canvas, 240)
    for i in range(7):
        press(demo, "Minus100")
    check_goliaths(canvas, 72)
    for sprite in canvas.get_children():
        if sprite != stretched and sprite != proportional:
            require(sprite.size.is_equal_approx(sprite.texture.get_size() * 3), "Other sprites keep native proportions at the same zoom")
    require(demo.get_node("Sizes").text.contains("24.0 × 24.0 → 72.0 × 72.0"), "Readout reports source and rendered sizes")
    root.size = Vector2i(1120, 800)
    await settle()
    for child in demo.get_children():
        if child is Control:
            require(Rect2(Vector2.ZERO, demo.size).encloses(child.get_rect()), "Minimum window contains control: " + str(child.name))
    await capture()
    root.size = Vector2i(1920, 1080)
    demo.set_process(true)
    await settle()
    await capture()

    # Observe real time and all textures across complete, synchronized pose cycles.
    var previous := textures(canvas)
    var transitions := 0
    var last_change := 0
    var deadline := Time.get_ticks_msec() + 4500
    while transitions < 3 and Time.get_ticks_msec() < deadline:
        await process_frame
        var current := textures(canvas)
        if current[0] == previous[0]:
            continue
        var now := Time.get_ticks_msec()
        if last_change:
            require(absi(now - last_change - 1000) < 150, "Pose changes once per second")
        for i in range(9):
            require(current[i] != previous[i], "All nine figures switch poses on the same frame")
        check_goliaths(canvas, 72)
        previous = current
        transitions += 1
        last_change = now
        if transitions == 1:
            await capture()
    require(transitions == 3, "Ready and action poses keep alternating")
    if not fixture_dir.is_empty():
        # Fail after loading player and terrain art; the scene must stay usable
        # as a diagnostic instead of trying to animate partially loaded figures.
        DirAccess.remove_absolute(fixture_dir.path_join("CPIC2.DAX"))
        change_scene_to_file("res://scenes/combat_sprite_demo.tscn")
        await settle()
        require(current_scene.get_node("Status").text.contains("Missing art archive: CPIC2.DAX"), "Missing art has a visible diagnostic")
        require(not current_scene.is_processing(), "Failed loads do not animate incomplete figures")
        for child in current_scene.get_children():
            if child is Button:
                require(child.disabled, "Incomplete demo disables customization and zoom")
    cleanup()
    print("Combat sprite demo checks passed: nine figures, both Goliath proportions and baselines, customization, source sizes, zoom, resize and one-second poses")
    quit(0)
