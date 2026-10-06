extends SceneTree
var legacy := false
var fixtures := ""
var originals := {}
var save_path := ""
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg == "--legacy": legacy = true
        if arg.begins_with("--component-fixtures="): fixtures = arg.trim_prefix("--component-fixtures=")
    call_deferred("run_checks")
func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            file.store_buffer(originals[path]); file.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)
func settle() -> void:
    for frame in range(4): await process_frame
func load_fixture(which: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(which + ".save"))
    require(not bytes.is_empty(), "Native character fixture exists")
    var file := FileAccess.open(save_path, FileAccess.WRITE)
    file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit()
    await settle()
func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()
func run_checks() -> void:
    save_path = ProjectSettings.globalize_path("res://../../user-data/combat.save" if legacy else "user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[save_path + suffix] = FileAccess.get_file_as_bytes(save_path + suffix) if FileAccess.file_exists(save_path + suffix) else null
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    combat.set_process(false)
    require(not combat.get_node("Save").visible and not combat.get_node("Load").visible, "No player combat save controls")
    # Spells have no components (CLASS-11): a weapon and shield in hand block nothing.
    for caster in ["cleric", "wizard"]:
        for shield in [true, false]:
            await load_fixture(caster + ("-shield" if shield else "-free"))
            var spells := [("FireBolt" if legacy else "CastCantrip"), "MagicMissile", "ScorchingRay"] if caster == "wizard" else ["CureWounds"]
            for button in spells:
                require(not combat.get_node(button).disabled, "Full hands never disable casting: " + button)
            if not legacy:
                require(not combat.get_node("Blindness").disabled, "Blindness remains available")
            if caster == "cleric":
                require(not combat.get_node("HealingWord").disabled, "Healing Word remains available")
            if not legacy and shield and caster == "cleric":
                var prompts := ""
                for i in range(16):
                    await key(KEY_A)
                    prompts += combat.get_node("Prompt").text + "\n"
                # Bless and Shield of Faith once needed a free hand.
                require(prompts.contains("Bless") and prompts.contains("Shield of Faith"), "Keyboard cycle offers casting with full hands")
            combat.get_node("Save").pressed.emit()
            var before := FileAccess.get_file_as_string(save_path)
            combat.get_node("Load").pressed.emit()
            combat.get_node("Save").pressed.emit()
            require(FileAccess.get_file_as_string(save_path) == before, "Internal checkpoint keeps eligibility and resources unchanged")
    await load_fixture("cleric-shield")
    if not legacy:
        require(combat.selected_character_id() == 1, "Caster is selected")
        combat.get_node("HealingWord").pressed.emit()
        await key(KEY_SPACE)
        require(combat.get_node("HealingWord").disabled and not combat.get_node("Dash").disabled, "Keyboard casts Healing Word using Bonus Action and retains Action")
    restore_files()
    print("Spell component view checks passed: legacy spell controls and internal continuation" if legacy else "Spell component view checks passed: full hands cast, keyboard action cycle and internal continuation")
    quit(0)
