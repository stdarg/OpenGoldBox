extends RefCounted
## Pool of Radiance ECL inspection, not a game interpreter.
## Format reference: Gold Box Explorer, pinned source in docs/asset-source-audit.md.
## Decode only reachable instructions; never search arbitrary bytes for opcodes.

const ORIGIN := 0x9900
const COUNTS := [0,1,1,2,3,3,3,3,2,2,1,3,3,0,1,2,
    2,1,1,0,4,3,0,0,0,0,0,0,0,1,6,2,
    1,3,2,4,0,2,2,8,3,14,3,2,6,1,5,3,
    3,0,1,0,1,3,2,3,1,1,0,3,1,0]
const NAMES := {0:"EXIT",1:"GOTO",2:"GOSUB",3:"COMPARE",9:"SAVE",
    11:"LOAD MONSTER",12:"SETUP MONSTER",14:"PICTURE",17:"PRINT",
    18:"PRINTCLEAR",19:"RETURN",21:"VERTICAL MENU",28:"CLEAR MONSTERS",
    32:"NEW ECL",33:"LOAD FILES",36:"COMBAT",37:"ON GOTO",38:"ON GOSUB",
    41:"ENCOUNTER MENU",43:"HORIZONTAL MENU",45:"CALL",49:"SPRITE OFF",51:"PRINT RETURN",54:"ADD NPC"}

var data := PackedByteArray()
var error := ""

static func unpack_text(bytes: PackedByteArray) -> String:
    var text := ""
    var buffer := 0
    var bits := 0
    for byte in bytes:
        buffer = (buffer << 8) | byte
        bits += 8
        while bits >= 6:
            bits -= 6
            var code := (buffer >> bits) & 63
            if code != 0: text += String.chr(code + 64 if code < 32 else code)
        buffer &= (1 << bits) - 1
    return text

func _operand(position: int) -> Dictionary:
    if position + 2 > data.size():
        error = "Truncated operand"
        return {}
    var code := int(data[position])
    var value := int(data[position + 1])
    var end := position + 2
    var result := {"code":code, "value":value, "kind":"constant", "end":end}
    if code in [1,2,3,129]:
        if end >= data.size():
            error = "Truncated word operand"
            return {}
        value |= int(data[end]) << 8
        result.value = value
        result.end = end + 1
        if code != 2: result.kind = "string_reference" if code == 129 else "memory"
    elif code == 128:
        if end + value > data.size():
            error = "Truncated packed text"
            return {}
        result.kind = "text"
        result.text = unpack_text(data.slice(end, end + value))
        result.end = end + value
    elif code != 0:
        error = "Unsupported operand type 0x%02X" % code
        return {}
    return result

func instruction(offset: int) -> Dictionary:
    error = ""
    if offset < 0 or offset >= data.size():
        error = "Instruction outside record"
        return {}
    var op := int(data[offset])
    if op >= COUNTS.size():
        error = "Unsupported opcode 0x%02X" % op
        return {}
    var args: Array = []
    var end := offset + 1
    var count: int = COUNTS[op]
    for i in range(count):
        var arg := _operand(end)
        if arg.is_empty(): return {}
        args.append(arg)
        end = arg.end
    if op in [21,37,38,43]:
        var count_arg: Dictionary = args[-1]
        if count_arg.kind != "constant" or count_arg.value > 255:
            error = "Dynamic or invalid argument list length"
            return {}
        for i in range(count_arg.value):
            var arg := _operand(end)
            if arg.is_empty(): return {}
            args.append(arg)
            end = arg.end
    return {"offset":offset, "address":ORIGIN+offset, "opcode":op,
        "command":NAMES.get(op, "OP %02X" % op), "args":args, "end":end}

func decode(bytes: PackedByteArray) -> Dictionary:
    # The record's leading two-byte length is outside the VM address space.
    data = bytes.slice(2)
    if bytes.size() < 2 or data.size() > 0x1e00:
        return {"nodes":{}, "roots":[], "diagnostics":["Invalid PoR ECL record size"], "size":data.size()}
    var nodes := {}
    var roots: Array = []
    var diagnostics: Array = []
    var cursor := 0
    # Five entry-point jump instructions: movement, search, camp checks, startup.
    for i in range(5):
        var entry := instruction(cursor)
        if entry.is_empty() or entry.opcode != 1:
            return {"nodes":{}, "roots":[], "diagnostics":["Invalid entry table at %d: %s" % [cursor,error]], "size":data.size()}
        roots.append(int(entry.args[0].value) - ORIGIN)
        cursor = entry.end
    var queue: Array = roots.duplicate()
    var occupied := {}
    var attempted := {}
    while not queue.is_empty():
        var offset: int = queue.pop_front()
        if attempted.has(offset): continue
        attempted[offset] = true
        if offset < cursor or offset >= data.size():
            diagnostics.append("Out-of-range target 0x%04X" % (offset+ORIGIN))
            continue
        var node := instruction(offset)
        if node.is_empty():
            diagnostics.append("0x%04X: %s" % [offset+ORIGIN,error])
            continue
        var overlaps := false
        for byte in range(offset, node.end):
            if occupied.has(byte): overlaps = true
        if overlaps:
            diagnostics.append("Overlapping instruction at 0x%04X" % (offset+ORIGIN))
            continue
        for byte in range(offset,node.end): occupied[byte] = offset
        var edges: Array = []
        var op: int = node.opcode
        if op in [1,2]: edges.append(int(node.args[0].value)-ORIGIN)
        if op in [37,38]:
            for arg in node.args.slice(2): edges.append(int(arg.value)-ORIGIN)
        # ON GOTO/GOSUB fall through when the selector is outside the table.
        # PRINT RETURN prints a blank line and continues; it does not pop the stack.
        if op not in [0,1,19,32]: edges.append(node.end)
        if op >= 22 and op <= 27:
            var skipped := instruction(node.end)
            if skipped.is_empty(): diagnostics.append("Cannot decode conditional skip at 0x%04X" % node.address)
            else: edges.append(skipped.end)
        node.edges = edges
        nodes[offset] = node
        queue.append_array(edges)
    return {"nodes":nodes, "roots":roots, "diagnostics":diagnostics,
        "size":data.size(), "decoded_bytes":occupied.size()}

static func operand_label(arg: Dictionary) -> String:
    if arg.kind == "text": return str(arg.text)
    if arg.kind == "constant": return str(arg.value)
    return "%s[0x%04X]" % [arg.kind,arg.value]
