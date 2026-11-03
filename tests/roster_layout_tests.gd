extends SceneTree

# The party roster at the smallest and the design window: the sheet keeps room to
# read beside the portrait, nothing overlaps, and the poses' captions show only
# once there is a member to show. Uses the real character pool.
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

func rect(creation: Control, name: String) -> Rect2:
    return (creation.get_node("PartyPanel/" + name) as Control).get_global_rect()

func check_layout(creation: Control, size: Vector2i) -> void:
    root.size = size
    await settle()
    var sheet := rect(creation, "Sheet")
    require(sheet.size.x >= 400, "The sheet is %d px wide at %s" % [sheet.size.x, size])
    var portrait := rect(creation, "Portrait")
    var window := Rect2(Vector2.ZERO, Vector2(size))
    for name in ["Sheet", "Inventory", "ReadySprite", "ActionSprite", "ReadyLabel", "ActionLabel",
            "ActionRow1", "Pool", "Load"]:
        require(window.encloses(rect(creation, name)), "%s fits the window at %s" % [name, size])
        if name != "Pool" and name != "Load":
            require(not portrait.intersects(rect(creation, name)),
                "The portrait clears %s at %s" % [name, size])
    require(not rect(creation, "Inventory").intersects(rect(creation, "ReadySprite")),
        "The inventory clears the ready pose at %s" % size)
    require(not rect(creation, "ReadySprite").intersects(rect(creation, "ActionSprite")),
        "The two poses stand apart at %s" % size)

func run_checks() -> void:
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/character_creation.tscn")
    await settle()
    var creation := current_scene as Control
    creation.get_node("Party").pressed.emit()
    await settle()
    var ready_caption: Label = creation.get_node("PartyPanel/ReadyLabel")
    var action_caption: Label = creation.get_node("PartyPanel/ActionLabel")
    require(not ready_caption.visible and not action_caption.visible,
        "An empty roster shows no pose captions")
    require(not (creation.get_node("PartyPanel/Status") as Label).text.contains("PC"),
        "The status line speaks of characters")
    creation.get_node("PartyPanel/Pool").pressed.emit()
    var choices: ItemList = creation.get_node("PoolModal/List")
    require(choices.item_count > 0, "The pool supplies a character")
    choices.select(0)
    choices.item_selected.emit(0)
    creation.get_node("PoolModal/Add").pressed.emit()
    creation.get_node("PoolModal/Close").pressed.emit()
    await settle()
    require(ready_caption.visible and action_caption.visible, "A member's poses are captioned")
    for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
        await check_layout(creation, size)
    print("Roster layout checks passed")
    quit()
