extends SceneTree

const CONTROL_TYPES := {
    "EquipmentChoice": {"Item": "Label", "HandLabel": "Label", "Hand": "OptionButton", "Explanation": "Label", "Cancel": "Button", "Equip": "Button"},
    "InitiativeChoice": {"ResolveLabel": "Label", "Resolve": "OptionButton", "AllyLabel": "Label", "Ally": "OptionButton", "Text": "Label", "Keep": "Button", "Swap": "Button", "Metabolism": "Button"},
    "NickAttack": {"Text": "Label", "Label": "Label", "Choices": "OptionButton", "Cancel": "Button", "Target": "Button"},
    "OptionalEffect": {"ResolveLabel": "Label", "Resolve": "OptionButton", "Text": "Label", "Skip": "Button", "Use": "Button"},
    "RestDialog": {"KindLabel": "Label", "Kind": "OptionButton", "Members": "ItemList", "Info": "RichTextLabel", "Result": "Label", "Start": "Button", "Heal": "Button", "Save": "Button", "Finish": "Button", "RecoveryLabel": "Label", "RecoveryChoice": "OptionButton", "Recover": "Button", "UseLabel": "Label", "UseAction": "OptionButton", "UseTarget": "OptionButton", "Use": "Button"},
    "RestSpells": {"Title": "Label", "Known": "RichTextLabel", "Choices": "ScrollContainer", "ReplaceLabel": "Label", "WithLabel": "Label", "Replace": "OptionButton", "With": "OptionButton", "Error": "Label", "Cancel": "Button", "Apply": "Button"},
    "RestTraining": {"Title": "Label", "Current": "Label", "Limit": "Label", "Choices": "ScrollContainer", "Error": "Label", "Cancel": "Button", "Apply": "Button", "Save": "Button"},
    "SaveSlots": {"Help": "Label", "Slots": "ItemList", "Name": "LineEdit", "Status": "Label", "Action": "Button", "Cancel": "Button"},
}

func _initialize() -> void:
    for group in CONTROL_TYPES:
        var scene: PackedScene = load("res://scenes/dialog_layouts/%s.tscn" % group)
        if scene == null:
            _fail("Missing dialog scene: " + group)
            return
        var layout: Control = scene.instantiate()
        for name in CONTROL_TYPES[group]:
            var control := layout.get_node_or_null(name)
            if control == null or control.get_class() != CONTROL_TYPES[group][name]:
                _fail("Missing or mistyped dialog control: %s/%s" % [group, name])
                return
        layout.free()

    var defeat: Window = load("res://scenes/dialog_layouts/Defeat.tscn").instantiate()
    if not defeat.get_node("Reload") is Button or not defeat.get_node("Exit") is Button:
        _fail("Defeat controls are missing")
        return
    defeat.free()

    var level_up: Window = load("res://scenes/level_up_dialog.tscn").instantiate()
    root.add_child(level_up)
    var positions := []
    var training_layout: AnimationPlayer = level_up.get_node("TrainingLayout")
    for variant in ["primary", "supplemental"]:
        training_layout.play(variant)
        training_layout.advance(0)
        var label: Label = level_up.get_node("AdvancementTrainingLabel")
        var choice: OptionButton = level_up.get_node("AdvancementTraining")
        if label.position.x != choice.position.x or label.position.y >= choice.position.y:
            _fail("Training label and choice are misaligned in " + variant)
            return
        positions.append(choice.position.y)
    if positions[1] <= positions[0]:
        _fail("Supplemental training did not move below the primary position")
        return
    level_up.free()

    for group in ["OptionalEffect", "RestDialog"]:
        var authored: Control = load("res://scenes/dialog_layouts/%s.tscn" % group).instantiate()
        var window := Window.new()
        window.size = Vector2i(authored.size)
        root.add_child(window)
        while authored.get_child_count() > 0:
            var child := authored.get_child(0)
            child.owner = null
            authored.remove_child(child)
            window.add_child(child)
        var animation: AnimationPlayer = window.get_node("Layout")
        var names := ["single", "multiple"] if group == "OptionalEffect" else ["plain", "options"]
        var heights := []
        for variant in names:
            animation.play(variant)
            animation.advance(0)
            heights.append(window.size.y if group == "OptionalEffect" else window.get_node("Info").size.y)
        if (group == "OptionalEffect" and heights[1] <= heights[0]) or (group == "RestDialog" and heights[1] >= heights[0]):
            _fail("Dialog state layout did not change: " + group)
            return
        window.free()
        authored.free()

    var guide: Control = load("res://scenes/dialog_layouts/SaveSlots.tscn").instantiate()
    var expected := Rect2(guide.get_node("Slots").position, guide.get_node("Slots").size)
    guide.free()
    var native := SaveSlots.new()
    native.name = "SaveSlots"
    root.add_child(native)
    call_deferred("_check_native", native, expected)

func _check_native(native: SaveSlots, expected: Rect2) -> void:
    if native.get_node_or_null("Slots") == null:
        _fail("Native save dialog did not attach its scene controls")
        return
    var actual := Rect2(native.get_node("Slots").position, native.get_node("Slots").size)
    if actual != expected:
        _fail("Native save dialog ignored its scene rectangle")
        return
    native.free()
    print("Dialog scenes passed")
    quit(0)

func _fail(message: String) -> void:
    push_error(message)
    quit(1)
