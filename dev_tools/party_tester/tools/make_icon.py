import math, struct, zlib, sys
N = 64          # design grid
SCALE = 8       # 512 x 512 output
px = [[(0, 0, 0, 0) for _ in range(N)] for _ in range(N)]

def put(x, y, c):
    if 0 <= x < N and 0 <= y < N:
        px[y][x] = c

def rounded(x0, y0, x1, y1, r, colour_at):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            cx = min(max(x, x0 + r), x1 - r)
            cy = min(max(y, y0 + r), y1 - r)
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r + r * 0.6:
                put(x, y, colour_at(x, y))

def poly(points, c):
    ys = [p[1] for p in points]
    for y in range(int(min(ys)), int(max(ys)) + 1):
        xs = []
        for i in range(len(points)):
            (ax, ay), (bx, by) = points[i], points[(i + 1) % len(points)]
            if (ay <= y + 0.5 < by) or (by <= y + 0.5 < ay):
                xs.append(ax + (y + 0.5 - ay) * (bx - ax) / (by - ay))
        xs.sort()
        for a, b in zip(xs[::2], xs[1::2]):
            for x in range(int(math.ceil(a - 0.5)), int(math.floor(b - 0.5)) + 1):
                put(x, y, c)

def line(ax, ay, bx, by, width, c):
    dx, dy = bx - ax, by - ay
    length = math.hypot(dx, dy)
    nx, ny = -dy / length * width / 2, dx / length * width / 2
    poly([(ax + nx, ay + ny), (bx + nx, by + ny), (bx - nx, by - ny), (ax - nx, ay - ny)], c)

OUTLINE = (34, 22, 8, 255)
GOLD = (233, 182, 54, 255)
GOLD_DARK = (168, 112, 24, 255)
GOLD_LIGHT = (255, 228, 120, 255)
STEEL = (214, 224, 236, 255)
STEEL_DARK = (128, 142, 160, 255)
GRIP = (110, 62, 30, 255)

# Badge: outline, gold rim, green field lighter at the top.
rounded(2, 2, 61, 61, 12, lambda x, y: OUTLINE)
rounded(3, 3, 60, 60, 11, lambda x, y: GOLD if y < 32 else GOLD_DARK)
rounded(6, 6, 57, 57, 9, lambda x, y: (24 + y // 6, 104 - y, 64 - y // 3, 255))
# A faint grid of battle squares on the field.
for y in range(8, 56):
    for x in range(8, 56):
        if (x % 8 == 0 or y % 8 == 0) and px[y][x][3] and px[y][x][1] < 120:
            r, g, b, a = px[y][x]
            put(x, y, (r + 10, g + 14, b + 10, 255))

def sword(flip):
    f = (lambda x: 63 - x) if flip else (lambda x: x)
    # blade with an outline and a bright edge
    line(f(17), 47, f(47), 17, 6, OUTLINE)
    line(f(18), 46, f(47), 17, 4, STEEL_DARK)
    line(f(18), 45.5, f(46.5), 17.5, 1.6, STEEL)
    poly([(f(45), 15), (f(50), 14), (f(49), 19)], OUTLINE)
    poly([(f(46), 16), (f(49), 15), (f(48.5), 18)], STEEL)
    # crossguard, grip and pommel
    line(f(11), 43, f(21), 53, 5, OUTLINE)
    line(f(12), 44, f(20), 52, 3, GOLD)
    line(f(10), 54, f(16), 48, 5, OUTLINE)
    line(f(11), 53, f(15.5), 48.5, 3, GRIP)
    rounded(int(f(9) if not flip else f(9) - 3), 53, int(f(9) if not flip else f(9) - 3) + 3, 56, 1,
            lambda x, y: GOLD_LIGHT if (x + y) % 3 == 0 else GOLD)

sword(False)
sword(True)

# Level-up arrow above the swords' crossing.
arrow = [(32, 8), (44, 21), (37, 21), (37, 31), (27, 31), (27, 21), (20, 21)]
poly([(x + (1 if x > 32 else -1 if x < 32 else 0), y + (-1 if y == 8 else 1 if y == 31 else 0))
      for x, y in arrow], OUTLINE)
poly(arrow, GOLD)
poly([(32, 11), (39, 19), (34.5, 19), (34.5, 29), (32, 29)], GOLD_LIGHT)

raw = bytearray()
for y in range(N * SCALE):
    raw.append(0)
    row = px[y // SCALE]
    for x in range(N * SCALE):
        raw.extend(row[x // SCALE])
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
size = N * SCALE
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) \
      + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
open(sys.argv[1], "wb").write(png)
