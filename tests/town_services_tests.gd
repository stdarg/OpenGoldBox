extends SceneTree

# New Phlan's shops, temples and inns through the real town screen. Each save
# in --fixtures (written by opengold_expedition_tests,
# OPENGOLD_TOWN_SERVICE_FIXTURES) is a party one step from a place's door,
# facing it: "<kind>-<x>-<y>[-<party>].ogs". The check loads it from Load
# game, steps in as a player does and uses every service:
# - a shop must list its data/shops record (names and prices) in alphabetical
#   order down its columns, put the party back outside its door facing away on
#   Leave, sell every item at its price to the buyer chosen, and refuse a party
#   that cannot pay;
#   in the first shop, Sell and Drop must each give up an item after asking;
# - a temple must list its record's services and prices, offer each to the
#   members it helps and no one else, take the price once paid, and cure;
# - an inn must charge its platinum once a night, and its first night must
#   restore the party.
# Exploring and in a shop the party's cards stand two to a row, in four rows
# with room for six characters and two NPCs, each with its name, race and
# class, then Armor Class and hit points; a wounded party's low hit points show
# in yellow (under a fifth) or red (under a tenth). A shop's filter lists its
# armor, weapons, other goods or all of them.
# Prints one "BUG:" line per problem and fails if there is any.
#   godot --headless --path src/OpenGoldBox/godot --script tests/town_services_tests.gd \
#       -- --fixtures=DIR [--only=temple-11-3] [--trail]

var fixtures := ""
var only := ""
var show_trail := false
var bugs := {}
var context := ""
var trail := []
var records := []
var hp_pattern := RegEx.new()
var gold_pattern := RegEx.new()
var buyer_pattern := RegEx.new()
# Purchases auto-equip put on (EQUIP-1, on by default): the shop says so.
# What the purse cannot pay for is listed in yellow.
var auto_equipped := 0
# Sell and Drop (SELL-1) are checked once, in the first shop a rich party buys at.
var sold_and_dropped := false
var offer_pattern := RegEx.new()
var place_pattern := RegEx.new()
# Hovering over a weapon tells its roll to hit, and over armor its Armor Class.
var weapon_hovers := 0
var armor_hovers := 0
# Rich parties buy their first item with a double click.
var double_click_buys := 0
var sale_line_pattern := RegEx.new()

func _initialize() -> void:
	hp_pattern.compile("HP (-?\\d+)/(\\d+)")
	gold_pattern.compile("party (\\d+) gp")
	offer_pattern.compile("The shop offers (\\d+) gp for ")
	place_pattern.compile("Party (\\(\\d+, \\d+\\))\\s+(North|East|South|West)")
	sale_line_pattern.compile(" / sells for (\\d+) gp$")
	buyer_pattern.compile("buying for (.+): party (\\d+) gp, carrying (\\d+)/(\\d+) lb")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--fixtures="): fixtures = arg.trim_prefix("--fixtures=")
		if arg.begins_with("--only="): only = arg.trim_prefix("--only=")
		if arg == "--trail": show_trail = true
	call_deferred("run")

func bug(kind: String, detail: String) -> void:
	var key := kind + " | " + detail
	if not bugs.has(key):
		bugs[key] = []
		print("BUG: ", key, " | ", context)
		if show_trail: print("    trail: ", " > ".join(trail.slice(-25)))
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
	trail.append("[" + path.get_file() + "]")
	button.pressed.emit()
	await settle()
	return true

# In a shop or at a treasure pile the news follows the header's divider.
func dialogue() -> String:
	return town().get_node("%Dialogue").get_parsed_text().strip_edges().trim_prefix("|").strip_edges()

func choices() -> Array:
	var list: ItemList = town().get_node("%Choices")
	var items := []
	if not list.is_visible_in_tree(): return items
	for i in range(list.item_count): items.append(list.get_item_text(i))
	return items

# The list's items as the player reads them on the screen: down the first
# column, then down the next.
func read_down_the_columns() -> Array:
	var list: ItemList = town().get_node("%Choices")
	var order := range(list.item_count)
	order.sort_custom(func(a: int, b: int) -> bool:
		var at_a := list.get_item_rect(a).position
		var at_b := list.get_item_rect(b).position
		return at_a.x < at_b.x or (at_a.x == at_b.x and at_a.y < at_b.y))
	return order.map(func(i: int) -> String: return list.get_item_text(i))

# Picks the choice that starts with `text` and continues; false if none does.
func choose(text: String) -> bool:
	var list: ItemList = town().get_node("%Choices")
	for i in range(list.item_count):
		if list.get_item_text(i).begins_with(text):
			trail.append(list.get_item_text(i))
			list.select(i)
			list.item_selected.emit(i)
			await press("CampaignTown/%Continue")
			return true
	return false

func exploring() -> bool:
	return not town().get_node("%Forward").disabled

func shopping() -> bool:
	return town().get_node("%LeaveShop").is_visible_in_tree()

func party_gold() -> int:
	var found := gold_pattern.search(town().get_node("%Party").text)
	if found: return int(found.get_string(1))
	found = buyer_pattern.search(town().get_node("%Speaker").text)
	return int(found.get_string(2)) if found else -1

# Each member's name and hit points from the party list.
func member_hp() -> Dictionary:
	var result := {}
	for slot in range(8):
		var row: Button = town().get_node_or_null("%PartyList/Rows/Member" + str(slot))
		if row == null or not row.visible: continue
		var name := row.tooltip_text.split("\n")[0].trim_prefix("★ ").strip_edges()
		var found := hp_pattern.search(row.tooltip_text)
		if found: result[name] = [int(found.get_string(1)), int(found.get_string(2))]
	return result

func slot_path(slot: String) -> String:
	return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

# Loads a fixture as a player does: Party, Load, its slot, Explore.
func load_fixture(file: String) -> bool:
	var slot := file.get_basename()
	var path := slot_path(slot)
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
		bug("a save does not load into the town", slot)
		return false
	await settle(8)
	return true

# Steps through the door and answers until `ready` holds, choosing a greeting's
# offer (YES), and the first member when one is asked for.
func enter(ready: Callable) -> bool:
	await press("CampaignTown/%Forward")
	for prompt in range(30):
		if ready.call(): return true
		if exploring():
			return false
		var list := choices()
		if list.has("YES"): await choose("YES")
		elif not list.is_empty(): await choose(list[0])
		elif not await press("CampaignTown/%Continue"): await settle()
	return ready.call()

func record_at(kind: String, x: int, y: int) -> Dictionary:
	for record in records:
		if record["kind"] != kind: continue
		for marker in record["fixed_location"]["markers"]:
			if int(marker[0]) == x and int(marker[1]) == y and record["fixed_location"]["map"] == "New Phlan":
				return record
	return {}

func load_records() -> void:
	var folder := "res://data/shops"
	for file in DirAccess.get_files_at(folder):
		if file.ends_with(".json") and file != "locations.json":
			records.append(JSON.parse_string(FileAccess.get_file_as_string(folder.path_join(file))))

# --- Shops ------------------------------------------------------------------

# Over the portrait, the shopkeeper's name and the shop's type; the stock in
# three columns; and each item's hover: its name, a weapon's roll to hit AC 13
# and damage or armor's Armor Class for the buyer, then price and weight.
func check_shop_layout(record: Dictionary, stock: Array) -> void:
	var heading: String = town().get_node("%Location").text
	if not record.is_empty() and heading != record["shopkeeper"] + " - " + record["type"]:
		bug("a shop's heading is not its shopkeeper and type", "'" + heading + "'")
	var list: ItemList = town().get_node("%Choices")
	if list.max_columns != 3 or (stock.size() > 2 and
			list.get_item_rect(2).position.y != list.get_item_rect(0).position.y):
		bug("a shop does not list its stock in three columns", str(list.max_columns) + " columns")
	for i in range(stock.size()):
		var lines: PackedStringArray = list.get_item_tooltip(i).split("\n")
		if lines.size() < 2 or not stock[i].begins_with(lines[0]) or not lines[-1].ends_with(" lb"):
			bug("a shop item's hover is not its name, then price and weight", stock[i] + ": '" + list.get_item_tooltip(i) + "'")
			continue
		if lines[1].begins_with("To hit AC 13: roll "):
			weapon_hovers += 1
			if not Array(lines.slice(2)).any(func(line: String) -> bool: return line.begins_with("Damage: ")):
				bug("a weapon's hover has no damage", list.get_item_tooltip(i))
		elif lines[1].begins_with("Armor Class: "):
			armor_hovers += 1

# A left double click on the item at `index`, as the mouse does it.
func double_click(index: int) -> void:
	var list: ItemList = town().get_node("%Choices")
	var at := list.get_global_transform_with_canvas() * list.get_item_rect(index).get_center()
	for second in [false, true]:
		for pressed in [true, false]:
			var click := InputEventMouseButton.new()
			click.button_index = MOUSE_BUTTON_LEFT
			click.pressed = pressed
			click.double_click = second and pressed
			click.position = at
			click.global_position = at
			root.push_input(click)
			await process_frame
	await settle()

# Where the party stands: "(13, 9)" and its facing, from the town's
# coordinates line.
func party_place() -> Array:
	var found := place_pattern.search(town().get_node("%Coordinates").text)
	return [found.get_string(1), found.get_string(2)] if found else ["", ""]

const OPPOSITE := {"North": "South", "South": "North", "East": "West", "West": "East"}

func check_shop(x: int, y: int, party: String) -> void:
	var door_side := party_place()
	if not await enter(shopping):
		bug("the shop does not open", "(" + str(x) + "," + str(y) + ") dialogue '" + dialogue() + "'")
		return
	await check_party_cards("in a shop")
	var record := record_at("shop", x, y)
	var stock: Array = choices()
	if record.is_empty():
		bug("a shop door has no shop record", "(" + str(x) + "," + str(y) + ") sells the original list of " + str(stock.size()))
	else:
		var expected := []
		for offering in record["offerings"]:
			if int(offering["quantity"]) != 0: expected.append(offering)
		if stock.size() != expected.size():
			bug("the shop lists other stock than its record", record["id"] + ": " + str(stock.size()) + " listed, " + str(expected.size()) + " in the record")
		# The list is the record's stock in alphabetical order.
		var listed := []
		for line in stock:
			listed.append(line.replace(",", "").to_lower())
		for offering in expected:
			var want: String = (offering["name"] + " / " + str(int(offering["price_gp"])) + " gp").to_lower()
			var at := listed.find_custom(func(shown: String) -> bool: return shown.begins_with(want))
			if at < 0:
				bug("a shop item differs from its record", record["id"] + ": no '" + want + "'")
			else:
				listed.remove_at(at)
		var read := read_down_the_columns()
		for i in range(1, read.size()):
			var before: String = read[i - 1].split(" / ")[0]
			if before.naturalnocasecmp_to(read[i].split(" / ")[0]) > 0:
				bug("a shop does not list its stock alphabetically down its columns", record["id"] + ": '" + before + "' before '" + read[i] + "'")
	var gold := party_gold()
	check_shop_colors(stock, gold)
	check_shop_layout(record, stock)
	await check_filter(stock)
	if party == "poor":
		await buy(0, stock[0], false)
	else:
		for i in range(stock.size()):
			await buy(i, stock[i], true, i == 0)
	print(context, ": ", stock.size(), " items listed, gold ", gold, " -> ", party_gold())
	if party != "poor" and not sold_and_dropped:
		sold_and_dropped = true
		await check_sell_and_drop()
	if not await press("CampaignTown/%LeaveShop"):
		bug("Leave shop cannot be pressed", "")
	await settle()
	for prompt in range(10):
		if exploring(): break
		if not await press("CampaignTown/%Continue"): await choose("")
	if not exploring():
		bug("leaving the shop does not return to the street", "dialogue '" + dialogue() + "'")
	# Back outside the door the party came in by, facing away from it.
	var outside := party_place()
	if outside[0] != door_side[0] or outside[1] != OPPOSITE.get(door_side[1], ""):
		bug("leaving the shop does not put the party outside its door facing away", str(door_side) + " -> " + str(outside))

# What the party purse cannot pay for is listed in yellow, unless the chosen
# member cannot use it (dark red), and the news says what yellow means.
func check_shop_colors(stock: Array, gold: int) -> void:
	var list: ItemList = town().get_node("%Choices")
	var yellow := Color(1.0, 0.86, 0.3)
	var dark_red := Color(0.78, 0.22, 0.22)
	var any_yellow := false
	for i in range(stock.size()):
		var price := gp(stock[i].split(" / ")[1].trim_suffix(" gp"))
		var color := list.get_item_custom_fg_color(i)
		var shown_yellow := color.is_equal_approx(yellow)
		any_yellow = any_yellow or shown_yellow
		var wrong := not (shown_yellow or color.is_equal_approx(dark_red)) if price > gold else shown_yellow
		if wrong:
			bug("a shop item's color does not say whether the party can afford it", stock[i] + " at " + str(gold) + " gp shows " + str(color))
	if any_yellow != dialogue().contains("Yellow: the party cannot afford it."):
		bug("the shop's news does not explain yellow", "'" + dialogue() + "'")

# At the game's full size the party's cards stand two to a row, with room
# for four rows (six characters and two NPCs) in the list, each drawing three
# lines; their tooltip adds class, race, gender, weapon and armor. The check's
# own smaller window keeps one card a row.
func check_party_cards(where: String) -> void:
	var window_size := root.size
	root.size = Vector2i(1920, 1080)
	await settle()
	var party: ScrollContainer = town().get_node("%PartyList")
	var rows: GridContainer = party.get_node("Rows")
	if rows.columns != 2:
		bug("the party list is not two cards wide " + where, str(rows.columns) + " columns")
	var cards := 0
	for row in rows.get_children():
		if not (row as Control).visible: continue
		cards += 1
		var drawn: PackedStringArray = row.get("_identity")
		var lines: PackedStringArray = (row as Button).tooltip_text.split("\n")
		if drawn.size() != 2 or not drawn[1].contains(" / ") or not (row.get("_condition") as PackedStringArray).is_empty() \
				or lines.size() != 7 or not lines[4].begins_with("AC "):
			bug("a party card " + where + " is not name, race / class, AC / HP", " | ".join(drawn) + " || " + " | ".join(lines))
		var four_rows: float = (row as Control).size.y * 4 + rows.get_theme_constant("v_separation") * 3
		if four_rows > party.size.y + 1:
			bug("the party list " + where + " has no room for two NPCs", str(four_rows) + " needed, " + str(party.size.y) + " tall")
	if cards == 0:
		bug("the party list " + where + " shows no card", "")
	root.size = window_size
	await settle()

# Each filter lists only its kind: armor and shields tell an Armor Class on
# hover, weapons a roll to hit, other goods neither; All lists the whole stock.
func check_filter(stock: Array) -> void:
	var filter: OptionButton = town().get_node("%ShopFilter")
	if not filter.is_visible_in_tree() or filter.get_item_text(filter.selected) != "All":
		bug("a shop's filter is missing or not on All", str(filter.is_visible_in_tree()))
		return
	var list: ItemList = town().get_node("%Choices")
	var listed := 0
	for kind in [0, 1, 2]:
		filter.select(kind)
		filter.item_selected.emit(kind)
		await settle()
		listed += list.item_count
		for i in range(list.item_count):
			var hover := list.get_item_tooltip(i)
			var armor := hover.contains("\nArmor Class: ")
			var weapon := hover.contains("\nTo hit AC ")
			if [armor, weapon, not armor and not weapon][kind] == false:
				bug("a shop's filter lists another kind", filter.get_item_text(kind) + ": " + list.get_item_text(i))
	filter.select(3)
	filter.item_selected.emit(3)
	await settle()
	if listed != stock.size() or choices() != stock:
		bug("a shop's filters do not share out its stock", str(listed) + " of " + str(stock.size()))

# Each card colors its current hit points by how low they are.
func check_hit_point_colors() -> void:
	for slot in range(8):
		var row: Button = town().get_node_or_null("%PartyList/Rows/Member" + str(slot))
		if row == null or not row.visible: continue
		var found := hp_pattern.search(row.tooltip_text)
		var current := int(found.get_string(1))
		var maximum := int(found.get_string(2))
		var expected := &"hp_critical" if current * 10 < maximum else &"hp_low" if current * 5 < maximum else &""
		if row.get("_health_color") != expected:
			bug("a card colors its hit points wrongly", str(current) + "/" + str(maximum) + " shows '" + str(row.get("_health_color")) + "'")

func buyer() -> Array:
	var found := buyer_pattern.search(town().get_node("%Speaker").text)
	return [found.get_string(1), int(found.get_string(2)), int(found.get_string(3))] if found else ["", -1, -1]

# Buys the item at `index` for a member who can carry it, trying the next
# member while the shop says it is too heavy, or with `affordable` false
# checks the purchase is refused with nothing spent.
func buy(index: int, shown: String, affordable: bool, by_double_click := false) -> void:
	var price := gp(shown.split(" / ")[1].trim_suffix(" gp")) if shown.contains(" / ") else -1
	var before := buyer()
	trail.append("buy " + shown)
	for slot in range(9):
		var list: ItemList = town().get_node("%Choices")
		before = buyer()
		if by_double_click and slot == 0:
			trail.append("double click")
			list.select(list.item_count - 1)
			await double_click(index)
			if list.get_selected_items() != PackedInt32Array([index]):
				bug("a double click does not select only the item it buys", str(list.get_selected_items()))
			if buyer()[1] != before[1]:
				double_click_buys += 1
		else:
			list.select(index)
			list.item_selected.emit(index)
			await press("CampaignTown/%Continue")
		if not dialogue().contains("Too heavy to carry"): break
		if slot == 8:
			bug("no member can carry an item", shown)
			return
		await press("CampaignTown/%PartyList/Rows/Member" + str(slot))
	var after := buyer()
	if not affordable:
		if after[1] != before[1] or after[2] != before[2]:
			bug("a party that cannot pay still buys", shown)
		return
	if after[1] != before[1] - price:
		bug("an item costs other than its price", shown + ": gold " + str(before[1]) + " -> " + str(after[1]))
	if after[0] != before[0] or after[2] < before[2] or not dialogue().begins_with("Bought"):
		bug("a bought item does not reach the buyer", shown + ": " + str(before) + " -> " + str(after) + " '" + dialogue() + "'")
	if dialogue().contains(" equips "):
		auto_equipped += 1

func inventory_count() -> int:
	return (town().get_node("InventoryPanel/%Items") as ItemList).item_count

func inventory_status() -> String:
	return town().get_node("InventoryPanel/%Status").text

# In the inventory, Sell's first press shows the shop's offer and the second
# sells for it; Drop's first press warns and the second drops the item, unpaid.
func check_sell_and_drop() -> void:
	if not await press("CampaignTown/%Inventory"):
		bug("the inventory does not open in a shop", "")
		return
	var items: ItemList = town().get_node("InventoryPanel/%Items")
	var count := inventory_count()
	if count < 2:
		bug("the buyer has too few items to sell and drop", str(count))
		await press("CampaignTown/InventoryPanel/%Close")
		return
	# In a shop, every line already says what it sells for.
	for index in range(count):
		if not sale_line_pattern.search(items.get_item_text(index)):
			bug("an inventory line in a shop has no sale price", "'" + items.get_item_text(index) + "'")
	var listed := sale_line_pattern.search(items.get_item_text(0))
	var gold := party_gold()
	items.select(0)
	items.item_selected.emit(0)
	await press("CampaignTown/InventoryPanel/%Sell")
	var found := offer_pattern.search(inventory_status())
	if found == null or inventory_count() != count:
		bug("Sell does not make an offer first", "'" + inventory_status() + "'")
	await press("CampaignTown/InventoryPanel/%Sell")
	var offer := int(found.get_string(1)) if found else 0
	if listed and int(listed.get_string(1)) != offer:
		bug("the listed sale price is not the shop's offer", listed.get_string(0) + " vs " + str(offer))
	if inventory_count() != count - 1 or party_gold() != gold + offer or not inventory_status().contains(" sells "):
		bug("Sell does not sell for the offer", str(count) + " -> " + str(inventory_count()) + " items, gold " + str(gold) + " -> " + str(party_gold()) + " for " + str(offer) + " '" + inventory_status() + "'")
	gold = party_gold()
	count = inventory_count()
	await press("CampaignTown/InventoryPanel/%Drop")
	if inventory_count() != count or not inventory_status().contains("gone for good"):
		bug("Drop does not warn first", "'" + inventory_status() + "'")
	await press("CampaignTown/InventoryPanel/%Drop")
	if inventory_count() != count - 1 or party_gold() != gold or not inventory_status().contains(" drops "):
		bug("Drop does not drop the item", str(count) + " -> " + str(inventory_count()) + " items, gold " + str(gold) + " -> " + str(party_gold()) + " '" + inventory_status() + "'")
	print(context, ": sold for ", offer, " gp and dropped an item")
	await press("CampaignTown/InventoryPanel/%Close")

# --- Temples ----------------------------------------------------------------

# Who each fixture party's members need, by service: the order of
# write_town_service_fixtures's patients (Arden, Bryn, Cora, Darin, Elin, Fenn).
const NEEDS := {
	"sick": {"Cure Light Wounds": ["Arden"], "Cure Serious Wounds": ["Arden"],
		"Cure Critical Wounds": ["Arden"], "Cure Blindness": ["Bryn"], "Neutralize Poison": ["Cora"],
		"Cure Disease": ["Darin"], "Remove Curse": ["Elin"], "Stone to Flesh": ["Fenn"],
		"Raise Dead": [], "Resurrection": []},
	"dead": {"Cure Light Wounds": ["Darin"], "Cure Serious Wounds": ["Darin"],
		"Cure Critical Wounds": ["Darin"], "Cure Blindness": [], "Neutralize Poison": [],
		"Cure Disease": [], "Remove Curse": [], "Stone to Flesh": [],
		"Raise Dead": ["Arden", "Bryn (beyond Raise Dead)", "Cora (beyond Raise Dead)"],
		"Resurrection": ["Bryn", "Cora"]},
}

func in_heal_list() -> bool:
	return choices().has("Leave") and choices().any(func(c): return c.contains(" gp)"))

func check_temple(x: int, y: int, party: String) -> void:
	if not await enter(in_heal_list):
		bug("the temple's Heal list does not open", "(" + str(x) + "," + str(y) + ") dialogue '" + dialogue() + "'")
		return
	var record := record_at("temple", x, y)
	var listed: Array = choices().filter(func(c): return c != "Leave")
	var expected := []
	if record.is_empty():
		bug("a temple door has no temple record", "(" + str(x) + "," + str(y) + ")")
	else:
		for offering in record["offerings"]:
			if offering["offered"]: expected.append(offering["name"] + " (" + str(int(offering["price_gp"])) + " gp)")
		var plain: Array = listed.map(func(c): return c.replace(",", ""))
		if plain != expected:
			bug("the temple lists other services than its record", record["id"] + ": " + str(listed) + " for " + str(expected))
	if party == "poor":
		await choose("Cure Light Wounds (")
		var before := party_gold()
		await choose("Arden")
		if not dialogue().contains("cannot pay") or party_gold() != before:
			bug("a temple serves a party that cannot pay", "'" + dialogue() + "' gold " + str(before) + " -> " + str(party_gold()))
		await choose("Back")
	else:
		# A party paying in platinum has the sick party's patients.
		for service in listed:
			await use_service(service, party if NEEDS.has(party) else "sick")
	await choose("Leave")
	for prompt in range(10):
		if exploring(): break
		if not await press("CampaignTown/%Continue"): await choose("")
	if not exploring():
		bug("leaving the temple does not return to the street", "dialogue '" + dialogue() + "'")

func gp(text: String) -> int:
	return int(text.replace(",", "").replace(".", ""))

# The member list of `service`; empty, with the Heal list back, when no one
# needs it.
func members_for(service: String) -> Array:
	await choose(service)
	if dialogue().begins_with("No one in the party needs"): return []
	return choices().filter(func(c): return c != "Back")

func use_service(service: String, party: String) -> void:
	var name := service.left(service.rfind(" ("))
	var price := gp(service.substr(service.rfind("(") + 1).trim_suffix(" gp)"))
	var needs: Array = NEEDS[party].get(name, [])
	var wounds := name.begins_with("Cure ") and name.ends_with("Wounds")
	var offered := await members_for(service)
	var sorted_offer := offered.duplicate()
	sorted_offer.sort()
	var sorted_needs := needs.duplicate()
	sorted_needs.sort()
	# A Cure stops being needed once an earlier one made the member whole.
	if sorted_offer != sorted_needs and not (wounds and offered.is_empty()):
		bug("a temple offers a service to the wrong members", name + ": " + str(offered) + " for " + str(needs))
	if not offered.is_empty(): await choose("Back")
	for member in offered:
		if not in_heal_list():
			bug("the temple does not return to its services", name + ": '" + dialogue() + "'")
			return
		await choose(service)
		if member.ends_with("(beyond Raise Dead)"):
			await choose(member)
			if not dialogue().contains("Raise Dead cannot bring back"):
				bug("a member beyond Raise Dead is not told why", member + ": '" + dialogue() + "'")
			await choose("Back")
			continue
		var who: String = member
		var hp_before: Array = member_hp().get(who, [0, 0])
		var gold_before := party_gold()
		await choose(who)
		var asked := dialogue()
		if gp(asked.substr(asked.rfind("for ") + 4).trim_suffix(" gp.")) != price:
			bug("the temple asks another price than it lists", name + " on " + who + ": '" + asked + "'")
		# Back first: nothing is spent.
		await choose("Back")
		if party_gold() != gold_before:
			bug("going back from a temple's price spends gold", name)
		await choose(who)
		await choose("Pay")
		if not dialogue().begins_with("The clerics cast"):
			bug("paying the temple does not cast", name + " on " + who + ": '" + dialogue() + "'")
		if party_gold() != gold_before - price:
			bug("the temple takes other than its price", name + ": " + str(gold_before) + " -> " + str(party_gold()))
		var hp_after: Array = member_hp().get(who, [0, 0])
		if wounds and hp_after[0] <= hp_before[0]:
			bug("a Cure heals nothing", name + " on " + who + ": " + str(hp_before) + " -> " + str(hp_after))
		if name == "Raise Dead" and hp_after[0] != 1:
			bug("Raise Dead does not return the member with 1 Hit Point", who + ": " + str(hp_after))
		if name == "Resurrection" and hp_after[0] != hp_after[1]:
			bug("Resurrection does not return the member with all its Hit Points", who + ": " + str(hp_after))
		# The member no longer needs it (a Cure may leave wounds to heal).
		var still := await members_for(service)
		if still.has(who) and not wounds:
			bug("a member still needs a service just paid for", name + " on " + who)
		if not still.is_empty(): await choose("Back")

# --- Inns -------------------------------------------------------------------

func asks_to_stay() -> bool:
	return choices().has("YES") and choices().has("NO") and dialogue().contains("PLATINUM")

func check_inn(x: int, y: int, party: String) -> void:
	place = Vector2i(x, y)
	var gold := party_gold()
	if not await enter_inn():
		bug("the inn does not quote its price", "(" + str(x) + "," + str(y) + ") dialogue '" + dialogue() + "'")
		return
	# 9 gp is less than a platinum piece: refused, nothing taken, no rest.
	if party == "poor":
		var wounded := member_hp()
		await choose("YES")
		await answer_until_street()
		if not seen_text.contains("YOU DON'T HAVE ENOUGH PLATINUM.") or party_gold() != gold or member_hp() != wounded:
			bug("the inn serves a party that cannot pay", "gold " + str(gold) + " -> " + str(party_gold()) + " '" + seen_text.strip_edges() + "'")
		return
	await choose("NO")
	await leave_place()
	if party_gold() != gold:
		bug("declining the inn costs gold", str(gold) + " -> " + str(party_gold()))
	if not await enter_inn():
		bug("the inn cannot be entered again after declining", "at " + town().get_node("%Coordinates").text + " '" + dialogue() + "'")
		return
	await choose("YES")
	await answer_until_street()
	var rested := member_hp()
	for name in rested:
		if rested[name][0] != rested[name][1]:
			bug("a night at the inn does not restore a member", name + ": " + str(rested[name]))
	if party_gold() != gold - 10:
		bug("the inn charges other than one platinum (10 gp)", str(gold) + " -> " + str(party_gold()))
	# A second night within a day: paid for, with no Long Rest.
	var paid := party_gold()
	if not await enter_inn():
		bug("the inn cannot be entered again after a night", "at " + town().get_node("%Coordinates").text + " '" + dialogue() + "'")
		return
	await choose("YES")
	await answer_until_street()
	if party_gold() != paid - 10:
		bug("a second night at the inn charges other than one platinum", str(paid) + " -> " + str(party_gold()))
	if not trail.any(func(t): return t.contains("Long Rest")) and not seen_text.contains("another Long Rest"):
		bug("a second night does not say it gives no Long Rest", "'" + dialogue() + "'")
	print(context, ": gold ", gold, " -> ", paid, " -> ", party_gold(), ", rested ", rested)

var place := Vector2i()

func position() -> Vector2i:
	var found := RegEx.create_from_string("\\((\\d+), (\\d+)\\)").search(town().get_node("%Coordinates").text)
	return Vector2i(int(found.get_string(1)), int(found.get_string(2))) if found else Vector2i(-1, -1)

# Through the door; already inside after a night, out into the street and
# back, as the innkeeper speaks only to those who come in.
func enter_inn() -> bool:
	if position() == place:
		for turn in ["Right", "Right", "Forward", "Right", "Right"]:
			await press("CampaignTown/%" + turn)
	return await enter(asks_to_stay)

# Answers what follows (the payer, the rest's notices) until the street.
var seen_text := ""

func answer_until_street() -> void:
	seen_text = ""
	for prompt in range(40):
		seen_text += dialogue() + "\n"
		if exploring(): return
		var rest_cancel: Button = town().get_node_or_null("RestTraining/Cancel")
		if rest_cancel and rest_cancel.is_visible_in_tree():
			await press("CampaignTown/RestTraining/Cancel")
			continue
		var list := choices()
		if not list.is_empty(): await choose(list[0])
		elif not await press("CampaignTown/%Continue"): await settle()
	bug("the inn does not return to the street", "dialogue '" + dialogue() + "'")

func leave_place() -> void:
	for prompt in range(10):
		if exploring(): return
		if not await press("CampaignTown/%Continue"): await settle()

# --- After a fight with the city watch ----------------------------------------

# A party that attacked the city watch is refused all service, as the
# original's places tell it, and spends nothing.
func check_refusal() -> void:
	var gold := party_gold()
	var hp := member_hp()
	await press("CampaignTown/%Forward")
	var served := false
	seen_text = ""
	for prompt in range(30):
		seen_text += dialogue() + "\n"
		if shopping() or in_heal_list() or asks_to_stay():
			served = true
			break
		if exploring(): break
		var list := choices()
		if list.has("YES"): await choose("YES")
		elif not list.is_empty(): await choose(list[0])
		elif not await press("CampaignTown/%Continue"): await settle()
	# Tempus's priests fight such a party instead.
	var tempus := context.begins_with("temple-0-12") or context.begins_with("temple-2-12")
	var attacked := seen_text.contains("YOU ATTACKED THE CITY WATCH, YOU WON'T BEAT US SO EASILY!")
	if seen_text.contains("not supported yet"):
		bug("a refusal or punishment is not supported", "'" + seen_text.strip_edges().replace("\n", " / ") + "'")
	if served or not (seen_text.contains("WE REFUSE YOU ALL SERVICE") or (tempus and attacked)):
		bug("a place serves a party that attacked the city watch", "'" + seen_text.strip_edges().replace("\n", " / ") + "'")
	if not attacked and (party_gold() != gold or member_hp() != hp):
		bug("a refused party loses gold or hit points", str(gold) + " -> " + str(party_gold()))

# A party that stays when the city watch rousts it from its camp in the
# street fights the watch on the combat screen.
func check_watch() -> void:
	await press("CampaignTown/%Camp")
	if not await press("CampaignTown/RestDialog/Start"):
		bug("the camp cannot start", "")
		return
	seen_text = ""
	for prompt in range(40):
		seen_text += dialogue() + "\n"
		if node("CampaignCombat") != null and node("CampaignCombat").is_visible_in_tree(): break
		if exploring(): break
		var list := choices()
		if list.has("STAY"): await choose("STAY")
		elif not list.is_empty(): await choose(list[0])
		elif not await press("CampaignTown/%Continue"): await settle()
	if node("CampaignCombat") == null or not node("CampaignCombat").is_visible_in_tree():
		bug("staying when the city watch comes starts no fight", "'" + seen_text.strip_edges().replace("\n", " / ") + "'")

# --- The plan -----------------------------------------------------------------

func run() -> void:
	root.gui_embed_subwindows = true
	load_records()
	var files := Array(DirAccess.get_files_at(fixtures)).filter(func(f): return f.ends_with(".ogs"))
	files.sort()
	var places := 0
	for file in files:
		if only != "" and not file.begins_with(only): continue
		var parts: PackedStringArray = file.get_basename().split("-")
		var kind := parts[0]
		var x := int(parts[1])
		var y := int(parts[2])
		var party := parts[3] if parts.size() > 3 else ""
		context = file.get_basename()
		trail = []
		if not await load_fixture(file): continue
		places += 1
		if party == "wounded": check_hit_point_colors()
		if places == 1: await check_party_cards("while exploring")
		if party == "outlaw":
			await check_refusal()
			continue
		if kind == "shop": await check_shop(x, y, party)
		elif kind == "temple": await check_temple(x, y, party)
		elif kind == "inn": await check_inn(x, y, party)
	if only == "" or "watch".begins_with(only):
		context = "watch"
		trail = []
		if await load_fixture("shop-15-8.ogs"):
			places += 1
			await check_watch()
	var shops := files.filter(func(f): return f.begins_with("shop-") and (only == "" or f.begins_with(only)))
	if not shops.is_empty() and only == "" and not sold_and_dropped:
		bug("Sell and Drop were never checked", "")
	if not shops.is_empty() and only == "" and auto_equipped == 0:
		bug("auto-equip never put on a bought weapon, armor or shield", "")
	if not shops.is_empty() and only == "" and (weapon_hovers == 0 or armor_hovers == 0):
		bug("no shop's hover told a weapon's roll to hit or armor's Armor Class", str(weapon_hovers) + " weapons, " + str(armor_hovers) + " armor")
	if not shops.is_empty() and only == "" and double_click_buys == 0:
		bug("a double click never bought an item", "")
	print("Checked ", places, " places; ", bugs.size(), " distinct problems; ", auto_equipped, " purchases auto-equipped")
	for key in bugs:
		print("  ", bugs[key].size(), "x ", key, "\n      e.g. ", bugs[key][0])
	if bugs.is_empty() and places > 0:
		print("Town services passed: ", places, " shops, temples and inns")
	quit(0 if bugs.is_empty() and places > 0 else 1)
