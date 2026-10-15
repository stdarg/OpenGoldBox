extends RefCounted
## Read-only evidence index. No review state is written or confirmed here.
const Decoder = preload("res://scripts/por_ecl_decoder.gd")
const Tables = preload("res://scripts/por_ecl_tables.gd")
const DIALOGUE_TARGETS := [
    {"key":"NPC:ROLF","name":"Rolf (city guide)","phrase":"I AM ROLF, APPOINTED BY THE COUNCIL"},
    {"key":"ROLE:SCRIBE","name":"Scribe","phrase":"A SCRIBE WALKS INTO THE ROOM"},
    {"key":"ROLE:TAVERN_BRAWLER","name":"Tavern brawler","phrase":"A DRUNKEN BRAWL BREAKS OUT"},
    {"key":"ROLE:BANE_PRIEST","name":"Priest of Bane / acolytes","phrase":"A PRIEST AND TWO ACOLYTES TURN"},
    {"key":"ROLE:CITY_WATCH","name":"City watch","phrase":"YOU ARE ROUSTED BY THE CITY WATCH"},
    {"key":"ROLE:TRADER","name":"Trader / wagon seller","phrase":"YOU QUICKLY KILL THE TRADER AND GRAB HIS WAGON"}
]
var scripts: Array = []
var references: Dictionary = {}
var characters: Array = []
var summary: Dictionary = {}
var assets: Dictionary = {}
var targets: Array = []

func build(folder: String) -> void:
    scripts.clear()
    references.clear()
    characters.clear()
    assets.clear()
    targets.clear()
    summary = {"scripts":0,"instructions":0,"diagnostics":[],"dynamic_art_references":0,"table_art_references":0}
    var directory := DirAccess.open(folder)
    if directory == null:
        summary.diagnostics.append("Game directory unavailable")
        return
    var loader := DaxSpriteLoader.new()
    var files := Array(directory.get_files())
    files.sort()
    for filename in files:
        var archive: String = filename.to_upper()
        var path := folder.path_join(filename)
        if archive.match("CPIC*.DAX") or archive.match("SPRIT*.DAX"):
            assets[archive] = loader.list_record_ids(path)
        if archive.match("MON*CHA.DAX"):
            var bytes := FileAccess.get_file_as_bytes(path)
            for id in loader.list_record_ids(path):
                var record := loader._extract_record(bytes,id)
                if record.size() == 285 and record[0] <= 15:
                    characters.append({"key":"%s:%d" % [archive,id],"name":record.slice(1,1+record[0]).get_string_from_ascii(),"id":id,"archive":archive})
        if archive.match("ECL*.DAX"):
            var bytes := FileAccess.get_file_as_bytes(path)
            for id in loader.list_record_ids(path):
                var record := loader._extract_record(bytes,id)
                var script: Dictionary = Decoder.new().decode(record)
                script.archive = archive
                script.record = id
                script.sha256 = record.hex_encode().sha256_text()
                var tables = Tables.new()
                tables.setup(script,record)
                for node in script.nodes.values():
                    if node.opcode == 11: node.table_pairs = tables.pairs(node)
                scripts.append(script)
                summary.scripts += 1
                summary.instructions += script.nodes.size()
                for diagnostic in script.diagnostics:
                    summary.diagnostics.append("%s/%d: %s" % [archive,id,diagnostic])
    for script in scripts:
        for node in script.nodes.values():
            if node.opcode not in [11,12,41]: continue
            var combat: bool = node.opcode == 11
            var arg: Dictionary = node.args[2 if combat else 0]
            if arg.kind != "constant":
                summary.dynamic_art_references += 1
                if combat and not node.get("table_pairs",[]).is_empty():
                    _add_table_references(script,node)
                continue
            if arg.value == 255: continue
            var family := "CPIC" if combat else "SPRIT"
            var context := _context(script, node.offset)
            var entry := {"script_archive":script.archive,"script_record":script.record,
                "address":node.address,"record_offset":node.offset+2,"command":node.command,
                "art_id":arg.value,"family":family,"script_sha256":script.sha256,
                "operands":node.args,"dialogue":context.dialogue,"related_characters":context.characters,
                "context_truncated":context.truncated,"candidates":[],
                "confidence":"Script reference; resource bank inferred; not gameplay verified"}
            var bank: String = str(script.archive).trim_prefix("ECL").trim_suffix(".DAX")
            var preferred := "%s%s.DAX" % [family,bank]
            var archives: Array = []
            if assets.get(preferred,[]).has(arg.value): archives.append(preferred)
            else:
                for archive in assets:
                    if str(archive).begins_with(family) and assets[archive].has(arg.value): archives.append(archive)
            entry.archive_basis = "Same bank as ECL (inferred)" if archives.has(preferred) else "ID exists in these banks; runtime bank unknown"
            entry.art_archives = archives
            if combat:
                var identity: Dictionary = node.args[0]
                if identity.kind == "constant": entry.candidates = _character_candidates(identity.value,bank,"Explicit LOAD MONSTER operands",node.address)
            else:
                for text in context.dialogue:
                    for target in DIALOGUE_TARGETS:
                        if target.phrase in str(text.text):
                            var candidate: Dictionary = target.duplicate()
                            candidate.basis = "Dialogue clue on a possible encounter path"
                            candidate.address = text.address
                            entry.candidates.append(candidate)
                            if not targets.has(target): targets.append(target)
                for identity in context.characters:
                    for candidate in _character_candidates(identity.id,bank,"Possible encounter path: " + identity.command,identity.address):
                        if not entry.candidates.has(candidate): entry.candidates.append(candidate)
            for archive in archives:
                var key := "%s:%d" % [archive,arg.value]
                if not references.has(key): references[key] = []
                references[key].append(entry)

func _character_candidates(id: int, bank: String, basis: String, address: int) -> Array:
    var same_bank: Array = []
    var other_banks: Array = []
    for character in characters:
        if character.id != id: continue
        var result: Dictionary = character.duplicate()
        result.basis = basis
        result.address = address
        if character.archive == "MON%sCHA.DAX" % bank: same_bank.append(result)
        else: other_banks.append(result)
    return same_bank if not same_bank.is_empty() else other_banks

func _context(script: Dictionary, start: int) -> Dictionary:
    # Explore possible paths with matched subroutine returns. Branch conditions
    # and runtime memory are unknown, so these are contextual clues, not bindings.
    var dialogue: Array = []
    var identities: Array = []
    var queue: Array = [{"pc":start,"stack":[]}]
    var seen := {}
    var emitted := {}
    var truncated := false
    while not queue.is_empty() and seen.size() < 512:
        var point: Dictionary = queue.pop_front()
        var pc: int = point.pc
        var stack: Array = point.stack
        var visit_key := "%d:%s" % [pc,str(stack)]
        if seen.has(visit_key) or not script.nodes.has(pc): continue
        seen[visit_key] = true
        var node: Dictionary = script.nodes[pc]
        var op: int = node.opcode
        if pc != start and op in [12,41,49,32,33,45]: continue
        if not emitted.has(pc):
            emitted[pc] = true
            # Literal text is reported only from display/menu instructions.
            if op in [17,18,21,41,43]:
                for arg in node.args:
                    if arg.kind == "text" and not str(arg.text).strip_edges().is_empty():
                        dialogue.append({"address":node.address,"text":arg.text})
            if op in [11,54] and node.args[0].kind == "constant":
                identities.append({"id":node.args[0].value,"command":node.command,"address":node.address})
            if op == 11:
                for pair in node.get("table_pairs",[]):
                    identities.append({"id":pair.monster_id,"command":"LOAD MONSTER (table candidate)","address":node.address})
        if op in [0,32,36,51]: continue
        if op == 19:
            if not stack.is_empty():
                var returned := stack.duplicate()
                queue.append({"pc":returned.pop_back(),"stack":returned})
            continue
        if op in [2,38]:
            if stack.size() >= 8:
                truncated = true
                continue
            var called := stack.duplicate()
            called.append(node.end)
            var targets: Array = [node.args[0]] if op == 2 else node.args.slice(2)
            for target in targets: queue.append({"pc":int(target.value)-Decoder.ORIGIN,"stack":called})
            # ON GOSUB can fall through for an out-of-range selector.
            if op == 38: queue.append({"pc":node.end,"stack":stack})
            continue
        for edge in node.edges: queue.append({"pc":edge,"stack":stack})
    return {"dialogue":dialogue,"characters":identities,"truncated":truncated or not queue.is_empty()}

func for_group(group: Dictionary) -> Array:
    var found: Array = []
    var seen := {}
    for refs in group.sources:
        for ref in refs:
            var key := "%s:%d" % [str(ref.archive).to_upper(),ref.record]
            for entry in references.get(key,[]):
                var identity := "%s:%d:%d:%d" % [entry.script_archive,entry.script_record,entry.address,entry.get("table_lookup",{}).get("index",-1)]
                if not seen.has(identity):
                    seen[identity] = true
                    found.append(entry)
    return found

func suggestions(group: Dictionary) -> Dictionary:
    var found := {}
    for entry in for_group(group):
        for candidate in entry.candidates:
            if not found.has(candidate.key):
                found[candidate.key] = {"status":"suggested","basis":"%s; %s/%d @0x%04X. %s. Not gameplay verified." % [candidate.basis,entry.script_archive,entry.script_record,candidate.address,entry.archive_basis],"evidence":"script"}
    return found

func export_data() -> Dictionary:
    return {"schema_version":1,"summary":summary,"references":references,"characters":characters,"dialogue_targets":targets,
        "limitations":"Static possible paths, not executed traces. Resource banks are inferred. Adjacent paired tables use bounded possible indices; other dynamic operands remain unresolved. Original review decisions are separate."}

func _add_table_references(script: Dictionary, node: Dictionary) -> void:
    var bank: String = str(script.archive).trim_prefix("ECL").trim_suffix(".DAX")
    for pair in node.table_pairs:
        var archive := "CPIC%s.DAX" % bank
        if not assets.get(archive,[]).has(pair.icon_id): continue
        var entry := {"script_archive":script.archive,"script_record":script.record,
            "address":node.address,"record_offset":node.offset+2,"command":"LOAD MONSTER (paired tables)",
            "art_id":pair.icon_id,"family":"CPIC","script_sha256":script.sha256,
            "operands":node.args,"dialogue":[],"related_characters":[],"context_truncated":false,
            "table_lookup":pair,"confidence":"Bounded table candidate; not gameplay verified",
            "archive_basis":"Same bank as ECL (inferred)","art_archives":[archive],
            "candidates":_character_candidates(pair.monster_id,bank,"Parallel character/icon table entries at possible index %d" % pair.index,node.address)}
        var key := "%s:%d" % [archive,pair.icon_id]
        if not references.has(key): references[key] = []
        references[key].append(entry)
        summary.table_art_references += 1
