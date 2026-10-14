extends Control

signal cell_clicked(cell: Vector2i)

var cells: Array = []
var pose := Vector2i(8, 8)
var facing := 0

func set_map(new_cells: Array, new_pose: Vector2i, new_facing: int) -> void:
    cells = new_cells
    pose = new_pose
    facing = new_facing
    queue_redraw()

func set_pose(new_pose: Vector2i, new_facing: int) -> void:
    pose = new_pose
    facing = new_facing
    queue_redraw()

func _grid_rect() -> Rect2:
    var side := floorf(minf(size.x, size.y) - 20.0)
    return Rect2((size - Vector2(side, side)) / 2.0, Vector2(side, side))

func _gui_input(event: InputEvent) -> void:
    if not event is InputEventMouseButton or event.button_index != MOUSE_BUTTON_LEFT or not event.pressed:
        return
    var area := _grid_rect()
    if not area.has_point(event.position):
        return
    var cell := Vector2i((event.position - area.position) / area.size * 16.0)
    cell_clicked.emit(Vector2i(clampi(cell.x, 0, 15), clampi(cell.y, 0, 15)))
    grab_focus()
    accept_event()

func _draw() -> void:
    if cells.size() != 256:
        return
    var area := _grid_rect()
    var unit := area.size.x / 16.0
    var faint := get_theme_color("grid", "MapGrid")
    var wall := get_theme_color("wall", "MapGrid")
    var door := get_theme_color("door", "MapGrid")
    var event_color := get_theme_color("event", "MapGrid")
    var selected := get_theme_color("selected", "MapGrid")
    var arrow := get_theme_color("arrow", "MapGrid")
    draw_rect(area, get_theme_color("background", "MapGrid"))
    for y in 16:
        for x in 16:
            var origin := area.position + Vector2(x, y) * unit
            var box := Rect2(origin, Vector2(unit, unit))
            draw_rect(box, faint, false, 1.0)
            var cell: Dictionary = cells[y * 16 + x]
            if int(cell.get("event_raw", 0)) != 0:
                draw_circle(origin + Vector2(unit, unit) / 2.0, maxf(2.0, unit * 0.09), event_color)
            var faces: Array = cell["faces"]
            for direction in 4:
                var face: Dictionary = faces[direction]
                if int(face["wall_id"]) == 0 and int(face["door_code"]) == 0:
                    continue
                var color: Color = door if int(face["door_code"]) != 0 else wall
                var inset := 3.0 + direction * 0.7
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
                draw_line(a, b, color, 2.0)
    var center := area.position + (Vector2(pose) + Vector2(0.5, 0.5)) * unit
    draw_rect(Rect2(area.position + Vector2(pose) * unit + Vector2(2, 2),
                    Vector2(unit - 4, unit - 4)), selected, false, 2.0)
    var directions := [Vector2.UP, Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT]
    var tip: Vector2 = center + directions[facing] * unit * 0.34
    draw_line(center - directions[facing] * unit * 0.14, tip, arrow, 3.0)
    draw_circle(tip, 3.0, arrow)
