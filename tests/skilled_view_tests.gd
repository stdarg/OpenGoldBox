extends "style_advancement_view_tests.gd"
# Skilled Training page: picking three proficiencies from one prefixed list,
# the count and limit, Back/Cancel, switching feat, and a Wizard's page order.

func skilled_boxes() -> Array:
    var rows: VBoxContainer = current_scene.get_node("LevelUp/SkilledPage/Rows")
    var boxes := []
    for child in rows.get_children():
        if child is CheckBox and child.visible: boxes.append(child)
    return boxes

func toggle(box: CheckBox, on: bool) -> void:
    box.button_pressed = on; box.toggled.emit(on); await settle()

# Select Skilled in the feat dropdown. It is appended last by the rules, so the
# index is locale-independent; the page assertion catches a wrong selection.
func choose_skilled() -> void:
    var level: Window = current_scene.get_node("LevelUp")
    var feat: OptionButton = level.get_node("Feat")
    var index := feat.item_count - 1
    require(not feat.is_item_disabled(index), "Skilled is selectable at level four")
    feat.select(index); feat.item_selected.emit(index); await settle()
    # A class that also owes a separate advancement choice (the Fighter's fourth
    # weapon mastery) must still satisfy it on the first page.
    var dropdown: OptionButton = level.get_node("AdvancementTraining")
    if dropdown.visible:
        dropdown.select(1); dropdown.item_selected.emit(1); await settle()
    require(not level.get_node("SkilledPage").visible, "The Skilled page waits for Next")
    require(level.get_node("Confirm").text != "Confirm", "Skilled makes Confirm advance a page")
    await press("LevelUp/Confirm")
    require(level.get_node("SkilledPage").visible and level.get_node("SkilledCount").visible,
        "Next opens the Skilled Training page")
    require(level.get_node("Back").visible, "Back is available on the Skilled page")
    for name in ["HP", "FeatLabel", "Feat", "Note"]:
        require(not level.get_node(name).visible, "First-page controls leave the Skilled page")

func run_checks() -> void:
    require(not fixture.is_empty(), "Skilled fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        var before: String = current_scene.get_node("PartyPanel/Sheet").text
        var level: Window = current_scene.get_node("LevelUp")

        # Every skill less what this character already holds, each offered once.
        await press("PartyPanel/Roster/Advance1"); await choose_skilled()
        var boxes := skilled_boxes()
        require(boxes.size() > 3 and boxes.size() < 18, "The page offers the unheld skills")
        var labels := {}
        for box in boxes:
            require(not labels.has(box.text), "Every proficiency is offered exactly once")
            labels[box.text] = true
            require(box.focus_mode == Control.FOCUS_ALL, "Each proficiency is keyboard accessible")
            require(box.name.begins_with("skill_"), "Only skills are offered")
        require(not labels.has(current_scene.get_node("PartyPanel/Sheet").text), "Sanity")

        # Count, limit and the confirm gate.
        require(level.get_node("Confirm").disabled, "No selection cannot be confirmed")
        await toggle(boxes[0], true)
        require(level.get_node("Confirm").disabled, "One selection cannot be confirmed")
        await toggle(boxes[1], true)
        require(level.get_node("Confirm").disabled, "Two selections cannot be confirmed")
        await toggle(boxes[2], true)
        require(not level.get_node("Confirm").disabled, "Exactly three selections confirm")
        var full := skilled_boxes()
        for box in full:
            if box.button_pressed: require(not box.disabled, "A chosen proficiency can be cleared")
            else: require(box.disabled, "A full selection cannot be exceeded")
        await toggle(boxes[2], false)
        require(level.get_node("Confirm").disabled and not skilled_boxes()[3].disabled,
            "Clearing a pick reopens the remaining options")
        await toggle(boxes[2], true)

        # Back keeps valid picks; returning to the page shows them still chosen.
        await press("LevelUp/Back")
        require(not level.get_node("SkilledPage").visible and level.get_node("Feat").visible,
            "Back returns to the first page")
        require(not level.get_node("Back").visible, "Back is hidden on the first page")
        await press("LevelUp/Confirm")
        var kept := 0
        for box in skilled_boxes():
            if box.button_pressed: kept += 1
        require(kept == 3, "Back preserves valid selections")

        # Switching away from Skilled discards only its unconfirmed picks.
        await press("LevelUp/Back")
        var feat: OptionButton = level.get_node("Feat")
        feat.select(0); feat.item_selected.emit(0); await settle()
        require(not level.get_node("SkilledPage").visible, "Switching feat closes the Skilled page")
        await choose_skilled()
        var stale := 0
        for box in skilled_boxes():
            if box.button_pressed: stale += 1
        require(stale == 0, "Switching away from Skilled discards its unconfirmed picks")

        # Cancel commits nothing.
        var chosen := []
        var fresh := skilled_boxes()
        for i in range(3):
            await toggle(fresh[i], true); chosen.append(fresh[i].name)
        await capture("skilled-" + klass + "-" + locale, level)
        await press("LevelUp/Cancel")
        require(current_scene.get_node("PartyPanel/Sheet").text == before,
            "Cancel preserves source records and wounds")
        require(current_scene.get_node("PartyPanel/Roster/Advance1").visible,
            "Cancel leaves the entitlement unspent")

        # Closing the window commits nothing either.
        await press("PartyPanel/Roster/Advance1"); await choose_skilled()
        level.close_requested.emit(); await settle()
        require(not level.visible and current_scene.get_node("PartyPanel/Sheet").text == before,
            "Closing the dialog preserves character and advancement")

        # Keyboard confirmation through every page.
        await press("PartyPanel/Roster/Advance1"); await choose_skilled()
        fresh = skilled_boxes()
        for i in range(3): await toggle(fresh[i], true)
        level.get_node("Confirm").grab_focus(); await key(level, KEY_ENTER)
        if klass == "wizard":
            require(level.visible and level.get_node("SpellChoicesPage").visible,
                "A Wizard's spell page follows the Skilled page")
            require(level.get_node("Back").visible, "Back remains available on the spell page")
            await press("LevelUp/Back")
            require(level.get_node("SkilledPage").visible,
                "Back from the spell page returns to the Skilled page")
            await press("LevelUp/Confirm")
            level.get_node("Confirm").grab_focus(); await key(level, KEY_ENTER)
        require(not level.visible and not current_scene.get_node("PartyPanel/Roster/Advance1").visible,
            "Keyboard commits one level-four entitlement")
        require(current_scene.get_node("PartyPanel/Sheet").text != before, "The sheet records Skilled")

        await press("PartyPanel/Save"); current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible,
            "A Skilled character saves through the existing out-of-combat flow")
        if not output.is_empty():
            f = FileAccess.open(output + "-" + locale + ".ogs", FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(slot)); f.close()
        current_scene.queue_free(); await settle()
    restore_files(); print("Skilled controls passed"); quit(0)
