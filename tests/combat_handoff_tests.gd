extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at an installed game.
func _initialize() -> void:
    call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4):
        await process_frame

func add_class(creation: Control, class_name_text: String) -> void:
    creation.get_node("PartyPanel/Pool").pressed.emit()
    var choices: ItemList = creation.get_node("PoolModal/List")
    var selected := -1
    for index in range(choices.item_count):
        if choices.get_item_text(index).begins_with(class_name_text + " / "):
            selected = index
            break
    require(selected >= 0, "Character pool must include " + class_name_text)
    choices.select(selected)
    choices.item_selected.emit(selected)
    creation.get_node("PoolModal/Add").pressed.emit()
    creation.get_node("PoolModal/Close").pressed.emit()

func capture() -> void:
    if not OS.get_cmdline_user_args().has("--capture"):
        return
    var screenshots := root.get_node("Screenshots")
    screenshots.request_capture()
    var result: Array = await screenshots.capture_completed
    require(result[1].is_empty(), "Combat handoff capture failed")

func run_checks() -> void:
    change_scene_to_file("res://scenes/character_creation.tscn")
    await settle()
    var creation := current_scene as Control
    require(creation.has_node("PartyPanel"), "Original character assets must load")
    creation.get_node("Party").pressed.emit()
    # Every created class can fight, so the refusal left to try is an empty party.
    creation.get_node("PartyPanel/Combat").pressed.emit()
    await settle()
    require(not creation.has_node("CampaignCombat"), "Failed initialization must not install a blank battlefield")
    require(creation.get_node("PartyPanel").is_visible_in_tree(), "Failed initialization must preserve the party screen")
    require(creation.get_node("PartyPanel/Status").text.contains("Add a party member first"), "The party screen must explain the failure")
    await capture()

    # The failed attempt must leave the party editable so users can retry.
    add_class(creation, "Fighter")
    creation.get_node("PartyPanel/Combat").pressed.emit()
    await settle()
    var combat := creation.get_node_or_null("CampaignCombat")
    require(combat != null and combat.is_visible_in_tree(), "Supported party must start combat after a rejection")
    require(not creation.get_node("PartyPanel").visible, "Successful handoff opens the combat screen")
    require(not combat.get_node("Roster").text.is_empty(), "Successful handoff must contain actual combatants")
    var scroll: ScrollContainer = combat.get_node("BattlefieldScroll")
    var canvas: Control = scroll.get_node("Canvas")
    # The board is scaled to fill the view along its larger ratio, then zoomed.
    require(canvas.size.is_equal_approx(canvas.custom_minimum_size) and canvas.size.x >= scroll.size.x - 1 and canvas.size.y >= scroll.size.y - 1, "Mounted campaign battlefield must have its full canvas")
    await capture()
    print("Combat handoff checks passed: refused empty party, visible diagnostic, party edits and successful retry")
    quit(0)
