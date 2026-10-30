extends SceneTree

func _initialize() -> void:
    call_deferred("run_check")

func run_check() -> void:
    var source := OS.get_environment("OPENGOLD_COMBAT_TEST_CONFIG")
    var temporary := "user://combat_tester_save_check.json"
    DirAccess.copy_absolute(source, ProjectSettings.globalize_path(temporary))
    OS.set_environment("OPENGOLD_COMBAT_TEST_CONFIG", ProjectSettings.globalize_path(temporary))
    var setup: Control = load("res://combat_tester/setup.tscn").instantiate()
    root.add_child(setup)
    await process_frame
    setup.name_entry.text = "Saved test member"
    setup._store_member()
    setup._save_party()
    var file := FileAccess.open(temporary, FileAccess.READ)
    var saved: Dictionary = JSON.parse_string(file.get_as_text())
    if saved.parties[0].members[0].name != "Saved test member":
        push_error("Save party did not persist edits")
        quit(1)
        return
    setup.name_entry.text = "Unsaved change"
    setup._store_member()
    setup._restore_preset()
    if setup.session_parties[0].members[0].name != "Saved test member":
        push_error("Restore preset did not load saved member")
        quit(1)
        return
    DirAccess.remove_absolute(ProjectSettings.globalize_path(temporary))
    print("COMBAT_TEST_SAVE_OK")
    quit(0)
