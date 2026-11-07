extends SceneTree

# The exploration screen's containers at each window size the game supports,
# exploring and shopping, and in each kind of prompt: nothing overlaps or leaves
# the window, the view keeps the original's 5:6 shape and the map is square.
const SIZES := [Vector2i(1120, 800), Vector2i(1440, 900), Vector2i(1600, 900), Vector2i(1600, 1000), Vector2i(1920, 1080)]

func _initialize() -> void:
    call_deferred("run_checks")

func fail(message: String) -> void:
    push_error(message)
    quit(1)

func rect(scene: Control, name: String) -> Rect2:
    return (scene.get_node("%" + name) as Control).get_global_rect()

func settle() -> void:
    for frame in range(3):
        await process_frame

func run_checks() -> void:
    for dimensions in SIZES:
        root.size = dimensions
        var scene: Control = load("res://scenes/rolf_tour.tscn").instantiate()
        root.add_child(scene)
        await process_frame
        # Below zero it would draw under the character creator beneath the town.
        if (scene.get_node("Backdrop") as Control).z_index < 0:
            return fail("The town's backdrop draws under the screen it covers")
        var window := Rect2(Vector2.ZERO, Vector2(dimensions))
        for shopping in [false, true]:
            scene.get_node("Layout").call("apply_layout", shopping)
            await settle()
            var view := rect(scene, "SceneBounds")
            var party := rect(scene, "PartyList")
            var map := rect(scene, "MapBounds")
            var dialogue := rect(scene, "DialogueBounds")
            if party.position.x < view.end.x or party.end.x > map.position.x:
                return fail("The party list covers the view or the map at %s" % dimensions)
            if dialogue.position.y < view.end.y or dialogue.end.x > map.position.x:
                return fail("The dialogue box covers the view or the map at %s" % dimensions)
            if absf(view.size.x / view.size.y - 5.0 / 6.0) > 0.01 or absf(map.size.x - map.size.y) > 1.0:
                return fail("The view is not 5:6 or the map not square at %s: %s %s" % [dimensions, view, map])
            if absf(map.size.x - (scene.get_node("%MapBounds").get_parent() as Control).size.x) > 1.0:
                return fail("The map is not as wide as its column at %s" % dimensions)
            for name in ["Title", "Menu", "Footer", "Continue", "Forward", "Right", "Inventory", "PartyList", "MapBounds"]:
                if not window.encloses(rect(scene, name)):
                    return fail("%s leaves the window at %s (shopping %s)" % [name, dimensions, shopping])
        var box: Node = scene.get_node("%DialogueBounds")
        var choices: ItemList = scene.get_node("%Choices")
        choices.visible = true
        for state in ["shopping", "multiple"]:
            box.call("show_mode", state)
            await settle()
            var text_rect := rect(scene, "Dialogue")
            var choices_rect := rect(scene, "Choices")
            var go := rect(scene, "Continue")
            if text_rect.end.y > choices_rect.position.y or not window.encloses(choices_rect) \
                    or text_rect.position.x != choices_rect.position.x or choices_rect.intersects(go):
                return fail("Tour %s layout overlaps or leaves the viewport at %s" % [state, dimensions])
            # A menu's list runs down beside Continue; a shop's list sits above the buttons.
            if (state == "multiple") != (choices_rect.end.x <= go.position.x):
                return fail("Tour %s puts its list in the wrong place at %s" % [state, dimensions])
        scene.queue_free()
        await process_frame
    print("Tour layout scene passed")
    quit(0)
