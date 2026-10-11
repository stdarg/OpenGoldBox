extends SceneTree

# Quick combat in the combat window (QUICK-1): Quick hands the member whose turn
# it is to the computer, M switches Quick magic, Space takes the party back and
# Q puts the whole party on Quick. Loads the "gear-ally" save from
# opengold_playtest_fixtures (the Hero, an Ally and a troll).
var fixtures := ""
var path := ""
var original: Variant = null

func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--playtest-fixtures="): fixtures = arg.trim_prefix("--playtest-fixtures=")
    call_deferred("run_checks")

func cleanup() -> void:
    if original == null:
        if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
    else:
        var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(original)

func require(ok: bool, message: String) -> void:
    if not ok:
        cleanup(); push_error(message); quit(1); assert(ok, message)

func settle() -> void:
    for frame in range(8): await process_frame

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down; root.push_input(event)
    await settle()

func shown(name: String) -> bool:
    return (current_scene.get_node(name) as Control).visible

func prompt() -> String:
    return current_scene.get_node("Prompt").text

func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Play-test fixture " + name + " exists")
    var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit(); await settle()

func run_checks() -> void:
    path = ProjectSettings.globalize_path("user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in ["en", "es"]:
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        TranslationServer.set_locale(locale)
        current_scene.set_process(false)
        await load_fixture("gear-ally")
        var computer := tr("{name} fights under computer control (Quick). Space or Take control: play the party yourself.").format({"name": "Hero"})
        require(shown("Quick") and not shown("TakeControl") and not shown("QuickMagic"),
            "A member's turn offers Quick: " + locale)
        current_scene.get_node("Quick").pressed.emit(); await settle()
        require(prompt() == computer and shown("TakeControl") and not shown("Quick") and
            not shown("Flee") and shown("QuickMagic"),
            "Quick hands the member to the computer, with Take control and Quick magic: " + locale)
        require(current_scene.get_node("QuickMagic").text == tr("Quick magic: Off"), "Quick magic starts off")
        await key(KEY_M)
        require(current_scene.get_node("QuickMagic").text == tr("Quick magic: On"), "M switches Quick magic on")
        await key(KEY_SPACE)
        require(not shown("TakeControl") and shown("Quick") and not shown("QuickMagic") and prompt() != computer,
            "Space takes the party back on the member's own turn: " + locale)
        await key(KEY_Q)
        require(prompt() == computer and shown("TakeControl"), "Q puts the whole party on Quick")
        var log: String = current_scene.get_node("LogStack/Log").get_parsed_text()
        current_scene.set_process(true)
        for frame in range(240):
            await process_frame
            if current_scene.get_node("LogStack/Log").get_parsed_text() != log: break
        current_scene.set_process(false)
        require(current_scene.get_node("LogStack/Log").get_parsed_text() != log, "The computer plays the member on Quick")
        current_scene.get_node("TakeControl").pressed.emit(); await settle()
        require(not shown("TakeControl"), "Take control returns the party")
    cleanup(); print("Quick combat controls passed"); quit()
