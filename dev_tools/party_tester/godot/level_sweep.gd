extends SceneTree

# The Party tester's level sweep (sweep.sh). Every class is taken from level 1
# to 15 on its own, and every pair of classes with the second class first
# taken at level 2, 9 and 15, all through the game's level-up dialog as a
# player takes them: the Class row, a feat or ability points, training, the
# checkbox pages, a replaced spell now and then and the spell pages. Where the
# second class needs a score of 13 the character lacks, Ability Score
# Improvements raise it first. After every level the sheet is checked against
# SRD 5.2.1's tables: Hit Points, Proficiency Bonus, spell slots (the
# Multiclass Spellcaster table, Pact Magic beside it) and Hit Dice. Each party
# of eight fights the Slums' orcs and Valjevo Castle's fire giant and trolls at
# levels 1, 5, 10 and 15, on Quick until the fight ends.
#
# OPENGOLD_SWEEP_FIGHTS=N fights with every Nth party (0: none); every party
# fights by default. OPENGOLD_SWEEP_PLANS=N stops after N characters, and
# OPENGOLD_SWEEP_MATCH=text takes only the characters whose plan names it;
# OPENGOLD_SWEEP_PARTY=N runs only the Nth party (each failure names its
# party), OPENGOLD_SWEEP_FROM=N and OPENGOLD_SWEEP_UNTIL=N a range of them;
# OPENGOLD_SWEEP_FIGHT_AT=5,15 fights at those levels only; OPENGOLD_SWEEP_VERBOSE=1 prints each page's picks.

const TOP_LEVEL := 15
const FIGHT_LEVELS := [1, 5, 10, 15]
const SPLITS := [2, 9, 15]
const EXPLAINED := ["A blast invocation needs the cantrip it improves.",
                    "Choose an Additional Fighting Style other than the new Fighting Style",
                    "Choose an available feat or ability points"]
const FIGHT_SECONDS := 240
const HIT_DIE := {"barbarian": 12, "fighter": 10, "paladin": 10, "ranger": 10, "bard": 8,
                  "cleric": 8, "druid": 8, "monk": 8, "rogue": 8, "warlock": 8, "sorcerer": 6,
                  "wizard": 6}
const FULL_CASTERS := ["bard", "cleric", "druid", "sorcerer", "wizard"]
const HALF_CASTERS := ["paladin", "ranger"]
# SRD 5.2.1's spell slots by level, 1st-level slots first: a full caster's,
# which is also the Multiclass Spellcaster table, and a Paladin's or Ranger's.
const FULL_SLOTS := [[], [2], [3], [4, 2], [4, 3], [4, 3, 2], [4, 3, 3], [4, 3, 3, 1],
                     [4, 3, 3, 2], [4, 3, 3, 3, 1], [4, 3, 3, 3, 2], [4, 3, 3, 3, 2, 1],
                     [4, 3, 3, 3, 2, 1], [4, 3, 3, 3, 2, 1, 1], [4, 3, 3, 3, 2, 1, 1],
                     [4, 3, 3, 3, 2, 1, 1, 1]]
const HALF_SLOTS := [[], [2], [2], [3], [3], [4, 2], [4, 2], [4, 3], [4, 3], [4, 3, 2],
                     [4, 3, 2], [4, 3, 3], [4, 3, 3], [4, 3, 3, 1], [4, 3, 3, 1],
                     [4, 3, 3, 2]]

var failures := PackedStringArray()
var bridge: Node
var host: Control
var labels := {}        # class ID -> its name in the dialog
var requirements := {}  # class ID -> {abilities, any}
var plans := []
var levels_taken := 0
var retried := 0
var explained := {}      # an explained conflict -> how often the varied picks met it
var pending_levels := {}  # class name -> levels with a choice left pending
var fights_fought := 0
var late_second := 0    # pairs whose second class came later than planned
var never_second := PackedStringArray()
var current_party := 0
var fight_levels := FIGHT_LEVELS
var verbose := not OS.get_environment("OPENGOLD_SWEEP_VERBOSE").is_empty()

func _initialize() -> void:
    call_deferred("run")

func settle(frames := 2) -> void:
    for frame in range(frames):
        await process_frame

func fail(message: String) -> void:
    message = "Party %d: %s" % [current_party, message]
    failures.append(message)
    push_error(message)

func environment_number(name: String, fallback: int) -> int:
    var value := OS.get_environment(name)
    return fallback if value.is_empty() else int(value)

func modifier(score: int) -> int:
    return floori((score - 10) / 2.0)

# --- What SRD 5.2.1 says the sheet holds ---------------------------------

func expected_slots(class_ids: Array) -> Array:
    # The Multiclass Spellcaster table takes the full casters' levels and half
    # of a Paladin's and of a Ranger's, each rounded up.
    var full := 0
    var casters := []
    var warlock := 0
    for id in class_ids:
        if id == "warlock":
            warlock += 1
        if (id in FULL_CASTERS or id in HALF_CASTERS) and not (id in casters):
            casters.append(id)
        if id in FULL_CASTERS:
            full += 1
    var slots := [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    var table := []
    if casters.size() == 1:
        table = (FULL_SLOTS[full] if full > 0 else HALF_SLOTS[class_ids.count(casters[0])])
    elif casters.size() > 1:
        table = FULL_SLOTS[full + ceili(class_ids.count("paladin") / 2.0)
                           + ceili(class_ids.count("ranger") / 2.0)]
    for n in range(table.size()):
        slots[n + 1] = table[n]
    if warlock > 0:
        var pact_slots := 1 if warlock == 1 else (2 if warlock <= 10 else 3)
        slots[mini(5, ceili(warlock / 2.0))] += pact_slots
    return slots

func expected_hit_points(member: Dictionary) -> int:
    var ids: Array = member.class_ids
    var constitution := modifier(member.scores[2])
    var total: int = HIT_DIE[ids[0]] + constitution
    for n in range(1, ids.size()):
        total += maxi(1, HIT_DIE[ids[n]] / 2 + 1 + constitution)
    if String(member.race).to_lower() == "dwarf":
        total += ids.size()
    var sorcerer := ids.count("sorcerer")
    if sorcerer >= 3:
        total += sorcerer
    return total

func expected_hit_dice(class_ids: Array) -> String:
    var counts := {}
    for id in class_ids:
        counts[HIT_DIE[id]] = counts.get(HIT_DIE[id], 0) + 1
    var dice := counts.keys()
    dice.sort()
    dice.reverse()
    var parts := PackedStringArray()
    for die in dice:
        parts.append("%dd%d" % [counts[die], die])
    return " + ".join(parts)

func check_sheet(plan: Dictionary, member: Dictionary, index: int) -> void:
    var who := "%s (%s, level %d)" % [plan.name, member.classes, member.level]
    var ids: Array = member.class_ids
    if ids != plan.taken:
        fail("%s: levels in %s, not %s" % [who, ids, plan.taken])
        return
    var hit_points := expected_hit_points(member)
    if member.maximum_hit_points != hit_points:
        fail("%s: %d maximum Hit Points, SRD %d" % [who, member.maximum_hit_points, hit_points])
    var proficiency: int = 2 + (member.level - 1) / 4
    for ability in range(6):
        if member.save_proficiencies[ability]:
            var bonus: int = member.saving_throws[ability] - modifier(member.scores[ability])
            if bonus != proficiency:
                fail("%s: Proficiency Bonus %d on save %d, SRD %d" % [who, bonus, ability,
                                                                      proficiency])
    var slots := expected_slots(ids)
    for slot_level in range(1, 10):
        var have: int = member.resources.get("spell_slot:%d" % slot_level, 0)
        if have != slots[slot_level]:
            fail("%s: %d level-%d spell slots, SRD %d" % [who, have, slot_level,
                                                           slots[slot_level]])
    if member.hit_dice != member.level:
        fail("%s: %d Hit Dice" % [who, member.hit_dice])
    var sheet: String = bridge.sheet(index)
    var found := RegEx.create_from_string("Hit Dice: ([0-9d +]+?)(   |\\n|$)").search(sheet)
    var dice := expected_hit_dice(ids)
    if found == null or found.get_string(1).strip_edges() != dice:
        fail("%s: the sheet's Hit Dice are '%s', SRD %s" % [who,
             found.get_string(1) if found else "missing", dice])

# --- Playing the level-up dialog --------------------------------------------

func visible_control(dialog: Node, path: String) -> Control:
    var control: Control = dialog.get_node_or_null(path)
    return control if control and control.is_visible_in_tree() else null

func choose(option: OptionButton, index: int) -> void:
    option.select(index)
    option.item_selected.emit(index)
    await settle()

# An enabled item, varied by `variety`, skipping the first `skip` items.
func varied_item(option: OptionButton, variety: int, skip := 0) -> int:
    var count := option.item_count - skip
    for step in range(count):
        var index := skip + (variety + step) % count
        if not option.is_item_disabled(index):
            return index
    return -1

func class_item(dialog: Node, id: String) -> int:
    var row: OptionButton = dialog.get_node("Class")
    for n in range(row.item_count):
        if row.get_item_text(n).begins_with(labels[id] + ":"):
            return n
    return -1

# The ability a class this character means to take still needs 13 in, or -1.
func needed_ability(scores: Array, ids: Array) -> int:
    for id in ids:
        var rule: Dictionary = requirements[id]
        var met := 0
        var lowest := -1
        var best := -1
        for ability in rule.abilities:
            if scores[ability] >= 13:
                met += 1
            elif lowest < 0:
                lowest = ability
            if best < 0 or scores[ability] > scores[best]:
                best = ability
        if rule.abilities.is_empty() or (rule.any and met > 0) or met == rule.abilities.size():
            continue
        return best if rule.any else lowest
    return -1

# Checks every box it may on a page of checkboxes, as many as the page takes,
# starting from a varied one.
func check_boxes(page: Node, variety: int) -> void:
    for attempt in range(40):
        var boxes := []
        for box in page.find_children("*", "CheckBox", true, false):
            if box.is_visible_in_tree() and not box.disabled and not box.button_pressed:
                boxes.append(box)
        if boxes.is_empty():
            return
        var box: CheckBox = boxes[variety % boxes.size()]
        box.button_pressed = true
        await settle()

func first_page(dialog: Node, plan: Dictionary, member: Dictionary, variety: int) -> void:
    var need := -1
    if not plan.second.is_empty() and not (plan.second in member.class_ids):
        need = needed_ability(member.scores, [plan.start, plan.second])
    # The style first, as the feats it leaves available follow it.
    var style: OptionButton = visible_control(dialog, "FightingStyle")
    var style_name := ""
    if style:
        await choose(style, varied_item(style, variety))
        style_name = style.get_item_text(style.selected)
    var feat: OptionButton = visible_control(dialog, "Feat")
    if feat and not feat.disabled:
        var pick := -1
        if need >= 0:
            for n in range(feat.item_count):
                if feat.get_item_text(n).begins_with("Ability points") \
                        and not feat.is_item_disabled(n):
                    pick = n
        if pick < 0:
            pick = varied_item(feat, variety)
        await choose(feat, pick)
    var ability_points: OptionButton = visible_control(dialog, "Ability0")
    if ability_points and feat and feat.selected >= 0 \
            and feat.get_item_text(feat.selected).begins_with("Ability points"):
        var target := need
        for step in range(6):
            if target >= 0:
                break
            if member.scores[(variety + step) % 6] <= 18:
                target = (variety + step) % 6
        for ability in range(6):
            await choose(dialog.get_node("Ability%d" % ability), 2 if ability == target else 0)
    var training: OptionButton = visible_control(dialog, "AdvancementTraining")
    if training:
        var pick := varied_item(training, variety, 1)
        # A Champion's Additional Fighting Style differs from the new style.
        if not style_name.is_empty() and training.get_item_text(pick).begins_with(style_name + " /"):
            pick = varied_item(training, variety + 1, 1)
        await choose(training, pick)
    var confirm: Button = dialog.get_node("Confirm")
    for n in range(4):
        var spell: CheckBox = visible_control(dialog, "Spell%d" % n)
        if confirm.disabled and spell and not spell.disabled and not spell.button_pressed:
            spell.button_pressed = true
            await settle()

# A cantrip, spell or option replaced with the first offered, on every fourth
# level the page shows.
func swap_page(page: Node, variety: int) -> void:
    if variety % 4 != 0:
        return
    for row in ["SwapCantrip", "SwapSpell", "SwapOption", "SwapDiscovery"]:
        var replaced: OptionButton = page.get_node(row)
        if replaced.is_visible_in_tree() and replaced.item_count > 1:
            await choose(replaced, 1)
            var replacement: OptionButton = page.get_node(row + "With")
            if replacement.item_count > 1:
                await choose(replacement, 1)
            return

# What the showing page has picked, for a failure's report.
func page_picks(dialog: Node) -> String:
    var picks := PackedStringArray()
    for option in dialog.find_children("*", "OptionButton", true, false):
        if option.is_visible_in_tree() and option.selected >= 0:
            picks.append("%s=%s" % [option.name, option.get_item_text(option.selected)])
    for box in dialog.find_children("*", "CheckBox", true, false):
        if box.is_visible_in_tree() and box.button_pressed:
            picks.append(box.text)
    # The spell page's groups, with how many of each are picked.
    for count in dialog.find_children("Count", "Label", true, false):
        if count.is_visible_in_tree():
            picks.append("[" + count.text + "]")
    return ", ".join(picks)

func take_level(plan: Dictionary, index: int, member: Dictionary, level: int,
                plain := false) -> bool:
    var who := "%s at level %d" % [plan.name, level]
    var wanted: String = plan.start
    if not plan.second.is_empty() and level >= plan.split:
        wanted = plan.second
    if bridge.open_level_up().is_empty():
        fail(who + ": the level-up dialog did not open")
        return false
    await settle()
    var dialog: Window = bridge.get_node("LevelUp")
    var item := class_item(dialog, wanted)
    var row: OptionButton = dialog.get_node("Class")
    if item < 0:
        fail("%s: the Class row has no %s" % [who, labels[wanted]])
        return false
    if row.is_item_disabled(item):
        if wanted == plan.start:
            fail("%s: the Class row greys out the starting class" % who)
            return false
        if plan.taken.has(plan.second):
            fail("%s: the Class row greys out %s, a class it has" % [who, labels[wanted]])
            return false
        if needed_ability(member.scores, [plan.start, plan.second]) < 0:
            fail("%s: %s is greyed out though its scores are met: %s" % [who, labels[wanted],
                 row.get_item_text(item)])
            return false
        wanted = plan.start
        item = class_item(dialog, wanted)
    elif wanted != plan.start and not plan.taken.has(wanted) \
            and needed_ability(member.scores, [plan.start, plan.second]) >= 0:
        fail("%s: %s is offered without its scores: %s" % [who, labels[wanted],
             row.get_item_text(item)])
        return false
    if item != 0 or wanted != plan.start:
        await choose(row, item)
    # Plain picks are the first offered, with nothing replaced: the retry after
    # a failure, so one failure does not end the party's sweep.
    var variety: int = 0 if plain else plan.variety + level
    await first_page(dialog, plan, member, variety)
    var confirm: Button = dialog.get_node("Confirm")
    var left_pending := false
    for page in range(16):
        if not dialog.visible:
            break
        # A spell the game does not have leaves its place pending (the Cleric's
        # spells decision in docs/SRD-DECISIONS.md): counted, not a failure.
        for pending in dialog.find_children("Pending", "Label", true, false):
            if pending.is_visible_in_tree():
                left_pending = true
        if confirm.disabled:
            var error := (dialog.get_node("Error") as Label).text
            # Two picks that cannot go together, which the dialog names so the
            # player can change one: the retry's plain picks avoid them.
            if error in EXPLAINED:
                explained[error] = explained.get(error, 0) + 1
            else:
                fail("%s (%s): Confirm is disabled on page %d (%s): %s" % [who, labels[wanted],
                     page, page_picks(dialog), error])
            dialog.hide()
            return false
        if verbose:
            print("%s (%s) page %d: %s" % [who, labels[wanted], page, page_picks(dialog)])
        confirm.pressed.emit()
        await settle()
        if not dialog.visible:
            break
        var skilled := visible_control(dialog, "SkilledPage")
        if skilled:
            await check_boxes(skilled, variety)
        var swaps := visible_control(dialog, "SwapPage")
        if swaps:
            await swap_page(swaps, 1 if plain else variety)
        var spells := visible_control(dialog, "SpellChoicesPage")
        if spells:
            await check_boxes(spells, variety)
    if dialog.visible:
        fail("%s (%s): the dialog is still open after 16 pages" % [who, labels[wanted]])
        dialog.hide()
        return false
    var after: Dictionary = bridge.members()[index]
    if after.level != level:
        fail("%s (%s): level %d after Confirm" % [who, labels[wanted], after.level])
        return false
    plan.taken.append(wanted)
    levels_taken += 1
    if left_pending:
        pending_levels[labels[wanted]] = pending_levels.get(labels[wanted], 0) + 1
    return true

# --- Fights --------------------------------------------------------------

func answer_prompt(combat: Node) -> void:
    for name in ["Decline", "Continue"]:
        var button: Button = combat.get_node_or_null(name)
        if button and button.is_visible_in_tree() and not button.disabled:
            button.pressed.emit()
            return
    for window in combat.find_children("*", "Window", true, false):
        if not window.visible:
            continue
        var keep: Button = window.find_child("Keep", true, false)
        if keep and not keep.disabled:
            keep.pressed.emit()
            return

# What a fight that did not end shows: its enabled buttons, open windows and
# the end of its log.
func stalled_screen(combat: Node) -> String:
    if combat == null:
        return "no combat screen"
    var shown := PackedStringArray()
    for button in combat.find_children("*", "Button", true, false):
        if button.is_visible_in_tree() and not button.disabled and not button.text.is_empty():
            shown.append(button.text)
    for window in combat.find_children("*", "Window", true, false):
        if window.visible:
            shown.append("window " + window.name)
    var log: RichTextLabel = combat.get_node_or_null("LogStack/Log")
    var tail := ""
    if log:
        var lines := log.get_parsed_text().strip_edges().split("\n")
        tail = " | ".join(lines.slice(maxi(0, lines.size() - 8)))
        if verbose:
            print("STALLED LOG\n", "\n".join(lines.slice(0, 400)))
    return "buttons: %s; log: %s" % [", ".join(shown), tail]

func fight(name: String, configuration: Dictionary) -> void:
    var error: String = bridge.start_fight(configuration)
    if not error.is_empty():
        fail("%s: the fight does not start: %s" % [name, error])
        return
    await settle(4)
    var combat: Node = host.get_node_or_null("CampaignCombat")
    var outcome := ""
    Engine.time_scale = 8.0
    var started := Time.get_ticks_msec()
    var deadline := started + FIGHT_SECONDS * 1000
    # The log keeps its last lines only, so a fight that stalls is read from
    # snapshots taken early on.
    var snapshots := []
    var snapshot_at := [2000, 6000, 15000, 40000]
    while Time.get_ticks_msec() < deadline:
        await settle()
        if verbose and not snapshot_at.is_empty() and Time.get_ticks_msec() - started > snapshot_at[0]:
            snapshot_at.pop_front()
            var log: RichTextLabel = combat.get_node_or_null("LogStack/Log") if combat else null
            if log:
                snapshots.append(log.get_parsed_text())
        outcome = bridge.outcome()
        if not outcome.is_empty():
            break
        var quick := InputEventKey.new()
        quick.keycode = KEY_Q
        quick.pressed = true
        root.push_input(quick)
        answer_prompt(combat)
    Engine.time_scale = 1.0
    if outcome.is_empty():
        fail("%s: the fight did not end in %d seconds; %s" % [name, FIGHT_SECONDS,
             stalled_screen(combat)])
        for n in range(snapshots.size()):
            print("SNAPSHOT %d\n%s" % [n, snapshots[n]])
    else:
        print("%s: %s" % [name, outcome])
    fights_fought += 1
    bridge.leave()
    await settle()

func index_of(items: Array, text: String) -> int:
    for n in range(items.size()):
        if items[n] == text or String(items[n]).begins_with(text + ","):
            return n
    return -1

func fights(party: int, level: int, catalog: Dictionary) -> void:
    var slums := index_of(catalog.areas, "The Slums")
    var castle := index_of(catalog.areas, "Valjevo Castle")
    var orc := index_of(catalog.creatures, "The Slums: Orc")
    if orc < 0:
        orc = index_of(catalog.creatures, "The Slums: ORC")
    var giant := index_of(catalog.creatures, "Valjevo Castle: FIRE GIANT")
    var troll := index_of(catalog.creatures, "Valjevo Castle: TROLL")
    if min(slums, castle, orc, giant, troll) < 0:
        fail("The fights' areas or creatures are missing from the catalog")
        return
    await fight("Party %d, level %d, the Slums' orcs" % [party, level],
                {"area": slums, "seed": party * 100 + level, "difficulty": "normal",
                 "groups": [{"creature": orc, "count": 3}]})
    await fight("Party %d, level %d, Valjevo Castle's fire giant and trolls" % [party, level],
                {"area": castle, "seed": party * 100 + level, "difficulty": "normal",
                 "groups": [{"creature": giant, "count": 1}, {"creature": troll, "count": 2}]})

# --- The plan ------------------------------------------------------------

func pool_scores(count: int) -> Array:
    var scores := []
    for first in range(0, count, 8):
        var chunk := []
        for n in range(first, mini(first + 8, count)):
            chunk.append(n)
        var error: String = bridge.build_party(chunk)
        if not error.is_empty():
            fail("The pool's party does not build: " + error)
            return []
        for member in bridge.members():
            scores.append(member.scores)
    return scores

# The pool character of class `start` nearest to meeting both classes' 13s.
func best_character(of_class: Array, scores: Array, start: String, second: String) -> int:
    var best := -1
    var best_short := 1000
    for index in of_class:
        var short := 0
        for id in [start, second]:
            var rule: Dictionary = requirements[id]
            var gaps := []
            for ability in rule.abilities:
                gaps.append(maxi(0, 13 - scores[index][ability]))
            if not gaps.is_empty():
                short += gaps.min() if rule.any else gaps.reduce(func(a, b): return a + b, 0)
        if short < best_short:
            best = index
            best_short = short
    return best

func make_plans(catalog: Dictionary) -> void:
    var pool_class := []
    for entry in catalog.pool:
        var found := ""
        for id in labels:
            if String(entry).ends_with(" " + labels[id]):
                found = id
        pool_class.append(found)
    var scores := pool_scores(catalog.pool.size())
    var ids: Array = labels.keys()
    for start in ids:
        var of_class := []
        for n in range(pool_class.size()):
            if pool_class[n] == start:
                of_class.append(n)
        if of_class.is_empty():
            fail("The pool has no " + labels[start])
            continue
        for index in of_class:
            plans.append({"name": "%s (%s)" % [catalog.pool[index], labels[start]],
                          "pool": index, "start": start, "second": "", "split": 99})
        for second in ids:
            if second == start:
                continue
            var index := best_character(of_class, scores, start, second)
            for split in SPLITS:
                plans.append({"name": "%s, %s from level %d" % [labels[start], labels[second],
                                                                split],
                              "pool": index, "start": start, "second": second, "split": split})
    for n in range(plans.size()):
        plans[n].variety = n * 7
        plans[n].taken = [plans[n].start]

# Parties of eight, in plan order, none with a pool character twice: the game
# lets each join a party once.
func parties(chosen: Array) -> Array:
    var result := []
    for plan in chosen:
        var placed := false
        for party in result:
            if party.size() < 8 and party.all(func(other): return other.pool != plan.pool):
                party.append(plan)
                placed = true
                break
        if not placed:
            result.append([plan])
    return result

func sweep_party(party: int, members: Array, fight_here: bool, catalog: Dictionary) -> void:
    current_party = party
    var indices := []
    for plan in members:
        indices.append(plan.pool)
    var error: String = bridge.build_party(indices)
    if not error.is_empty():
        fail("Party %d does not build: %s" % [party, error])
        return
    for n in range(members.size()):
        check_sheet(members[n], bridge.members()[n], n)
    if fight_here and 1 in fight_levels:
        await fights(party, 1, catalog)
    for level in range(2, TOP_LEVEL + 1):
        error = bridge.set_level(level)
        if not error.is_empty():
            fail("Party %d: level %d: %s" % [party, level, error])
            return
        for attempt in range(members.size()):
            var all: Array = bridge.members()
            var index := -1
            for n in range(all.size()):
                if all[n].can_advance:
                    index = n
                    break
            if index < 0:
                break
            if not await take_level(members[index], index, all[index], level):
                retried += 1
                if not await take_level(members[index], index, all[index], level, true):
                    return
            check_sheet(members[index], bridge.members()[index], index)
        if fight_here and level in fight_levels:
            await fights(party, level, catalog)
    for plan in members:
        if plan.second.is_empty():
            continue
        if not plan.taken.has(plan.second):
            never_second.append(plan.name)
        elif plan.taken.find(plan.second) + 1 > plan.split:
            late_second += 1

func run() -> void:
    root.gui_embed_subwindows = true
    host = Control.new()
    host.set_anchors_preset(Control.PRESET_FULL_RECT)
    root.add_child(host)
    bridge = ClassDB.instantiate("PartyTestBridge")
    bridge.name = "Bridge"
    host.add_child(bridge)
    await settle()
    var catalog: Dictionary = bridge.catalog()
    if catalog.has("error"):
        printerr(catalog.error)
        quit(1)
        return
    for row in bridge.classes():
        labels[row.id] = row.label
        requirements[row.id] = row
        if not HIT_DIE.has(row.id):
            fail("No Hit Die known for class " + row.id)
    make_plans(catalog)
    var match := OS.get_environment("OPENGOLD_SWEEP_MATCH")
    if not match.is_empty():
        plans = plans.filter(func(plan): return String(plan.name).contains(match))
    var limit := environment_number("OPENGOLD_SWEEP_PLANS", plans.size())
    var fight_every := environment_number("OPENGOLD_SWEEP_FIGHTS", 1)
    var at := OS.get_environment("OPENGOLD_SWEEP_FIGHT_AT")
    if not at.is_empty():
        fight_levels = Array(at.split(",")).map(func(n): return int(n))
    var started := Time.get_ticks_msec()
    var only_party := environment_number("OPENGOLD_SWEEP_PARTY", -1)
    var from_party := environment_number("OPENGOLD_SWEEP_FROM", 0)
    var until_party := environment_number("OPENGOLD_SWEEP_UNTIL", -1)
    var party := 0
    for members in parties(plans.slice(0, limit)):
        if (only_party >= 0 and party != only_party) or party < from_party \
                or (until_party >= 0 and party > until_party):
            party += 1
            continue
        var fight_here := fight_every > 0 and party % fight_every == 0
        await sweep_party(party, members, fight_here, catalog)
        print("Party %d done: %d levels taken, %d fights, %d failures, %d s" % [party,
              levels_taken, fights_fought, failures.size(),
              (Time.get_ticks_msec() - started) / 1000])
        party += 1
    print("Characters: %d, levels taken: %d (%d retried with plain picks), fights: %d" % [
          mini(limit, plans.size()), levels_taken, retried, fights_fought])
    print("Pairs whose second class came later than planned: %d" % late_second)
    print("Levels with a spell choice left pending, by class: ", pending_levels)
    print("Conflicting picks the dialog explained: ", explained)
    if not never_second.is_empty():
        print("Pairs that never met the second class's scores: ", ", ".join(never_second))
    if failures.is_empty():
        print("Level sweep passed")
        quit(0)
        return
    for failure in failures:
        printerr(failure)
    printerr("%d failures" % failures.size())
    quit(1)
