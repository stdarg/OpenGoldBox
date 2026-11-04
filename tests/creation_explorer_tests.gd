extends SceneTree

# Drives the real character-creation screen through every species, species
# option, background and class, one screen making character after character
# the way a player does. On every page it checks what the player sees: each
# heading "(picked / owed)" has enough clickable choices drawn under it, every
# offered choice can be clicked, a click undone leaves the page as it was,
# Next turns on once everything is chosen, Back keeps every page, the scores
# are the dice plus the bonuses shown, nothing is drawn off the screen or cut
# off, and the sheet shows every choice made. A new character starts like one
# on a fresh screen, and a character made again on a fresh screen sees the same
# pages. Prints one "BUG:" line per problem and fails if there is any.
#
# The suite runs a sample (opengold_godot_creation_explorer). Every
# combination, both fill orders and the detours, one Godot per species:
#   godot --headless --path src/OpenGoldBox/godot --script tests/creation_explorer_tests.gd \
#       -- --race=Elf [--policy=1 --detours --shuffle=17 --fresh=5] [--trail]
# --class=Cleric limits it to one class; --sample=3 makes every third
# character; --fresh=5 makes every fifth character again on a fresh screen;
# --replay=LOG [--from=N --to=M] repeats a game log's clicks on the creation
# screen instead (lines N to M), failing when a click does not stick or shows
# an error; the dice are rolled for the class the log picks next.
# --size=1920x1080 sets the window; --limit=4 stops after four characters;
# --shots=DIR saves each page once, in a windowed run; --trail prints the clicks that led to each
# problem.

const ABILITY_NEEDS := {
	"Barbarian": [0], "Bard": [5], "Cleric": [4], "Druid": [4], "Fighter": [0],
	"Monk": [1, 4], "Paladin": [0, 5], "Ranger": [1, 4], "Rogue": [1],
	"Sorcerer": [5], "Warlock": [5], "Wizard": [3]}
const ABILITIES := ["strength", "dexterity", "constitution", "intelligence", "wisdom", "charisma"]
const HEADING := "\\((\\d+) / (\\d+)\\)$"
const RACE_PAGE := "Name, Race & Gender"

var heading_pattern := RegEx.new()
var bonus_pattern := RegEx.new()
var bugs := {}
var characters := 0
var context := ""
var only_race := ""
var only_class := ""
var policy := 0
var trail := []
var forced_policy := -1
var detours := false
var shuffle_seed := 0
var show_trail := false
var sample := 1
var fresh_every := 0
var window := Vector2i(1120, 800)
# The first page of a fresh screen, which every new character must match.
var first_page := []
# The pages of the character being made, as it left each one.
var record := []
# Each page as it was last left, and the choices ticked on it.
var left := {}
var chosen := {}
var races := []
# Where to save a picture of each page, once (a windowed run).
var shots := ""
var limit := 0
var replay_log := ""
var replay_from := 1
var replay_to := 1 << 30
var shot := {}

func _initialize() -> void:
	heading_pattern.compile(HEADING)
	bonus_pattern.compile("\\(([+-]\\d+)\\)$")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--race="): only_race = arg.trim_prefix("--race=")
		if arg.begins_with("--class="): only_class = arg.trim_prefix("--class=")
		if arg == "--trail": show_trail = true
		if arg.begins_with("--policy="): forced_policy = int(arg.trim_prefix("--policy="))
		if arg == "--detours": detours = true
		if arg.begins_with("--shuffle="): shuffle_seed = int(arg.trim_prefix("--shuffle="))
		if arg.begins_with("--sample="): sample = int(arg.trim_prefix("--sample="))
		if arg.begins_with("--fresh="): fresh_every = int(arg.trim_prefix("--fresh="))
		if arg.begins_with("--shots="): shots = arg.trim_prefix("--shots=")
		if arg.begins_with("--limit="): limit = int(arg.trim_prefix("--limit="))
		if arg.begins_with("--replay="): replay_log = arg.trim_prefix("--replay=")
		if arg.begins_with("--from="): replay_from = int(arg.trim_prefix("--from="))
		if arg.begins_with("--to="): replay_to = int(arg.trim_prefix("--to="))
		if arg.begins_with("--size="):
			var parts := arg.trim_prefix("--size=").split("x")
			window = Vector2i(int(parts[0]), int(parts[1]))
	call_deferred("run")

func bug(kind: String, detail: String) -> void:
	var key := kind + " | " + detail
	if not bugs.has(key):
		bugs[key] = []
		print("BUG: ", key, " | ", context)
		if show_trail: print("    trail: ", " ".join(trail))
	bugs[key].append(context)

func settle(frames := 2) -> void:
	for frame in range(frames):
		await process_frame

func node(path: String) -> Node:
	return current_scene.get_node_or_null(path)

func status_text() -> String:
	var status: Label = node("Status")
	return status.text if status and status.visible else ""

func press(path: String) -> bool:
	var button: Button = node(path)
	if button == null or not button.is_visible_in_tree() or button.disabled:
		return false
	trail.append("[" + path + "]")
	button.pressed.emit()
	await settle()
	return true

func list_items(path: String) -> Array:
	var list = node(path)
	var items := []
	for i in range(list.item_count):
		items.append(list.get_item_text(i))
	return items

func choose(path: String, text: String) -> bool:
	var list = node(path)
	for i in range(list.item_count):
		if list.get_item_text(i) == text:
			if list is ItemList and list.is_item_disabled(i): return false
			trail.append(path + "=" + text)
			list.select(i)
			list.item_selected.emit(i)
			await settle()
			return true
	return false

func choose_index(path: String, index: int) -> void:
	var list = node(path)
	trail.append(path + "#" + str(index))
	list.select(index)
	list.item_selected.emit(index)
	await settle()

# The first line where two descriptions of a page differ.
func difference(before: Array, after: Array) -> String:
	for i in range(maxi(before.size(), after.size())):
		var a: String = before[i] if i < before.size() else "(nothing)"
		var b: String = after[i] if i < after.size() else "(nothing)"
		if a != b: return "'" + a.strip_edges() + "' -> '" + b.strip_edges() + "'"
	return ""

# --- Choice groups as the player sees them -------------------------------

func collect(root_node: Node, into: Array) -> void:
	for child in root_node.get_children():
		if child is Control and not child.is_visible_in_tree(): continue
		if child is CheckBox: into.append(child)
		elif child is Label and heading_pattern.search(child.text): into.append(child)
		collect(child, into)

func by_position(a: Control, b: Control) -> bool:
	var pa := a.get_global_rect().position
	var pb := b.get_global_rect().position
	if absf(pa.y - pb.y) > 2: return pa.y < pb.y
	return pa.x < pb.x

# Each heading with the checkboxes drawn under it, before the next heading.
func groups(container_path: String) -> Array:
	var container: Control = node(container_path)
	if container == null or not container.is_visible_in_tree(): return []
	var items := []
	collect(container, items)
	items.sort_custom(by_position)
	var result := []
	var stray := []
	for item in items:
		if item is Label:
			var found := heading_pattern.search(item.text)
			result.append({"title": item, "picked": int(found.get_string(1)), "owed": int(found.get_string(2)), "boxes": []})
		elif result.is_empty():
			stray.append(item)
		else:
			result.back()["boxes"].append(item)
	for box in stray:
		bug("choice above every heading", container_path + "/" + box.name + " '" + box.text + "'")
	return result

func heading_name(group: Dictionary) -> String:
	return heading_pattern.sub(group["title"].text, "").strip_edges()

func pressed_set(container_path: String) -> Array:
	var names := []
	for group in groups(container_path):
		for box in group["boxes"]:
			if box.button_pressed: names.append(heading_name(group) + "/" + box.name)
	names.sort()
	return names

# The ticked choices of the choice pages, with the names the player reads.
func ticked() -> Array:
	var result := []
	for path in ["SpeciesChoices", "Training", "SpellChoices"]:
		for group in groups(path):
			for box in group["boxes"]:
				if box.button_pressed: result.append({"heading": heading_name(group), "name": str(box.name), "text": box.text})
	return result

# A page's headings and choices as the player sees them, in screen order.
func describe(container_path: String) -> Array:
	var lines := []
	for group in groups(container_path):
		lines.append(group["title"].text)
		for box in group["boxes"]:
			lines.append("  " + str(box.name) + " '" + box.text + "'" + (" [x]" if box.button_pressed else "") + (" off" if box.disabled else ""))
	return lines

func selected_text(path: String) -> String:
	var list = node(path)
	if list is ItemList:
		var picked: PackedInt32Array = list.get_selected_items()
		return list.get_item_text(picked[0]) if picked.size() > 0 else "(none)"
	if list is OptionButton:
		return list.get_item_text(list.selected) if list.selected >= 0 else "(none)"
	return ""

# Everything a page shows the player; `with_scores` adds the rolled numbers,
# which a fresh screen rolls differently.
func page_snapshot(with_scores := true) -> Array:
	var title := page()
	var lines := [title, "Next " + ("off" if node("Next").disabled else "on"),
		"Instructions: " + node("Instructions").text, "Steps: " + node("Steps").text.replace("\n", " "),
		"Status: " + status_text()]
	if title == RACE_PAGE:
		lines.append_array(["Name: " + node("Name").text, "Race: " + selected_text("Choices"), "Gender: " + selected_text("Gender")])
	elif title == "Alignment" or title == "Class":
		lines.append("Chosen: " + selected_text("Choices"))
	elif title == "Attributes":
		lines.append_array(["Background: " + selected_text("Background"), "Bonus: " + selected_text("Bonus")])
		for i in range(ABILITY_NEEDS.size()):
			var target: CheckBox = node("Targets/Rows/Class" + str(i))
			if target: lines.append("Target " + target.text + (" [x]" if target.button_pressed else ""))
		for i in range(6):
			if with_scores:
				lines.append("Score " + str(i) + ": dice '" + node("Dice" + str(i)).get_parsed_text() + "' score '" + node("Score" + str(i)).text + "' bonus '" + node("BonusScore" + str(i)).text + "'")
	elif title.begins_with("Combat"):
		lines.append_array(["Head " + node("CombatHeadLabel").text, "Body " + node("BodyLabel").text,
			"Size " + selected_text("Size"), node("PaletteHint").text])
		for bank in range(2):
			for part in range(6): lines.append("Color" + str(bank) + "_" + str(part) + " " + node("Color" + str(bank) + "_" + str(part)).text)
	elif title == "Portrait":
		lines.append_array([node("PortraitIdentity").text, "Portrait " + str(node("PortraitPreview").get("_file")),
			"Filters " + selected_text("PortraitGender") + " / " + selected_text("PortraitClass") + " / " + selected_text("PortraitRace")])
	for path in ["SpeciesChoices", "Training", "SpellChoices"]:
		lines.append_array(describe(path))
	if node("TrainingFixed").is_visible_in_tree():
		lines.append("Fixed: " + node("TrainingFixed").get_parsed_text().replace("\n", " / "))
	return lines

func click(box: CheckBox, container_path: String, on: bool) -> bool:
	var scroll: ScrollContainer = node(container_path)
	if scroll is ScrollContainer:
		scroll.ensure_control_visible(box)
		await settle(1)
		var view := scroll.get_global_rect().grow(1)
		var rect := box.get_global_rect()
		if not view.encloses(rect) and not view.intersects(rect):
			bug("choice not shown on screen", container_path + "/" + box.name)
	var before := status_text()
	trail.append(("+" if on else "-") + str(box.name))
	box.set_pressed(on)
	await settle()
	var after := status_text()
	if after != "" and after != before:
		bug("clicking a choice shows an error", box.name + ": " + after)
		return false
	return true

func find_box(container_path: String, title: String, box_name: String) -> CheckBox:
	for group in groups(container_path):
		if heading_name(group) == title:
			for box in group["boxes"]:
				if box.name == box_name: return box
	return null

func picked_count(container_path: String, title: String) -> int:
	for g in groups(container_path):
		if heading_name(g) == title: return g["picked"]
	return -1

# Clicks each offered choice on and off again, checking the heading counts it
# and the page is as it was afterwards.
func try_every_choice(container_path: String) -> void:
	for group in groups(container_path):
		var title := heading_name(group)
		for listed in group["boxes"]:
			var name: String = listed.name
			# Earlier clicks can redraw the page: read the box and count afresh.
			var box := find_box(container_path, title, name)
			if box == null or box.disabled or box.button_pressed or box.button_group != null: continue
			var picked := picked_count(container_path, title)
			var page_before := describe(container_path)
			if not await click(box, container_path, true): continue
			var after := picked_count(container_path, title)
			if after != picked + 1:
				bug("click does not count", container_path + " " + title + "/" + name + " (" + str(picked) + " -> " + str(after) + ")")
			var again := find_box(container_path, title, name)
			if again and again.button_pressed:
				await click(again, container_path, false)
				var page_after := describe(container_path)
				if page_after != page_before:
					bug("a choice ticked and unticked changes the page", container_path + " " + title + "/" + name + ": " + difference(page_before, page_after))
			elif again == null:
				bug("choice moved away after clicking it", title + "/" + name)
		# A full heading's other choices are off; a click that reaches one
		# anyway is refused, and the box must not stay ticked.
		var blocked: Array = groups(container_path).filter(func(g): return heading_name(g) == title)
		if not blocked.is_empty():
			var off: Array = blocked[0]["boxes"].filter(func(b): return b.disabled and not b.button_pressed and b.button_group == null)
			if not off.is_empty():
				var box: CheckBox = off[0]
				trail.append("!" + str(box.name))
				box.set_pressed(true)
				await settle()
				var again := find_box(container_path, title, box.name)
				if again and again.button_pressed and status_text() != "":
					bug("refused click leaves the box ticked", container_path + " " + title + "/" + box.name + ": " + status_text())
		# Radio choices: pick each one in turn, then back to the first picked,
		# which must bring the page back as it was.
		var radios: Array = group["boxes"].filter(func(b): return b.button_group != null and not b.disabled)
		if radios.size() > 1:
			var original := ""
			for b in radios:
				if b.button_pressed: original = b.name
			var page_before := describe(container_path)
			for b in radios:
				var again := find_box(container_path, title, b.name)
				if again and not again.button_pressed: await click(again, container_path, true)
			if original != "":
				var back := find_box(container_path, title, original)
				if back and not back.button_pressed: await click(back, container_path, true)
				var page_after := describe(container_path)
				if page_after != page_before:
					bug("going through a choice's options and back changes the page", container_path + " " + title + " back on " + original + ": " + difference(page_before, page_after))

# Fills every heading; `top_down` picks headings in screen order, otherwise
# from the bottom up (spell lists before class cantrips).
func fill(container_path: String, top_down: bool, prefer: Dictionary = {}) -> void:
	for attempt in range(60):
		var open := groups(container_path).filter(func(g): return g["picked"] < g["owed"])
		if open.is_empty(): return
		if not top_down: open.reverse()
		var progressed := false
		for group in open:
			var title := heading_name(group)
			var choices: Array = group["boxes"].filter(func(b): return not b.disabled and not b.button_pressed)
			if prefer.has(title):
				choices.sort_custom(func(a, b): return a.name == prefer[title] and b.name != prefer[title])
			if choices.is_empty(): continue
			if await click(choices[0], container_path, true):
				progressed = true
				break
		if not progressed:
			for group in open:
				if group["boxes"].filter(func(b): return not b.disabled and not b.button_pressed).is_empty():
					var shown: Array = group["boxes"].map(func(b): return str(b.name) + ("(off)" if b.disabled else ""))
					if node("Next").disabled:
						bug("stuck: heading owes choices none of which can be clicked", container_path + " " + group["title"].text + " under it: " + str(shown))
			return

# --- What every page must show ---------------------------------------------

func inside_scroll(control: Node) -> bool:
	var parent := control.get_parent()
	while parent and parent != current_scene:
		if parent is ScrollContainer: return true
		parent = parent.get_parent()
	return false

func gather_shown(root_node: Node, into: Array) -> void:
	for child in root_node.get_children():
		if not child is Control: continue
		if not child.is_visible_in_tree() or child.modulate.a <= 0.01: continue
		if child is Button or child is Label or child is LineEdit or child is ItemList or child is RichTextLabel:
			if child.size.x > 0 and child.size.y > 0: into.append(child)
		gather_shown(child, into)

func readable(control: Control) -> bool:
	if control is Label or control is Button: return control.text.strip_edges() != ""
	return true

# Where a control's text is drawn: a label's box is often wider and taller
# than its words.
func text_rect(control: Control) -> Rect2:
	var rect := control.get_global_rect()
	if control is RichTextLabel:
		return Rect2(rect.position, Vector2(rect.size.x, minf(rect.size.y, control.get_content_height())))
	if not control is Label:
		return rect
	var label: Label = control
	var font: Font = label.get_theme_font("font")
	var font_size := label.get_theme_font_size("font_size")
	var wraps := label.autowrap_mode != TextServer.AUTOWRAP_OFF
	var lines := label.get_visible_line_count() if wraps else label.text.count("\n") + 1
	var height := lines * font.get_height(font_size) + maxi(lines - 1, 0) * label.get_theme_constant("line_spacing")
	var width := rect.size.x
	if not wraps:
		width = 0.0
		for line in label.text.split("\n"):
			width = maxf(width, font.get_string_size(line, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x)
		width = minf(width, rect.size.x)
	var x := rect.position.x
	if label.horizontal_alignment == HORIZONTAL_ALIGNMENT_CENTER: x += (rect.size.x - width) / 2
	elif label.horizontal_alignment == HORIZONTAL_ALIGNMENT_RIGHT: x += rect.size.x - width
	height = minf(height, rect.size.y)
	var y := rect.position.y
	if label.vertical_alignment == VERTICAL_ALIGNMENT_CENTER: y += (rect.size.y - height) / 2
	elif label.vertical_alignment == VERTICAL_ALIGNMENT_BOTTOM: y += rect.size.y - height
	return Rect2(x, y, width, height)

# Nothing the player reads or clicks is off the window, cut off, or drawn
# over something else.
func check_layout() -> void:
	if shots != "" and not shot.has(page()):
		shot[page()] = true
		RenderingServer.force_draw(false)
		root.get_texture().get_image().save_png(shots + "/" + page().replace(" ", "_").replace("&", "and") + ".png")
	var view := Rect2(Vector2.ZERO, Vector2(root.size)).grow(1)
	var shown := []
	gather_shown(current_scene, shown)
	shown = shown.filter(func(c): return readable(c))
	var title := page()
	for control in shown:
		var rect: Rect2 = control.get_global_rect()
		if not inside_scroll(control) and not view.encloses(rect):
			bug("drawn off the window", title + " " + control.name + " at " + str(rect))
		if control is Label and control.autowrap_mode != TextServer.AUTOWRAP_OFF and control.get_line_count() > control.get_visible_line_count():
			bug("text cut off", title + " " + control.name + " '" + control.text.substr(0, 60) + "'")
		elif control is Label and control.text_overrun_behavior != TextServer.OVERRUN_NO_TRIMMING and control.autowrap_mode == TextServer.AUTOWRAP_OFF:
			var font: Font = control.get_theme_font("font")
			var width := font.get_string_size(control.text, HORIZONTAL_ALIGNMENT_LEFT, -1, control.get_theme_font_size("font_size")).x
			if width > control.size.x + 1:
				bug("text cut off", title + " " + control.name + " '" + control.text.substr(0, 60) + "'")
		elif control is RichTextLabel and not control.scroll_active and not control.fit_content and control.get_content_height() > control.size.y + 2:
			bug("text cut off", title + " " + control.name + " '" + control.get_parsed_text().substr(0, 60) + "'")
	for i in range(shown.size()):
		var a: Control = shown[i]
		var ra := text_rect(a)
		for j in range(i + 1, shown.size()):
			var b: Control = shown[j]
			if a.is_ancestor_of(b) or b.is_ancestor_of(a): continue
			if inside_scroll(a) != inside_scroll(b): continue
			var overlap := ra.intersection(text_rect(b))
			if overlap.size.x > 2 and overlap.size.y > 2:
				bug("controls drawn over each other", title + " " + a.name + " and " + b.name)

# --- One character --------------------------------------------------------

# Rolls until the class can be met and drags the dice onto the scores; returns
# each ability's die.
func roll_for(klass: String) -> Dictionary:
	var needs: Array = ABILITY_NEEDS[klass]
	for attempt in range(4000):
		node("Roll").pressed.emit()
		var values := []
		for i in range(6): values.append(node("Dice" + str(i)).get_parsed_text().to_int())
		if values.filter(func(v): return v >= 13).size() >= needs.size(): break
	await settle()
	var order := range(6)
	var values := []
	for i in range(6): values.append(node("Dice" + str(i)).get_parsed_text().to_int())
	order.sort_custom(func(a, b): return values[a] > values[b])
	var abilities: Array = needs.duplicate()
	for a in range(6):
		if not abilities.has(a): abilities.append(a)
	var dice := {}
	for k in range(6):
		await drag(node("Dice" + str(order[k])), node("Score" + str(abilities[k])))
		dice[abilities[k]] = values[order[k]]
	return dice

func drag(from_control: Control, to_control: Control) -> void:
	var from := from_control.get_global_rect().get_center()
	var to := to_control.get_global_rect().get_center()
	var down := InputEventMouseButton.new()
	down.button_index = MOUSE_BUTTON_LEFT
	down.pressed = true
	down.position = from
	down.global_position = from
	root.push_input(down, true)
	await settle(1)
	var motion := InputEventMouseMotion.new()
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	motion.position = to
	motion.global_position = to
	motion.relative = to - from
	root.push_input(motion, true)
	await settle(1)
	var up := InputEventMouseButton.new()
	up.button_index = MOUSE_BUTTON_LEFT
	up.position = to
	up.global_position = to
	root.push_input(up, true)
	await settle(1)

# Each score is its die plus the bonus shown beside it, and a background's
# bonuses add up to three.
func check_scores(dice: Dictionary, label: String) -> void:
	var total := 0
	for i in range(6):
		var shown: String = node("BonusScore" + str(i)).text
		var change := 0
		var found := bonus_pattern.search(shown)
		if found: change = int(found.get_string(1))
		total += change
		var score := int(node("Score" + str(i)).text)
		if score != int(dice[i]) + change:
			bug("a score is not its die plus its bonus", label + " " + ABILITIES[i] + ": die " + str(dice[i]) + ", bonus '" + shown + "', score " + str(score))
	if total != 3:
		bug("background bonuses do not add up to 3", label + " adds " + str(total))

func try_scores(dice: Dictionary, klass: String, n: int) -> Dictionary:
	var bonus := node("Bonus") as OptionButton
	if n % 4 == 0:
		for i in range(bonus.item_count):
			await choose_index("Bonus", i)
			check_scores(dice, selected_text("Bonus"))
	elif n % 4 == 1:
		# Two abilities clicked one after the other trade scores.
		await press("Ability0")
		await press("Ability1")
		var swapped := dice.duplicate()
		swapped[0] = dice[1]
		swapped[1] = dice[0]
		check_scores(swapped, "after swapping Strength and Dexterity")
		await press("Ability1")
		await press("Ability0")
	elif n % 4 == 2:
		# Rolling again empties the scores until the new dice are placed.
		await press("Roll")
		for i in range(6):
			if node("Score" + str(i)).text != "":
				bug("rolling again keeps an old score", ABILITIES[i] + " '" + node("Score" + str(i)).text + "'")
		if not node("Next").disabled: bug("Next is on after rolling again", "")
		dice = await roll_for(klass)
	await choose_index("Bonus", n % bonus.item_count)
	check_scores(dice, selected_text("Bonus"))
	return dice

func page() -> String:
	return node("PageTitle").text

func next_page(expected_from: String) -> bool:
	if node("Next").disabled:
		var open := []
		for path in ["SpeciesChoices", "Training", "SpellChoices"]:
			for g in groups(path):
				if g["picked"] < g["owed"]: open.append(g["title"].text)
		bug("Next stays off", expected_from + " open headings " + str(open) + " status '" + status_text() + "'")
		return false
	var before := page()
	left[before] = page_snapshot()
	chosen[before] = ticked()
	record.append(page_snapshot(false))
	check_layout()
	await press("Next")
	if page() == before:
		bug("Next does nothing", before + " status '" + status_text() + "'")
		return false
	return true

# A page as the player left it, less what follows other pages: the steps
# follow the class and background, and a refusal's message goes once the
# player moves on.
func lasting(lines: Array) -> Array:
	return lines.filter(func(line: String): return not line.begins_with("Steps:") and not line.begins_with("Status:"))

# Back to the first page and forward again, twice: the first walk leaves
# each page's final choices for the sheet check (a later page can change an
# earlier one: a background taking a species skill, or the feat Versatile
# offers), and the second must find every page as the first left it.
func walk_through() -> void:
	var title := page()
	for compare in [false, true]:
		if not await back_to(RACE_PAGE): return
		while page() != title:
			var here := page()
			if compare and left.has(here):
				var was := lasting(left[here])
				var now := lasting(page_snapshot())
				if now != was: bug("walking back to the start changes a page", here + ": " + difference(was, now))
			if not await next_page("walking forward"): return

# Back then Next must keep this page and the one before as they were.
func back_keeps() -> void:
	var here := page_snapshot()
	var title := page()
	await press("Back")
	var previous := page()
	if left.has(previous):
		var was := lasting(left[previous])
		var now := lasting(page_snapshot())
		if now != was: bug("Back shows the page before changed", previous + ": " + difference(was, now))
	await press("Next")
	if page() != title:
		bug("Back then Next lands elsewhere", title + " -> " + page())
		return
	if page_snapshot() != here:
		bug("Back loses choices", title + ": " + difference(here, page_snapshot()))

func try_appearance(n: int) -> void:
	# A Small character starts Short, a Goliath Large, anyone else Regular.
	var race_page: Array = left.get(RACE_PAGE, [])
	var small := race_page.has("Race: Gnome") or race_page.has("Race: Halfling") or race_page.has("  small 'Small' [x]")
	var expected := "Size: Short" if small else "Size: Large" if race_page.has("Race: Goliath") else "Size: Regular"
	if selected_text("Size") != expected:
		bug("the combat icon starts at the wrong size", selected_text("Size") + " instead of " + expected)
	var head: String = node("CombatHeadLabel").text
	await press("CombatHeadNext")
	if node("CombatHeadLabel").text == head: bug("the next head is the same head", head)
	var body: String = node("BodyLabel").text
	await press("BodyNext")
	if node("BodyLabel").text == body and not body.ends_with("/ 1"): bug("the next body is the same body", body)
	await choose_index("Size", (n + 1) % 3)
	for part in range(6):
		if await press("Color1_" + str((part + n) % 6)): break
	await press("Palette" + str(n % 16))
	if status_text() != "": bug("changing the combat icon shows an error", status_text())

func try_portrait(n: int) -> void:
	var sheet := node("PortraitScroll/PortraitSheet")
	trail.append("portrait#" + str(n % 3))
	sheet.emit_signal("portrait_chosen", n % 3)
	await settle()
	if status_text() != "": bug("choosing a portrait shows an error", status_text())

func make_character(race: String, species_picks: Dictionary, background: String, klass: String, n: int) -> void:
	context = "#" + str(characters) + " " + race + " " + str(species_picks) + " " + background + " " + klass + (" top-down" if policy == 0 else " bottom-up")
	characters += 1
	trail = []
	record = []
	left = {}
	chosen = {}
	if page() != RACE_PAGE:
		bug("new character does not start on the first page", page())
	var name: LineEdit = node("Name")
	name.text = "Explorer " + str(n)
	name.text_changed.emit(name.text)
	await settle()
	if not await choose("Choices", race):
		bug("race not choosable", race)
		return
	var genders := node("Gender") as OptionButton
	await choose_index("Gender", n % genders.item_count)
	for title in species_picks:
		var box := find_box("SpeciesChoices", title, species_picks[title])
		if box == null: bug("species option missing", title + "/" + species_picks[title])
		elif not box.button_pressed: await click(box, "SpeciesChoices", true)
	if n % 7 == 0: await try_every_choice("SpeciesChoices")
	await fill("SpeciesChoices", true)
	if not await next_page("race"): return
	var alignments := list_items("Choices")
	await choose_index("Choices", n % alignments.size())
	await back_keeps()
	if not await next_page("alignment"): return
	if not await choose("Background", background):
		bug("background not choosable", background)
		return
	var dice := await roll_for(klass)
	dice = await try_scores(dice, klass, n)
	await back_keeps()
	if not await next_page("attributes"): return
	if not await choose("Choices", klass):
		bug("class not choosable after rolling for it", klass + " scores " + str(range(6).map(func(i): return node("Score" + str(i)).text)))
		return
	await back_keeps()
	if not await next_page("class"): return
	if page() == "Training":
		await try_every_choice("Training")
		await fill("Training", true)
		await back_keeps()
		if not await next_page("training"): return
	if page() == "Spell Choices":
		await try_every_choice("SpellChoices")
		await fill("SpellChoices", policy == 0)
		await back_keeps()
		if not await next_page("spell choices"): return
	if detours: await detour(background, klass, n)
	if not page().begins_with("Combat"): return
	await try_appearance(n)
	await back_keeps()
	if not await next_page("combat icon"): return
	await try_portrait(n)
	await back_keeps()
	await walk_through()
	if page() != "Portrait": return
	if not await next_page("portrait"): return
	check_sheet(klass)

# The sheet names the character and shows every choice the pages hold.
func check_sheet(klass: String) -> void:
	var sheet: String = node("Description").get_parsed_text()
	if not sheet.contains("Supported training choices complete"):
		bug("sheet says training is not complete", klass)
		if show_trail: print("    sheet: ", sheet.replace("\n", " / ").substr(0, 3000))
	if not sheet.contains(node("Name").text if node("Name").text != "" else "Explorer"):
		bug("sheet leaves out the name", "")
	var internal := RegEx.new()
	internal.compile("\\b[a-z_]+:[a-z_]+")
	for found in internal.search_all(sheet):
		bug("sheet shows an internal name", found.get_string())
	var steps: String = node("Steps").text
	var seen := {}
	for title in chosen:
		if not steps.contains(title): continue
		for choice in chosen[title]:
			var text: String = choice["text"]
			if not sheet.contains(text):
				bug("sheet leaves out a choice", title + " " + choice["heading"] + "/" + text)
				if show_trail: print("    sheet: ", sheet.replace("\n", " / ").substr(0, 3000))
			var key: String = choice["name"]
			# Expertise sharpens a skill the character already has.
			# A Wizard prepares spells from its spellbook.
			if ABILITIES.has(key) or choice["heading"].contains("Expertise") or choice["heading"].begins_with("Prepared"): continue
			if seen.has(key) and seen[key] != choice["heading"]:
				bug("the same choice made twice", key + " under " + seen[key] + " and " + choice["heading"])
			seen[key] = choice["heading"]
	check_layout()

# Back to the attributes page for another background, to the class page for
# another class and then the first one again, or to the race page for another
# species option; finishing each time.
func back_to(title: String) -> bool:
	for i in range(8):
		if page() == title: return true
		if not await press("Back"): break
	if page() != title: bug("Back cannot reach a page", title + " from " + page())
	return page() == title

func forward_filling() -> bool:
	for i in range(8):
		var title := page()
		if title == RACE_PAGE: await fill("SpeciesChoices", true)
		elif title == "Training": await fill("Training", true)
		elif title == "Spell Choices": await fill("SpellChoices", policy == 0)
		elif title.begins_with("Combat"): return true
		if not await next_page(title + " after a change"): return false
	return true

# Changes a species choice to one the class training took if it can (a
# Human's Skillful on a class skill), otherwise to its next option; every
# other time, the species itself.
func change_species(n: int) -> String:
	if n % 2 == 0:
		var race := selected_text("Choices")
		for k in range(1, races.size()):
			var other: String = races[(races.find(race) + k) % races.size()]
			if await choose("Choices", other): return other
	var taken: Array = chosen.get("Training", []).map(func(c): return c["name"])
	for group in groups("SpeciesChoices"):
		var title := heading_name(group)
		for box in group["boxes"]:
			if box.button_pressed or box.button_group != null or not taken.has(str(box.name)): continue
			if group["picked"] >= group["owed"]:
				var first: Array = group["boxes"].filter(func(b): return b.button_pressed)
				await click(first[0], "SpeciesChoices", false)
			var again := find_box("SpeciesChoices", title, box.name)
			if again and not again.disabled and await click(again, "SpeciesChoices", true): return title + "/" + str(box.name)
	for group in groups("SpeciesChoices"):
		var radios: Array = group["boxes"].filter(func(b): return b.button_group != null and not b.disabled and not b.button_pressed)
		if not radios.is_empty() and await click(radios[0], "SpeciesChoices", true):
			return heading_name(group) + "/" + str(radios[0].name)
	return ""

func detour(background: String, klass: String, n: int) -> void:
	if n % 3 == 0:
		var backgrounds := ["Acolyte", "Criminal", "Sage", "Soldier"]
		var other: String = backgrounds[(backgrounds.find(background) + 1 + n / 3 % 3) % 4]
		if not await back_to("Attributes"): return
		await choose("Background", other)
		context += " then " + other
		if not await next_page("attributes after a background change"): return
		await forward_filling()
	elif n % 3 == 1:
		if not await back_to("Class"): return
		# Another class the same scores qualify for, whatever the dice, so a
		# fresh screen takes the same detour; failing that, any that qualifies.
		var needs: Array = ABILITY_NEEDS[klass]
		var classes := list_items("Choices").filter(func(c): return c != klass)
		var sure := classes.filter(func(c): return ABILITY_NEEDS[c].all(func(a): return needs.has(a)))
		var other := ""
		for c in sure + classes:
			if await choose("Choices", c):
				other = c
				break
		if other != "" and not sure.has(other):
			record.append(["(a detour the dice chose)"])
		if other == "":
			# No other class qualifies: carry on with this one.
			await forward_filling()
			return
		context += " then " + other + " then " + klass
		if not await next_page("class " + other): return
		if not await forward_filling(): return
		if not await back_to("Class"): return
		if not await choose("Choices", klass):
			bug("first class no longer choosable", klass)
			return
		if not await next_page("class " + klass + " again"): return
		await forward_filling()
	else:
		if not await back_to(RACE_PAGE): return
		var changed := await change_species(n / 3)
		context += " then species " + changed
		await forward_filling()
	if not (page().begins_with("Combat")):
		bug("after a change the pages do not reach the combat icon", page())

func finish_character() -> void:
	# Like a player: add to the party and create the next one, or start over.
	var started := false
	if characters % 3 == 0 and await press("AddParty"):
		var back: Button = node("Back")
		var joined: bool = not node("AddParty").is_visible_in_tree()
		if joined and back.is_visible_in_tree() and not back.disabled:
			bug("Back stays on after Add to party", "edits after it would not reach the party")
		started = await press("PartyPanel/ActionRow1/Create")
	if not started and not await press("Restart"):
		bug("cannot start the next character", page())
		await fresh_screen()
		return
	var now := page_snapshot()
	if now != first_page:
		bug("a new character does not start like one on a fresh screen", difference(first_page, now))

func fresh_screen() -> void:
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle(6)

# --- The plan -----------------------------------------------------------

# The species' single choices, each option of each, as dictionaries of
# heading -> option node name, read from the race page.
func species_variants(race: String) -> Array:
	await choose("Choices", race)
	var variants := [{}]
	for group in groups("SpeciesChoices"):
		var title := heading_name(group)
		if group["owed"] != 1 or title.contains("skill") or title.contains("Skill") or title.contains("Keen"): continue
		var names: Array = group["boxes"].map(func(b): return str(b.name))
		var grown := []
		for v in variants:
			for option in names:
				var copy: Dictionary = v.duplicate()
				copy[title] = option
				grown.append(copy)
		# Two option groups multiply; keep each option once beside the others' first.
		if grown.size() > 40:
			grown = []
			for option in names:
				var copy: Dictionary = variants[0].duplicate()
				copy[title] = option
				grown.append(copy)
			grown.append_array(variants.slice(1))
		variants = grown
	return variants

# The character made again on a fresh screen must see the same pages.
func make_again_fresh(race: String, variant: Dictionary, background: String, klass: String, n: int) -> void:
	var reused := record.duplicate()
	var reused_context := context
	await fresh_screen()
	await make_character(race, variant, background, klass, n)
	if record != reused:
		for i in range(mini(record.size(), reused.size()) + 1):
			var a: Array = reused[i] if i < reused.size() else []
			var b: Array = record[i] if i < record.size() else []
			if a.size() == 1 or b.size() == 1: break
			if a != b:
				bug("a reused screen shows a page unlike a fresh one", (b[0] if not b.is_empty() else "?") + ": reused " + difference(a, b).replace("' -> '", "' fresh '"))
				if show_trail: print("    reused: ", " / ".join(a), "\n    fresh:  ", " / ".join(b))
				break
	context = reused_context
	await finish_character()

# --- Replaying a game log -------------------------------------------------

# The class a log picks after line `from`, for rolling scores it qualifies with.
func class_picked_after(lines: PackedStringArray, from: int) -> String:
	for i in range(from, lines.size()):
		for klass in ABILITY_NEEDS:
			if lines[i].contains('ui: selected Choices "' + klass + '"'): return klass
	return "Fighter"

func replay() -> void:
	var lines := FileAccess.get_file_as_string(replay_log).split("\n")
	var action := RegEx.new()
	action.compile('ui: (pressed|selected) (\\S+)(?: "((?:[^"\\\\]|\\\\.)*)")?(?: \\((on|off)\\))?')
	var replayed := 0
	for i in range(replay_from - 1, mini(replay_to, lines.size())):
		var found := action.search(lines[i])
		if found == null: continue
		var verb := found.get_string(1)
		var path := found.get_string(2)
		var text := found.get_string(3)
		var state := found.get_string(4)
		context = "log line " + str(i + 1) + ": " + verb + " " + path + " '" + text + "'"
		var target := node(path)
		if target == null or not (target is CanvasItem and target.is_visible_in_tree()):
			continue
		replayed += 1
		var before := status_text()
		# The roster lists the names the player typed, which the log lacks.
		if path.begins_with("PartyPanel/Roster"):
			continue
		if verb == "selected":
			if not await choose(path, text): bug("a choice the log made is not offered", path + " '" + text + "'")
		elif target is CheckBox:
			if target.disabled: continue
			trail.append(("+" if state == "on" else "-") + path)
			target.set_pressed(state == "on")
			await settle()
			var again: CheckBox = node(path)
			if again and again.is_visible_in_tree() and again.button_pressed != (state == "on"):
				bug("a click does not stick", path + " '" + text + "' should be " + state)
		elif target is OptionButton:
			continue
		elif target is Button:
			# The log has neither the typed name nor where the dice went.
			if path == "Next" and page() == RACE_PAGE and node("Name").text == "":
				node("Name").text = "Replay"
				node("Name").text_changed.emit("Replay")
				await settle()
			if path == "Next" and page() == "Attributes" and node("Next").disabled:
				await roll_for(class_picked_after(lines, i))
			if not await press(path): continue
		var after := status_text()
		if after != "" and after != before: bug("a replayed click shows an error", after)
	print("Replayed ", replayed, " clicks of ", replay_log)

func run() -> void:
	await fresh_screen()
	root.size = window
	await settle(4)
	if replay_log != "":
		await replay()
		for key in bugs:
			print("  ", bugs[key].size(), "x ", key, "\n      e.g. ", bugs[key][0])
		if bugs.is_empty(): print("Creation replay passed")
		quit(0 if bugs.is_empty() else 1)
		return
	first_page = page_snapshot()
	races = list_items("Choices")
	# The Background list fills on the attributes page.
	var backgrounds := ["Acolyte", "Criminal", "Sage", "Soldier"]
	var n := 0
	for race in races:
		if only_race != "" and race != only_race: continue
		var variants: Array = await species_variants(race)
		for variant in variants:
			for b in range(backgrounds.size()):
				var classes: Array = ABILITY_NEEDS.keys()
				if shuffle_seed:
					seed(shuffle_seed + n)
					classes.shuffle()
				for klass in classes:
					if only_class != "" and klass != only_class: continue
					# Each species option meets every class; backgrounds all
					# meet every class with the species' first options.
					if variant != variants[0] and b != (n % backgrounds.size()): continue
					policy = n % 2 if forced_policy < 0 else forced_policy
					if n % sample == 0 and (limit == 0 or characters < limit):
						await make_character(race, variant, backgrounds[b], klass, n)
						await finish_character()
						if fresh_every > 0 and (n / sample) % fresh_every == fresh_every - 1:
							await make_again_fresh(race, variant, backgrounds[b], klass, n)
					n += 1
	print("Explored ", characters, " characters; ", bugs.size(), " distinct problems")
	for key in bugs:
		print("  ", bugs[key].size(), "x ", key, "\n      e.g. ", bugs[key][0])
	if bugs.is_empty():
		print("Creation explorer passed: ", characters, " characters made on one screen")
	quit(0 if bugs.is_empty() else 1)
