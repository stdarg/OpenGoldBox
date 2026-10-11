extends Node


func _place(path: String, rect: Rect2) -> void:
	var control := get_parent().get_node(path) as Control
	control.position = rect.position
	control.size = rect.size


func apply(shopping: bool) -> void:
	var view := get_parent() as Control
	var width := view.size.x
	var height := view.size.y
	var margin := 24.0
	var gutter := 24.0
	var sidebar := minf(clampf(width * 0.30, 270.0, 430.0), height - 380.0)
	var main_width := width - margin * 2 - gutter - sidebar
	var view_height := minf(main_width * 0.625, height - (490.0 if shopping else 410.0))
	var scene := Rect2(margin, 102, view_height / 1.2, view_height)
	var map := Rect2(margin + main_width + gutter, 102, sidebar, sidebar)
	var dialogue := Rect2(margin, scene.end.y + 18, main_width,
		height - scene.end.y - 76)
	_place("SceneBounds", scene)
	_place("MapBounds", map)
	_place("DialogueBounds", dialogue)
	_place("SaveGame", Rect2(width - 520, 20, 140, 36))
	_place("LoadGame", Rect2(width - 370, 20, 140, 36))
	_place("Title", Rect2(margin, 20, main_width, 34))
	_place("PartyList", Rect2(scene.end.x + 16, 102,
		main_width - scene.size.x - 16, view_height))
	_place("Location", Rect2(margin, 68, main_width, 26))
	_place("MapTitle", Rect2(map.position.x, 68, sidebar - 130, 26))
	_place("MapMode", Rect2(width - margin - 122, 64, 122, 30))
	_place("Coordinates", Rect2(map.position.x, map.end.y + 12, sidebar, 28))
	_place("Legend", Rect2(map.position.x, map.end.y + 48, sidebar, 50))
	_place("Speaker", Rect2(dialogue.position + Vector2(18, 12), Vector2(main_width - 36, 26)))
	_place("Dialogue", Rect2(dialogue.position + Vector2(18, 46),
		Vector2(main_width - 36, dialogue.size.y - 112)))
	_place("Continue", Rect2(dialogue.end - Vector2(182, 54), Vector2(164, 40)))
	_place("Progress", Rect2(dialogue.position + Vector2(18, dialogue.size.y - 48),
		Vector2(main_width - 220, 30)))
	_place("Movement", Rect2(map.position.x, height - 178, sidebar, 26))
	var button_width := (sidebar - 12) / 3.0
	_place("Left", Rect2(map.position.x, height - 140, button_width, 40))
	_place("Forward", Rect2(map.position.x + button_width + 6,
		height - 140, button_width, 40))
	_place("Right", Rect2(map.position.x + (button_width + 6) * 2,
		height - 140, button_width, 40))
	_place("Restart", Rect2(map.position.x, height - 88, sidebar, 34))
	_place("Footer", Rect2(margin, height - 36, width - 2 * margin, 26))
	_place("Party", Rect2(map.position.x, map.end.y + 46, sidebar, 50))
	(view.get_node("Legend") as Label).hide()
	var utility_width := (sidebar - 12) / 3.0
	_place("Look", Rect2(map.position.x, height - 226, utility_width, 34))
	_place("Camp", Rect2(map.position.x + utility_width + 6,
		height - 226, utility_width, 34))
	_place("Inventory", Rect2(map.position.x + 2 * (utility_width + 6),
		height - 226, utility_width, 34))
	_place("Choices", Rect2(dialogue.position + Vector2(18, 84),
		Vector2(main_width - 36, dialogue.size.y - 148)))
	_place("Answer", Rect2(dialogue.position + Vector2(18, dialogue.size.y - 100),
		Vector2(main_width - 36, 36)))
	_place("LeaveShop", Rect2(dialogue.end - Vector2(332, 54), Vector2(140, 40)))
	var inventory := Rect2(margin + 40, 90, width - 2 * margin - 80, height - 160)
	_place("InventoryPanel", inventory)
	_place("InventoryPanel/Items", Rect2(20, 70,
		inventory.size.x - 40, inventory.size.y - 210))
	_place("InventoryPanel/Close", Rect2(20, inventory.size.y - 54, 180, 36))
	_place("InventoryPanel/Header", Rect2(20, 18, inventory.size.x - 40, 44))
	_place("InventoryPanel/Status", Rect2(20, inventory.size.y - 132,
		inventory.size.x - 40, 68))
	_place("InventoryPanel/Equip", Rect2(220, inventory.size.y - 54, 150, 36))
	_place("InventoryPanel/Unequip", Rect2(390, inventory.size.y - 54, 150, 36))
	(view.get_node("MemberSheet") as Window).size = Vector2i(width - 120, height - 120)
	_place("MemberSheet/Text", Rect2(24, 24, width - 168, height - 220))
	_place("MemberSheet/Close", Rect2(width - 290, height - 180, 130, 36))


func place_dialogue(shopping: bool, multiple: bool, answer: bool) -> void:
	var dialogue := (get_parent().get_node("DialogueBounds") as Control).get_rect()
	var text_height := dialogue.size.y - (160 if answer else 112)
	if shopping or multiple:
		text_height = 36
	if multiple:
		text_height = minf((dialogue.size.y - 122) * 0.55,
			maxf(28.0, dialogue.size.y - 192))
		_place("Choices", Rect2(dialogue.position + Vector2(18, 56 + text_height),
			Vector2(dialogue.size.x - 36, dialogue.size.y - 120 - text_height)))
	else:
		_place("Choices", Rect2(dialogue.position + Vector2(18, 84),
			Vector2(dialogue.size.x - 36, dialogue.size.y - 148)))
	(get_parent().get_node("Dialogue") as RichTextLabel).size = Vector2(
		dialogue.size.x - 36, text_height)
