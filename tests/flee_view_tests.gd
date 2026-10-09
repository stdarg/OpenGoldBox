extends SceneTree

# Fleeing in the combat window, as in the original: moving a character off the
# edge of the field with the arrow keys tries to run away. Loads the "flee"
# save from opengold_playtest_fixtures (the hero on the west edge).
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
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        current_scene.set_process(false)
        await load_fixture("flee")
        await key(KEY_UP)
        var log: String = current_scene.get_node("Log").get_parsed_text()
        require(not log.contains(tr("{name} flees the battle.").format({"name": "Hero"})), "Moving along the edge does not flee")
        await load_fixture("flee")
        await key(KEY_LEFT)
        log = current_scene.get_node("Log").get_parsed_text()
        var fled := log.contains(tr("{name} flees the battle.").format({"name": "Hero"}))
        var stayed := log.contains(tr("{name} cannot get away and must stay.").format({"name": "Hero"}))
        require(fled or stayed, "Moving off the edge tries to flee: " + log.left(300))
        if fled:
            require(current_scene.get_node("Turn").text.contains(tr("Your party flees the battle.")), "With nobody left on the field, the party has fled")
        else:
            await key(KEY_LEFT)
            require(current_scene.get_node("Prompt").text == tr("You cannot run off the battlefield now: an enemy is faster, you have no movement left, or you must stay."), "A member who must stay is told why it cannot leave")
        # The Flee button hands the party to the AI, which runs it off the field.
        await load_fixture("flee")
        var flee: Button = current_scene.get_node("Flee")
        require(flee.visible, "Flee is offered on the party's turn")
        flee.pressed.emit(); await settle()
        require(not flee.visible and current_scene.get_node("Prompt").text == tr("Your party is fleeing."), "While fleeing, the AI has the party")
        current_scene.set_process(true)
        for frame in range(240):
            await process_frame
            log = current_scene.get_node("Log").get_parsed_text()
            if log.contains(tr("{name} flees the battle.").format({"name": "Hero"})) or log.contains(tr("{name} cannot get away and must stay.").format({"name": "Hero"})): break
        current_scene.set_process(false)
        require(log.contains(tr("{name} flees the battle.").format({"name": "Hero"})) or log.contains(tr("{name} cannot get away and must stay.").format({"name": "Hero"})), "The fleeing party tries to run off the field")
    cleanup(); print("Flee controls passed"); quit()
