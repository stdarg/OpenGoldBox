"""Starts fights on the original's wilderness map and compares where both sides
stand with OpenGoldBox's formation on the same field.

    python3 wilderness.py PROBE MAPS_TOOL GAME_DIR OUTPUT_DIR

PROBE is opengold_dos_probe and MAPS_TOOL opengold_maps. For each case a new
party is made as battlefield.py's are. On its first step in New Phlan a program
at the town script's entry 0 (ECL3 0), which runs before anything else a step
does, takes it onto the central wilderness map at the case's square,
as the city's gates do (SAVE the window column to 0x49C3 and the row to 0x49C4,
7 to 0x6E12, NEW ECL 26). The wilderness script's arrival entry is replaced by
EXIT and its step entry by a program staging lizard men at the case's distance;
the case's step then starts the fight on the field the original grows from the
square's terrain. The field (50 by 25 combat tiles, row by row) goes to
`opengold_maps --wild-formation`, and each combatant's first
square in the occupancy map is compared with OpenGoldBox's formation facing the
way the party stepped, at distance 2 (FACING_OF_STEP, WILD_DISTANCE). Later
cases stage minotaurs and displacer beasts, whose art fills two squares
(FOOT-1), and check every square they fill. The field is
random, so it is read from the original rather than grown again; it lives on
the heap, 7 bytes into the block the far pointer at DS:557E (DS 148C) names.
Exits 1 if any case differs.
"""

import json
import pathlib
import subprocess
import sys

import eclasm
from battlefield import OCCUPANCY, party_keys
from eclasm import assemble, imm, var

# The central wilderness script (ECL7 26): its step entry, run before a step
# moves the party, and its arrival entry, as `opengold_scripts --list` gives them.
WILD_SCRIPT, WILD_STEP, WILD_ARRIVAL = 26, 0x9914, 0x9AE0
# New Phlan's script (ECL3 0), where a new party starts: its step entry.
TOWN_STEP = 0x9914
# The far pointer (offset, then segment) in the data segment (148C) to the
# block holding the field; the field starts 7 bytes into it.
FIELD_POINTER = 0x148C * 16 + 0x557E
# Lizard men (MON7CHA record 57), as the central script loads them.
LIZARD_MAN = 57
# The squares MON7CHA's records fill (FOOT-1), as byte 108 of each gives:
# lizard men one, minotaurs two one above the other, displacer beasts two
# side by side.
FOOTPRINTS = {LIZARD_MAN: (1, 1), 62: (1, 2), 68: (2, 1)}

# Each case: the window square, the step's arrow key, the distance SETUP
# MONSTER asks for, party size, monsters and, past lizard men, their record.
CASES = [
    ((13, 20), 'up', 2, 1, 6),
    ((13, 20), 'right', 2, 3, 6),
    ((13, 20), 'down', 1, 4, 10),
    ((13, 20), 'left', 0, 2, 4),
    ((13, 20), 'up', 3, 6, 12),
    ((5, 10), 'up', 0, 6, 20),
    ((8, 30), 'right', 3, 5, 12),
    ((10, 15), 'down', 2, 2, 30),
    ((6, 25), 'left', 1, 3, 8),
    ((14, 33), 'up', 2, 4, 16),
    ((13, 20), 'up', 2, 3, 6, 62),
    ((13, 20), 'right', 2, 2, 8, 68),
    ((10, 15), 'down', 1, 4, 10, 62),
    ((6, 25), 'left', 2, 3, 6, 68),
]
# The original's fight faces the way the party stepped (0 north to 3 west) and
# stands the monsters as if at distance 2, whatever SETUP MONSTER asked.
FACING_OF_STEP = {'up': 0, 'right': 1, 'down': 2, 'left': 3}
WILD_DISTANCE = 2

eclasm.OPCODES.update({'NEW ECL': 0x20, 'SETUP MONSTER': 0x0C, 'LOAD MONSTER': 0x0B,
                       'COMBAT': 0x24})


def run_case(number, case, tools, output):
    (wx, wy), step, distance, party, monsters = case[:5]
    record = case[5] if len(case) > 5 else LIZARD_MAN
    width, height = FOOTPRINTS[record]
    folder = output / f'case{number}'
    leave = assemble(TOWN_STEP, [
        ('SAVE', imm(wx), var(0x49C3)), ('SAVE', imm(wy), var(0x49C4)),
        ('SAVE', imm(7), var(0x6E12)), ('NEW ECL', imm(WILD_SCRIPT)), ('EXIT',)])
    fight = assemble(WILD_STEP, [
        ('SETUP MONSTER', imm(record), imm(distance), imm(record)),
        ('LOAD MONSTER', imm(record), imm(monsters), imm(record)),
        ('COMBAT',), ('EXIT',)])
    lines = party_keys(party) + ['locate', f'poke {TOWN_STEP:X} {leave.hex()}', 'key up']
    lines += [f'poke {WILD_ARRIVAL:X} 00', f'poke {WILD_STEP:X} {fight.hex()}',
              f'watch {OCCUPANCY:X} {OCCUPANCY + 1249:X} occupancy', f'key {step}', 'unwatch',
              'shot start', 'dump memory', 'key enter', 'shot combat']
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'scenario.txt').write_text('\n'.join(lines) + '\n')
    subprocess.run([tools['probe'], '--fresh', tools['game'], str(folder / 'scenario.txt'),
                    str(folder)], check=True, stdout=subprocess.DEVNULL)

    memory = (folder / 'memory.bin').read_bytes()
    offset, segment = (memory[FIELD_POINTER + n] | memory[FIELD_POINTER + n + 1] << 8
                       for n in (0, 2))
    field = segment * 16 + offset + 7
    (folder / 'field.bin').write_bytes(memory[field:field + 1250])
    (folder / 'memory.bin').unlink()
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
    original_monsters = sorted(cell for who, cell in standing.items() if who > party)
    filled = all(sorted(squares[who]) == [(x + dx, y + dy) for dx in range(width)
                                          for dy in range(height)]
                 for who, (x, y) in standing.items() if who > party)

    facing = FACING_OF_STEP[step]
    expected = json.loads(subprocess.run(
        [tools['maps'], tools['game'], '--wild-formation', str(folder / 'field.bin'),
         str(facing), str(WILD_DISTANCE), str(party), f'{monsters}:{width}x{height}'],
        check=True, capture_output=True, text=True).stdout)
    expected_party = [tuple(c) if c else None for c in expected['party']]
    expected_monsters = sorted(tuple(c) for c in expected['enemies'] if c)
    same = (original_party == expected_party and original_monsters == expected_monsters and
            filled)
    print(f"{'ok  ' if same else 'FAIL'} ({wx},{wy}) step {step}, distance {distance}, "
          f"party {party}, {monsters} of record {record} ({width}x{height})")
    if not same:
        print(f'     original party {original_party} monsters {original_monsters}')
        print(f'     OpenGoldBox party {expected_party} monsters {expected_monsters}')
    return same


def main():
    arguments = sys.argv[1:]
    if len(arguments) != 4:
        print(__doc__)
        return 2
    tools = {'probe': arguments[0], 'maps': arguments[1], 'game': arguments[2]}
    output = pathlib.Path(arguments[3])
    results = [run_case(n, case, tools, output) for n, case in enumerate(CASES)]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
