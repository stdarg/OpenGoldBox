extends Node

var _free_movement := false


func _place(path: String, rect: Rect2) -> void:
	var control := get_parent().get_node(path) as Control
	control.position = rect.position
	control.size = rect.size


func apply(board_size: Vector2i) -> void:
	var view := get_parent() as Control
	var width := view.size.x
	var height := view.size.y
	var sidebar := 358.0
	var left_width := width - sidebar - 72.0
	var recovery := (view.get_node("Stabilize") as Button).visible \
		or (view.get_node("StandUp") as Button).visible
	var weapon_height := 44.0 if (view.get_node("Weapons") as OptionButton).visible else 0.0
	var bonus_height := 44.0 if (view.get_node("CunningAction") as OptionButton).visible else 0.0
	var thrown := (view.get_node("ThrownWeapon") as OptionButton).visible
	var reserve := 368.0 if thrown else 324.0 if recovery else 280.0
	var tile := minf(left_width / board_size.x,
		(height - weapon_height - bonus_height - reserve) / board_size.y)
	var board := Rect2(24, 116, tile * board_size.x, tile * board_size.y)
	_place("BattlefieldBounds", board)
	var right := width - sidebar - 24.0
	_place("Title", Rect2(24, 18, left_width, 34))
	_place("Subtitle", Rect2(24, 62, left_width, 45))
	_place("Training", Rect2(right, 20, 112, 34))
	_place("Slums", Rect2(right + 120, 20, 112, 34))
	_place("Replay", Rect2(right + 240, 20, 118, 34))
	_place("Turn", Rect2(right, 70, sidebar, 70))
	_place("Roster", Rect2(right, 148, sidebar, 160))
	_place("Prompt", Rect2(right, 318, sidebar, 46))
	var actions := ["Move", "Melee", "Ranged", "FireBolt", "MagicMissile", "CureWounds",
		"HealingWord", "ScorchingRay", "SpellSlot", "SecondWind", "Dash", "Dodge",
		"Disengage", "End", "Continue"]
	for index in actions.size():
		var row: int = index / 3
		var column: int = index % 3
		_place(actions[index], Rect2(right + column * 122, 370 + row * 43, 114, 36))
	_place("React", Rect2(right, 590, 174, 36))
	_place("Decline", Rect2(right + 184, 590, 174, 36))
	_place("Nick", Rect2(right + 184, 590, 174, 36))
	_place("Save", Rect2(right, 639, 112, 34))
	_place("Load", Rect2(right + 122, 639, 112, 34))
	_place("Revisit", Rect2(right + 244, 639, 114, 34))
	_place("Help", Rect2(right, 686, sidebar, height - 732))
	var weapon_top := board.end.y + 16
	_place("WeaponLabel", Rect2(24, weapon_top, 180, 36))
	_place("Weapons", Rect2(214, weapon_top, 450, 36))
	var bonus_top := weapon_top + weapon_height
	_place("CunningActionLabel", Rect2(24, bonus_top, 180, 36))
	_place("CunningAction", Rect2(214, bonus_top, 200, 36))
	_place("UseCunningAction", Rect2(424, bonus_top, 240, 36))
	var top := bonus_top + bonus_height
	_place("Stabilize", Rect2(24 + left_width - 170, top, 170, 36))
	_place("StandUp", Rect2(24, top + 44, 180, 36))
	_place("ThrownWeaponLabel", Rect2(24, top + 88, 190, 36))
	_place("ThrownWeapon", Rect2(224, top + 88, 276, 36))
	_place("Throw", Rect2(510, top + 88, 204, 36))
	var log_inset := 132.0 if thrown else 88.0 if recovery else 0.0
	_place("Log", Rect2(24, top + log_inset, left_width,
		maxf(0.0, height - board.end.y - 64 - weapon_height - bonus_height - log_inset)))
	_place("Footer", Rect2(24, height - 34, width - 48, 24))
	set_free_movement(_free_movement)


func set_free_movement(active: bool) -> void:
	_free_movement = active
	var view := get_parent()
	var end := view.get_node("End") as Button
	var continued := view.get_node("Continue") as Button
	end.size.x = (continued.position.x + continued.size.x - end.position.x
		if active else continued.size.x)
	continued.visible = not active
