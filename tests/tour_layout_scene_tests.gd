extends SceneTree

const SIZES := [Vector2i(1120, 800), Vector2i(1600, 900), Vector2i(1920, 1080)]

func _initialize() -> void:
    call_deferred("run_checks")

func run_checks() -> void:
    for dimensions in SIZES:
        root.size = dimensions
        var scene: Control = load("res://scenes/rolf_tour.tscn").instantiate()
        root.add_child(scene)
        await process_frame
        var shopping: AnimationPlayer = scene.get_node("ShoppingLayout")
        shopping.play("shopping")
        shopping.seek(clampf((dimensions.x - 1120.0) / 800.0, 0.0, 1.0), true)
        var scene_rect: Rect2 = (scene.get_node("SceneBounds") as Control).get_global_rect()
        var party_rect: Rect2 = (scene.get_node("PartyList") as Control).get_global_rect()
        if scene_rect.end.x > party_rect.position.x or party_rect.end.x > dimensions.x:
            push_error("Shopping map and party list overlap or leave the viewport at %s" % dimensions)
            quit(1)
            return
        var layout: AnimationPlayer = scene.get_node("DialogueLayout")
        for state in ["shopping", "multiple"]:
            layout.play(state)
            layout.advance(0)
            var dialogue: RichTextLabel = scene.get_node("Dialogue")
            var choices: ItemList = scene.get_node("Choices")
            var text_rect := dialogue.get_global_rect()
            var choices_rect := choices.get_global_rect()
            if text_rect.end.y > choices_rect.position.y or choices_rect.end.y > dimensions.y or text_rect.position.x != choices_rect.position.x:
                push_error("Tour %s layout overlaps or leaves the viewport at %s" % [state, dimensions])
                quit(1)
                return
        scene.queue_free()
        await process_frame
    print("Tour layout scene passed")
    quit(0)
