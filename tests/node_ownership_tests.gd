extends SceneTree

# Exercise the native child-owner handoff without requiring original game data.
func _initialize() -> void:
    call_deferred("run_checks")

func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message)
        quit(1)
        assert(ok, message)

func run_checks() -> void:
    for iteration in range(3):
        var dialog := ClassDB.instantiate("SaveSlots") as Window
        # Its layout scene is found by the node name the game's scenes give it.
        dialog.name = "SaveSlots"
        root.add_child(dialog)
        require(dialog.get_child_count() == 6, "Native save dialog must attach all controls")
        var children: Array[WeakRef] = []
        for name in ["Help", "Slots", "Name", "Status", "Action", "Cancel"]:
            var child := dialog.get_node(name)
            require(child.get_parent() == dialog, "Native control must belong to its dialog")
            children.append(weakref(child))
        require(dialog.get_node("Action") is Button and dialog.get_node("Cancel") is Button,
            "Actions must remain native buttons")
        dialog.free()
        for child in children:
            require(child.get_ref() == null, "Freeing the dialog must release its native children")
    print("Native node ownership checks passed: attachment and subtree teardown")
    quit(0)
