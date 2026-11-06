"""Grows wilderness fields in the original and in OpenGoldBox with the same dice,
and compares them tile for tile.

    python3 wilderness_field.py PROBE MAPS_TOOL GAME_DIR OUTPUT_DIR

PROBE is opengold_dos_probe and MAPS_TOOL opengold_maps. Each case starts a
fight on a central wilderness square as wilderness.py does, and logs every
instruction of the step that starts it. The original grows the field in code
segment 2890 of START.EXE, which rolls each die with a far call to 08C0:0048
(one die of the size it pushes); the number each call returns is read from AX
when the call comes back. Those numbers, in order, are OpenGoldBox's dice for
the same square's overland tile (`opengold_maps --wild-field`), from the
first. The two fields must have the same art frame and blocking on every
square, and OpenGoldBox must use every die the original rolled. Exits 1 if
any case differs.
"""

import json
import pathlib
import subprocess
import sys

import wilderness as w
from battlefield import party_keys
from eclasm import assemble, imm, var

# START.EXE's field builder and its dice routine (PC 1.3, as recorded in
# dev_tools/docs/dos-probe.md).
BUILDER, DICE = '2890', ('08c0', '0048')
# The central wilderness window's first world column (window_start[1]).
CENTRAL_COLUMN = 13
# Enough instructions for the step to grow the field.
STEP_INSTRUCTIONS = 7_000_000
# The wilderness experiment's squares, and squares of the terrain that changes
# the field most, by window square: a road (tiles C0 and C8), a ridge (AD), a
# ridge on rough ground (B2), and the terrain flags 0x44, 0x48, 0x03 and 0x02.
SQUARES = sorted({case[0] for case in w.CASES}) + [(20, 26), (22, 20), (14, 28), (28, 26),
                                                   (24, 2), (26, 8), (0, 2), (6, 6)]


def dice_of(log):
    """The numbers the builder's dice calls returned, in order."""
    dice, previous, waiting = [], None, False
    with open(log) as lines:
        for line in lines:
            cs, ip = line[0:4], line[5:9]
            if previous and previous[0] == BUILDER and (cs, ip) == DICE:
                waiting = True
            elif waiting and cs == BUILDER and previous[0] != BUILDER:
                dice.append(int(line.split('ax=')[1][:4], 16))
                waiting = False
            previous = (cs, ip)
    return dice


def run_case(number, square, tools, output):
    wx, wy = square
    folder = output / f'field{number}'
    folder.mkdir(parents=True, exist_ok=True)
    leave = assemble(w.TOWN_STEP, [
        ('SAVE', imm(wx), var(0x49C3)), ('SAVE', imm(wy), var(0x49C4)),
        ('SAVE', imm(7), var(0x6E12)), ('NEW ECL', imm(w.WILD_SCRIPT)), ('EXIT',)])
    fight = assemble(w.WILD_STEP, [
        ('SETUP MONSTER', imm(w.LIZARD_MAN), imm(2), imm(w.LIZARD_MAN)),
        ('LOAD MONSTER', imm(w.LIZARD_MAN), imm(4), imm(w.LIZARD_MAN)), ('COMBAT',), ('EXIT',)])
    lines = party_keys(1) + ['locate', f'poke {w.TOWN_STEP:X} {leave.hex()}', 'key up',
                             f'poke {w.WILD_ARRIVAL:X} 00', f'poke {w.WILD_STEP:X} {fight.hex()}',
                             'press up', f'cpu {STEP_INSTRUCTIONS}', 'dump memory']
    (folder / 'scenario.txt').write_text('\n'.join(lines) + '\n')
    subprocess.run([tools['probe'], '--fresh', tools['game'], str(folder / 'scenario.txt'),
                    str(folder)], check=True, stdout=subprocess.DEVNULL)
    memory = (folder / 'memory.bin').read_bytes()
    offset, segment = (memory[w.FIELD_POINTER + n] | memory[w.FIELD_POINTER + n + 1] << 8
                       for n in (0, 2))
    (folder / 'memory.bin').unlink()
    if not segment and not offset:
        print(f'FAIL ({wx},{wy}): the step grew no field')
        return False
    field = segment * 16 + offset + 7
    (folder / 'field.bin').write_bytes(memory[field:field + 1250])
    dice = dice_of(folder / 'cpu.log')
    (folder / 'cpu.log').unlink()
    if not dice:
        print(f'FAIL ({wx},{wy}): the step rolled no field (a square at the edge of the '
              f'map leads into the next wilderness)')
        return False
    (folder / 'dice.txt').write_text('\n'.join(map(str, dice)) + '\n')

    tile = int(subprocess.run([tools['maps'], tools['game'], '--wild-tile',
                               str(wx + CENTRAL_COLUMN), str(wy)],
                              check=True, capture_output=True, text=True).stdout)
    original = json.loads(subprocess.run(
        [tools['maps'], tools['game'], '--wild-frames', str(folder / 'field.bin')],
        check=True, capture_output=True, text=True).stdout)
    grown = subprocess.run([tools['maps'], tools['game'], '--wild-field', str(tile),
                            str(folder / 'dice.txt'), '0'], capture_output=True, text=True)
    if grown.returncode:
        print(f'FAIL ({wx},{wy}) tile {tile}: {grown.stderr.strip()}')
        return False
    ours = json.loads(grown.stdout)
    differing = [n for n in range(1250) if (ours['frames'][n], ours['blocked'][n]) !=
                 (original['frames'][n], original['blocked'][n])]
    same = not differing and len(ours['sides']) == len(dice)
    print(f"{'ok  ' if same else 'FAIL'} ({wx},{wy}) tile {tile}: {len(dice)} dice, "
          f"{len(ours['sides'])} used, {1250 - len(differing)} of 1250 squares the same")
    if differing:
        print('     first differing squares (row, column, original, OpenGoldBox):',
              [(n // 50, n % 50, original['frames'][n], ours['frames'][n])
               for n in differing[:8]])
    return same


def main():
    arguments = sys.argv[1:]
    if len(arguments) != 4:
        print(__doc__)
        return 2
    tools = {'probe': arguments[0], 'maps': arguments[1], 'game': arguments[2]}
    output = pathlib.Path(arguments[3])
    results = [run_case(n, square, tools, output) for n, square in enumerate(SQUARES)]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
