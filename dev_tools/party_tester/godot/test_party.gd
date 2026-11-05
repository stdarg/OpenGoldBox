extends SceneTree

# The Party tester's headless check (check.sh), through its own controls: eight
# pool characters, one of them levelled through the game's level-up dialog and
# the rest by its defaults, then a fight on the game's combat screen played to
# the end with Quick, after which the party is as it was.

var failures := PackedStringArray()

func _initialize() -> void:
    call_deferred("run")

func settle(frames := 3) -> void:
    for frame in range(frames):
        await process_frame

func check(condition: bool, message: String) -> void:
    if not condition:
        failures.append(message)
        push_error(message)

func press(node: Node, path: String) -> void:
    for button in node.find_children("*", "Button", true, false):
        if button.text == path:
            button.pressed.emit()
            return
    failures.append("No button " + path)

func pick(control: OptionButton, text: String) -> bool:
    for n in range(control.item_count):
        if control.get_item_text(n) == text:
            control.select(n)
            return true
    return false

func answer_prompt(combat: Node) -> void:
    for name in ["Decline", "Continue"]:
        var button: Button = combat.get_node_or_null(name)
        if button and button.is_visible_in_tree() and not button.disabled:
            button.pressed.emit()
            return
    for window in combat.find_children("*", "Window", true, false):
        if not window.visible:
            continue
        var keep: Button = window.find_child("Keep", true, false)
        if keep and not keep.disabled:
            keep.pressed.emit()
            return

func finish(code: int) -> void:
    if failures.is_empty() and code == 0:
        print("Party tester passed")
        quit(0)
    else:
        for failure in failures:
            printerr(failure)
        quit(1)

func run() -> void:
    root.gui_embed_subwindows = true
    change_scene_to_file("res://party_tester/setup.tscn")
    await settle(6)
    var setup: Control = current_scene
    var bridge: Node = setup.get_node("Bridge")
    var pool: ItemList = setup.find_child("Pool", true, false)
    check(pool != null and pool.item_count == 48, "The Character Pool lists its 48 characters")
    if pool == null:
        finish(1)
        return
    # Eight classes: the pool holds four characters of each class in turn.
    for n in range(8):
        pool.select(n * 4, false)
    press(setup, "Build party")
    await settle()
    var party: ItemList = setup.find_child("Party", true, false)
    check(party.item_count == 8, "Eight pool characters make the party")
    var level: SpinBox = setup.find_child("Level", true, false)
    level.value = 4
    press(setup, "Level up")
    await settle()
    var dialog: Window = bridge.get_node("LevelUp")
    check(dialog.visible, "Level up opens the game's level-up dialog")
    var first: String = bridge.members()[0].name
    for page in range(8):
        if not dialog.visible:
            break
        (dialog.get_node("Confirm") as Button).pressed.emit()
        await settle()
    check(bridge.members()[0].level == 2, first + " took level 2 through the dialog")
    check(dialog.visible, "The dialog opens again for the next level")
    press(setup, "Use defaults for the rest")
    await settle()
    for member in bridge.members():
        check(member.level == 4, "%s is level 4 (level %d)" % [member.name, member.level])
        check(not member.can_advance, member.name + " has no level left to take")
    var before: Array = bridge.members()
    var area: OptionButton = setup.find_child("Area", true, false)
    var creature: OptionButton = setup.find_child("Creature", true, false)
    check(pick(area, "The Slums"), "The Slums are an area to fight in")
    check(pick(creature, "The Slums: Orc") or pick(creature, "The Slums: ORC"),
          "The Slums' orcs are a choice")
    (setup.find_child("Count", true, false) as SpinBox).value = 3
    press(setup, "Add")
    press(setup, "Fight")
    await settle(10)
    var combat: Node = setup.get_node_or_null("CampaignCombat")
    check(combat != null, "Fight opens the game's combat screen")
    if combat == null:
        finish(1)
        return
    # Every member on Quick, as the game's own checks play a fight; time runs
    # eight times as fast, for two minutes at most.
    var outcome := ""
    Engine.time_scale = 8.0
    var deadline := Time.get_ticks_msec() + 120000
    while Time.get_ticks_msec() < deadline:
        await settle(2)
        outcome = bridge.outcome()
        if not outcome.is_empty():
            break
        var quick := InputEventKey.new()
        quick.keycode = KEY_Q
        quick.pressed = true
        root.push_input(quick)
        # The prompts a player answers: a reaction declined, Alert's
        # initiative and temporary Hit Points kept, a message continued.
        answer_prompt(combat)
    Engine.time_scale = 1.0
    check(not outcome.is_empty(), "The fight ends with Quick combat")
    print("Fight outcome: ", outcome)
    await settle()
    var back: Button = setup.find_child("Return", true, false)
    check(back != null, "A finished fight offers the way back")
    if back != null:
        back.pressed.emit()
        await settle()
    check(setup.get_node_or_null("CampaignCombat") == null, "Returning closes the fight")
    var after: Array = bridge.members()
    for n in range(after.size()):
        check(after[n].hit_points == before[n].hit_points,
              after[n].name + " is back at the Hit Points before the fight")
        check(after[n].level == before[n].level, after[n].name + " keeps its level")
    # Every monster and NPC on the list can be put in a fight.
    var creatures: Array = bridge.catalog().creatures
    for n in range(creatures.size()):
        var error: String = bridge.start_fight({"area": area.selected, "seed": 1,
            "difficulty": "normal", "groups": [{"creature": n, "count": 1}]})
        if error == "Add a monster or NPC that fights the party":
            continue
        check(error.is_empty(), "%s can be fought: %s" % [creatures[n], error])
        bridge.leave()
        await process_frame
    print("Creatures fought: ", creatures.size())
    finish(0)
