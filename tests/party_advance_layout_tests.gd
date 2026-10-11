extends SceneTree

func _initialize() -> void:
    call_deferred("run_checks")


func run_checks() -> void:
    var list := ItemList.new()
    list.size = Vector2(500, 120)
    list.add_item("Ari")
    root.add_child(list)
    var roster_arrow: Button = load("res://scenes/control_templates/party_advance.tscn").instantiate()
    list.add_child(roster_arrow)
    await process_frame
    if not roster_arrow.call("place_in_roster", 0, "Ari") or not list.get_rect().encloses(roster_arrow.get_rect()):
        push_error("Roster advance arrow must be visible inside its list")
        quit(1)
        return
    var short_x := roster_arrow.position.x
    roster_arrow.call("place_in_roster", 0, "Aribeth Longname")
    if roster_arrow.position.x <= short_x or not list.get_rect().encloses(roster_arrow.get_rect()):
        push_error("Roster advance arrow must follow the rendered name without leaving the list")
        quit(1)
        return
    var row := Button.new()
    row.size = Vector2(400, 60)
    root.add_child(row)
    var town_arrow: Button = load("res://scenes/control_templates/party_advance.tscn").instantiate()
    row.add_child(town_arrow)
    await process_frame
    town_arrow.call("place_in_member_row", "Ari")
    short_x = town_arrow.position.x
    town_arrow.call("place_in_member_row", "Aribeth Longname")
    if town_arrow.position.x <= short_x or not row.get_rect().encloses(town_arrow.get_rect()):
        push_error("Town advance arrow must follow the rendered name inside its row")
        quit(1)
        return
    print("Party advance layout passed")
    quit(0)
