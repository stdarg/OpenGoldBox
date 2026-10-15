extends RefCounted
## Conservative finite-value analysis for adjacent, parallel GETTABLE lookups.
## No general VM emulation: calls, unknown writes and unbounded sets stop analysis.
const ORIGIN := 0x9900
const LIMIT := 64
var nodes: Dictionary
var previous: Dictionary = {}
var bytes: PackedByteArray

func setup(script: Dictionary, record: PackedByteArray) -> void:
    nodes = script.nodes
    bytes = record.slice(2)
    previous.clear()
    for node in nodes.values():
        for edge in node.edges:
            if not previous.has(edge): previous[edge] = []
            previous[edge].append(node.offset)

func _destination(node: Dictionary) -> int:
    var op: int = node.opcode
    if op in [9,8,15,16]: return int(node.args[1].value)
    if op in [4,5,6,7,42,47,48]: return int(node.args[2].value)
    if op in [21,29,43]: return int(node.args[0].value)
    return -1

func _barrier(node: Dictionary) -> bool:
    return node.opcode in [2,10,30,32,33,36,38,41,44,45,53,54,56,57,59]

func _definitions(offset: int, address: int, visited: Dictionary = {}) -> Array:
    if visited.has(offset) or visited.size() > 200: return []
    var path := visited.duplicate()
    path[offset] = true
    var result: Array = []
    var predecessors: Array = previous.get(offset,[])
    if predecessors.is_empty(): return []
    for before in predecessors:
        var node: Dictionary = nodes[before]
        if _barrier(node): return []
        if _destination(node) == address:
            result.append(node)
        else:
            var earlier := _definitions(before,address,path)
            if earlier.is_empty(): return []
            for entry in earlier:
                if not result.has(entry): result.append(entry)
    return result

func _values(offset: int, arg: Dictionary, depth: int = 0) -> Array:
    if arg.kind == "constant": return [int(arg.value)]
    if arg.kind != "memory" or depth > 16: return []
    var definitions := _definitions(offset,arg.value)
    if definitions.is_empty(): return []
    var result: Array = []
    for node in definitions:
        var values: Array = []
        if node.opcode == 9:
            values = _values(node.offset,node.args[0],depth+1)
        elif node.opcode == 8:
            var bounds := _values(node.offset,node.args[0],depth+1)
            if bounds.is_empty(): return []
            var maximum: int = bounds.max()
            if maximum < 0 or maximum >= LIMIT: return []
            # Inclusive envelope; no claim that every value is realized at runtime.
            values = range(maximum+1)
        elif node.opcode in [4,5]:
            var left := _values(node.offset,node.args[0],depth+1)
            var right := _values(node.offset,node.args[1],depth+1)
            if left.is_empty() or right.is_empty(): return []
            for a in left:
                for b in right:
                    var value: int = a+b if node.opcode == 4 else b-a
                    if not values.has(value): values.append(value)
        else: return []
        if values.is_empty(): return []
        for value in values:
            if not result.has(value): result.append(value)
        if result.size() > LIMIT: return []
    return result

func pairs(node: Dictionary) -> Array:
    if node.opcode != 11 or node.args[0].kind != "memory" or node.args[2].kind != "memory": return []
    var identities := _definitions(node.offset,node.args[0].value)
    var icons := _definitions(node.offset,node.args[2].value)
    if identities.size() != 1 or icons.size() != 1: return []
    var identity: Dictionary = identities[0]
    var icon: Dictionary = icons[0]
    if identity.opcode != 42 or icon.opcode != 42 or identity.end != icon.offset: return []
    if identity.args[1].kind != icon.args[1].kind or identity.args[1].value != icon.args[1].value: return []
    if identity.args[0].kind != "memory" or icon.args[0].kind != "memory": return []
    var indices := _values(identity.offset,identity.args[1])
    if indices.is_empty(): return []
    var separation: int = absi(int(identity.args[0].value)-int(icon.args[0].value))
    for index in indices:
        if index < 0 or index >= separation: return []
    # A table write anywhere in this record could invalidate the extracted bytes.
    for other in nodes.values():
        if other.opcode == 53: return []
        var destination := _destination(other)
        if destination >= ORIGIN and destination < ORIGIN + bytes.size(): return []
    var result: Array = []
    for index in indices:
        var name_offset: int = int(identity.args[0].value) - ORIGIN + index
        var icon_offset: int = int(icon.args[0].value) - ORIGIN + index
        if name_offset < 0 or icon_offset < 0 or name_offset >= bytes.size() or icon_offset >= bytes.size(): return []
        for instruction in nodes.values():
            if (name_offset >= instruction.offset and name_offset < instruction.end) or (icon_offset >= instruction.offset and icon_offset < instruction.end): return []
        result.append({"monster_id":int(bytes[name_offset]),"icon_id":int(bytes[icon_offset]),"index":index,
            "monster_table":identity.args[0].value,"icon_table":icon.args[0].value,
            "lookup_addresses":[identity.address,icon.address]})
    return result
