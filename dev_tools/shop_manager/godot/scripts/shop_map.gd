extends Control

signal location_selected(id: String)

var cells: Array = []
var locations: Array = []
var selected_id := ""

func set_content(new_cells: Array, new_locations: Array, new_selected_id: String) -> void:
    cells = new_cells
    locations = new_locations
    selected_id = new_selected_id
    queue_redraw()

func _grid_rect() -> Rect2:
    var side := floorf(minf(size.x, size.y) - 16.0)
    return Rect2((size - Vector2(side, side)) / 2.0, Vector2(side, side))

func _gui_input(event: InputEvent) -> void:
    if not event is InputEventMouseButton or event.button_index != MOUSE_BUTTON_LEFT or not event.pressed:
        return
    var area := _grid_rect()
    if not area.has_point(event.position):
        return
    var cell := Vector2i((event.position - area.position) / area.size * 16.0)
    for location in locations:
        for marker in location["markers"]:
            if Vector2i(int(marker[0]), int(marker[1])) == cell:
                location_selected.emit(String(location["id"]))
                accept_event()
                return

func _draw() -> void:
    if cells.size() != 256:
        return
    var area := _grid_rect()
    var unit := area.size.x / 16.0
    draw_rect(area, Color("101c24"))
    for y in 16:
        for x in 16:
            var origin := area.position + Vector2(x, y) * unit
            draw_rect(Rect2(origin, Vector2(unit, unit)), Color("2a3d49"), false, 1.0)
            var cell: Dictionary = cells[y * 16 + x]
            var faces: Array = cell["faces"]
            for direction in 4:
                var face: Dictionary = faces[direction]
                if int(face["wall_id"]) == 0 and int(face["door_code"]) == 0:
                    continue
                var color := Color("d6ad60") if int(face["door_code"]) != 0 else Color("63879a")
                if face.get("secret", false):
                    color = Color("e0605a")
                var inset := 2.0
                var a: Vector2
                var b: Vector2
                match direction:
                    0:
                        a = origin + Vector2(inset, inset)
                        b = origin + Vector2(unit - inset, inset)
                    1:
                        a = origin + Vector2(unit - inset, inset)
                        b = origin + Vector2(unit - inset, unit - inset)
                    2:
                        a = origin + Vector2(inset, unit - inset)
                        b = origin + Vector2(unit - inset, unit - inset)
                    _:
                        a = origin + Vector2(inset, inset)
                        b = origin + Vector2(inset, unit - inset)
                draw_line(a, b, color, 3.0 if face.get("secret", false) else 1.5)
    for location in locations:
        var is_temple := String(location["kind"]) == "temple"
        var color := Color("8d9df2") if is_temple else Color("66c7a7")
        var is_selected := String(location["id"]) == selected_id
        for marker in location["markers"]:
            var origin := area.position + Vector2(int(marker[0]), int(marker[1])) * unit
            var rect := Rect2(origin + Vector2(3, 3), Vector2(unit - 6, unit - 6))
            draw_rect(rect, color)
            if is_selected:
                draw_rect(rect.grow(2), Color("fff2a8"), false, 2.0)
            if is_temple:
                var center := origin + Vector2(unit, unit) / 2.0
                draw_line(center + Vector2(-4, 0), center + Vector2(4, 0), Color("15202f"), 2.0)
                draw_line(center + Vector2(0, -4), center + Vector2(0, 4), Color("15202f"), 2.0)
