extends "style_advancement_view_tests.gd"
func run_checks() -> void:
    require(not fixture.is_empty(), "Pending mastery fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var file := FileAccess.open(slot, FileAccess.WRITE); file.store_buffer(FileAccess.get_file_as_bytes(fixture)); file.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        await press("PartyPanel/Explore")
        var mastery: Window = current_scene.get_node("CampaignTown/RestTraining")
        require(mastery.visible, "Loading pending rest reopens mastery")
        var rows: VBoxContainer = mastery.get_node("Choices/Rows")
        var original: CheckBox
        var replacement: CheckBox
        for box in rows.get_children():
            if box.button_pressed and original == null: original = box
            if not box.button_pressed and replacement == null: replacement = box
        require(original != null and replacement != null, "Fixture has mastery choices")
        var original_name := NodePath(str(original.name))
        var replacement_name := NodePath(str(replacement.name))
        original.button_pressed = false; replacement.button_pressed = true; await settle()
        require(not mastery.get_node("Apply").disabled, "Unapplied replacement is valid")
        await capture("mastery-rest-save-" + locale, mastery)
        var before := FileAccess.get_file_as_bytes(slot)
        mastery.get_node("Save").grab_focus(); await key(mastery, KEY_SPACE)
        var saves: Window = current_scene.get_node("SaveSlots")
        require(saves.visible and not mastery.visible, "Keyboard Save opens existing SaveSlots exclusively")
        await press("SaveSlots/Cancel")
        require(mastery.visible and not original.button_pressed and replacement.button_pressed, "Cancel returns to the same unapplied choices")
        require(FileAccess.get_file_as_bytes(slot) == before, "Cancel does not write a save")
        await press("CampaignTown/RestTraining/Save")
        saves.get_node("Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not saves.visible and mastery.visible, "Saving returns to pending mastery")
        require(not original.button_pressed and replacement.button_pressed, "Saving retains local edits without applying them")
        # Discard this local dialog before reloading its saved entitlement.
        await key(mastery, KEY_ESCAPE); require(not mastery.visible, "Escape keeps current")
        await press("ReturnParty"); await load_slot(); await press("PartyPanel/Explore")
        mastery = current_scene.get_node("CampaignTown/RestTraining")
        require(mastery.visible, "Saved pending entitlement survives load")
        rows = mastery.get_node("Choices/Rows")
        require(rows.get_node(original_name).button_pressed and not rows.get_node(replacement_name).button_pressed, "Save did not commit the unapplied replacement")
        await press("CampaignTown/RestTraining/Apply")
        require(not mastery.visible, "Loaded entitlement can be resolved once")
        current_scene.queue_free(); await settle()
    restore_files(); print("Weapon Mastery rest saving controls passed"); quit(0)
