extends Panel

# A portrait shown full size over the page, with its metadata; Esc returns.


func open(file: String, caption: String) -> void:
    # A file in the new art's folder, read as it is rather than imported.
    var image := Image.load_from_file(file)
    (get_node("Image") as TextureRect).texture = null if image == null else ImageTexture.create_from_image(image)
    (get_node("Caption") as Label).text = caption
    show()


func _input(event: InputEvent) -> void:
    if visible and event.is_action_pressed("ui_cancel"):
        hide()
        get_viewport().set_input_as_handled()
