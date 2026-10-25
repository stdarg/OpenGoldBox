"""A small ECL assembler for DOS probe experiments.

A program is a list of instructions and labels. An instruction is a tuple of an
opcode name and its operands; a label is a string. Operands:

    imm(v)   a one-byte immediate (tag 0)
    word(v)  a two-byte immediate (tag 2)
    var(a)   a script address, read as a variable (tag 1)
    to(l)    the address of label l (tag 1)
"""

OPCODES = {
    'EXIT': 0x00, 'GOTO': 0x01, 'GOSUB': 0x02, 'COMPARE': 0x03, 'ADD': 0x04,
    'SUBTRACT': 0x05, 'RANDOM': 0x08, 'SAVE': 0x09, 'RETURN': 0x13,
    'IF=': 0x16, 'IF<>': 0x17, 'IF<': 0x18, 'IF>': 0x19, 'IF<=': 0x1A, 'IF>=': 0x1B,
    'ON GOTO': 0x25, 'GETTABLE': 0x2A, 'AND': 0x2F, 'OR': 0x30, 'SAVE TABLE': 0x35,
}


def imm(value):
    return ('imm', value)


def word(value):
    return ('word', value)


def var(address):
    return ('var', address)


def to(label):
    return ('to', label)


def _operand_size(operand):
    return 2 if operand[0] == 'imm' else 3


def assemble(origin, program):
    """Returns the program's bytes when placed at script address `origin`."""
    labels, address = {}, origin
    for item in program:
        if isinstance(item, str):
            labels[item] = address
        else:
            address += 1 + sum(_operand_size(operand) for operand in item[1:])
    code = bytearray()
    for item in program:
        if isinstance(item, str):
            continue
        code.append(OPCODES[item[0]])
        for kind, value in item[1:]:
            if kind == 'imm':
                code += bytes([0, value])
            elif kind == 'word':
                code += bytes([2, value & 0xFF, value >> 8])
            else:
                target = labels[value] if kind == 'to' else value
                code += bytes([1, target & 0xFF, target >> 8])
    return bytes(code)
