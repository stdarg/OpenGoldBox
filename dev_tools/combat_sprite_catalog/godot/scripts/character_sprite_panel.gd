extends VBoxContainer

const COLOR_NAMES := [
    "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown", "Light gray",
    "Dark gray", "Light blue", "Light green", "Light cyan", "Light red", "Pink",
    "Yellow", "White"
]
const REGIONS := ["Weapon", "Body", "Hair / Face", "Shield", "Arms", "Legs"]
const DEFAULT_COLORS := [7, 1, 6, 6, 1, 6, 15, 9, 12, 14, 9, 14]

var catalog: SpriteCatalog
var body_groups: Array = []
var colors: Array = DEFAULT_COLORS.duplicate()
var head_index := 0
var body_group_index := 0
var color_bank := 0
var color_part := 0

@onready var head_value: Label = $Top/Head/Value
@onready var body_value: Label = $Top/Body/Value
@onready var size_picker: OptionButton = $Top/Size
@onready var weapon_picker: OptionButton = $Top/Weapon
@onready var shield_picker: CheckButton = $Top/Shield
@onready var palette_hint: Label = $PaletteHint
@onready var ready_preview: Control = $Previews/Ready/Panel/Pose
@onready var action_preview: Control = $Previews/Action/Panel/Pose
@onready var character_status: Label = $CharacterStatus

func _ready() -> void:
    $Top/Head/Previous.pressed.connect(func(): _step_head(-1))
    $Top/Head/Next.pressed.connect(func(): _step_head(1))
    $Top/Body/Previous.pressed.connect(func(): _step_body(-1))
    $Top/Body/Next.pressed.connect(func(): _step_body(1))
    size_picker.item_selected.connect(func(_index: int): _refresh())
    weapon_picker.item_selected.connect(func(_index: int): _refresh())
    shield_picker.toggled.connect(func(_enabled: bool): _refresh())
    for bank in 2:
        for part in 6:
            var button: Button = get_node("Colors/Color%d_%d" % [bank, part])
            button.pressed.connect(_select_color.bind(bank, part))
    for index in 16:
        var swatch: Button = get_node("Palette/Swatch%d" % index)
        swatch.configure_palette_index(index)
        swatch.tooltip_text = COLOR_NAMES[index]
        swatch.pressed.connect(_choose_palette.bind(index))
    size_picker.add_item("Size: Short", 0)
    size_picker.add_item("Size: Regular", 1)
    size_picker.add_item("Size: Large", 2)
    size_picker.select(1)

func set_catalog(value: SpriteCatalog) -> void:
    catalog = value
    body_groups = catalog.combat_body_groups()
    weapon_picker.clear()
    for option in catalog.weapons():
        weapon_picker.add_item(option.label, option.type)
    _refresh()

func _step_head(direction: int) -> void:
    head_index = posmod(head_index + direction, 14)
    _refresh()

func _step_body(direction: int) -> void:
    if body_groups.is_empty():
        return
    body_group_index = posmod(body_group_index + direction, body_groups.size())
    _refresh()

func _select_color(bank: int, part: int) -> void:
    color_bank = bank
    color_part = part
    _refresh()

func _choose_palette(index: int) -> void:
    colors[color_bank * 6 + color_part] = index
    _refresh()

func _refresh() -> void:
    if catalog == null or body_groups.is_empty() or weapon_picker.item_count == 0:
        return
    var size_index := size_picker.get_selected_id()
    var result: Dictionary = catalog.character_art(head_index, body_groups[body_group_index],
        size_index, weapon_picker.get_selected_id(), shield_picker.button_pressed, colors)
    if result.has("error"):
        character_status.text = result.error
        return
    head_value.text = "%d / 14" % (head_index + 1)
    body_value.text = "%d / %d" % [body_group_index + 1, body_groups.size()]
    ready_preview.set_pose(result.ready, size_index)
    action_preview.set_pose(result.action, size_index)
    var usage: Array = result.color_usage
    if not usage[color_bank * 6 + color_part] and color_part != 0 and color_part != 3:
        for index in 12:
            if usage[index]:
                color_bank = index / 6
                color_part = index % 6
                break
    palette_hint.text = "%s / Color-%d: choose a swatch" % [
        REGIONS[color_part], color_bank + 1]
    for bank in 2:
        for part in 6:
            var button: Button = get_node("Colors/Color%d_%d" % [bank, part])
            var present: bool = usage[bank * 6 + part] or part == 0 or part == 3
            var selected := bank == color_bank and part == color_part
            var color_index: int = colors[bank * 6 + part]
            button.disabled = not present
            button.text = ("> " if selected else "") + COLOR_NAMES[color_index] if present else "Not present"
            button.configure_part_index(color_index, selected)
            button.tooltip_text = (
                "Choose a swatch to recolor this part when equipped in combat."
                if part == 0 or part == 3 else
                "Choose a swatch to recolor this part in the preview."
                if present else "This part is not present in either pose."
            )
    character_status.text = "%s · artwork body %d%s" % [result.selection, result.body,
        "" if result.matched else " · no mapped art; unarmed appearance shown"]

func check_controls() -> String:
    if not find_children("*", "ScrollContainer", true, true).is_empty():
        return "Character choices should not use a scroll panel"
    if body_groups.is_empty() or ready_preview.sprite == null or action_preview.sprite == null:
        return "Character preview did not load"
    var original_head := head_index
    $Top/Head/Next.pressed.emit()
    if head_index != (original_head + 1) % 14:
        return "Head Next did not advance"
    $Top/Head/Previous.pressed.emit()
    if head_index != original_head:
        return "Head Previous did not return"
    var original_body := body_group_index
    $Top/Body/Next.pressed.emit()
    if body_group_index != (original_body + 1) % body_groups.size():
        return "Body Next did not advance by distinct looks"
    $Top/Body/Previous.pressed.emit()
    if body_group_index != original_body:
        return "Body Previous did not return"
    var original_color: int = colors[0]
    get_node("Colors/Color0_0").pressed.emit()
    get_node("Palette/Swatch13").pressed.emit()
    if colors[0] != 13 or ready_preview.sprite == null or action_preview.sprite == null:
        return "Palette swatch did not recolor both poses"
    colors[0] = original_color
    size_picker.select(2)
    _refresh()
    if ready_preview.size_index != 2 or action_preview.size_index != 2:
        return "Large size did not reach both previews"
    size_picker.select(1)
    _refresh()
    if weapon_picker.item_count > 1:
        var original_weapon := weapon_picker.selected
        weapon_picker.select(1)
        weapon_picker.item_selected.emit(1)
        shield_picker.button_pressed = true
        if ready_preview.sprite == null or action_preview.sprite == null:
            return "Weapon and shield controls did not update both previews"
        shield_picker.button_pressed = false
        weapon_picker.select(original_weapon)
        _refresh()
    return ""
