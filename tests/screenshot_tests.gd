extends SceneTree

var completions: Array[Dictionary] = []
var screenshots: Node
var directory: String
var graphical := false
var failed := false

func _initialize() -> void:
    call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
    if not ok:
        failed = true
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(5):
        await process_frame

func completed(files: Array, error: String) -> void:
    completions.append({"files": files, "error": error})

func key(ctrl := true, pressed := true, echo := false) -> InputEventKey:
    var event := InputEventKey.new()
    event.keycode = KEY_S
    event.unicode = 115
    event.ctrl_pressed = ctrl
    event.pressed = pressed
    event.echo = echo
    return event

func wait_capture(expected: int) -> Dictionary:
    for frame in range(180):
        if completions.size() >= expected:
            break
        await process_frame
    require(completions.size() == expected, "Exactly one capture completes per request")
    var result: Dictionary = completions.back()
    var report = JSON.parse_string(FileAccess.get_file_as_string(directory.path_join("latest.json")))
    require(report is Dictionary and report.files.size() == result.files.size() and report.error == result.error,
        "Capture report matches completion, including failures")
    for index in result.files.size():
        for field in ["path", "window", "width", "height", "main"]:
            require(report.files[index][field] == result.files[index][field], "Manifest identifies the captured window and image")
    if graphical:
        require(result.error.is_empty() and report.ok, "Graphical capture must succeed: " + result.error)
        require(result.files.size() == 2, "Capture includes the main viewport and visible dialog only")
        for file in result.files:
            var image := Image.load_from_file(file.path)
            require(image != null and image.get_width() == file.width and image.get_height() == file.height,
                "Saved PNG has the captured viewport dimensions")
            var expected_color := Color(0.1, 0.2, 0.3) if file.main else Color(0.2, 0.6, 0.4)
            var pixel := image.get_pixel(10, 10)
            require(Vector3(pixel.r - expected_color.r, pixel.g - expected_color.g,
                pixel.b - expected_color.b).length() < 0.02, "Saved PNG contains rendered pixels")
    else:
        require(not report.ok and not result.error.is_empty() and result.files.is_empty(),
            "Headless mode explicitly reports unavailable rendering")
    return result

func run_checks() -> void:
    screenshots = root.get_node("Screenshots")
    directory = screenshots.get_directory()
    graphical = DisplayServer.get_name() != "headless"
    screenshots.capture_completed.connect(completed)
    root.size = Vector2i(640, 480)
    root.gui_embed_subwindows = false
    var scene := Control.new()
    scene.name = "ScreenshotFixture"
    root.add_child(scene)
    current_scene = scene
    var background := ColorRect.new()
    background.color = Color(0.1, 0.2, 0.3)
    background.size = root.size
    scene.add_child(background)
    var dialog := Window.new()
    dialog.name = "CaptureDialog"
    dialog.size = Vector2i(240, 160)
    scene.add_child(dialog)
    var dialog_background := ColorRect.new()
    dialog_background.color = Color(0.2, 0.6, 0.4)
    dialog_background.size = dialog.size
    dialog.add_child(dialog_background)
    var field := LineEdit.new()
    field.position = Vector2(20, 60)
    field.size = Vector2(180, 40)
    field.text = "Unchanged"
    dialog.add_child(field)
    var hidden := Window.new()
    scene.add_child(hidden)
    hidden.hide()
    dialog.show()
    field.grab_focus()
    await settle()

    root.push_input(key())
    var first := await wait_capture(1)
    var notice: Control = screenshots.get_node("NoticeLayer/Notice")
    require(notice.visible and notice.mouse_filter == Control.MOUSE_FILTER_IGNORE,
        "Capture confirmation is visible and does not intercept clicks")
    await settle()
    require(notice.position.x >= 0 and notice.position.y >= 0 and
        notice.position.x + notice.size.x <= root.size.x and notice.position.y + notice.size.y <= root.size.y,
        "Confirmation stays inside the viewport: position=%s size=%s viewport=%s" % [notice.position, notice.size, root.size])
    require(is_equal_approx(notice.position.x, 20.0) and is_equal_approx(notice.size.x, 600.0)
        and is_equal_approx(notice.get_rect().end.y, 460.0),
        "Notice keeps its established 640-pixel layout")
    root.size = Vector2i(1920, 1080)
    await settle()
    require(is_equal_approx(notice.position.x, 20.0) and is_equal_approx(notice.size.x, 800.0)
        and is_equal_approx(notice.get_rect().end.y, 1060.0),
        "Notice keeps its established large-window width and bottom inset")
    root.size = Vector2i(640, 480)
    await settle()
    if graphical:
        await RenderingServer.frame_post_draw
        require(root.get_texture().get_image().save_png(directory.path_join("notice-check.png")) == OK,
            "Confirmation review image saves")
    root.push_input(key(false))
    root.push_input(key(true, false))
    root.push_input(key(true, true, true))
    await settle()
    require(completions.size() == 1, "Plain S, key release and held-key repeat do not capture")

    paused = true
    dialog.window_input.emit(key())
    var second := await wait_capture(2)
    require(paused and field.text == "Unchanged" and field.has_focus(),
        "Shortcut preserves paused state, focused field and text")
    if graphical:
        require(first.files[0].path != second.files[0].path and FileAccess.file_exists(first.files[0].path),
            "Repeated captures keep earlier screenshots")
    # This is the same external trigger used by tools while the game is running.
    var request := FileAccess.open(directory.path_join("capture.request"), FileAccess.WRITE)
    require(request != null, "Request file opens")
    request.store_string("screenshot-regression")
    request.close()
    await wait_capture(3)
    require(not FileAccess.file_exists(directory.path_join("capture.request")), "File request is consumed")
    var report = JSON.parse_string(FileAccess.get_file_as_string(directory.path_join("latest.json")))
    require(report.request_id == "screenshot-regression", "Tool request correlates with its report")
    paused = false
    # A file occupying the output directory must fail without overwriting it.
    var blocked := directory.path_join("blocked-directory")
    var sentinel := FileAccess.open(blocked, FileAccess.WRITE)
    sentinel.store_string("retain this file")
    sentinel.close()
    var original_override := OS.get_environment("OPENGOLD_SCREENSHOT_DIR")
    OS.set_environment("OPENGOLD_SCREENSHOT_DIR", blocked)
    var failing: Node = ClassDB.instantiate("ScreenshotService")
    root.add_child(failing)
    OS.set_environment("OPENGOLD_SCREENSHOT_DIR", original_override)
    var failures: Array[String] = []
    failing.capture_completed.connect(func(_files: Array, error: String): failures.append(error))
    failing.request_capture()
    require(failures.size() == 1 and not failures[0].is_empty(), "Unwritable output reports failure")
    require(FileAccess.get_file_as_string(blocked) == "retain this file", "Failed capture preserves existing files")
    failing.free()
    if failed:
        quit(1)
        return
    print("Screenshot checks passed: shortcut, dialogs, pause, request file, report and saved images")
    quit(0)
