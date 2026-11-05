extends SceneTree

# Every event of the Slums through the real town screen. Each save in
# --fixtures (opengold_expedition_tests, OPENGOLD_SLUMS_EVENT_FIXTURES) is a
# level-4 party one step from an event's door, facing it:
# "slums-<event>-<x>-<y>.ogs". The check loads it from Load game, steps in and
# takes every branch of the event's choices, loading the save again for each:
# a choice list's every entry, a typed answer's right word and a wrong one, a
# number's 0 and 100. A branch ends back in the street, in a fight on the
# combat screen, or in a shop. It fails on a notice that the event is not
# supported, on a dead end (nothing to press), on a won fight's coins not
# named on the town screen, on a treasure whose items cannot be taken or left,
# or when an attempt runs long;
# each branch's ending is printed with what it cost or gave the party.
#   godot --headless --path src/OpenGoldBox/godot --script tests/slums_events_tests.gd \
#       -- --fixtures=DIR [--only=slums-3-] [--sample=5]
# A branch that ends back in the street also Looks there and steps out and in
# again. --sample=5 explores every fifth save.

const WORDS := ["OHLO", "XYZZY"]
const NUMBERS := ["0", "100"]
const BRANCHES := 40

var fixtures := ""
var only := ""
var sample := 1
# --finish-fights plays the first fight of each event to its end, the party on
# Quick at eight times speed, and goes on with what follows it.
var finish_fights := false
var finished := {}
var bugs := {}
var context := ""
var hp_pattern := RegEx.new()
var gold_pattern := RegEx.new()
var endings := 0

func _initialize() -> void:
	hp_pattern.compile("HP (-?\\d+)/(\\d+)")
	gold_pattern.compile("party (\\d+) gp")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--fixtures="): fixtures = arg.trim_prefix("--fixtures=")
		if arg.begins_with("--only="): only = arg.trim_prefix("--only=")
		if arg.begins_with("--sample="): sample = int(arg.trim_prefix("--sample="))
		if arg == "--finish-fights": finish_fights = true
	call_deferred("run")

func bug(kind: String, detail: String) -> void:
	var key := kind + " | " + detail
	if not bugs.has(key):
		bugs[key] = []
		print("BUG: ", key, " | ", context)
	bugs[key].append(context)

func settle(frames := 4) -> void:
	for frame in range(frames):
		await process_frame

func node(path: String) -> Node:
	return current_scene.get_node_or_null(path)

func town() -> Node:
	return node("CampaignTown")

func press(path: String) -> bool:
	var button: Button = node(path)
	if button == null or not button.is_visible_in_tree() or button.disabled:
		return false
	button.pressed.emit()
	await settle()
	return true

func dialogue() -> String:
	return town().get_node("Dialogue").get_parsed_text().strip_edges()

func choices() -> Array:
	var list: ItemList = town().get_node("Choices")
	var items := []
	if not list.is_visible_in_tree(): return items
	for i in range(list.item_count): items.append(list.get_item_text(i))
	return items

func pick(index: int) -> void:
	var list: ItemList = town().get_node("Choices")
	list.select(index)
	list.item_selected.emit(index)
	await press("CampaignTown/Continue")

func exploring() -> bool:
	return not town().get_node("Forward").disabled

func fighting() -> bool:
	var combat := node("CampaignCombat")
	return combat != null and combat.is_visible_in_tree()

func shopping() -> bool:
	return town().get_node("LeaveShop").is_visible_in_tree() and not at_treasure()

func at_treasure() -> bool:
	return town().get_node("LeaveShop").is_visible_in_tree() and town().get_node("Speaker").text.begins_with("Treasure")

# A treasure lists what the fight or the script left: the selected member takes
# the first item and the second member the next, each with Enter, and both must
# then carry them; the party leaves the rest behind, Leave asking first.
func take_treasure() -> String:
	var list: ItemList = town().get_node("Choices")
	var count := list.item_count
	var taken := []
	for slot in range(mini(2, count)):
		var row: Button = town().get_node("PartyList/Rows/Member" + str(slot))
		row.pressed.emit()
		await settle()
		list = town().get_node("Choices")
		var item := list.get_item_text(0)
		var full: bool = town().get_node("Speaker").text.ends_with(" 16/16 items")
		var before := list.item_count
		list.select(0)
		await press("CampaignTown/Continue")
		var left := (town().get_node("Choices") as ItemList).item_count
		# A member carrying 16 items is refused, and the item stays.
		if full:
			if not dialogue().contains("can carry no more") or left != before:
				bug("a member with 16 items is not refused", item + ": " + dialogue().right(160))
			continue
		taken.append(item)
		if not dialogue().contains(" takes " + item + ".") or left != before - 1:
			bug("a treasure item is not taken", item + ": " + dialogue().right(160))
	await press("CampaignTown/LeaveShop")
	if (town().get_node("Choices") as ItemList).item_count > 0 or count > taken.size():
		if not at_treasure() or not dialogue().begins_with("There is still treasure here"):
			bug("leaving a treasure with items in it does not ask first", dialogue().substr(0, 160))
		await press("CampaignTown/LeaveShop")
	await settle(8)
	if at_treasure():
		bug("the party cannot leave a treasure", dialogue().substr(0, 160))
	return "took " + ", ".join(taken) + " of " + str(count)

# What the town screen shows besides the text, for a dead end's report.
func screen_state() -> String:
	return "[speaker '%s', %s, progress '%s', continue %s, combat %s]" % [
		town().get_node("Speaker").text, town().get_node("Movement").text,
		town().get_node("Progress").text,
		"off" if town().get_node("Continue").disabled else "on", node("CampaignCombat") != null]

func typing() -> bool:
	return town().get_node("Answer").is_visible_in_tree()

func party_gold() -> int:
	var found := gold_pattern.search(town().get_node("Party").text)
	return int(found.get_string(1)) if found else -1

func party_hp() -> int:
	var total := 0
	for slot in range(8):
		var row: Button = town().get_node_or_null("PartyList/Rows/Member" + str(slot))
		if row == null or not row.visible: continue
		var found := hp_pattern.search(row.text)
		if found: total += int(found.get_string(1))
	return total

func party_names() -> Array:
	var names := []
	for slot in range(8):
		var row: Button = town().get_node_or_null("PartyList/Rows/Member" + str(slot))
		if row and row.visible: names.append(row.text.split("\n")[0].trim_prefix("★ ").strip_edges())
	return names

func slot_path(slot: String) -> String:
	return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

func load_fixture(file: String) -> bool:
	var slot := file.get_basename()
	var path := slot_path(slot)
	if not FileAccess.file_exists(path):
		DirAccess.make_dir_recursive_absolute(path.get_base_dir())
		var out := FileAccess.open(path, FileAccess.WRITE)
		out.store_buffer(FileAccess.get_file_as_bytes(fixtures.path_join(file)))
		out.close()
	change_scene_to_file("res://scenes/character_creation.tscn")
	await settle(8)
	await press("Party")
	await press("PartyPanel/Load")
	var list: ItemList = node("SaveSlots/Slots")
	for i in range(list.item_count):
		if list.get_item_text(i) == slot:
			list.select(i)
			list.item_selected.emit(i)
	await press("SaveSlots/Action")
	await press("SaveSlots/Action")
	if not await press("PartyPanel/ActionRow2/Explore") or town() == null:
		bug("a save does not load into the Slums", slot)
		return false
	await settle(8)
	return true

# Plays one branch: steps in, then answers each decision with the next entry
# of `path` (a choice's index or a typed word). Returns the ending, or the
# next decision's options when `path` runs out there.
func play(path: Array) -> Dictionary:
	var gold := party_gold()
	var hp := party_hp()
	var heard := []
	var taken := []
	var step := 0
	var still := 0
	var waited_since := -1
	var phase := 0
	await press("CampaignTown/Forward")
	for move in range(200):
		# A wait is timed from when it began: a step that does not wait (a
		# fight played, a treasure, a look) ends the last one.
		if not (town().get_node("Continue").disabled and choices().is_empty()) or fighting() or at_treasure() or exploring():
			waited_since = -1
		var text := dialogue()
		if text != "" and (heard.is_empty() or heard.back() != text): heard.append(text)
		if text.contains("not supported yet"):
			bug("an event is not supported", text.substr(0, 160))
		if at_treasure():
			taken.append("[treasure]")
			heard.append("[" + await take_treasure() + "]")
			continue
		var ending := ""
		if fighting(): ending = "fight"
		elif shopping(): ending = "shop"
		elif exploring() and move > 0: ending = "street"
		# Back in the street on the square: then a Look, as a searching player
		# does, then out and in again, where a repeat must not pay twice.
		if ending == "street" and phase < 2:
			phase += 1
			if phase == 1:
				taken.append("[look]")
				heard.append("[look]")
				await press("CampaignTown/Look")
			else:
				taken.append("[again]")
				heard.append("[again]")
				for turn in ["Right", "Right", "Forward", "Right", "Right", "Forward"]:
					if exploring(): await press("CampaignTown/" + turn)
			continue
		var event: String = context.split("-")[1] if context.count("-") >= 1 else context
		if ending == "fight" and finish_fights and not finished.has(event):
			finished[event] = true
			taken.append("[fight]")
			var gold_before_fight := party_gold()
			var outcome := await finish_fight()
			heard.append("[" + outcome + "]")
			# The town names what the won fight gave the party.
			if outcome == "won" and party_gold() > gold_before_fight and not dialogue().contains("The party takes "):
				bug("a won fight's loot is not named", dialogue().substr(0, 160))
			if outcome == "endless":
				bug("a fight does not end", str(taken))
				return {"ending": "endless fight", "heard": heard, "taken": taken, "gold": party_gold() - gold, "hp": party_hp() - hp}
			if outcome == "defeat":
				return {"ending": "defeat", "heard": heard, "taken": taken, "gold": party_gold() - gold, "hp": party_hp() - hp}
			continue
		if ending != "":
			return {"ending": ending, "heard": heard, "taken": taken, "gold": party_gold() - gold, "hp": party_hp() - hp}
		var options := []
		if typing():
			options = NUMBERS if town().get_node("Answer").max_length <= 6 else WORDS
		else:
			var list := choices()
			# A member is chosen as the first: the event is the same for each.
			if list.size() >= 2 and not party_names().has(list[0]): options = list
		if options.size() >= 2:
			if step >= path.size():
				return {"ending": "decision", "options": options, "heard": heard, "taken": taken}
			var answer = path[step]
			step += 1
			# The replay met another decision than the first run did (a street
			# group's dice): this branch is left there.
			if (typing() and not answer is String) or (not typing() and (answer is String or answer >= options.size())):
				return {"ending": "diverged", "heard": heard, "taken": taken, "gold": party_gold() - gold, "hp": party_hp() - hp}
			if typing():
				taken.append("'" + str(answer) + "'")
				town().get_node("Answer").text = str(answer)
				await press("CampaignTown/Continue")
			else:
				taken.append(options[answer])
				await pick(answer)
			continue
		var before := text + str(choices())
		# Continue is off while the monster close-up plays (the fight then
		# opens by itself) and while the script pauses (DELAY): wait, up to
		# ten seconds of nothing to press.
		var waiting: bool = town().get_node("Continue").disabled and choices().is_empty()
		if waiting:
			if waited_since < 0: waited_since = Time.get_ticks_msec()
			await settle(20)
		elif not choices().is_empty(): await pick(0)
		else: await press("CampaignTown/Continue")
		if not waiting: waited_since = -1
		still = still + 1 if dialogue() + str(choices()) == before and not waiting else 0
		if still >= 6 or (waiting and Time.get_ticks_msec() - waited_since > 10000):
			bug("a dead end: nothing moves the event on", "'" + text.substr(0, 120) + "' " + str(choices()) + " " + screen_state())
			return {"ending": "stuck", "heard": heard, "taken": taken, "gold": party_gold() - gold, "hp": party_hp() - hp}
	bug("an event runs on without end", str(taken))
	return {"ending": "endless", "heard": heard, "taken": taken, "gold": 0, "hp": 0}

# Plays the fight on the combat screen with every member on Quick until the
# town takes the party back: "won" or "defeat", or "endless" after two
# minutes of play.
func finish_fight() -> String:
	var combat: Control = node("CampaignCombat")
	Engine.time_scale = 8.0
	var deadline := Time.get_ticks_msec() + 120000
	var outcome := "endless"
	while Time.get_ticks_msec() < deadline:
		await settle(2)
		if not fighting():
			outcome = "won"
			break
		if combat.get_node("StatusStack/Turn").text.contains("defeat"):
			outcome = "defeat"
			break
		var quick := InputEventKey.new()
		quick.keycode = KEY_Q
		quick.pressed = true
		root.push_input(quick)
		for name in ["Decline", "Continue"]:
			var button: Button = combat.get_node_or_null(name)
			if button and button.is_visible_in_tree() and not button.disabled:
				button.pressed.emit()
				break
	Engine.time_scale = 1.0
	await settle(8)
	return outcome

func explore(file: String) -> void:
	var queue := [[]]
	var branches := 0
	while not queue.is_empty() and branches < BRANCHES:
		var path: Array = queue.pop_front()
		if not await load_fixture(file): return
		branches += 1
		var result := await play(path)
		if result["ending"] == "decision":
			var options: Array = result["options"]
			for i in range(options.size()):
				queue.append(path + [options[i] if typing() else i])
			continue
		endings += 1
		var said: Array = result["heard"]
		print("  ", context, " ", " > ".join(result["taken"]), " => ", result["ending"],
			"  gold ", result["gold"], " hp ", result["hp"], "  | ",
			" / ".join(said).substr(0, 600))
	if not queue.is_empty():
		print("  ", context, ": ", queue.size(), " branches left after ", BRANCHES)

func run() -> void:
	root.gui_embed_subwindows = true
	var files := Array(DirAccess.get_files_at(fixtures)).filter(func(f): return f.ends_with(".ogs"))
	files.sort()
	for n in range(files.size()):
		var file: String = files[n]
		if only != "" and not file.begins_with(only): continue
		if n % sample != 0: continue
		context = file.get_basename()
		await explore(file)
	print("Explored ", endings, " endings; ", bugs.size(), " distinct problems")
	for key in bugs:
		print("  ", bugs[key].size(), "x ", key, "\n      e.g. ", bugs[key][0])
	if bugs.is_empty() and endings > 0:
		print("Slums events passed: ", endings, " endings")
	quit(0 if bugs.is_empty() and endings > 0 else 1)
