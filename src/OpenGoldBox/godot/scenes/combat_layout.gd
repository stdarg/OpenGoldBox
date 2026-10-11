extends Node

# Presentation-only layout for CombatDemo. Control rectangles are authored in
# combat_demo.tscn; spacing and reserves are in the OpenGold theme. This script
# keeps the current geometry when action rows appear without rebuilding C++.
var _authored_offsets: Dictionary = {}
var _show_controls := false


func _view() -> Control:
    return get_parent() as Control


func _metric(name: String) -> float:
    return _view().get_theme_constant(name, "OpenGoldMetrics")


func _restore(control: Control) -> void:
    if not _authored_offsets.has(control):
        _authored_offsets[control] = [control.offset_left, control.offset_top,
            control.offset_right, control.offset_bottom]
    var offsets: Array = _authored_offsets[control]
    control.offset_left = offsets[0]
    control.offset_top = offsets[1]
    control.offset_right = offsets[2]
    control.offset_bottom = offsets[3]


func controls_height(show_controls: bool) -> float:
    var view := _view()
    var row := _metric("combat_action_row_step")
    var weapon_height := row if view.get_node("Weapons").visible else 0.0
    var rush: bool = view.get_node("AdrenalineRush").visible
    var spells: bool = view.get_node("Cantrip").visible
    var surge: bool = view.get_node("ActionSurge").visible
    var cunning: bool = view.get_node("CunningAction").visible
    var aid: bool = view.get_node("Stabilize").visible
    var standing: bool = view.get_node("StandUp").visible
    var inset := weapon_height
    if view.get_node("ThrownWeapon").visible:
        inset += _metric("combat_thrown_action_rows") * row
    elif standing:
        inset += _metric("combat_standing_action_rows") * row
    elif cunning or aid:
        inset += _metric("combat_aid_action_rows") * row
    else:
        inset += (row if show_controls else 0.0) + (row if rush or spells or surge else 0.0)
    if view.get_node("ItemAction").visible:
        inset += row
    return inset


func apply_layout(board_size: Vector2i, zoom: float, show_controls: bool) -> Dictionary:
    var view := _view()
    for child in view.get_children():
        if child is Control and child.has_meta("layout_reference"):
            _restore(child)
    var scroll: ScrollContainer = view.get_node("BattlefieldScroll")
    var reserved := controls_height(show_controls)
    var height := minf(scroll.size.y, view.size.y - _metric("combat_board_bottom_reserve")
        - reserved - _metric("combat_min_log_height"))
    scroll.offset_bottom = scroll.position.y + height - scroll.anchor_bottom * view.size.y
    var base_tile := maxf(scroll.size.x / board_size.x, scroll.size.y / board_size.y)
    var canvas: Control = view.get_node("BattlefieldScroll/Canvas")
    canvas.custom_minimum_size = Vector2(board_size) * base_tile * zoom
    layout_reaction_controls(show_controls)
    return {"controls_height": reserved, "base_tile": base_tile}


func layout_reaction_controls(show_controls: bool) -> void:
    _show_controls = show_controls
    var view := _view()
    var row := _metric("combat_action_row_step")
    var scroll: ScrollContainer = view.get_node("BattlefieldScroll")
    var top := scroll.position.y + scroll.size.y + _metric("combat_action_top_gap")
    for child in view.get_children():
        if child is Control and child.has_meta("action_row"):
            _restore(child)
    var authored_top: float = view.get_node("React").position.y
    var weapon_height := row if view.get_node("Weapons").visible else 0.0
    for child in view.get_children():
        if child is Control and child.has_meta("action_row"):
            var extra := weapon_height if child.has_meta("action_after_weapon") else 0.0
            child.position.y += top - authored_top + extra
    var inset := controls_height(show_controls)
    var item_y := top + inset - (row if view.get_node("ItemAction").visible else 0.0)
    for child in view.get_children():
        if child is Control and child.has_meta("action_items"):
            child.position.y = item_y
    var cunning_layout: AnimationPlayer = view.get_node("CunningLayout")
    cunning_layout.play("aid" if view.get_node("Stabilize").visible else "normal")
    cunning_layout.advance(0)
    layout_log()


func layout_log() -> void:
    var view := _view()
    var scroll: ScrollContainer = view.get_node("BattlefieldScroll")
    var top := scroll.position.y + scroll.size.y + _metric("combat_action_top_gap")
    var log_top := top + controls_height(_show_controls)
    var stack: Control = view.get_node("LogStack")
    stack.offset_top = log_top - stack.anchor_top * view.size.y
    stack.offset_bottom = maxf(log_top, view.size.y - _metric("combat_log_bottom_margin")) \
        - stack.anchor_bottom * view.size.y


func place_hover(local: Vector2) -> void:
    var view := _view()
    var panel: PanelContainer = view.get_node("HoverInfo")
    var size := panel.size
    panel.position = Vector2(
        clampf(local.x + _metric("combat_hover_offset_x"), 0.0, maxf(0.0, view.size.x - size.x)),
        clampf(local.y + _metric("combat_hover_offset_y"), 0.0, maxf(0.0, view.size.y - size.y)))
