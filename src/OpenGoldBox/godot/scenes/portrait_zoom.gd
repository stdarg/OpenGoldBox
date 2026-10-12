extends Panel

# A portrait shown full size over the page, with its metadata; Esc returns.


func open(file: String, caption: String) -> void:
    (get_node("Image") as TextureRect).texture = load(file)
    (get_node("Caption") as Label).text = caption
    show()


func _input(event: InputEvent) -> void:
    if visible and event.is_action_pressed("ui_cancel"):
        hide()
        get_viewport().set_input_as_handled()
