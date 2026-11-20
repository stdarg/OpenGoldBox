extends SceneTree
const Decoder = preload("res://scripts/por_ecl_decoder.gd")
const Evidence = preload("res://scripts/art_script_evidence.gd")
const Tables = preload("res://scripts/por_ecl_tables.gd")
var failures := 0

func check(value: bool, description: String) -> void:
    if not value:
        failures += 1
        push_error(description)

func record(body: Array) -> PackedByteArray:
    var bytes := PackedByteArray([0,0])
    for i in range(5): bytes.append_array(PackedByteArray([1,1,0x14,0x99]))
    bytes.append_array(PackedByteArray(body))
    return bytes

func _initialize() -> void:
    call_deferred("verify")

func verify() -> void:
    # Packed 6-bit ABC followed by padding; no ASCII/opcode scanning.
    check(Decoder.unpack_text(PackedByteArray([4,32,192])) == "ABC","Packed text decoding")
    var decoded: Dictionary = Decoder.new().decode(record([11,0,25,0,1,0,11,0,11,0,99,0,1,0,16]))
    check(decoded.nodes.size() == 2,"EXIT must exclude trailing data that resembles an art command")
    check(decoded.nodes[20].args[0].value == 25 and decoded.nodes[20].args[2].value == 11,"Character/icon operands are independent")
    decoded = Decoder.new().decode(record([11,1,0x79,0x6e,0,1,0,11,0]))
    check(decoded.nodes[20].args[0].kind == "memory","Dynamic identities must remain symbolic")
    decoded = Decoder.new().decode(record([17,128,10,0]))
    check(not decoded.diagnostics.is_empty() and decoded.nodes.is_empty(),"Truncated strings must stop decoding")
    decoded = Decoder.new().decode(record([22,0,11,0,25,0,1,0,11,0]))
    check(decoded.nodes.has(22),"Conditional EXIT must preserve the other branch")
    decoded = Decoder.new().decode(record([52,0,1,0]))
    check(decoded.nodes[20].end == 23 and decoded.diagnostics.is_empty(),"Pool ECL CLOCK has one operand")
    decoded = Decoder.new().decode(record([51,11,0,25,0,1,0,11,0]))
    check(decoded.nodes.has(21) and decoded.diagnostics.is_empty(),"PRINT RETURN falls through to the art command")
    # A call returns to its caller; separate event text beyond EXIT is excluded.
    # The synthetic subroutine begins at body offset 37 (0x9925).
    var fixture := record([12,0,15,0,0,0,25,2,1,0x25,0x99,54,0,25,0,0,0,17,128,3,4,32,192,19])
    decoded = Decoder.new().decode(fixture)
    var context: Dictionary = Evidence.new()._context(decoded,20)
    check(context.dialogue.size() == 1 and context.dialogue[0].text == "ABC","Dialogue in a called subroutine")
    check(context.characters.size() == 1 and context.characters[0].id == 25,"NPC association after matched subroutine return")
    var table_fixture := record([8,0,1,1,0,0x97,
        42,1,0x60,0x99,1,0,0x97,1,1,0x97,
        42,1,0x62,0x99,1,0,0x97,1,2,0x97,
        11,1,1,0x97,0,1,1,2,0x97,0])
    table_fixture.resize(98)
    table_fixture.append_array(PackedByteArray([25,51,11,24]))
    decoded = Decoder.new().decode(table_fixture)
    var tables = Tables.new()
    tables.setup(decoded,table_fixture)
    var pairs: Array = tables.pairs(decoded.nodes[46])
    check(pairs.size() == 2 and pairs[0].monster_id == 25 and pairs[0].icon_id == 11 and pairs[1].monster_id == 51 and pairs[1].icon_id == 24,"Parallel table pairs must not form a cross product")
    # Unknown input destroys the bounded index proof.
    table_fixture[22] = 15
    decoded = Decoder.new().decode(table_fixture)
    tables.setup(decoded,table_fixture)
    check(tables.pairs(decoded.nodes[46]).is_empty(),"Unbounded user-input table index must remain unresolved")
    var folder := GameFinder.game_folder("POOLRAD")
    if not folder.is_empty():
        var evidence = Evidence.new()
        evidence.build(folder)
        check(evidence.summary.scripts > 0,"Local ECL inventory")
        var scribe: Array = evidence.references.get("SPRIT5.DAX:22",[])
        check(scribe.size() == 1,"Scribe encounter reference")
        if not scribe.is_empty():
            check(scribe[0].script_record == 4 and scribe[0].address == 0xb160,"Scribe script provenance")
            check(scribe[0].candidates.any(func(c): return c.key == "ROLE:SCRIBE"),"Scribe dialogue suggestion")
        var mad_man: Array = evidence.references.get("SPRIT2.DAX:15",[])
        check(mad_man.any(func(e): return e.candidates.any(func(c): return c.key == "MON2CHA.DAX:25")),"Mad Man recruitment context")
        check(evidence.references.get("CPIC2.DAX:11",[]).any(func(e): return e.candidates.any(func(c): return c.key == "MON2CHA.DAX:25")),"Mad Man explicit combat icon operands")
        var brawler: Array = evidence.references.get("CPIC3.DAX:24",[])
        check(brawler.size() == 1 and brawler[0].table_lookup.index == 5,"Bounded parallel tables find previously missing icon 24")
        if not brawler.is_empty(): check(brawler[0].candidates[0].key == "MON3CHA.DAX:51","Table pairing retains character/icon correlation")
        var scene = load("res://scenes/monster_art_review.tscn").instantiate()
        root.add_child(scene)
        await process_frame
        var index := -1
        for i in range(scene.groups.size()):
            if scene.groups[i].sources[0][0].archive == "SPRIT5.DAX" and scene.groups[i].sources[0][0].record == 22: index = i
        check(index >= 0,"Scribe review group exists")
        if index >= 0:
            var group: Dictionary = scene.groups[index]
            scene.state = {group.key:{"links":{"ROLE:SCRIBE":{"status":"rejected","basis":"Test review"}},"resolution":"unresolved"}}
            check(scene._links(group)["ROLE:SCRIBE"].status == "rejected","Human rejection overrides new evidence")
            scene.state = {group.key:{"links":{"ROLE:SCRIBE":{"status":"confirmed","basis":"Test review"}},"resolution":"confirmed"}}
            check(scene._links(group)["ROLE:SCRIBE"].status == "confirmed","Existing confirmation preserved")
            scene.group_position = index
            scene._show()
            check("A SCRIBE WALKS" in scene.evidence_text.text,"Evidence visible in review tool")
            scene.search.text = "Scribe"
            scene._choices()
            check(scene.choices.item_count >= 1,"Dialogue role searchable")
        scene.queue_free()
        await process_frame
    print("PASS: ECL decoder, script evidence, and review integration" if failures == 0 else "FAIL: %d checks" % failures)
    quit(0 if failures == 0 else 1)
