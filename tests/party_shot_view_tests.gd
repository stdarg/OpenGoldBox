extends SceneTree

# A party member drawn with its sword draws its carried bow to shoot: the sprite
# changes with the shot, and its action pose plays. Uses the demo's real campaign.
func _initialize() -> void:
    call_deferred("check")

func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message); quit(1); assert(ok, message)

func settle(frames := 6) -> void:
    for frame in range(frames): await process_frame

func item_index(combat: Node, label: String) -> int:
    var items: OptionButton = combat.get_node("ItemAction")
    for i in range(items.item_count):
        if items.get_item_text(i).contains(label): return i
    return -1

func check() -> void:
    root.size = Vector2i(1920, 1080)
    TranslationServer.set_locale("en")
    change_scene_to_file("res://scenes/combat_demo.tscn"); await settle(10)
    var combat := current_scene
    var dialog: Window = combat.get_node("InitiativeChoice")
    while dialog.visible:
        dialog.get_node("Keep").pressed.emit(); await settle(4)
    combat.set_process(true)
    for step in range(600):
        await process_frame
        if item_index(combat, "Shoot") >= 0: break
    var index := item_index(combat, "Shoot")
    require(index >= 0, "The Rogue can shoot its carried bow")
    combat.set_process(false)
    var id: int = -1
    var shown: Dictionary = combat.weapon_sets_shown()
    for key in shown:
        if key < 1000: id = key
    require(id >= 0 and shown[id] == "staged", "It stands with its melee weapon: " + str(shown))
    var enemy := Vector2i(-1, -1)
    for actor in combat.battlefield_visuals()["actors"]:
        if not actor.get("party", false): enemy = actor["cell"]
    var items: OptionButton = combat.get_node("ItemAction")
    items.select(index); items.item_selected.emit(index); await settle()
    combat.get_node("UseItemAction").pressed.emit(); await settle()
    var canvas: Control = combat.get_node("BattlefieldScroll/Canvas")
    var point: Vector2 = canvas.get_global_transform_with_canvas() * ((Vector2(enemy) + Vector2(0.5, 0.5)) * combat.cell_pixels())
    for down in [true, false]:
        var event := InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT
        event.position = point; event.pressed = down; root.push_input(event, true)
    await settle()
    var flights: Array = combat.missiles_in_flight()
    require(not flights.is_empty() and flights[0]["actor"] == id, "The shot flies from the Rogue: " + str(flights))
    require(combat.weapon_sets_shown()[id] == "ranged", "The Rogue shows its bow: " + str(combat.weapon_sets_shown()))
    print("Party shot checks passed")
    quit()
