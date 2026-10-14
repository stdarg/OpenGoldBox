extends SceneTree

# Exercises the reviewer UI through its real controls. The combinations prove
# that its fixture generator accepts every rules-offered identity and level.
var failures := PackedStringArray()
var tested := 0

func _initialize() -> void:
    call_deferred("run")

func settle(frames := 2) -> void:
    for frame in range(frames): await process_frame

func check(condition: bool, message: String) -> void:
    if not condition: failures.append(message)

func option_index(control: OptionButton, id: String) -> int:
    for i in range(control.item_count):
        if str(control.get_item_metadata(i)) == id: return i
    return -1

func run() -> void:
    root.gui_embed_subwindows = true
    change_scene_to_file("res://scenes/level_up_lab.tscn")
    await settle(6)
    var lab := current_scene
    var klass: OptionButton = lab.get_node("Class")
    var race: OptionButton = lab.get_node("Race")
    var gender: OptionButton = lab.get_node("Gender")
    var level: OptionButton = lab.get_node("CurrentLevel")
    var create: Button = lab.get_node("CreateCharacter")
    var open: Button = lab.get_node("OpenLevelUp")
    var status: Label = lab.get_node("Status")
    check(klass.item_count == 12, "Expected all twelve classes")
    check(race.item_count == 9, "Expected all nine races")
    check(gender.item_count == 3, "Expected all three genders")
    check(level.item_count == 3, "Expected starting levels 1–3")
    for c in range(klass.item_count):
        klass.select(c)
        for r in range(race.item_count):
            race.select(r)
            for g in range(gender.item_count):
                gender.select(g)
                for l in range(level.item_count):
                    level.select(l)
                    create.pressed.emit()
                    tested += 1
                    if open.disabled:
                        failures.append("%s / %s / %s / level %d: %s" % [
                            klass.get_item_text(c), race.get_item_text(r),
                            gender.get_item_text(g), l + 1, status.text])
                    await process_frame
    var wizard := option_index(klass, "wizard")
    var fighter := option_index(klass, "fighter")
    check(wizard >= 0 and fighter >= 0, "Wizard and Fighter are available")
    if wizard >= 0: klass.select(wizard)
    race.select(0); gender.select(0); level.select(2)
    create.pressed.emit()
    check(not open.disabled, "Final review character is ready")
    if not open.disabled:
        open.pressed.emit()
        await settle()
        var dialog: Window = lab.get_node("LevelUp")
        check(dialog.visible, "Open Level-Up shows the production dialog")
        var before: String = (lab.get_node("Summary") as Label).text
        (dialog.get_node("Cancel") as Button).pressed.emit()
        await settle()
        check(not dialog.visible, "Cancel closes the dialog")
        check((lab.get_node("Summary") as Label).text == before,
              "Cancel leaves the character unchanged")
        open.pressed.emit()
        await settle()
        (dialog.get_node("Confirm") as Button).pressed.emit()
        await settle()
        check((dialog.get_node("SpellChoicesPage") as Control).visible,
              "Wizard level four reaches the spell-choice page")
        (dialog.get_node("Back") as Button).pressed.emit()
        await settle()
        check(not (dialog.get_node("SpellChoicesPage") as Control).visible,
              "Back returns to the first page")
        (dialog.get_node("Cancel") as Button).pressed.emit()
        await settle()
    # A Sorcerer's two Metamagic options and a Warlock's two invocations are
    # checked on the dialog's checkbox page; level 2 completes once both are.
    for id in ["sorcerer", "warlock"]:
        klass.select(option_index(klass, id)); level.select(0)
        create.pressed.emit(); await settle()
        open.pressed.emit(); await settle()
        var dialog: Window = lab.get_node("LevelUp")
        var next: Button = dialog.get_node("Confirm")
        next.pressed.emit(); await settle()
        var boxes := []
        for child in (dialog.get_node("SkilledPage/Rows") as VBoxContainer).get_children():
            if child is CheckBox and child.visible: boxes.append(child)
        check((dialog.get_node("SkilledPage") as Control).visible and boxes.size() > 2 and
              (dialog.get_node("SkilledCount") as Label).text.ends_with(": 0/2 selected") and next.disabled,
              id + " level 2 lists its options to check, none checked yet")
        for n in range(2):
            (boxes[n] as CheckBox).button_pressed = true
            await settle()
        check((boxes[2] as CheckBox).disabled and not next.disabled, id + " takes exactly two")
        for page in range(3):
            if next.disabled or not dialog.visible: break
            next.pressed.emit(); await settle()
        check((lab.get_node("Summary") as Label).text.contains("Level 2"), id + " reaches level 2")
        var sheet := lab.get_node_or_null("CharacterSheet") as Window
        if sheet and sheet.visible: (sheet.get_node("Close") as Button).pressed.emit(); await settle()
        if dialog.visible: (dialog.get_node("Cancel") as Button).pressed.emit(); await settle()
    # Gaining level 2, a Sorcerer may replace a cantrip and a spell on the
    # Replace page; the spell page then fills the place the swap freed.
    klass.select(option_index(klass, "sorcerer")); level.select(0)
    create.pressed.emit(); await settle()
    open.pressed.emit(); await settle()
    var up: Window = lab.get_node("LevelUp")
    var go: Button = up.get_node("Confirm")
    go.pressed.emit(); await settle()
    var picks := []
    for child in (up.get_node("SkilledPage/Rows") as VBoxContainer).get_children():
        if child is CheckBox and child.visible: picks.append(child)
    for n in range(2):
        (picks[n] as CheckBox).button_pressed = true
        await settle()
    go.pressed.emit(); await settle()
    check((up.get_node("SwapPage") as Control).visible, "The Replace page follows the Metamagic picks")
    var cantrip: OptionButton = up.get_node("SwapPage/SwapCantrip")
    var with: OptionButton = up.get_node("SwapPage/SwapCantripWith")
    check(cantrip.get_item_text(0) == "Keep current" and with.disabled, "Nothing is replaced until chosen")
    cantrip.select(1); cantrip.item_selected.emit(1); await settle()
    check(go.disabled and not with.disabled, "A replaced cantrip waits for its replacement")
    var old_cantrip := cantrip.get_item_text(1)
    var new_cantrip := with.get_item_text(1)
    with.select(1); with.item_selected.emit(1); await settle()
    check(not go.disabled and cantrip.selected == 1, "The swap is complete and stays shown")
    go.pressed.emit(); await settle()
    var rows: VBoxContainer = up.get_node("SpellChoicesPage/Rows")
    for box in rows.get_node("prepared").get_children():
        if box is CheckBox and not box.button_pressed and not box.disabled and go.disabled:
            box.button_pressed = true
            await settle()
    check(not go.disabled, "The spell page completes")
    go.pressed.emit(); await settle()
    check((lab.get_node("CharacterSheet/Text") as RichTextLabel).text.contains("Level 2"),
          "The Sorcerer reaches level 2")
    (lab.get_node("CharacterSheet/Close") as Button).pressed.emit(); await settle()
    # The next level's Replace list names the new cantrip, not the old one.
    open.pressed.emit(); await settle()
    go.pressed.emit(); await settle()
    var known := []
    for n in range(cantrip.item_count): known.append(cantrip.get_item_text(n))
    check((up.get_node("SwapPage") as Control).visible and new_cantrip in known and not old_cantrip in known,
          "The swapped cantrip is known: " + old_cantrip + " -> " + new_cantrip + " in " + str(known))
    (up.get_node("Cancel") as Button).pressed.emit(); await settle()
    if fighter >= 0:
        klass.select(fighter); level.select(0)
        create.pressed.emit()
        check(not open.disabled, "Level-one Fighter is ready")
        if not open.disabled:
            open.pressed.emit(); await settle()
            var confirm: Button = lab.get_node("LevelUp/Confirm")
            check(not confirm.disabled, "Fighter has a valid default advancement")
            if not confirm.disabled: confirm.pressed.emit(); await settle()
            check((lab.get_node("Summary") as Label).text.contains("Level 2"),
                  "Confirm applies a level and refreshes the review screen")
            var sheet: Window = lab.get_node("CharacterSheet")
            check(sheet.visible, "Completed level-up opens the character sheet")
            check((sheet.get_node("Text") as RichTextLabel).text.contains("Level 2"),
                  "Character sheet shows the attained level")
            (sheet.get_node("Close") as Button).pressed.emit(); await settle()
            check(not sheet.visible, "Character sheet closes")
            check((lab.get_node("CurrentLevelLabel") as Label).text == "Starting level",
                  "Creation level selector is labeled as a starting value")
            (lab.get_node("SaveCharacter") as Button).pressed.emit(); await settle()
            var slots: Window = lab.get_node("SaveSlots")
            check(slots.visible, "Save Character opens named slots")
            (slots.get_node("Name") as LineEdit).text = "Fighter route"
            (slots.get_node("Action") as Button).pressed.emit(); await settle()
            check(not slots.visible, "Character save completes")
            change_scene_to_file("res://scenes/level_up_lab.tscn")
            await settle(6)
            lab = current_scene
            check((lab.get_node("SaveCharacter") as Button).disabled,
                  "Fresh review scene starts without a character")
            (lab.get_node("LoadCharacter") as Button).pressed.emit(); await settle()
            slots = lab.get_node("SaveSlots")
            var list: ItemList = slots.get_node("Slots")
            check(list.item_count == 1, "Separate review directory contains the saved character")
            if list.item_count == 1:
                list.select(0)
                (slots.get_node("Action") as Button).pressed.emit(); await settle()
                check(slots.visible, "Loading asks before discarding current state")
                (slots.get_node("Action") as Button).pressed.emit(); await settle()
                check(not slots.visible, "Confirmed character load completes")
                check((lab.get_node("Summary") as Label).text.contains("Level 2"),
                      "Reloaded character retains its attained level")
                (lab.get_node("ViewCharacter") as Button).pressed.emit(); await settle()
                sheet = lab.get_node("CharacterSheet")
                check((sheet.get_node("Text") as RichTextLabel).text.contains("Level 2"),
                      "Reloaded full sheet shows the attained level")
                (sheet.get_node("Close") as Button).pressed.emit(); await settle()
    for failure in failures: printerr("Level-up lab: " + failure)
    if failures.is_empty():
        print("Level-up lab passed: %d combinations, Wizard pages, Cancel, Fighter confirmation, character sheet and save/load" % tested)
        quit(0)
    else:
        printerr("Level-up lab failed: %d findings" % failures.size())
        quit(1)
