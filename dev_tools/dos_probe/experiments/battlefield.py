"""Starts fights in the original's Slums and compares where both sides stand
with OpenGoldBox's formation (PLACE-1 in docs/SRD-DECISIONS.md).

    python3 battlefield.py PROBE MAPS_TOOL GAME_DIR OUTPUT_DIR

PROBE is opengold_dos_probe and MAPS_TOOL opengold_maps. For each case a new
party walks from New Phlan into the Slums, whose step entries are replaced:
no roaming monsters come, and on the case's square and facing a program
stages orcs at the case's distance and starts the fight. The first square the original writes for
each combatant into its combat occupancy map gives each side's squares; for a
monster whose art fills more than one square (FOOT-1) the squares written
right after it are the rest of its footprint, compared with the one
OpenGoldBox gives it.
Exits 1 if any case differs.
"""

import json
import pathlib
import subprocess
import sys

from eclasm import assemble, imm, to, var
from route import blocked, keys as route_keys, route

HERE = pathlib.Path(__file__).resolve().parent
SLUMS = ('GEO2.DAX', 20)
# The Slums script's entries 0 (run before a step moves the party) and 1 (on
# the new square), and 2, as `opengold_scripts --list` gives them. The
# original reads entry addresses when it loads a script, so the program goes
# at entry 1's code and an EXIT at entry 0's, which would otherwise roll
# roaming monsters.
ENTRY_MOVE, ENTRY_SEARCH, ENTRY_NEXT = 0x9914, 0x9975, 0x9A0E
# Where the original keeps the combat occupancy map (50 by 25 bytes: 0, or a
# combatant's number), found by moving a combatant square by square
# (dev_tools/docs/dos-probe.md).
OCCUPANCY = 0x1A99B
FACINGS = 'NESW'

# Each case: square, the last step's direction (the party's facing), the
# distance SETUP MONSTER asks for, party size and orcs.
CASES = [
    ((14, 5), 'S', 2, 1, 1),
    ((14, 5), 'S', 1, 3, 4),
    ((13, 1), 'W', 2, 3, 4),
    ((13, 1), 'W', 0, 2, 3),
    ((10, 4), 'E', 1, 4, 6),
    ((13, 4), 'N', 2, 6, 12),
    ((10, 8), 'E', 2, 5, 8),
    ((12, 8), 'E', 0, 3, 5),
    ((13, 7), 'W', 1, 4, 10),
    ((9, 8), 'S', 0, 1, 16),
    ((13, 4), 'N', 2, 1, 30),
    ((13, 4), 'N', 2, 6, 20),
    ((9, 8), 'S', 0, 6, 20),
    ((10, 8), 'E', 2, 1, 30),
    ((13, 7), 'W', 1, 1, 30),
]

# Monsters whose art fills two squares (FOOT-1), as MON2CHA records with the
# footprint byte 108 of each gives: gnolls and trolls one above the other,
# basilisks side by side. Each case: square, facing, distance, party size,
# record and how many.
LARGE_CASES = [
    ((13, 4), 'N', 2, 2, 73, 3),
    ((13, 4), 'N', 2, 2, 26, 3),
    ((13, 4), 'N', 2, 4, 31, 4),
    ((10, 8), 'E', 2, 3, 73, 6),
    ((9, 8), 'S', 0, 2, 26, 5),
    ((13, 4), 'N', 1, 6, 73, 12),
    ((13, 7), 'W', 1, 4, 26, 10),
    ((12, 8), 'E', 0, 3, 31, 8),
]
FOOTPRINTS = {4: (1, 1), 73: (1, 2), 31: (1, 2), 26: (2, 1)}

# Opcodes the experiment adds to eclasm's.
SETUP_MONSTER, LOAD_MONSTER, COMBAT = 0x0C, 0x0B, 0x24


def party_keys(size):
    """Creates `size` dwarf fighters, adds them, and begins the adventure."""
    lines = ['wait', 'key enter']
    for n in range(size):
        lines += ['key c'] + ['key enter'] * 4 + ['key y', f'type PROBE{"ABCDEF"[n]}',
                                                  'key enter', 'key k', 'key e', 'key y']
    lines.append('key a')
    for n in range(size):
        lines += (['key end'] if n else []) + ['key a']
    # The list closes by itself once the party is full.
    lines += (['key e'] if size < 6 else []) + ['key b'] + ['key enter'] * 10
    return lines


def program(square, facing, distance, orcs, record=4):
    """Stages the fight when the party stands on `square` facing `facing`
    (0 north to 3 west, as the original's 0xC04D holds it), with `orcs` of
    MON2CHA's `record` (4, the orc, unless a case names another)."""
    (x, y) = square
    code = [
        ('COMPARE', var(0xC04B), imm(x)), ('IF<>',), ('GOTO', to('done')),
        ('COMPARE', var(0xC04C), imm(y)), ('IF<>',), ('GOTO', to('done')),
        ('COMPARE', var(0xC04D), imm(facing)), ('IF<>',), ('GOTO', to('done')),
        ('SETUP MONSTER', imm(record), imm(distance), imm(record)),
        ('LOAD MONSTER', imm(record), imm(orcs), imm(record)),
        ('COMBAT',),
        'done', ('EXIT',),
    ]
    return code


def assemble_program(origin, code):
    import eclasm
    eclasm.OPCODES.update({'SETUP MONSTER': SETUP_MONSTER, 'LOAD MONSTER': LOAD_MONSTER,
                           'COMBAT': COMBAT})
    return assemble(origin, code)


def open_squares_ahead(cells, x, y, facing):
    steps = [(0, -1), (1, 0), (0, 1), (-1, 0)]
    squares = 0
    while squares < 2 and not cells[y * 16 + x]['walls'][facing]:
        squares += 1
        x, y = (x + steps[facing][0]) % 16, (y + steps[facing][1]) % 16
    return squares


def run_case(number, case, tools, cells, output, record=4):
    square, facing_letter, distance, party, orcs = case
    width, height = FOOTPRINTS[record]
    facing = FACINGS.index(facing_letter)
    folder = output / f'case{number}'
    # From the Slums gate (15,4) to the square before the case's square, then
    # the program goes in and the last step, in the case's direction, runs it.
    steps = [(0, -1), (1, 0), (0, 1), (-1, 0)]
    before = (square[0] - steps[facing][0], square[1] - steps[facing][1])
    path = route(cells, (15, 4), before) + [square]
    if blocked(cells[before[1] * 16 + before[0]], facing) or \
            blocked(cells[square[1] * 16 + square[0]], (facing + 2) % 4):
        raise SystemExit(f'Case {number}: no open edge into {square} facing {facing_letter}')
    keys = list(route_keys(path, FACINGS.index('W')))
    code = assemble_program(ENTRY_SEARCH, program(square, facing, distance, orcs, record))
    assert ENTRY_SEARCH + len(code) <= ENTRY_NEXT
    lines = party_keys(party) + ['key up', 'locate', f'poke {ENTRY_MOVE:X} 00',
                                 f'poke {ENTRY_SEARCH:X} {code.hex()}']
    # Monsters may act before the party's first turn, so each combatant's
    # square is the first the original writes into the occupancy map for it.
    lines += [f'key {key}' for key in keys[:-1]]
    lines += [f'watch {OCCUPANCY:X} {OCCUPANCY + 1249:X} occupancy', f'key {keys[-1]}',
              'unwatch', 'shot start', 'key enter', 'shot combat']
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'scenario.txt').write_text('\n'.join(lines) + '\n')
    subprocess.run([tools['probe'], '--fresh', tools['game'], str(folder / 'scenario.txt'),
                    str(folder)], check=True, stdout=subprocess.DEVNULL)

    # Each combatant's first square written is its top-left one; the squares
    # written for it right after are the rest of its footprint.
    standing, squares, last = {}, {}, None
    for line in (folder / 'occupancy.watch').read_text().splitlines():
        address, who = (int(field, 16) for field in line.split()[:2])
        cell = ((address - OCCUPANCY) % 50, (address - OCCUPANCY) // 50)
        if who and who not in standing:
            standing[who] = cell
            squares[who] = [cell]
        elif who and who == last and len(squares[who]) < width * height:
            squares[who].append(cell)
        last = who
    original_party = [standing.get(n + 1) for n in range(party)]
    original_orcs = sorted(cell for who, cell in standing.items() if who > party)
    filled = all(sorted(squares[who]) == [(x + dx, y + dy) for dx in range(width)
                                          for dy in range(height)]
                 for who, (x, y) in standing.items() if who > party)

    capped = min(distance, open_squares_ahead(cells, square[0], square[1], facing))
    expected = json.loads(subprocess.run(
        [tools['maps'], tools['game'], '--formation', SLUMS[0], str(SLUMS[1]), str(square[0]),
         str(square[1]), str(facing), str(capped), str(party), f'{orcs}:{width}x{height}'],
        check=True, capture_output=True, text=True).stdout)
    expected_party = [tuple(c) if c else None for c in expected['party']]
    expected_orcs = sorted(tuple(c) for c in expected['enemies'] if c)
    same = original_party == expected_party and original_orcs == expected_orcs and filled
    print(f"{'ok  ' if same else 'FAIL'} {square} facing {facing_letter}, distance {distance} "
          f"(seen {capped}), party {party}, {orcs} of record {record} ({width}x{height})")
    if not same:
        print(f'     original party {original_party} orcs {original_orcs}')
        print(f'     OpenGoldBox party {expected_party} orcs {expected_orcs}')
    return same


def main():
    arguments = sys.argv[1:]
    if len(arguments) != 4:
        print(__doc__)
        return 2
    tools = {'probe': arguments[0], 'maps': arguments[1], 'game': arguments[2]}
    output = pathlib.Path(arguments[3])
    maps = json.loads(subprocess.run([tools['maps'], tools['game']], check=True,
                                     capture_output=True, text=True).stdout)
    cells = next(m for m in maps['maps']
                 if m['archive'] == SLUMS[0] and m['id'] == SLUMS[1])['cells']
    results = [run_case(n, case, tools, cells, output) for n, case in enumerate(CASES)]
    results += [run_case(len(CASES) + n, case[:4] + case[5:], tools, cells, output, case[4])
                for n, case in enumerate(LARGE_CASES)]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
