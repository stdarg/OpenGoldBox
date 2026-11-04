extends SceneTree

# Drives the real character-creation screen through every species, species
# option, background and class, one screen making character after character
# the way a player does. On every page it checks what the player sees: each
# heading "(picked / owed)" has enough clickable choices drawn under it, every
# offered choice can be clicked, Next turns on once everything is chosen, and
# Back keeps the choices. Prints one "BUG:" line per problem and fails if
# there is any.
#
# The suite runs a sample (opengold_godot_creation_explorer). Every
# combination, both fill orders and the detours, one Godot per species:
#   godot --headless --path src/OpenGoldBox/godot --script tests/creation_explorer_tests.gd \
#       -- --race=Elf [--policy=1 --detours --shuffle=17] [--trail]
# --class=Cleric limits it to one class; --sample=3 makes every third
# character; --trail prints the clicks that led to each problem.

const ABILITY_NEEDS := {
	"Barbarian": [0], "Bard": [5], "Cleric": [4], "Druid": [4], "Fighter": [0],
	"Monk": [1, 4], "Paladin": [0, 5], "Ranger": [1, 4], "Rogue": [1],
	"Sorcerer": [5], "Warlock": [5], "Wizard": [3]}
const HEADING := "\\((\\d+) / (\\d+)\\)$"

var heading_pattern := RegEx.new()
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

func _initialize() -> void:
	heading_pattern.compile(HEADING)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--race="): only_race = arg.trim_prefix("--race=")
		if arg.begins_with("--class="): only_class = arg.trim_prefix("--class=")
		if arg == "--trail": show_trail = true
		if arg.begins_with("--policy="): forced_policy = int(arg.trim_prefix("--policy="))
		if arg == "--detours": detours = true
		if arg.begins_with("--shuffle="): shuffle_seed = int(arg.trim_prefix("--shuffle="))
		if arg.begins_with("--sample="): sample = int(arg.trim_prefix("--sample="))
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
			list.select(i)
			list.item_selected.emit(i)
			await settle()
			return true
	return false

func choose_index(path: String, index: int) -> void:
	var list = node(path)
	list.select(index)
	list.item_selected.emit(index)
	await settle()

# --- Choice groups as the player sees them -------------------------------

func collect(root: Node, into: Array) -> void:
	for child in root.get_children():
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

# Clicks each offered choice on and off again, checking the heading counts it.
func try_every_choice(container_path: String) -> void:
	for group in groups(container_path):
		var title := heading_name(group)
		for listed in group["boxes"]:
			var name: String = listed.name
			# Earlier clicks can redraw the page: read the box and count afresh.
			var box := find_box(container_path, title, name)
			if box == null or box.disabled or box.button_pressed or box.button_group != null: continue
			var picked := -1
			for g in groups(container_path):
				if heading_name(g) == title: picked = g["picked"]
			if not await click(box, container_path, true): continue
			var now := groups(container_path)
			var after := -1
			for g in now:
				if heading_name(g) == title: after = g["picked"]
			if after != picked + 1:
				bug("click does not count", container_path + " " + title + "/" + name + " (" + str(picked) + " -> " + str(after) + ")")
			var again := find_box(container_path, title, name)
			if again and again.button_pressed:
				await click(again, container_path, false)
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
		# Radio choices: pick each one in turn, then back to the first picked.
		var radios: Array = group["boxes"].filter(func(b): return b.button_group != null and not b.disabled)
		if radios.size() > 1:
			var original := ""
			for b in radios:
				if b.button_pressed: original = b.name
			for b in radios:
				var again := find_box(container_path, title, b.name)
				if again and not again.button_pressed: await click(again, container_path, true)
			if original != "":
				var back := find_box(container_path, title, original)
				if back and not back.button_pressed: await click(back, container_path, true)

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

# --- One character --------------------------------------------------------

func roll_for(klass: String) -> void:
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
	for k in range(6):
		await drag(node("Dice" + str(order[k])), node("Score" + str(abilities[k])))

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
	await press("Next")
	if page() == before:
		bug("Next does nothing", before + " status '" + status_text() + "'")
		return false
	return true

# Back then Next must keep every choice on the page.
func back_keeps(container_path: String) -> void:
	var before := pressed_set(container_path)
	var title := page()
	await press("Back")
	await press("Next")
	if page() != title:
		bug("Back then Next lands elsewhere", title + " -> " + page())
		return
	var after := pressed_set(container_path)
	if after != before:
		bug("Back loses choices", title + ": " + str(before) + " -> " + str(after))

func make_character(race: String, species_picks: Dictionary, background: String, klass: String, n: int) -> void:
	context = "#" + str(characters) + " " + race + " " + str(species_picks) + " " + background + " " + klass + (" top-down" if policy == 0 else " bottom-up")
	characters += 1
	trail = []
	if page() != "Name, Race & Gender" and not page().begins_with("Name"):
		bug("new character does not start on the first page", page())
	var name: LineEdit = node("Name")
	name.text = "Explorer " + str(characters)
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
	if not await next_page("alignment"): return
	if not await choose("Background", background):
		bug("background not choosable", background)
		return
	await roll_for(klass)
	var bonus := node("Bonus") as OptionButton
	await choose_index("Bonus", n % bonus.item_count)
	if not await next_page("attributes"): return
	if not await choose("Choices", klass):
		bug("class not choosable after rolling for it", klass + " scores " + str(range(6).map(func(i): return node("TotalScore" + str(i)).text)))
		return
	if not await next_page("class"): return
	if page() == "Training":
		await try_every_choice("Training")
		await fill("Training", true)
		await back_keeps("Training")
		if not await next_page("training"): return
	if page() == "Spell Choices":
		await try_every_choice("SpellChoices")
		await fill("SpellChoices", policy == 0)
		await back_keeps("SpellChoices")
		if not await next_page("spell choices"): return
	if detours: await detour(background, klass, n)
	for step in ["combat icon", "portrait"]:
		if not await next_page(step): return
	var sheet: String = node("Description").get_parsed_text()
	if not sheet.contains("Supported training choices complete"):
		bug("sheet says training is not complete", klass)
		if show_trail: print("    sheet: ", sheet.replace("\n", " / ").substr(0, 3000))

# Back to the attributes page for another background, or to the class page for
# another class and then the first one again, finishing each time.
func back_to(title: String) -> bool:
	for i in range(8):
		if page() == title: return true
		if not await press("Back"): break
	if page() != title: bug("Back cannot reach a page", title + " from " + page())
	return page() == title

func forward_filling() -> bool:
	for i in range(4):
		var title := page()
		if title == "Training": await fill("Training", true)
		elif title == "Spell Choices": await fill("SpellChoices", policy == 0)
		elif title == "Combat icon" or title.begins_with("Combat"): return true
		if not await next_page(title + " after a change"): return false
	return true

func detour(background: String, klass: String, n: int) -> void:
	var here := page()
	if n % 2 == 0:
		var backgrounds := ["Acolyte", "Criminal", "Sage", "Soldier"]
		var other: String = backgrounds[(backgrounds.find(background) + 1 + n / 2 % 3) % 4]
		if not await back_to("Attributes"): return
		await choose("Background", other)
		context += " then " + other
		if not await next_page("attributes after a background change"): return
		if page() == "Class" and not await next_page("class after a background change"): return
		await forward_filling()
	else:
		if not await back_to("Class"): return
		var classes := list_items("Choices")
		var other := ""
		for c in classes:
			if c != klass and await choose("Choices", c):
				other = c
				break
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
	if not (page().begins_with("Combat")):
		bug("after a change the pages do not reach the combat icon", page())

func finish_character() -> void:
	# Like a player: add to the party and create the next one, or start over.
	if characters % 3 == 0 and await press("AddParty"):
		if await press("PartyPanel/ActionRow1/Create"): return
	if not await press("Restart"):
		bug("cannot start the next character", page())
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

func run() -> void:
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle(6)
	root.size = Vector2i(1120, 800)
	await settle(4)
	var races := list_items("Choices")
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
					if n % sample == 0:
						await make_character(race, variant, backgrounds[b], klass, n)
						await finish_character()
					n += 1
	print("Explored ", characters, " characters; ", bugs.size(), " distinct problems")
	for key in bugs:
		print("  ", bugs[key].size(), "x ", key, "\n      e.g. ", bugs[key][0])
	if bugs.is_empty():
		print("Creation explorer passed: ", characters, " characters made on one screen")
	quit(0 if bugs.is_empty() else 1)
