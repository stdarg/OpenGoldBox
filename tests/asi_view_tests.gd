extends "style_advancement_view_tests.gd"
func allocate_points() -> void:
    var level: Window = current_scene.get_node("LevelUp")
    if klass == "fighter":
        var mastery: OptionButton = level.get_node("AdvancementTraining")
        require(mastery.visible and level.get_node("Confirm").disabled, "Fighter fourth mastery must be chosen")
        mastery.select(1); mastery.item_selected.emit(1); await settle()
    for i in range(6):
        var choice: OptionButton = level.get_node("Ability" + str(i))
        require(choice.visible and choice.focus_mode == Control.FOCUS_ALL, "ASI ability control is visible and keyboard accessible")
        choice.select(0); choice.item_selected.emit(0)
    require(level.get_node("Confirm").disabled, "No ability allocation cannot be confirmed")
    level.get_node("Ability2").select(1); level.get_node("Ability2").item_selected.emit(1)
    require(level.get_node("Confirm").disabled, "One point cannot be confirmed")
    level.get_node("Ability3").select(1); level.get_node("Ability3").item_selected.emit(1)
    require(not level.get_node("Confirm").disabled, "Two distinct one-point choices are valid")
    await settle()
func run_checks() -> void:
    require(not fixture.is_empty(), "ASI fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        var before: String = current_scene.get_node("PartyPanel/Sheet").text
        await press("PartyPanel/Roster/Advance1"); await allocate_points()
        var level: Window = current_scene.get_node("LevelUp")
        await capture("asi-" + klass + "-" + locale, level)
        level.close_requested.emit(); await settle()
        require(not level.visible and current_scene.get_node("PartyPanel/Sheet").text == before, "Closing the dialog preserves character and advancement")
        await press("PartyPanel/Roster/Advance1"); await allocate_points(); await press("LevelUp/Cancel")
        require(current_scene.get_node("PartyPanel/Sheet").text == before, "Cancel preserves source records and wounds")
        await press("PartyPanel/Roster/Advance1"); await allocate_points()
        level.get_node("Confirm").grab_focus(); await key(level, KEY_ENTER)
        if klass == "wizard":
            require(level.visible and level.get_node("SpellChoicesPage").visible, "Wizard uses approved second spell page")
            level.get_node("Confirm").grab_focus(); await key(level, KEY_ENTER)
        require(not level.visible and not current_scene.get_node("PartyPanel/Roster/Advance1").visible, "Keyboard commits one level-four entitlement")
        await press("PartyPanel/Save"); current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Advanced character saves through existing out-of-combat flow")
        if not output.is_empty():
            f = FileAccess.open(output + "-" + locale + ".ogs", FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(slot)); f.close()
        current_scene.queue_free(); await settle()
    restore_files(); print("ASI controls passed"); quit(0)
