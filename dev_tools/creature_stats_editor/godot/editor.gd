extends Control
# The Creature Stats Editor (dev_tools/docs/creature-stats-editor.md). Every
# value it shows comes from the game's own code through CreatureEditor; edits
# wait in the Pending tab, saved in edits/edits.json, until Publish imports them
# into data/ and copies data/ into the game project.

const DIFFICULTIES := ["easy", "normal", "hard", "nightmare", "infernal"]
const ABILITIES := ["Str", "Dex", "Con", "Int", "Wis", "Cha"]

var editor := CreatureEditor.new()
var repository := OS.get_environment("OPENGOLD_REPOSITORY")
var game_directory := OS.get_environment("OPENGOLD_GAME_DIR")
var edits_path := repository.path_join("dev_tools/creature_stats_editor/edits/edits.json")

var edits: Array = [] # pending, in the edits file's format
var creatures: Array = []
var stat_lines: Array = []
var encounters: Array = []
var chances: Array = []
var companions: Array = []

var status: Label
var pending_button: Button
var tabs: TabContainer
var mono := SystemFont.new()
var last_reason := ""

# Creatures tab
var creature_search: LineEdit
var creature_list: ItemList
var creature_view: VBoxContainer
var creature: Dictionary = {}

# Fights tab
var fight_area: OptionButton
var fight_list: ItemList
var fight_view: VBoxContainer
var party_levels: LineEdit
var challenge: SpinBox

# Chances tab
var chance_area: OptionButton
var chance_kind: OptionButton
var chance_list: ItemList
var chance_view: VBoxContainer

# Companions tab
var companion_list: ItemList
var companion_view: VBoxContainer

# Pending tab
var pending_list: ItemList
var report_view: TextEdit


func _ready() -> void:
	mono.font_names = PackedStringArray(["Menlo", "Monaco", "Courier New"])
	var root := VBoxContainer.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 6)
	add_child(root)
	var bar := HBoxContainer.new()
	root.add_child(bar)
	status = Label.new()
	status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	bar.add_child(status)
	pending_button = _button("Pending edits: 0", func(): tabs.current_tab = 4)
	bar.add_child(pending_button)
	bar.add_child(_button("Start the game", _start_game))
	tabs = TabContainer.new()
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(tabs)
	_build_creatures_tab()
	_build_fights_tab()
	_build_chances_tab()
	_build_companions_tab()
	_build_pending_tab()
	_load_edits()
	_open()


# --- Loading -------------------------------------------------------------------

func _open() -> void:
	if repository.is_empty() or game_directory.is_empty():
		_say("Start the editor with run.sh: it sets OPENGOLD_REPOSITORY and OPENGOLD_GAME_DIR.")
		return
	var error := editor.open(repository, game_directory)
	if not error.is_empty():
		_say("The game would refuse its data: " + error)
		return
	creatures = _query(editor.creatures(), "creatures")
	stat_lines = _query(editor.stat_lines(), "stat_lines")
	encounters = _query(editor.encounters(), "encounters")
	chances = _query(editor.chances(), "chances")
	companions = _query(editor.companions(), "companions")
	_fill_area_filter(fight_area, encounters)
	_fill_area_filter(chance_area, chances)
	_show_creature_list()
	_show_fight_list()
	_show_chance_list()
	_show_companion_list()
	_say("Read %s and %s." % [repository.path_join("data"), game_directory])


# The list `key` of a JSON answer, or [] after saying what went wrong.
func _query(answer: String, key: String) -> Array:
	var parsed = JSON.parse_string(answer)
	if typeof(parsed) != TYPE_DICTIONARY:
		_say("Unreadable answer from the game's code.")
		return []
	if parsed.has("error"):
		_say(parsed["error"])
		return []
	return parsed.get(key, [])


func _answer(answer: String) -> Dictionary:
	var parsed = JSON.parse_string(answer)
	return parsed if typeof(parsed) == TYPE_DICTIONARY else {"ok": false, "error": "Unreadable answer"}


func _fill_area_filter(filter: OptionButton, entries: Array) -> void:
	filter.clear()
	filter.add_item("Every area")
	var seen := {}
	for entry in entries:
		if not seen.has(entry["area"]):
			seen[entry["area"]] = true
			filter.add_item(entry["area"])


# --- Small helpers ---------------------------------------------------------------

func _say(text: String) -> void:
	status.text = text


func _button(text: String, pressed: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.pressed.connect(pressed)
	return button


func _label(text: String, bold := false) -> Label:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	if bold:
		label.add_theme_font_size_override("font_size", 18)
	return label


func _spin(low: float, high: float, value: float) -> SpinBox:
	var spin := SpinBox.new()
	spin.min_value = low
	spin.max_value = high
	spin.value = value
	spin.custom_minimum_size.x = 110
	return spin


func _reason_field() -> LineEdit:
	var reason := LineEdit.new()
	reason.placeholder_text = "Why (one line, written to docs/DEVIATIONS.md)"
	# Kept across the screens a change redraws, so a second edit has it too.
	reason.text = last_reason
	reason.text_changed.connect(func(text): last_reason = text)
	reason.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return reason


func _row(children: Array) -> HBoxContainer:
	var row := HBoxContainer.new()
	for child in children:
		_unwrapped(child)
		row.add_child(child)
	return row


# A label in a row or a grid cell keeps its text on one line: wrapping it
# there would shrink it to a letter a line.
func _unwrapped(child: Control) -> Control:
	if child is Label:
		child.autowrap_mode = TextServer.AUTOWRAP_OFF
	return child


func _clear(container: Node) -> void:
	for child in container.get_children():
		container.remove_child(child)
		child.queue_free()


func _split(tab_name: String, left: Control, right: Control) -> void:
	var split := HSplitContainer.new()
	split.name = tab_name
	split.split_offset = 430
	tabs.add_child(split)
	left.custom_minimum_size.x = 380
	split.add_child(left)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.add_child(scroll)
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(right)


func _list_column(filters: Array, list: ItemList) -> VBoxContainer:
	var column := VBoxContainer.new()
	for filter in filters:
		column.add_child(filter)
	list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(list)
	return column


func _attack_text(attack) -> String:
	if attack == null:
		return "-"
	var damage := "%dd%d" % [int(attack["dice"]), int(attack["sides"])]
	var bonus := int(attack["damage_bonus"])
	if bonus != 0:
		damage += "%+d" % bonus
	var text := "%+d, %s %s" % [int(attack["to_hit"]), damage, attack["damage_type"]]
	if int(attack["range"]) > 0:
		text += ", %d/%d ft" % [int(attack["range"]), int(attack["long_range"])]
	return text


func _stat_line(definition: String) -> Dictionary:
	for line in stat_lines:
		if line["definition"] == definition:
			return line
	# A stat line a pending edit copies keeps its original's values until published.
	for edit in edits:
		if edit["change"] == "new_stat_line" and edit["definition"] == definition:
			return _stat_line(edit["copy_of"])
	return {}


# --- Pending edits -----------------------------------------------------------------

# The pending edit of the same thing, whose `from` the new one keeps.
func _same_subject(a: Dictionary, b: Dictionary) -> bool:
	if a["change"] != b["change"]:
		return false
	for key in ["definition", "script", "record", "site", "operand", "bank"]:
		if a.get(key) != b.get(key):
			return false
	return true


# The entry as the pending edits leave it: the `to` of the pending edit of the
# same thing, or the entry the data file holds. A second change builds on it.
func _pending_entry(subject: Dictionary, entry: Dictionary) -> Dictionary:
	for edit in edits:
		if _same_subject(edit, subject) and edit["to"] != null:
			return edit["to"].duplicate(true)
	return entry.duplicate(true)


func _add_edit(edit: Dictionary) -> void:
	if str(edit.get("reason", "")).strip_edges().is_empty():
		_say("Give a reason: it goes in docs/DEVIATIONS.md.")
		return
	for n in range(edits.size()):
		if _same_subject(edits[n], edit):
			if edit.has("from"):
				edit["from"] = edits[n]["from"]
			edits[n] = edit
			_edits_changed()
			_say("Replaced the pending edit of the same value.")
			return
	edits.append(edit)
	_edits_changed()
	_say("Added. Preview and publish it in the Pending tab.")


func _edits_text() -> String:
	return JSON.stringify({"format": "opengold-creature-edits-v1", "edits": edits}, "  ")


func _load_edits() -> void:
	if not FileAccess.file_exists(edits_path):
		return
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(edits_path))
	if typeof(parsed) == TYPE_DICTIONARY and parsed.get("edits") is Array:
		edits = parsed["edits"]
	_edits_changed(false)


func _edits_changed(save := true) -> void:
	if save:
		DirAccess.make_dir_recursive_absolute(edits_path.get_base_dir())
		var file := FileAccess.open(edits_path, FileAccess.WRITE)
		if file:
			file.store_string(_edits_text() + "\n")
	pending_button.text = "Pending edits: %d" % edits.size()
	_show_pending()


func _date() -> String:
	return Time.get_date_string_from_system()


# --- Creatures ----------------------------------------------------------------------

func _build_creatures_tab() -> void:
	creature_search = LineEdit.new()
	creature_search.placeholder_text = "Search name, stat line, area or record"
	creature_search.text_changed.connect(func(_text): _show_creature_list())
	creature_list = ItemList.new()
	creature_list.item_selected.connect(_select_creature)
	creature_view = VBoxContainer.new()
	_split("Creatures", _list_column([creature_search], creature_list), creature_view)


func _show_creature_list() -> void:
	creature_list.clear()
	var wanted := creature_search.text.to_lower()
	for entry in creatures:
		var line := "%s  %s  (%s, %s)" % [entry["name"], entry["key"], entry["definition"], entry["area"]]
		if wanted.is_empty() or line.to_lower().contains(wanted):
			var index := creature_list.add_item(line)
			creature_list.set_item_metadata(index, entry)


func _select_creature(index: int) -> void:
	creature = creature_list.get_item_metadata(index)
	_show_creature()


func _show_creature() -> void:
	_clear(creature_view)
	var script := int(creature["scripts"][0])
	var record := int(creature["record"])
	var details := _answer(editor.creature(script, record))
	if details.has("error"):
		_say(details["error"])
		return
	var header := HBoxContainer.new()
	var art := TextureRect.new()
	art.custom_minimum_size = Vector2(128, 128)
	art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	art.texture = editor.creature_art(script, record)
	header.add_child(art)
	var titles := VBoxContainer.new()
	titles.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	titles.add_child(_label("%s  (%s)" % [creature["name"], creature["key"]], true))
	titles.add_child(_label("%s. Fights as %s. Sizes a fight at %d XP, awards %d XP." % [
		creature["area"], creature["definition"], int(creature["fit_xp"]), int(creature["award_xp"])]))
	titles.add_child(_label("The original: %d Hit Points, AC %d (descending)." % [
		int(creature["original_hit_points"]), int(creature["original_armor_class"])]))
	var sharing := []
	for other in creature["sharing"]:
		sharing.append("%s %d" % [other["area"], int(other["record"])])
	titles.add_child(_label("Records that fight as %s: %s." % [creature["definition"], ", ".join(sharing)]))
	header.add_child(titles)
	creature_view.add_child(header)

	creature_view.add_child(_label("As a fight fields it (a party of average level 2 and up):", true))
	var grid := GridContainer.new()
	grid.columns = 7
	grid.add_theme_constant_override("h_separation", 24)
	for heading in ["Difficulty", "AC", "Hit Points", "Initiative", "Speed", "Melee", "Ranged"]:
		grid.add_child(_unwrapped(_label(heading)))
	var normal := {}
	for stats in details["stats"]:
		if stats["difficulty"] == "normal":
			normal = stats
		var melee := _attack_text(stats["melee"])
		if int(stats["attacks"]) > 1:
			melee = "%d x %s" % [int(stats["attacks"]), melee]
		for cell in [stats["difficulty"].capitalize(), str(int(stats["armor_class"])),
				str(int(stats["hit_points"])), "%+d" % int(stats["initiative"]),
				"%d ft" % int(stats["speed"]), melee, _attack_text(stats["ranged"])]:
			grid.add_child(_unwrapped(_label(cell)))
	creature_view.add_child(grid)
	var saves := []
	for n in range(6):
		saves.append("%s %+d" % [ABILITIES[n], int(normal["saves"][n])])
	creature_view.add_child(_label("Saves: " + ", ".join(saves) + "."))
	if int(normal["spell_slots"]) > 0 or not normal["spells"].is_empty():
		creature_view.add_child(_label("Spellcasting: %d slots, casting bonus %+d, level %d. Spells: %s." % [
			int(normal["spell_slots"]), int(normal["casting_bonus"]), int(normal["spell_level"]),
			", ".join(normal["spells"])]))
	creature_view.add_child(_label("Gear (from the Combat Sprite Catalog's export):", true))
	if details["gear"].is_empty():
		creature_view.add_child(_label("None."))
	var uses := {"weapon": "fights with it", "hard_and_above": "uses it on Hard and above",
		"loot": "loot only: does not change the fight"}
	for item in details["gear"]:
		creature_view.add_child(_label("%s%s: %s" % [item["name"], " (readied)" if item["readied"] else "",
			uses[item["use"]]]))
	creature_view.add_child(_label("Gear is edited in the Combat Sprite Catalog, the one gear editor."))
	creature_view.add_child(HSeparator.new())
	_creature_edits(normal)


func _creature_edits(normal: Dictionary) -> void:
	var definition: String = creature["definition"]
	var line := _stat_line(definition)
	var reason := _reason_field()
	creature_view.add_child(_label("Change (the stat line changes every record listed above):", true))
	creature_view.add_child(reason)

	var hit_points := _spin(1, 1000, line.get("hit_points", 1))
	var original := int(creature["original_hit_points"])
	creature_view.add_child(_row([_label("Stat line Hit Points"), hit_points,
		_button("The original's %d" % original, func(): hit_points.value = original),
		_button("Add edit", func():
			_add_edit({"change": "hit_points", "definition": definition,
				"from": int(line["hit_points"]), "to": int(hit_points.value), "reason": reason.text}))]))

	var fit := _spin(0, 1000000, creature["fit_xp"])
	var award := _spin(0, 1000000, creature["award_xp"])
	creature_view.add_child(_row([_label("Sizes a fight at"), fit, _label("XP; awards"), award, _label("XP"),
		_button("Add edit", func(): _conversion_edit({"fit_xp": int(fit.value), "award_xp": int(award.value)}, reason.text))]))

	var lines := OptionButton.new()
	for other in stat_lines:
		lines.add_item(other["definition"])
		if other["definition"] == definition:
			lines.select(lines.item_count - 1)
	creature_view.add_child(_row([_label("Fight as another stat line"), lines,
		_button("Add edit", func(): _conversion_edit({"definition": lines.get_item_text(lines.selected)}, reason.text))]))

	var new_key := LineEdit.new()
	new_key.placeholder_text = "new stat line key, such as tower-knight"
	new_key.custom_minimum_size.x = 260
	creature_view.add_child(_row([_label("Split: copy %s for this record alone as" % definition), new_key,
		_button("Add edits", func(): _split_stat_line(definition, new_key.text.strip_edges(), reason.text))]))

	var profiles := OptionButton.new()
	var seen := {}
	for other in stat_lines:
		var profile := "%d slots, casting %+d, level %d" % [int(other["spell_slots"]), int(other["casting_bonus"]), int(other["level"])]
		if not seen.has(profile):
			seen[profile] = true
			profiles.add_item(profile)
			profiles.set_item_metadata(profiles.item_count - 1, other)
			if int(other["spell_slots"]) == int(line.get("spell_slots", 0)) and int(other["casting_bonus"]) == int(line.get("casting_bonus", 0)) and int(other["level"]) == int(line.get("level", 0)):
				profiles.select(profiles.item_count - 1)
	creature_view.add_child(_row([_label("Spellcasting profile (one the rules already have)"), profiles,
		_button("Add edit", func():
			var chosen: Dictionary = profiles.get_item_metadata(profiles.selected)
			_add_edit({"change": "spell_profile", "definition": definition,
				"from": {"spell_slots": int(line["spell_slots"]), "casting_bonus": int(line["casting_bonus"]), "level": int(line["level"])},
				"to": {"spell_slots": int(chosen["spell_slots"]), "casting_bonus": int(chosen["casting_bonus"]), "level": int(chosen["level"])},
				"reason": reason.text}))]))


# The record's conversion entry with `changes`, from its entry now.
func _conversion_edit(changes: Dictionary, reason: String) -> void:
	var edit := {"change": "conversion", "script": int(creature["scripts"][0]), "record": int(creature["record"]),
		"from": creature["entry"], "reason": reason}
	var to := _pending_entry(edit, creature["entry"])
	for key in changes:
		to[key] = changes[key]
	edit["to"] = to
	_add_edit(edit)


func _split_stat_line(definition: String, key: String, reason: String) -> void:
	if key.is_empty() or not _stat_line(key).is_empty():
		_say("Name a stat line key no stat line has.")
		return
	if reason.strip_edges().is_empty():
		_say("Give a reason: it goes in docs/DEVIATIONS.md.")
		return
	_add_edit({"change": "new_stat_line", "definition": key, "copy_of": definition, "reason": reason})
	_conversion_edit({"definition": key}, reason)


# --- Fights -------------------------------------------------------------------------

func _build_fights_tab() -> void:
	fight_area = OptionButton.new()
	fight_area.item_selected.connect(func(_index): _show_fight_list())
	fight_list = ItemList.new()
	fight_list.item_selected.connect(func(index): _show_fight(fight_list.get_item_metadata(index)))
	party_levels = LineEdit.new()
	party_levels.text = "10, 10, 10, 10, 10, 10, 10, 6"
	party_levels.custom_minimum_size.x = 260
	challenge = _spin(1, 200, 33)
	fight_view = VBoxContainer.new()
	_split("Fights", _list_column([fight_area], fight_list), fight_view)


func _show_fight_list() -> void:
	fight_list.clear()
	var area := fight_area.get_item_text(fight_area.selected) if fight_area.selected > 0 else ""
	for site in encounters:
		if not area.is_empty() and site["area"] != area:
			continue
		var what := "rolled from the script's tables (read only)"
		if site["editable"]:
			what = "%d x %s" % [int(site["count"]), _record_name(site, int(site["record"]))]
		if site["override"] != null:
			what += "  [overridden]"
		var index := fight_list.add_item("%s  %s: %s" % [site["site"], site["area"], what])
		fight_list.set_item_metadata(index, site)


func _record_name(site: Dictionary, record: int) -> String:
	for entry in creatures:
		if int(entry["record"]) == record and int(site["script"]) in entry["scripts"].map(func(s): return int(s)):
			return "%s (%d)" % [entry["name"], record]
	return "record %d" % record


func _show_fight(site: Dictionary) -> void:
	_clear(fight_view)
	fight_view.add_child(_label("%s, %s" % [site["site"], site["area"]], true))
	if not site["editable"]:
		fight_view.add_child(_label("The script reads this fight's record or count from its variables, as Podol Plaza's roaming groups do; it cannot be overridden in this version."))
		return
	fight_view.add_child(_label("The script loads %d x %s, icon %s." % [int(site["count"]),
		_record_name(site, int(site["record"])), str(int(site["icon"])) if site["icon"] != null else "from a variable"]))
	var change = site["override"]
	if change != null:
		fight_view.add_child(_label("Overridden: %s x %s. Reason: %s" % [str(change.get("count", site["count"])),
			_record_name(site, int(change.get("record", site["record"]))), change["reason"]]))
	fight_view.add_child(_label("What the game makes of the fight:", true))
	var result := VBoxContainer.new()
	fight_view.add_child(_row([_label("Party levels"), party_levels, _label("Encounter challenge"), challenge,
		_button("Size", func(): _size_fight(site, result))]))
	fight_view.add_child(result)
	_size_fight(site, result)
	fight_view.add_child(HSeparator.new())

	fight_view.add_child(_label("Override what this instruction loads:", true))
	var reason := _reason_field()
	fight_view.add_child(reason)
	var records := OptionButton.new()
	for entry in creatures:
		if int(site["script"]) in entry["scripts"].map(func(s): return int(s)):
			records.add_item("%s (%d)" % [entry["name"], int(entry["record"])])
			records.set_item_metadata(records.item_count - 1, int(entry["record"]))
			var current := int(change.get("record", site["record"])) if change != null else int(site["record"])
			if int(entry["record"]) == current:
				records.select(records.item_count - 1)
	var count := _spin(1, 250, change.get("count", site["count"]) if change != null else site["count"])
	var icon := _spin(0, 255, change.get("icon", 0) if change != null else 0)
	fight_view.add_child(_row([_label("Record"), records, _label("Most met"), count, _label("Icon (0 keeps it)"), icon]))
	var buttons := [_button("Add edit", func():
		var to := {"site": site["site"], "original": {"record": int(site["record"]), "count": int(site["count"])}}
		var record: int = records.get_selected_metadata()
		if record != int(site["record"]):
			to["record"] = record
		if int(count.value) != int(site["count"]):
			to["count"] = int(count.value)
		if int(icon.value) > 0:
			to["icon"] = int(icon.value)
		if to.size() == 2:
			_say("That is the original's fight; use Remove the override instead.")
			return
		to["reason"] = reason.text
		_add_edit({"change": "override", "site": site["site"], "from": change, "to": to, "reason": reason.text}))]
	if change != null:
		buttons.append(_button("Remove the override", func():
			_add_edit({"change": "override", "site": site["site"], "from": change, "to": null, "reason": reason.text})))
	fight_view.add_child(_row(buttons))
	fight_view.add_child(_label("The count is still sized to the party: it is the most the party can meet."))


func _size_fight(site: Dictionary, result: VBoxContainer) -> void:
	_clear(result)
	var levels := PackedInt32Array()
	for part in party_levels.text.split(",", false):
		if part.strip_edges().is_valid_int():
			levels.append(clampi(int(part), 1, 15))
	var sized := _answer(editor.fight(site["site"], levels, int(challenge.value)))
	if sized.has("error"):
		result.add_child(_label(sized["error"]))
		return
	var groups := []
	for group in sized["groups"]:
		groups.append("%d x %s at %d XP (%s)" % [int(group["loaded"]), group["name"], int(group["fit_xp"]), group["site"]])
	result.add_child(_label("Loaded: " + "; ".join(groups) + "."))
	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 24)
	for heading in ["Difficulty", "XP budget", "Met, by group"]:
		grid.add_child(_unwrapped(_label(heading)))
	for row in sized["sized"]:
		grid.add_child(_unwrapped(_label(row["difficulty"].capitalize())))
		grid.add_child(_unwrapped(_label(str(int(row["budget"])))))
		grid.add_child(_unwrapped(_label(", ".join(row["met"].map(func(n): return str(int(n)))))))
	result.add_child(grid)


# --- Chances -------------------------------------------------------------------------

func _build_chances_tab() -> void:
	chance_area = OptionButton.new()
	chance_area.item_selected.connect(func(_index): _show_chance_list())
	chance_kind = OptionButton.new()
	for kind in ["Every kind", "random", "compare", "rest_interval", "rest_chance"]:
		chance_kind.add_item(kind)
	chance_kind.item_selected.connect(func(_index): _show_chance_list())
	chance_list = ItemList.new()
	chance_list.item_selected.connect(func(index): _show_chance(chance_list.get_item_metadata(index)))
	chance_view = VBoxContainer.new()
	_split("Chances", _list_column([chance_area, chance_kind], chance_list), chance_view)


func _chance_text(chance: Dictionary) -> String:
	var number := int(chance["original"])
	match chance["kind"]:
		"random":
			return "rolls 0 to %d into variable %d" % [number, int(chance["variable"])]
		"compare":
			return "tests rolled variable %d against %d" % [int(chance["variable"]), number]
		"rest_interval":
			return "rest check every %d five-minute steps" % number
		_:
			return "rest interrupted on %d%%" % number


func _show_chance_list() -> void:
	chance_list.clear()
	var area := chance_area.get_item_text(chance_area.selected) if chance_area.selected > 0 else ""
	var kind := chance_kind.get_item_text(chance_kind.selected) if chance_kind.selected > 0 else ""
	for chance in chances:
		if (not area.is_empty() and chance["area"] != area) or (not kind.is_empty() and chance["kind"] != kind):
			continue
		var text := "%s  %s: %s" % [chance["site"], chance["area"], _chance_text(chance)]
		if chance["entry"] != null:
			text += "  [now %d]" % int(chance["entry"]["value"])
		var index := chance_list.add_item(text)
		chance_list.set_item_metadata(index, chance)


func _show_chance(chance: Dictionary) -> void:
	_clear(chance_view)
	chance_view.add_child(_label("%s, operand %d, %s" % [chance["site"], int(chance["operand"]), chance["area"]], true))
	chance_view.add_child(_label("The script " + _chance_text(chance) + "."))
	chance_view.add_child(_label("A RANDOM's variable often feeds more than the encounter, such as Podol Plaza's rumours: read the script (opengold_scripts --inspect) before changing a number."))
	var entry = chance["entry"]
	if entry != null:
		chance_view.add_child(_label("Changed to %d. Reason: %s" % [int(entry["value"]), entry["reason"]]))
	var reason := _reason_field()
	chance_view.add_child(reason)
	var value := _spin(0, 255, entry["value"] if entry != null else chance["original"])
	var buttons := [_button("Add edit", func():
		if int(value.value) == int(chance["original"]):
			_say("That is the original's number; use Restore the original instead.")
			return
		_add_edit({"change": "chance", "site": chance["site"], "operand": int(chance["operand"]), "from": entry,
			"to": {"site": chance["site"], "operand": int(chance["operand"]), "original": int(chance["original"]),
				"value": int(value.value), "reason": reason.text}, "reason": reason.text}))]
	if entry != null:
		buttons.append(_button("Restore the original", func():
			_add_edit({"change": "chance", "site": chance["site"], "operand": int(chance["operand"]),
				"from": entry, "to": null, "reason": reason.text})))
	chance_view.add_child(_row([_label("New number"), value] + buttons))


# --- Companions -------------------------------------------------------------------------

func _build_companions_tab() -> void:
	companion_list = ItemList.new()
	companion_list.item_selected.connect(func(index): _show_companion(companion_list.get_item_metadata(index)))
	companion_view = VBoxContainer.new()
	_split("Companions", _list_column([], companion_list), companion_view)


func _show_companion_list() -> void:
	companion_list.clear()
	for companion in companions:
		var index := companion_list.add_item("%s  (level %d %s, MON%dCHA.DAX:%d)" % [companion["name"],
			int(companion["level"]), companion["class"], int(companion["entry"]["bank"]), int(companion["entry"]["record"])])
		companion_list.set_item_metadata(index, companion)


func _show_companion(companion: Dictionary) -> void:
	_clear(companion_view)
	var entry: Dictionary = companion["entry"]
	companion_view.add_child(_label("%s, level %d %s" % [companion["name"], int(companion["level"]), companion["class"]], true))
	companion_view.add_child(_label("Built as the game builds a joining companion: %d Hit Points." % int(companion["hit_points"])))
	var scores := []
	var saves := []
	for n in range(6):
		scores.append("%s %d" % [ABILITIES[n], int(companion["scores"][n])])
		saves.append("%s %+d" % [ABILITIES[n], int(companion["saves"][n])])
	companion_view.add_child(_label("Scores: " + ", ".join(scores) + "."))
	companion_view.add_child(_label("Saves: " + ", ".join(saves) + "."))
	companion_view.add_child(_label("Features: " + (", ".join(companion["features"]) if not companion["features"].is_empty() else "none") + "."))
	companion_view.add_child(_label("Spells: " + (", ".join(companion["spells"]) if not companion["spells"].is_empty() else "none") + "."))
	companion_view.add_child(_label("Items: " + (", ".join(companion["items"]) if not companion["items"].is_empty() else "none") + "."))
	companion_view.add_child(HSeparator.new())
	companion_view.add_child(_label("Change (a companion already in a save keeps the character):", true))
	var reason := _reason_field()
	companion_view.add_child(reason)
	var subject := {"change": "companion", "bank": int(entry["bank"]), "record": int(entry["record"])}
	var change_companion := func(change: Callable):
		var to := _pending_entry(subject, entry)
		change.call(to)
		var edit := subject.duplicate()
		edit.merge({"from": entry, "to": to, "reason": reason.text})
		_add_edit(edit)
		_show_companion(companion)
	var pending := _pending_entry(subject, entry)
	var level := _spin(1, 15, pending["level"])
	companion_view.add_child(_row([_label("Level"), level, _button("Add edit", func():
		change_companion.call(func(to): to["level"] = int(level.value)))]))
	companion_view.add_child(_label("Items, with pending edits (added after advancing):"))
	var items: Array = pending.get("items", [])
	var item_list := ItemList.new()
	item_list.custom_minimum_size.y = 120
	for item in items:
		item_list.add_item("%s (%s) x%d" % [item["name"], item["key"], int(item.get("quantity", 1))])
	companion_view.add_child(item_list)
	var key := LineEdit.new()
	key.placeholder_text = "item key, such as mace"
	key.custom_minimum_size.x = 220
	var item_name := LineEdit.new()
	item_name.placeholder_text = "name, such as Mace"
	item_name.custom_minimum_size.x = 220
	companion_view.add_child(_row([key, item_name,
		_button("Add item", func():
			if key.text.strip_edges().is_empty() or item_name.text.strip_edges().is_empty():
				_say("An item needs its key and name.")
				return
			change_companion.call(func(to):
				var listed: Array = to.get("items", [])
				listed.append({"key": key.text.strip_edges(), "name": item_name.text.strip_edges()})
				to["items"] = listed)),
		_button("Remove the chosen item", func():
			var chosen := item_list.get_selected_items()
			if not chosen.is_empty():
				change_companion.call(func(to): to["items"].remove_at(chosen[0])))]))
	companion_view.add_child(_label("Preview builds the companion as the game will and shows whether the game accepts it."))


# --- Pending, preview and publish ----------------------------------------------------------

func _build_pending_tab() -> void:
	var page := VBoxContainer.new()
	page.name = "Pending"
	tabs.add_child(page)
	page.add_child(_label("Edits apply in order. Preview shows every file Publish would write; nothing is written until you publish."))
	pending_list = ItemList.new()
	pending_list.custom_minimum_size.y = 180
	page.add_child(pending_list)
	page.add_child(_row([
		_button("Remove the chosen edit", func():
			var chosen := pending_list.get_selected_items()
			if not chosen.is_empty():
				edits.remove_at(chosen[0])
				_edits_changed()),
		_button("Preview", _preview),
		_button("Publish", _confirm_publish)]))
	report_view = TextEdit.new()
	report_view.editable = false
	report_view.size_flags_vertical = Control.SIZE_EXPAND_FILL
	report_view.add_theme_font_override("font", mono)
	page.add_child(report_view)


func _show_pending() -> void:
	if pending_list == null:
		return
	pending_list.clear()
	for edit in edits:
		var subject := str(edit.get("definition", edit.get("site", "")))
		if edit.has("record"):
			subject += " record %d" % int(edit["record"])
		pending_list.add_item("%s  %s: %s" % [edit["change"], subject, edit["reason"]])


func _report_text(report: Dictionary) -> String:
	if not report.get("ok", false):
		return "Refused, nothing would be written:\n" + str(report.get("error", ""))
	var lines := []
	for edit in report["edits"]:
		lines.append("%s %s: %s (%s)" % [edit["change"], edit["subject"], edit["summary"], edit["status"]])
	if report["rules_version"] != null:
		lines.append("The rules version becomes %s: saves made before can no longer be loaded." % report["rules_version"])
	for edit in edits:
		if edit["change"] == "conversion" and edit["to"] != null and edit["from"] != null and edit["to"].get("award_xp") != edit["from"].get("award_xp"):
			lines.append("A changed award changes the party's experience, which DEVIATIONS says stays the original encounter's.")
	for file in report["files_changed"]:
		lines.append("\n=== " + file["path"])
		lines.append(file["diff"])
	return "\n".join(lines)


func _preview() -> void:
	if edits.is_empty():
		report_view.text = "No pending edits."
		return
	report_view.text = _report_text(_answer(editor.check(_edits_text(), _date())))


func _confirm_publish() -> void:
	if edits.is_empty():
		_say("No pending edits.")
		return
	var report := _answer(editor.check(_edits_text(), _date()))
	report_view.text = _report_text(report)
	if not report.get("ok", false):
		_say("The game would refuse these edits; see the report.")
		return
	var dialog := ConfirmationDialog.new()
	dialog.dialog_text = "Write %d files to the repository (shown below) and copy data/ into the game project?" % report["files_changed"].size()
	dialog.confirmed.connect(_publish)
	add_child(dialog)
	dialog.popup_centered()


func _publish() -> void:
	var report := _answer(editor.publish(_edits_text(), _date()))
	report_view.text = _report_text(report)
	if not report.get("ok", false):
		_say("Nothing was written: " + str(report.get("error", "")))
		return
	edits.clear()
	_edits_changed()
	_open()
	var copied := _copy_into_game()
	_say("Published. " + copied + " Commit the changed files with the full test suite passing; after a combat.rules change rebuild the AI lab's scenario packs.")


# The game project reads its own copy of data/; the build's opengoldbox_game_data
# target makes it, the same copy every build makes.
func _copy_into_game() -> String:
	var cmake := OS.get_environment("OPENGOLD_CMAKE")
	var build := OS.get_environment("OPENGOLD_EDITOR_BUILD")
	if cmake.is_empty() or build.is_empty():
		return "Run opengoldbox_game_data to copy data/ into the game project."
	var output := []
	var code := OS.execute(cmake, ["--build", build, "--target", "opengoldbox_game_data"], output, true)
	return "Copied data/ into the game project." if code == 0 else "Copying data/ into the game project failed: " + "".join(output)


func _start_game() -> void:
	var godot := OS.get_executable_path()
	var project := repository.path_join("src/OpenGoldBox/godot")
	if not FileAccess.file_exists(project.path_join("project.godot")):
		_say("No game project at " + project)
		return
	OS.create_process(godot, ["--path", project])
	_say("Started the game from %s. A game already running keeps the data it read." % project)
