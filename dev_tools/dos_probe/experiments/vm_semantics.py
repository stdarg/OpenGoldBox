"""Runs ECL experiments in the original game through the DOS probe and checks
the results recorded in dev_tools/docs/dos-probe.md.

    python3 vm_semantics.py PROBE GAME_DIR OUTPUT_DIR

PROBE is the opengold_dos_probe executable. The experiment program replaces
New Phlan's movement entry after Rolf's tour; one step east runs it, and its
results are read from the script trace. Exits 1 if a result differs.
"""

import pathlib
import subprocess
import sys

from eclasm import assemble, imm, to, var, word

HERE = pathlib.Path(__file__).resolve().parent
ROUNDS = 3000

PROGRAM = [
    ('SAVE', imm(0), var(0x4BF1)), ('SAVE', imm(0), var(0x4BF2)),
    ('SAVE', imm(0), var(0x4BF3)), ('SAVE', imm(0), var(0x4BF4)),
    # GETTABLE after an equal COMPARE: does IF = still hold?
    ('COMPARE', imm(1), imm(1)), ('GETTABLE', var(0x9900), imm(0), var(0x4BF0)),
    ('IF=',), ('SAVE', imm(1), var(0x4BF1)), ('IF<>',), ('SAVE', imm(1), var(0x4BF2)),
    # After COMPARE 2, 1: IF > (2 > 1) should hold, IF < not.
    ('COMPARE', imm(2), imm(1)), ('GETTABLE', var(0x9900), imm(0), var(0x4BF0)),
    ('IF>',), ('SAVE', imm(1), var(0x4BF3)), ('IF<',), ('SAVE', imm(1), var(0x4BF4)),
    # Dice: counts of RANDOM 255 giving 255 and 254, RANDOM 8 giving 8 and
    # more, RANDOM 0 giving more than 0, and RANDOM 1 giving 1.
    *[('SAVE', imm(0), var(cell)) for cell in range(0x4BE0, 0x4BE9)],
    'roll',
    ('RANDOM', imm(255), var(0x4BE9)),
    ('COMPARE', var(0x4BE9), imm(255)), ('IF=',), ('ADD', var(0x4BE0), imm(1), var(0x4BE0)),
    ('COMPARE', var(0x4BE9), imm(254)), ('IF=',), ('ADD', var(0x4BE1), imm(1), var(0x4BE1)),
    ('RANDOM', imm(8), var(0x4BEA)),
    ('COMPARE', var(0x4BEA), imm(8)), ('IF=',), ('ADD', var(0x4BE2), imm(1), var(0x4BE2)),
    ('COMPARE', var(0x4BEA), imm(8)), ('IF>',), ('ADD', var(0x4BE3), imm(1), var(0x4BE3)),
    ('RANDOM', imm(0), var(0x4BEB)),
    ('COMPARE', var(0x4BEB), imm(0)), ('IF>',), ('ADD', var(0x4BE4), imm(1), var(0x4BE4)),
    ('RANDOM', imm(1), var(0x4BEC)),
    ('COMPARE', var(0x4BEC), imm(1)), ('IF=',), ('ADD', var(0x4BE5), imm(1), var(0x4BE5)),
    ('ADD', var(0x4BE8), imm(1), var(0x4BE8)),
    ('COMPARE', var(0x4BE8), word(ROUNDS)), ('IF<',), ('GOTO', to('roll')),
    ('SAVE', imm(1), var(0x4BEF)),
    ('EXIT',),
]

# Each cell's expected final value: a number, or a test of it.
EXPECTED = {
    0x4BF1: (1, 'GETTABLE keeps an equal COMPARE: IF = holds'),
    0x4BF2: (0, 'GETTABLE keeps an equal COMPARE: IF <> fails'),
    0x4BF3: (1, 'GETTABLE keeps COMPARE 2, 1: IF > holds'),
    0x4BF4: (0, 'GETTABLE keeps COMPARE 2, 1: IF < fails'),
    0x4BE0: (0, 'RANDOM 255 never gives 255'),
    0x4BE1: (lambda n: n > 0, 'RANDOM 255 gives 254'),
    0x4BE2: (lambda n: n > 0, 'RANDOM 8 gives 8: the bound is inclusive'),
    0x4BE3: (0, 'RANDOM 8 never gives more than 8'),
    0x4BE4: (0, 'RANDOM 0 always gives 0'),
    0x4BE5: (lambda n: n > 0, 'RANDOM 1 gives 1'),
    0x4BEF: (1, f'all {ROUNDS} rounds ran'),
}


def scenario(code):
    lines = (HERE / 'fresh_party.txt').read_text().splitlines()
    lines += [
        'locate',
        # Entry 0, the movement entry, jumps to the experiment at 9914.
        'poke 9900 01011499',
        'poke 9914 ' + code.hex(),
        'key right', 'key right',
        'trace vm',
        'key up',
        'untrace',
    ]
    return '\n'.join(lines) + '\n'


def final_values(trace):
    values = {}
    for line in trace.read_text().splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[0] == 'W':
            values[int(fields[1], 16)] = int(fields[2], 16)
    return values


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    probe, game, output = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3])
    output.mkdir(parents=True, exist_ok=True)
    scenario_file = output / 'vm_semantics.txt'
    scenario_file.write_text(scenario(assemble(0x9914, PROGRAM)))
    subprocess.run([probe, '--fresh', game, str(scenario_file), str(output)], check=True)
    values = final_values(output / 'vm.trace')
    failed = False
    for cell, (expected, meaning) in EXPECTED.items():
        value = values.get(cell)
        ok = value is not None and (expected(value) if callable(expected) else value == expected)
        failed |= not ok
        print(f"{'ok  ' if ok else 'FAIL'} {cell:04X} = {value}: {meaning}")
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
