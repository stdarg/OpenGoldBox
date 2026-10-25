"""Plans the keys that walk the original's party between two squares of an
area, over the open edges of OpenGoldBox's map export:

    python3 route.py MAPS_JSON ARCHIVE RECORD X Y FACING TO_X TO_Y

MAPS_JSON is `opengold_maps GAME_DIR` output. FACING is N, E, S or W. Prints
scenario `key` lines: Up steps forward, Left and Right turn, Down turns around.
"""

import collections
import json
import sys

STEPS = [(0, -1), (1, 0), (0, 1), (-1, 0)]  # north, east, south, west
TURNS = {1: ['right'], 2: ['down'], 3: ['left']}


def blocked(cell, side):
    wall, door = cell['walls'][side], cell['doors'][side]
    return door > 1 or (wall and not door)


def route(cells, start, goal):
    previous = {start: None}
    queue = collections.deque([start])
    while queue:
        x, y = queue.popleft()
        if (x, y) == goal:
            break
        for side, (dx, dy) in enumerate(STEPS):
            nx, ny = x + dx, y + dy
            if not (0 <= nx < 16 and 0 <= ny < 16) or (nx, ny) in previous:
                continue
            if blocked(cells[y * 16 + x], side) or blocked(cells[ny * 16 + nx], (side + 2) % 4):
                continue
            previous[(nx, ny)] = (x, y)
            queue.append((nx, ny))
    if goal not in previous:
        raise SystemExit(f'No open way to {goal}')
    path = [goal]
    while previous[path[-1]] is not None:
        path.append(previous[path[-1]])
    return path[::-1]


def keys(path, facing):
    for (x, y), (nx, ny) in zip(path, path[1:]):
        side = STEPS.index((nx - x, ny - y))
        yield from TURNS.get((side - facing) % 4, [])
        facing = side
        yield 'up'


def main():
    if len(sys.argv) != 9:
        print(__doc__)
        return 2
    maps = json.load(open(sys.argv[1]))
    area = next(m for m in maps['maps'] if m['archive'] == sys.argv[2] and m['id'] == int(sys.argv[3]))
    start = (int(sys.argv[4]), int(sys.argv[5]))
    facing = 'NESW'.index(sys.argv[6])
    for key in keys(route(area['cells'], start, (int(sys.argv[7]), int(sys.argv[8]))), facing):
        print('key', key)
    return 0


if __name__ == '__main__':
    sys.exit(main())
