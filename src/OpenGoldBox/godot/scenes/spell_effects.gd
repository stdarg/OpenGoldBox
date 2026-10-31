extends RefCounted

# Draws what a spell looks like (docs/DEVIATIONS.md). The native table
# (spell_effects.h) names a travel drawing t_<name> and an arrival drawing
# m_<name> for each spell. Every drawing is a pure function of its progress, so
# nothing here keeps state between frames.

var color := Color.WHITE
var accent := Color.WHITE
var amount := 1
var seed_value := 0.0

var _soft: Texture2D


# 0..1 from a seed: the same value on every frame of one effect.
func _rand(i: float) -> float:
    return fposmod(sin((i + 1.0) * 12.9898 + seed_value * 78.233) * 43758.5453, 1.0)


func _fade(t: float, rise := 0.15, fall := 0.3) -> float:
    return clampf(t / rise, 0.0, 1.0) * clampf((1.0 - t) / fall, 0.0, 1.0)


func _soft_texture() -> Texture2D:
    if _soft == null:
        var gradient := Gradient.new()
        gradient.set_color(0, Color.WHITE)
        gradient.set_color(1, Color(1, 1, 1, 0))
        var texture := GradientTexture2D.new()
        texture.gradient = gradient
        texture.width = 64
        texture.height = 64
        texture.fill = GradientTexture2D.FILL_RADIAL
        texture.fill_from = Vector2(0.5, 0.5)
        texture.fill_to = Vector2(1.0, 0.5)
        _soft = texture
    return _soft


func _glow(c: CanvasItem, at: Vector2, radius: float, tint: Color, alpha := 1.0) -> void:
    var shade := tint
    shade.a *= alpha
    c.draw_texture_rect(_soft_texture(), Rect2(at - Vector2.ONE * radius, Vector2.ONE * radius * 2.0), false, shade)


func _tint(tint: Color, alpha: float) -> Color:
    return Color(tint.r, tint.g, tint.b, tint.a * clampf(alpha, 0.0, 1.0))


func _ring(c: CanvasItem, at: Vector2, radius: float, tint: Color, width := 1.5) -> void:
    c.draw_arc(at, radius, 0.0, TAU, 40, tint, width, true)


func _star(c: CanvasItem, at: Vector2, radius: float, tint: Color) -> void:
    var inner := radius * 0.18
    var points := PackedVector2Array()
    for i in range(8):
        var r := radius if i % 2 == 0 else inner
        points.append(at + Vector2.from_angle(i * TAU / 8.0) * r)
    c.draw_colored_polygon(points, tint)


func _polygon(c: CanvasItem, at: Vector2, radius: float, sides: int, turn: float, tint: Color) -> void:
    var points := PackedVector2Array()
    for i in range(sides):
        points.append(at + Vector2.from_angle(turn + i * TAU / sides) * radius)
    c.draw_colored_polygon(points, tint)


# Where a thing is along a gently curved path from `from` to `to`.
func _along(from: Vector2, to: Vector2, p: float, arc: float) -> Vector2:
    var side := (to - from).orthogonal().normalized()
    return from.lerp(to, p) + side * sin(p * PI) * arc


# --- What flies from the caster ------------------------------------------------

# Magic Missile: a glowing arrowhead on a curving path, with a shining tail.
func t_dart(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var arc := (_rand(1.0) - 0.5) * size * 3.0
    for i in range(10, 0, -1):
        var back := p - i * 0.022
        if back > 0.0:
            var fade := 1.0 - i / 11.0
            _glow(c, _along(from, to, back, arc), size * 0.34 * fade, color, fade)
    var head := _along(from, to, p, arc)
    var heading := (head - _along(from, to, maxf(p - 0.03, 0.0), arc)).normalized()
    if heading.is_zero_approx():
        heading = (to - from).normalized()
    _glow(c, head, size * 0.7, color)
    var side := heading.orthogonal()
    var tip := head + heading * size * 0.42
    var tail := head - heading * size * 0.3
    c.draw_colored_polygon(PackedVector2Array([tip, head + side * size * 0.15, tail, head - side * size * 0.15]), color.lightened(0.35))
    c.draw_colored_polygon(PackedVector2Array([tip, head + side * size * 0.06, head - heading * size * 0.1, head - side * size * 0.06]), accent)


func t_ember(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    for i in range(8):
        var back := p - i * 0.03
        if back > 0.0:
            var drift := Vector2(_rand(i) - 0.5, _rand(i + 9.0) - 0.5) * size * 0.3 * i * 0.25
            c.draw_circle(from.lerp(to, back) + drift, size * 0.09 * (1.0 - i / 9.0), _tint(accent if i < 3 else color, 1.0 - i / 9.0))
    var head := from.lerp(to, p)
    _glow(c, head, size * 0.6, color)
    c.draw_circle(head, size * 0.16, accent)


func t_comet(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    for i in range(16, 0, -1):
        var back := p - i * 0.02
        if back > 0.0:
            _glow(c, from.lerp(to, back), size * (0.5 - i * 0.022), color, 0.8 - i * 0.045)
    var head := from.lerp(to, p)
    _glow(c, head, size * 0.9, color)
    c.draw_circle(head, size * 0.26, accent)
    c.draw_circle(head, size * 0.13, Color.WHITE)


func t_orb(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var head := _along(from, to, p, size * 0.5)
    var pulse := 0.8 + sin(p * 30.0) * 0.2
    _glow(c, head, size * 0.8 * pulse, color)
    c.draw_circle(head, size * 0.24 * pulse, _tint(color, 0.9))
    c.draw_circle(head + Vector2(-0.07, -0.07) * size, size * 0.09, accent)
    for i in range(3):
        var angle := p * 16.0 + i * TAU / 3.0
        c.draw_circle(head + Vector2.from_angle(angle) * size * 0.42, size * 0.05, accent)


func t_shard(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var head := from.lerp(to, p)
    var turn := p * 22.0
    _glow(c, head, size * 0.5, color, 0.8)
    var along := Vector2.from_angle(turn)
    var across := along.orthogonal()
    c.draw_colored_polygon(PackedVector2Array([head + along * size * 0.4, head + across * size * 0.1, head - along * size * 0.28, head - across * size * 0.1]), color)
    c.draw_line(head - along * size * 0.28, head + along * size * 0.4, accent, 1.0, true)


func t_spit(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var hop := Vector2(0.0, -sin(p * PI) * size * 1.6)
    for i in range(5):
        var back := p - i * 0.05
        if back > 0.0:
            var drop := from.lerp(to, back) + Vector2(0.0, -sin(back * PI) * size * 1.6)
            c.draw_circle(drop, size * 0.05 + (5 - i) * size * 0.012, _tint(color, 1.0 - i * 0.16))
    var head := from.lerp(to, p) + hop
    _glow(c, head, size * 0.45, color, 0.8)
    c.draw_circle(head, size * 0.17, color)
    c.draw_circle(head + Vector2(-0.05, -0.05) * size, size * 0.06, accent)


func t_mote(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var arc := size * 0.8
    for i in range(7):
        var back := p - i * 0.04
        if back > 0.0:
            _star(c, _along(from, to, back, arc) + Vector2(_rand(i) - 0.5, _rand(i + 7.0) - 0.5) * size * 0.2, size * 0.1, _tint(accent, 1.0 - i / 8.0))
    var head := _along(from, to, p, arc)
    _glow(c, head, size * 0.5, color)
    _star(c, head, size * (0.3 + sin(p * 25.0) * 0.08), color)


func t_note(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var head := from.lerp(to, p) + Vector2(0.0, sin(p * 18.0) * size * 0.25)
    _glow(c, head, size * 0.45, color, 0.7)
    _music_note(c, head, size * 0.55, color)
    _music_note(c, from.lerp(to, maxf(p - 0.12, 0.0)) + Vector2(0.0, cos(p * 18.0) * size * 0.25), size * 0.35, _tint(accent, 0.7))


func _music_note(c: CanvasItem, at: Vector2, size: float, tint: Color) -> void:
    c.draw_circle(at + Vector2(-0.18, 0.3) * size, size * 0.17, tint)
    c.draw_line(at + Vector2(-0.04, 0.3) * size, at + Vector2(-0.04, -0.4) * size, tint, maxf(size * 0.08, 1.0), true)
    c.draw_line(at + Vector2(-0.04, -0.4) * size, at + Vector2(0.3, -0.2) * size, tint, maxf(size * 0.08, 1.0), true)


# A beam reaches the target over the first half of its time and then burns out.
func _reach(p: float) -> float:
    return clampf(p * 2.0, 0.0, 1.0)


func _beam_fade(p: float) -> float:
    return clampf((1.0 - p) * 3.0, 0.0, 1.0)


func t_ray(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var end := from.lerp(to, _reach(p))
    var fade := _beam_fade(p)
    var pulse := 0.8 + sin(p * 40.0) * 0.2
    c.draw_line(from, end, _tint(color, 0.35 * fade), size * 0.34 * pulse, true)
    c.draw_line(from, end, _tint(color, 0.8 * fade), size * 0.15 * pulse, true)
    c.draw_line(from, end, _tint(accent, fade), size * 0.05, true)
    _glow(c, from, size * 0.4, color, fade)
    _glow(c, end, size * 0.55, color, fade)


func t_zigzag(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var fade := _beam_fade(p)
    var end := from.lerp(to, _reach(p))
    var frame := floorf(p * 14.0)
    for strand in range(2):
        var points := _bolt_points(from, end, 9, size * 0.35, frame + strand * 31.0)
        c.draw_polyline(points, _tint(color, fade), maxf(size * 0.07, 1.0))
        c.draw_polyline(points, _tint(accent, fade), maxf(size * 0.025, 1.0))
    _glow(c, end, size * 0.6, color, fade)


func _bolt_points(from: Vector2, to: Vector2, steps: int, wander: float, salt: float) -> PackedVector2Array:
    var points := PackedVector2Array()
    var side := (to - from).orthogonal().normalized()
    for i in range(steps + 1):
        var shift := 0.0 if i == 0 or i == steps else (_rand(i + salt) - 0.5) * 2.0 * wander
        points.append(from.lerp(to, float(i) / steps) + side * shift)
    return points


func t_helix(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var fade := _beam_fade(p)
    var reach := _reach(p)
    var side := (to - from).orthogonal().normalized()
    var length := from.distance_to(to)
    var turns := maxf(length / (size * 1.1), 1.0)
    for strand in range(2):
        var points := PackedVector2Array()
        for i in range(41):
            var along := reach * i / 40.0
            var wave := sin(along * turns * TAU + p * 26.0 + strand * PI) * size * 0.22
            points.append(from.lerp(to, along) + side * wave)
        c.draw_polyline(points, _tint(color if strand == 0 else accent, fade), maxf(size * 0.06, 1.0), true)
    _glow(c, from.lerp(to, reach), size * 0.6, color, fade)


func t_wave(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var fade := _beam_fade(p)
    var reach := _reach(p)
    for i in range(14):
        var along := reach * (i + _rand(i)) / 14.0
        var at := from.lerp(to, along) + Vector2(_rand(i + 5.0) - 0.5, sin(along * 9.0 + p * 10.0) - 0.5) * size * 0.3 * along * 1.6
        _glow(c, at, size * (0.15 + along * 0.3), color, fade * 0.6)
        c.draw_circle(at, size * 0.035, _tint(accent, fade))


func t_needle(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var head := from.lerp(to, minf(p * 1.3, 1.0))
    var back := from.lerp(to, maxf(p * 1.3 - 0.25, 0.0))
    var fade := clampf((1.0 - p) * 5.0, 0.0, 1.0)
    c.draw_line(back, head, _tint(color, 0.5 * fade), size * 0.12, true)
    c.draw_line(back, head, _tint(accent, fade), maxf(size * 0.03, 1.0), true)
    _glow(c, head, size * 0.35, color, fade)


func t_chainbeam(c: CanvasItem, from: Vector2, to: Vector2, p: float, size: float) -> void:
    var fade := _beam_fade(p)
    var reach := _reach(p)
    var links := int(maxf(from.distance_to(to) / (size * 0.28), 2.0))
    var drawn := int(links * reach)
    for i in range(drawn):
        var at := from.lerp(to, (i + 0.5) / links)
        var flat := i % 2 == 0
        var turn := (to - from).angle()
        _turned(c, at, turn, Vector2(1.0, 0.5 if flat else 0.18))
        c.draw_arc(Vector2.ZERO, size * 0.12, 0.0, TAU, 12, _tint(color, fade), maxf(size * 0.035, 1.0), true)
    _restore(c)
    _glow(c, from.lerp(to, reach), size * 0.4, accent, fade)


# The canvas draws at its zoom; a drawing that turned its pen puts it back.
var restore_transform := Transform2D.IDENTITY


func _restore(c: CanvasItem) -> void:
    c.draw_set_transform_matrix(restore_transform)


func _turned(c: CanvasItem, at: Vector2, turn: float, scale := Vector2.ONE) -> void:
    c.draw_set_transform_matrix(restore_transform * Transform2D(turn, scale, 0.0, at))


# --- What plays where the spell lands -------------------------------------------

func m_flames(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.12, 0.4)
    _glow(c, at + Vector2(0.0, r * 0.2), r * 1.2, color, fade * 0.7)
    for i in range(amount):
        var offset := (_rand(i) - 0.5) * r * 1.5
        var life := fposmod(t * (1.4 + _rand(i + 3.0)) + _rand(i + 7.0), 1.0)
        var height := r * (0.5 + _rand(i + 11.0) * 0.9)
        var base := at + Vector2(offset, r * 0.7)
        var sway := sin(t * 18.0 + i) * r * 0.1
        var tip := base + Vector2(sway, -height * (0.4 + life))
        var width := r * (0.26 - life * 0.14)
        var shape := PackedVector2Array([base + Vector2(-width, 0.0), tip, base + Vector2(width, 0.0)])
        c.draw_colored_polygon(shape, _tint(color, fade * (1.0 - life * 0.5)))
        var core := PackedVector2Array([base + Vector2(-width * 0.45, 0.0), base.lerp(tip, 0.6), base + Vector2(width * 0.45, 0.0)])
        c.draw_colored_polygon(core, _tint(accent, fade))


func m_explosion(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var grow := 1.0 - pow(1.0 - t, 3.0)
    var fade := 1.0 - t
    _glow(c, at, r * (0.6 + grow * 1.5), color, fade)
    _glow(c, at, r * (0.5 + grow * 0.7), accent, fade)
    _glow(c, at, r * (0.9 - t * 0.6), Color.WHITE, fade * fade)
    _ring(c, at, r * grow * 1.6, _tint(color, fade), maxf(r * 0.08, 1.0))
    for i in range(amount * 2):
        var dir := Vector2.from_angle(_rand(i) * TAU)
        var far := r * grow * (1.0 + _rand(i + 5.0) * 1.1)
        c.draw_circle(at + dir * far, r * 0.07 * fade, _tint(accent if i % 2 == 0 else color, fade))


func m_frost(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.45)
    var grow := minf(t * 3.0, 1.0)
    _glow(c, at, r * 1.2, color, fade * 0.5)
    for i in range(amount):
        var dir := Vector2.from_angle(i * TAU / amount + 0.3)
        var tip := at + dir * r * grow * (0.9 + _rand(i) * 0.3)
        var side := dir.orthogonal() * r * 0.11
        c.draw_colored_polygon(PackedVector2Array([at + side, tip, at - side]), _tint(color, fade))
        c.draw_line(at, tip, _tint(accent, fade), maxf(r * 0.03, 1.0), true)
        var branch := at.lerp(tip, 0.6)
        c.draw_line(branch, branch + dir.rotated(0.8) * r * 0.25 * grow, _tint(accent, fade), maxf(r * 0.03, 1.0), true)
        c.draw_line(branch, branch + dir.rotated(-0.8) * r * 0.25 * grow, _tint(accent, fade), maxf(r * 0.03, 1.0), true)


func m_strike(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var flicker := 1.0 if fposmod(t * 9.0, 1.0) < 0.65 else 0.3
    var fade := (1.0 - t) * flicker
    var frame := floorf(t * 9.0)
    for strand in range(amount):
        var start := at + Vector2((_rand(strand + frame) - 0.5) * r, -r * 3.0)
        var points := _bolt_points(start, at, 7, r * 0.35, frame + strand * 13.0)
        c.draw_polyline(points, _tint(color, fade * 0.7), maxf(r * 0.16, 1.0), true)
        c.draw_polyline(points, _tint(accent, fade), maxf(r * 0.05, 1.0), true)
    _glow(c, at, r * 1.2, color, fade)


func m_shock(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := 1.0 - t
    _glow(c, at, r * (0.6 + t), color, fade * 0.6)
    for i in range(amount):
        var ring := clampf(t * 1.6 - i * 0.18, 0.0, 1.0)
        if ring > 0.0:
            _ring(c, at, r * 1.6 * ring, _tint(color if i % 2 == 0 else accent, (1.0 - ring)), maxf(r * 0.12 * (1.0 - ring) + 1.0, 1.0))


func m_acid(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.4)
    c.draw_set_transform_matrix(restore_transform * Transform2D(0.0, Vector2(1.0, 0.55), 0.0, at + Vector2(0.0, r * 0.45)))
    c.draw_circle(Vector2.ZERO, r * minf(t * 3.0, 1.0), _tint(color, fade * 0.65))
    _restore(c)
    for i in range(amount):
        var angle := _rand(i) * TAU
        var life := fposmod(t * 1.8 + _rand(i + 4.0), 1.0)
        var hop := Vector2.from_angle(angle) * r * 0.8 * life + Vector2(0.0, -sin(life * PI) * r * 0.9 + r * 0.3)
        c.draw_circle(at + hop, r * (0.14 - life * 0.08), _tint(accent if i % 3 == 0 else color, fade))


func m_gas(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.25, 0.45)
    for i in range(amount):
        var angle := _rand(i) * TAU + t * (0.8 + _rand(i + 2.0))
        var far := r * (0.2 + _rand(i + 6.0) * 0.8) * (0.7 + t * 0.5)
        var puff := r * (0.55 + _rand(i + 8.0) * 0.5) * (0.8 + t * 0.4)
        _glow(c, at + Vector2.from_angle(angle) * far, puff, color if i % 2 == 0 else accent, fade * 0.75)


func m_radiance(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.4)
    var grow := minf(t * 2.5, 1.0)
    _glow(c, at, r * 1.5 * grow, color, fade)
    for i in range(amount):
        var dir := Vector2.from_angle(i * TAU / amount + t * 1.2)
        var length := r * grow * (1.1 if i % 2 == 0 else 1.7)
        var side := dir.orthogonal() * r * 0.07
        c.draw_colored_polygon(PackedVector2Array([at + side, at + dir * length, at - side]), _tint(color, fade))
    c.draw_circle(at, r * 0.26 * grow, _tint(accent, fade))


func m_pillar(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.45)
    var top := at + Vector2(0.0, -r * 4.0)
    var width := r * 0.8 * minf(t * 4.0, 1.0) * (0.85 + sin(t * 30.0) * 0.15)
    c.draw_colored_polygon(PackedVector2Array([top + Vector2(-width * 0.6, 0.0), top + Vector2(width * 0.6, 0.0), at + Vector2(width, r * 0.5), at + Vector2(-width, r * 0.5)]), _tint(color, fade * 0.55))
    c.draw_colored_polygon(PackedVector2Array([top + Vector2(-width * 0.2, 0.0), top + Vector2(width * 0.2, 0.0), at + Vector2(width * 0.35, r * 0.5), at + Vector2(-width * 0.35, r * 0.5)]), _tint(accent, fade * 0.9))
    _glow(c, at + Vector2(0.0, r * 0.4), r * 1.2, color, fade)
    for i in range(amount):
        var life := fposmod(t * 1.5 + _rand(i), 1.0)
        c.draw_circle(at + Vector2((_rand(i + 3.0) - 0.5) * r * 1.5, r * 0.5 - life * r * 2.5), r * 0.05, _tint(accent, fade * (1.0 - life)))


func m_wisps(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.4)
    _glow(c, at, r * 1.1, accent, fade * 0.6)
    for i in range(amount):
        var points := PackedVector2Array()
        var x := (_rand(i) - 0.5) * r * 1.5
        var phase := _rand(i + 2.0) * TAU
        for j in range(12):
            var up := j / 11.0
            points.append(at + Vector2(x + sin(up * 7.0 + phase + t * 9.0) * r * 0.22, r * 0.7 - up * r * 1.9 * minf(t * 2.2, 1.0)))
        c.draw_polyline(points, _tint(color, fade), maxf(r * 0.08, 1.0), true)


func m_skull(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    _glow(c, at, r * 1.3, color, fade * 0.7)
    for i in range(amount):
        var turn := 0.0 if amount == 1 else i * TAU / amount + t * 2.0
        var spot := at + (Vector2.ZERO if amount == 1 else Vector2.from_angle(turn) * r * 0.7)
        var size := r * (0.55 if amount == 1 else 0.3) * (0.7 + minf(t * 3.0, 1.0) * 0.5)
        var bone := _tint(Color(0.93, 0.93, 0.84), fade)
        var rise := Vector2(0.0, -t * r * 0.4)
        c.draw_circle(spot + rise, size, bone)
        c.draw_rect(Rect2(spot + rise + Vector2(-size * 0.5, size * 0.55), Vector2(size, size * 0.6)), bone)
        c.draw_circle(spot + rise + Vector2(-size * 0.4, -size * 0.05), size * 0.25, _tint(accent, fade))
        c.draw_circle(spot + rise + Vector2(size * 0.4, -size * 0.05), size * 0.25, _tint(accent, fade))
        c.draw_circle(spot + rise + Vector2(-size * 0.4, -size * 0.05), size * 0.1, _tint(color, fade))
        c.draw_circle(spot + rise + Vector2(size * 0.4, -size * 0.05), size * 0.1, _tint(color, fade))
        for tooth in range(3):
            var x := (tooth - 1) * size * 0.32
            c.draw_line(spot + rise + Vector2(x, size * 0.6), spot + rise + Vector2(x, size * 1.15), _tint(accent, fade), maxf(size * 0.08, 1.0))


func m_rings(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    for i in range(amount):
        var phase := fposmod(1.0 - t * 1.8 + float(i) / amount, 1.0)
        _ring(c, at, r * 1.5 * phase, _tint(accent if i % 2 == 0 else color, fade * (1.0 - phase * 0.5)), maxf(r * 0.08, 1.0))
    _glow(c, at, r * 0.8, color, fade * 0.5)


func m_sparkles(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.4)
    _glow(c, at, r * 1.1, color, fade * 0.35)
    for i in range(amount):
        var spot := at + Vector2(_rand(i) - 0.5, _rand(i + 3.0) - 0.5) * r * 2.2
        var twinkle := absf(sin(t * 9.0 + _rand(i + 6.0) * TAU))
        _star(c, spot, r * 0.3 * twinkle + 1.0, _tint(color if i % 2 == 0 else accent, fade))


func m_runes(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    var base := at + Vector2(0.0, r * 0.35)
    _turned(c, base, 0.0, Vector2(1.0, 0.55))
    _ring(c, Vector2.ZERO, r * 1.15, _tint(color, fade), maxf(r * 0.06, 1.0))
    _ring(c, Vector2.ZERO, r * 0.8, _tint(accent, fade * 0.8), maxf(r * 0.04, 1.0))
    for i in range(amount):
        var angle := i * TAU / amount + t * 2.4
        var mark := Vector2.from_angle(angle) * r * 0.97
        var side := Vector2.from_angle(angle + i) * r * 0.13
        c.draw_line(mark - side, mark + side, _tint(accent, fade), maxf(r * 0.06, 1.0))
        c.draw_line(mark, mark + side.orthogonal(), _tint(accent, fade), maxf(r * 0.06, 1.0))
    _restore(c)
    _glow(c, base, r * 1.0, color, fade * 0.35)


func m_ward(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.4)
    var grow := minf(t * 3.5, 1.0)
    var points := PackedVector2Array()
    for i in range(amount + 1):
        points.append(at + Vector2.from_angle(i * TAU / amount - PI * 0.5 + t * 0.6) * r * 1.2 * grow)
    c.draw_colored_polygon(points.slice(0, amount), _tint(color, fade * 0.2))
    c.draw_polyline(points, _tint(color, fade), maxf(r * 0.08, 1.0), true)
    for i in range(amount):
        c.draw_line(at, points[i], _tint(accent, fade * 0.35), 1.0, true)
    var sweep := fposmod(t * 1.8, 1.0)
    _ring(c, at, r * 1.2 * sweep, _tint(accent, fade * (1.0 - sweep)), 1.0)


func m_chains(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.3)
    var close := 1.0 - pow(1.0 - minf(t * 2.0, 1.0), 2.0)
    for band in range(2):
        var height := at + Vector2(0.0, (band - 0.5) * r * 0.8)
        var size := r * (1.7 - close * 0.7)
        for i in range(amount * 2):
            var angle := i * TAU / (amount * 2) + t * 1.5 * (1.0 if band == 0 else -1.0)
            var link := height + Vector2(cos(angle), sin(angle) * 0.35) * size
            _turned(c, link, angle + PI * 0.5, Vector2(1.0, 0.45 if i % 2 == 0 else 0.15))
            c.draw_arc(Vector2.ZERO, r * 0.18, 0.0, TAU, 10, _tint(accent if i % 2 == 0 else color, fade), maxf(r * 0.06, 1.0), true)
    _restore(c)
    _glow(c, at, r, color, fade * 0.35)


func m_vines(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.05, 0.25)
    var grow := minf(t * 2.2, 1.0)
    for i in range(amount):
        var start := at + Vector2((_rand(i) - 0.5) * r * 1.6, r * 0.8)
        var phase := _rand(i + 4.0) * TAU
        var points := PackedVector2Array()
        for j in range(12):
            var up := j / 11.0 * grow
            points.append(start + Vector2(sin(up * 8.0 + phase) * r * 0.35, -up * r * 1.9))
        c.draw_polyline(points, _tint(color, fade), maxf(r * 0.1, 1.0), true)
        for leaf in range(1, 4):
            var spot: Vector2 = points[leaf * 3 - 1]
            var lean := Vector2(0.25 if leaf % 2 == 0 else -0.25, -0.08) * r
            c.draw_colored_polygon(PackedVector2Array([spot, spot + lean + Vector2(0.0, -r * 0.1), spot + lean * 1.6, spot + lean + Vector2(0.0, r * 0.1)]), _tint(accent, fade))


func m_web(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.3)
    var grow := minf(t * 2.5, 1.0)
    var size := r * 1.3 * grow
    var spokes := PackedVector2Array()
    for i in range(amount):
        spokes.append(at + Vector2.from_angle(i * TAU / amount + _rand(i) * 0.25) * size)
        c.draw_line(at, spokes[i], _tint(color, fade), 1.0, true)
    for band in range(1, 4):
        var points := PackedVector2Array()
        for i in range(amount + 1):
            points.append(at.lerp(spokes[i % amount], band * 0.3))
        c.draw_polyline(points, _tint(accent, fade), 1.0, true)


func m_blades(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.3)
    for i in range(amount):
        var angle := i * TAU / amount + t * 16.0
        var spot := at + Vector2.from_angle(angle) * r * 0.6
        var along := Vector2.from_angle(angle + PI * 0.5)
        var side := along.orthogonal()
        c.draw_colored_polygon(PackedVector2Array([spot + along * r * 0.7, spot + side * r * 0.14, spot - along * r * 0.45, spot - side * r * 0.14]), _tint(color, fade))
        c.draw_line(spot - along * r * 0.45, spot + along * r * 0.7, _tint(accent, fade), maxf(r * 0.04, 1.0), true)
    _ring(c, at, r * 0.6, _tint(color, fade * 0.3), maxf(r * 0.2, 1.0))


func m_note(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    for i in range(amount):
        var angle := i * TAU / amount + t * 5.0
        var spot := at + Vector2(cos(angle), sin(angle) * 0.5) * r * 0.95 + Vector2(0.0, -t * r * 0.5)
        _music_note(c, spot, r * 0.55, _tint(color if i % 2 == 0 else accent, fade))


func m_smiles(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    for i in range(amount):
        var life := fposmod(t * 1.3 + float(i) / amount, 1.0)
        var spot := at + Vector2((_rand(i) - 0.5) * r * 1.6 + sin(t * 20.0 + i) * r * 0.1, r * 0.5 - life * r * 1.8)
        var size := r * 0.32
        c.draw_circle(spot, size, _tint(color, fade * (1.0 - life * 0.4)))
        c.draw_circle(spot + Vector2(-0.35, -0.25) * size, size * 0.13, _tint(Color.BLACK, fade))
        c.draw_circle(spot + Vector2(0.35, -0.25) * size, size * 0.13, _tint(Color.BLACK, fade))
        c.draw_arc(spot + Vector2(0.0, 0.05) * size, size * 0.6, 0.2, PI - 0.2, 10, _tint(accent, fade), maxf(size * 0.17, 1.0), true)


func m_hearts(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    for i in range(amount):
        var life := fposmod(t * 1.1 + float(i) / amount, 1.0)
        var spot := at + Vector2((_rand(i) - 0.5) * r * 1.6 + sin(life * 8.0 + i) * r * 0.15, r * 0.6 - life * r * 1.9)
        var size := r * 0.27 * (0.7 + life * 0.5)
        var tint := _tint(color if i % 3 else accent, fade * (1.0 - life * 0.5))
        c.draw_circle(spot + Vector2(-size * 0.5, 0.0), size * 0.55, tint)
        c.draw_circle(spot + Vector2(size * 0.5, 0.0), size * 0.55, tint)
        c.draw_colored_polygon(PackedVector2Array([spot + Vector2(-size * 1.03, size * 0.14), spot + Vector2(size * 1.03, size * 0.14), spot + Vector2(0.0, size * 1.25)]), tint)


func m_eye(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.25)
    var open := sin(minf(t * 1.5, 1.0) * PI) * 0.7 + 0.15
    for i in range(amount):
        var spot := at + (Vector2.ZERO if amount == 1 else Vector2.from_angle(i * TAU / amount + t) * r * 0.85)
        var width := r * (1.0 if amount == 1 else 0.5)
        var points := PackedVector2Array()
        for j in range(25):
            var a := j * TAU / 24.0
            points.append(spot + Vector2(cos(a) * width, sin(a) * width * 0.6 * open))
        c.draw_colored_polygon(points.slice(0, 24), _tint(Color(0.96, 0.95, 0.88), fade))
        c.draw_polyline(points, _tint(color, fade), maxf(r * 0.07, 1.0), true)
        var look := Vector2(sin(t * 7.0), 0.0) * width * 0.15
        c.draw_circle(spot + look, width * 0.3 * minf(open * 1.6, 1.0), _tint(color, fade))
        c.draw_circle(spot + look, width * 0.13 * minf(open * 1.6, 1.0), _tint(Color.BLACK, fade))
    _glow(c, at, r * 1.3, color, fade * 0.35)


func m_leaves(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    for i in range(amount):
        var angle := i * TAU / amount + t * (2.0 + _rand(i))
        var far := r * (0.5 + _rand(i + 2.0) * 0.8)
        var spot := at + Vector2(cos(angle), sin(angle) * 0.6) * far + Vector2(0.0, t * r * 0.4 - r * 0.2)
        var along := Vector2.from_angle(angle * 2.0) * r * 0.2
        var side := along.orthogonal() * 0.45
        c.draw_colored_polygon(PackedVector2Array([spot - along, spot + side, spot + along, spot - side]), _tint(color if i % 2 == 0 else accent, fade))


func m_insects(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at, r * 1.2, color, fade * 0.45)
    for i in range(amount):
        var a := t * (12.0 + _rand(i) * 8.0) + _rand(i + 1.0) * TAU
        var b := t * (9.0 + _rand(i + 2.0) * 9.0) + _rand(i + 3.0) * TAU
        var spot := at + Vector2(sin(a), cos(b)) * r * (0.5 + _rand(i + 4.0) * 0.8)
        c.draw_circle(spot, maxf(r * 0.055, 1.0), _tint(Color(0.07, 0.05, 0.03), fade))
        var flap := r * 0.1 * absf(sin(t * 60.0 + i))
        c.draw_line(spot + Vector2(-flap, -flap * 0.6), spot + Vector2(flap, -flap * 0.6), _tint(accent, fade), 1.0)


func m_tentacles(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.3)
    var grow := sin(minf(t * 1.1, 1.0) * PI) * 0.4 + minf(t * 3.0, 0.6)
    _turned(c, at + Vector2(0.0, r * 0.6), 0.0, Vector2(1.0, 0.45))
    c.draw_circle(Vector2.ZERO, r, _tint(color, fade * 0.8))
    _restore(c)
    for i in range(amount):
        var x := (i - (amount - 1) * 0.5) * r * 0.65
        var points := PackedVector2Array()
        for j in range(13):
            var up := j / 12.0
            points.append(at + Vector2(x + sin(up * 6.0 + t * 12.0 + i * 2.0) * r * 0.35 * up, r * 0.6 - up * r * 2.0 * grow))
        c.draw_polyline(points, _tint(color, fade), maxf(r * 0.2, 1.0), true)
        c.draw_polyline(points, _tint(accent, fade), maxf(r * 0.06, 1.0), true)


func m_rocks(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.05, 0.25)
    for i in range(amount):
        var angle := i * TAU / amount + _rand(i) * 0.5
        var land := minf(t * 2.5 + _rand(i + 2.0) * 0.1, 1.0)
        var rest := at + Vector2(cos(angle), sin(angle) * 0.7) * r * (0.45 + _rand(i + 3.0) * 0.45)
        var spot := rest + Vector2(0.0, -(1.0 - land) * r * 2.4)
        _polygon(c, spot, r * (0.2 + _rand(i + 5.0) * 0.18), 5, _rand(i + 6.0) * TAU + t, _tint(color, fade))
        _polygon(c, spot + Vector2(-0.04, -0.04) * r, r * 0.1, 4, _rand(i + 7.0), _tint(accent, fade))


func m_rain(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at + Vector2(0.0, -r * 1.2), r * 1.1, color, fade * 0.5)
    for i in range(amount * 2):
        var life := fposmod(t * 2.6 + _rand(i), 1.0)
        var x := (_rand(i + 3.0) - 0.5) * r * 2.0
        var top := at + Vector2(x + life * r * 0.3, -r * 1.2 + life * r * 2.2)
        c.draw_line(top, top + Vector2(r * 0.08, r * 0.3), _tint(accent if i % 3 == 0 else color, fade), maxf(r * 0.05, 1.0), true)


func m_wind(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    for i in range(amount):
        var y := (float(i) / maxf(amount - 1.0, 1.0) - 0.5) * r * 1.6
        var life := fposmod(t * 1.8 + _rand(i), 1.0)
        var x := lerpf(-r * 1.3, r * 0.9, life)
        var points := PackedVector2Array()
        for j in range(9):
            var u := j / 8.0
            points.append(at + Vector2(x + u * r * 0.9, y + sin(u * 3.2 + i) * r * 0.08))
        var curl := points[8]
        c.draw_polyline(points, _tint(color if i % 2 == 0 else accent, fade), maxf(r * 0.05, 1.0), true)
        c.draw_arc(curl + Vector2(0.0, -r * 0.1), r * 0.1, PI * 0.5, -PI * 0.7, 8, _tint(accent, fade), maxf(r * 0.04, 1.0), true)


func m_heal(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.4)
    _glow(c, at, r * 1.25, color, fade * 0.7)
    for i in range(amount):
        var life := fposmod(t * 1.2 + float(i) / amount, 1.0)
        var spot := at + Vector2((_rand(i) - 0.5) * r * 1.5, r * 0.7 - life * r * 1.9)
        var size := r * 0.24
        var width := maxf(size * 0.5, 1.0)
        var tint := _tint(accent if i % 2 == 0 else color, fade * (1.0 - life * 0.6))
        c.draw_line(spot + Vector2(-size, 0.0), spot + Vector2(size, 0.0), tint, width)
        c.draw_line(spot + Vector2(0.0, -size), spot + Vector2(0.0, size), tint, width)


func m_shadow(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.2, 0.4)
    for i in range(amount * 2):
        var angle := _rand(i) * TAU + t * 2.0
        var spot := at + Vector2.from_angle(angle) * r * (0.1 + _rand(i + 2.0) * 0.65)
        _glow(c, spot, r * 1.2, color, fade)
    for i in range(amount):
        var angle := i * TAU / amount - t * 3.0
        var points := PackedVector2Array()
        for j in range(10):
            var u := j / 9.0
            points.append(at + Vector2.from_angle(angle + u * 2.2) * r * (0.2 + u * 0.9))
        c.draw_polyline(points, _tint(accent, fade), maxf(r * 0.05, 1.0), true)


func m_mirrors(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    for i in range(amount):
        var angle := i * TAU / amount + t * 3.0
        var spot := at + Vector2(cos(angle), sin(angle) * 0.4) * r * 1.0 * minf(t * 3.0, 1.0)
        var points := PackedVector2Array([spot + Vector2(0.0, -r * 0.75), spot + Vector2(r * 0.34, 0.0), spot + Vector2(0.0, r * 0.75), spot + Vector2(-r * 0.34, 0.0)])
        c.draw_colored_polygon(points, _tint(color, fade * 0.4))
        points.append(points[0])
        c.draw_polyline(points, _tint(accent, fade), 1.0, true)
        c.draw_line(spot + Vector2(-r * 0.1, -r * 0.25), spot + Vector2(r * 0.14, -r * 0.45), _tint(Color.WHITE, fade), 1.0, true)


func _chevron(c: CanvasItem, at: Vector2, r: float, up: float, tint: Color) -> void:
    var width := maxf(r * 0.14, 1.0)
    c.draw_polyline(PackedVector2Array([at + Vector2(-r * 0.5, up * r * 0.26), at + Vector2(0.0, -up * r * 0.2), at + Vector2(r * 0.5, up * r * 0.26)]), tint, width, true)


func m_rise(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at, r * 1.1, color, fade * 0.45)
    for i in range(amount):
        var life := fposmod(t * 1.5 + float(i) / amount, 1.0)
        _chevron(c, at + Vector2(0.0, r * 0.8 - life * r * 1.9), r, 1.0, _tint(accent if i % 2 == 0 else color, fade * (1.0 - life * 0.5)))


func m_sink(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at, r * 1.1, color, fade * 0.45)
    for i in range(amount):
        var life := fposmod(t * 1.4 + float(i) / amount, 1.0)
        _chevron(c, at + Vector2(0.0, -r + life * r * 1.9), r, -1.0, _tint(accent if i % 2 == 0 else color, fade * (1.0 - life * 0.5)))


func m_moon(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    var top := at + Vector2(0.0, -r * 2.6)
    c.draw_colored_polygon(PackedVector2Array([top + Vector2(-r * 0.35, 0.0), top + Vector2(r * 0.35, 0.0), at + Vector2(r, r * 0.5), at + Vector2(-r, r * 0.5)]), _tint(color, fade * 0.4))
    _glow(c, top, r * 1.0, color, fade)
    c.draw_circle(top, r * 0.5, _tint(accent, fade))
    c.draw_circle(top + Vector2(r * 0.2, -r * 0.08), r * 0.45, _tint(Color(0.1, 0.13, 0.25), fade))
    for i in range(amount * 2):
        var life := fposmod(t * 1.2 + _rand(i), 1.0)
        _star(c, at + Vector2((_rand(i + 3.0) - 0.5) * r * 1.5, -r * 1.5 + life * r * 2.0), r * 0.13, _tint(accent, fade))


func m_orbit(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at, r * 0.9, color, fade * 0.4)
    for i in range(amount):
        var angle := i * TAU / amount + t * 9.0
        var spot := at + Vector2(cos(angle), sin(angle) * 0.42) * r * 1.1
        var near := sin(angle) > 0.0
        for back in range(1, 6):
            var a := angle - back * 0.16
            c.draw_circle(at + Vector2(cos(a), sin(a) * 0.42) * r * 1.1, r * 0.07 * (1.0 - back / 6.0), _tint(color, fade * (1.0 - back / 6.0)))
        _glow(c, spot, r * 0.35, color if i % 2 == 0 else accent, fade)
        c.draw_circle(spot, r * (0.13 if near else 0.08), _tint(accent if i % 2 == 0 else color, fade))


func m_claws(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.05, 0.4)
    for i in range(amount):
        var cut := clampf(t * 5.0 - i * 0.5, 0.0, 1.0)
        var x := (i - (amount - 1) * 0.5) * r * 0.5
        var start := at + Vector2(x + r * 0.45, -r)
        var end := at + Vector2(x - r * 0.45, r)
        var reach := start.lerp(end, cut)
        var side := Vector2(r * 0.08, r * 0.03)
        c.draw_colored_polygon(PackedVector2Array([start, start.lerp(reach, 0.5) + side, reach, start.lerp(reach, 0.5) - side]), _tint(color, fade))
        c.draw_line(start, reach, _tint(accent, fade), 1.0, true)


func m_hand(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.35)
    _glow(c, at, r * 1.3, color, fade * 0.6)
    var drop := Vector2(0.0, -(1.0 - minf(t * 3.0, 1.0)) * r)
    var palm := at + drop + Vector2(0.0, r * 0.25)
    c.draw_circle(palm, r * 0.45, _tint(color, fade))
    for i in range(amount):
        var turn := -PI * 0.5 + (i - (amount - 1) * 0.5) * 0.42
        var bend := Vector2.from_angle(turn) * r * (0.95 if i != 0 else 0.75)
        c.draw_line(palm + Vector2.from_angle(turn) * r * 0.3, palm + bend, _tint(color, fade), maxf(r * 0.2, 1.0), true)
    c.draw_circle(palm, r * 0.2, _tint(accent, fade))


func _sword(c: CanvasItem, at: Vector2, r: float, turn: float, tint: Color, shine: Color) -> void:
    _turned(c, at, turn)
    c.draw_colored_polygon(PackedVector2Array([Vector2(-r * 0.12, r * 0.4), Vector2(-r * 0.1, -r), Vector2(0.0, -r * 1.3), Vector2(r * 0.1, -r), Vector2(r * 0.12, r * 0.4)]), tint)
    c.draw_line(Vector2(0.0, r * 0.4), Vector2(0.0, -r * 1.25), shine, maxf(r * 0.04, 1.0))
    c.draw_rect(Rect2(Vector2(-r * 0.4, r * 0.4), Vector2(r * 0.8, r * 0.13)), tint)
    c.draw_rect(Rect2(Vector2(-r * 0.07, r * 0.53), Vector2(r * 0.14, r * 0.35)), shine)
    _restore(c)


func m_sword(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.25)
    _glow(c, at, r * 1.2, color, fade * 0.55)
    for i in range(amount):
        var side := 1.0 if i % 2 == 0 else -1.0
        var swing := lerpf(-1.5 * side, 1.0 * side, minf(t * 2.4, 1.0))
        _sword(c, at + Vector2(side * r * 0.2 * (amount - 1), r * 0.3), r * 0.85, swing, _tint(color, fade), _tint(accent, fade))


func m_hammer(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.25)
    var swing := lerpf(-1.8, 0.5, minf(t * 2.8, 1.0))
    var tint := _tint(color, fade)
    var pivot := at + Vector2(-r * 0.5, r * 0.5)
    _turned(c, pivot, swing + PI * 0.25)
    c.draw_rect(Rect2(Vector2(-r * 0.07, -r * 1.3), Vector2(r * 0.14, r * 1.4)), _tint(accent, fade))
    c.draw_rect(Rect2(Vector2(-r * 0.42, -r * 1.65), Vector2(r * 0.84, r * 0.5)), tint)
    _restore(c)
    if t > 0.35:
        _ring(c, at, r * (t - 0.35) * 2.6, _tint(accent, 1.0 - t), maxf(r * 0.1, 1.0))
    _glow(c, at, r * 1.1, color, fade * 0.5)


func m_prism(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.35)
    var grow := minf(t * 2.2, 1.0)
    for i in range(amount):
        var hue := fposmod(float(i) / amount + t * 0.7, 1.0)
        var tint := Color.from_hsv(hue, 0.8, 1.0, fade * 0.85)
        var angle := i * TAU / amount + t * 2.5
        var side := 0.22
        c.draw_colored_polygon(PackedVector2Array([at, at + Vector2.from_angle(angle - side) * r * 1.5 * grow, at + Vector2.from_angle(angle + side) * r * 1.5 * grow]), tint)
    c.draw_circle(at, r * 0.18, _tint(Color.WHITE, fade))
    _polygon(c, at, r * 0.3, 3, t * 4.0, _tint(accent, fade * 0.8))


func m_vortex(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at, r * 1.2, accent, fade * 0.6)
    for arm in range(amount):
        var points := PackedVector2Array()
        for j in range(18):
            var u := j / 17.0
            var angle := arm * TAU / amount + u * 4.2 + t * 12.0
            points.append(at + Vector2.from_angle(angle) * r * 1.35 * u * (1.0 - t * 0.35))
        c.draw_polyline(points, _tint(color if arm % 2 == 0 else accent, fade), maxf(r * 0.08, 1.0), true)


func m_poof(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := 1.0 - t
    _glow(c, at, r * (0.4 + t * 0.9), color, fade * 0.8)
    for i in range(amount):
        var dir := Vector2.from_angle(i * TAU / amount + _rand(i))
        var far := r * (1.0 - pow(1.0 - t, 2.0)) * (0.8 + _rand(i + 2.0) * 0.9)
        var spot := at + dir * far + Vector2(0.0, -t * r * 0.4)
        _star(c, spot, r * 0.22 * fade + 1.0, _tint(accent if i % 2 == 0 else color, fade))


func m_crack(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.05, 0.35)
    var grow := minf(t * 3.0, 1.0)
    var shake := Vector2(sin(t * 70.0), cos(t * 60.0)) * r * 0.05 * (1.0 - t)
    for i in range(amount):
        var dir := Vector2.from_angle(i * TAU / amount + _rand(i) * 0.7)
        var points := _bolt_points(at + shake, at + shake + dir * r * 1.5 * grow, 5, r * 0.18, i * 7.0)
        c.draw_polyline(points, _tint(Color(0.04, 0.03, 0.03), fade), maxf(r * 0.14, 1.0), true)
        c.draw_polyline(points, _tint(accent, fade), maxf(r * 0.03, 1.0), true)
    for i in range(amount):
        var life := minf(t * 1.6, 1.0)
        var dir := Vector2.from_angle(_rand(i + 9.0) * TAU)
        _polygon(c, at + dir * r * life + Vector2(0.0, -sin(life * PI) * r * 0.8), r * 0.1, 4, t * 8.0 + i, _tint(color, fade))


func m_zzz(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    _glow(c, at, r * 0.9, color, fade * 0.4)
    for i in range(amount + 1):
        var life := fposmod(t * 1.1 + float(i) / (amount + 1), 1.0)
        var spot := at + Vector2(life * r * 0.8 - r * 0.2 + sin(life * 6.0) * r * 0.12, r * 0.2 - life * r * 1.5)
        _zed(c, spot, r * (0.16 + life * 0.26), _tint(accent if i % 2 == 0 else color, fade * (1.0 - life * 0.5)))


# Drawn, not written: the mark is the same in every language and font.
func _zed(c: CanvasItem, at: Vector2, size: float, tint: Color) -> void:
    c.draw_polyline(PackedVector2Array([at + Vector2(-size, -size), at + Vector2(size, -size), at + Vector2(-size, size), at + Vector2(size, size)]), tint, maxf(size * 0.35, 1.0), true)


func m_question(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    for i in range(amount):
        var angle := i * TAU / amount + t * 5.0
        var spot := at + Vector2(cos(angle), sin(angle) * 0.45) * r * 0.9
        var size := r * 0.32
        var tint := _tint(color if i % 2 == 0 else accent, fade)
        var width := maxf(size * 0.35, 1.0)
        c.draw_arc(spot + Vector2(0.0, -size * 0.4), size * 0.6, -PI, PI * 0.45, 12, tint, width, true)
        c.draw_line(spot + Vector2(size * 0.25, size * 0.1), spot + Vector2(0.0, size * 0.6), tint, width, true)
        c.draw_circle(spot + Vector2(0.0, size * 1.1), width * 0.65, tint)


func m_bubble(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    if amount <= 1:
        var size := r * 1.1 * minf(t * 3.0, 1.0)
        c.draw_circle(at, size, _tint(color, fade * 0.18))
        _ring(c, at, size, _tint(color, fade), maxf(r * 0.06, 1.0))
        c.draw_arc(at, size * 0.8, PI * 1.05, PI * 1.45, 10, _tint(accent, fade), maxf(r * 0.08, 1.0), true)
        return
    for i in range(amount):
        var life := fposmod(t * 1.5 + float(i) / amount, 1.0)
        var spot := at + Vector2((_rand(i) - 0.5) * r * 1.6 + sin(life * 9.0 + i) * r * 0.1, r * 0.7 - life * r * 1.9)
        var size := r * (0.12 + _rand(i + 2.0) * 0.18)
        c.draw_circle(spot, size, _tint(color, fade * 0.4))
        _ring(c, spot, size, _tint(accent, fade), 1.0)


func m_sigil(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    var turn := t * 3.0
    var grow := minf(t * 3.0, 1.0)
    _glow(c, at, r * 1.2, color, fade * 0.45)
    _ring(c, at, r * 1.05 * grow, _tint(color, fade), maxf(r * 0.06, 1.0))
    var points := PackedVector2Array()
    var skip := 2 if amount > 4 else 1
    for i in range(amount + 1):
        points.append(at + Vector2.from_angle(turn + i * skip * TAU / amount - PI * 0.5) * r * 1.05 * grow)
    c.draw_polyline(points, _tint(accent, fade), maxf(r * 0.06, 1.0), true)


func m_crown(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    var spot := at + Vector2(0.0, -r * 1.05 + sin(t * 8.0) * r * 0.05)
    _glow(c, spot, r * 0.9, color, fade * 0.7)
    for i in range(amount):
        var angle := i * TAU / amount + t * 4.0
        var point := spot + Vector2(cos(angle) * r * 0.7, sin(angle) * r * 0.2)
        var tip := point + Vector2(0.0, -r * (0.35 if sin(angle) > 0.0 else 0.22))
        c.draw_line(point, tip, _tint(accent, fade), maxf(r * 0.09, 1.0), true)
        _star(c, tip, r * 0.13, _tint(color, fade))
    c.draw_set_transform_matrix(restore_transform * Transform2D(0.0, Vector2(1.0, 0.28), 0.0, spot))
    _ring(c, Vector2.ZERO, r * 0.7, _tint(color, fade), maxf(r * 0.16, 1.0))
    _restore(c)


func m_beast(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.35)
    for i in range(amount):
        var step := float(i) / amount
        var show := clampf((t - step * 0.5) * 6.0, 0.0, 1.0)
        var spot := at + Vector2((step - 0.4) * r * 1.5, (0.4 - step) * r * 1.2 + (0.2 if i % 2 else -0.2) * r)
        var tint := _tint(color if i % 2 == 0 else accent, fade * show)
        c.draw_circle(spot, r * 0.26, tint)
        for toe in range(4):
            c.draw_circle(spot + Vector2.from_angle(-PI * 0.5 + (toe - 1.5) * 0.55) * r * 0.42, r * 0.11, tint)


func m_cage(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.3)
    var drop := (1.0 - minf(t * 3.5, 1.0)) * r * 2.0
    var top := at + Vector2(0.0, -r * 1.1 - drop)
    _glow(c, at, r * 1.2, color, fade * 0.3)
    var bars := amount * 2 + 2
    for i in range(bars + 1):
        var x := (float(i) / bars - 0.5) * r * 1.9
        var shine := 0.6 + sin(t * 10.0 + i) * 0.4
        c.draw_line(top + Vector2(x, 0.0), top + Vector2(x, r * 2.2), _tint(accent if i % 2 == 0 else color, fade * shine), maxf(r * 0.06, 1.0), true)
    c.draw_line(top + Vector2(-r, 0.0), top + Vector2(r, 0.0), _tint(color, fade), maxf(r * 0.1, 1.0), true)
    c.draw_line(top + Vector2(-r, r * 2.2), top + Vector2(r, r * 2.2), _tint(color, fade), maxf(r * 0.1, 1.0), true)


func m_thorns(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.05, 0.3)
    for i in range(amount):
        var grow := clampf(t * 5.0 - _rand(i) * 1.5, 0.0, 1.0)
        var x := (_rand(i + 2.0) - 0.5) * r * 1.8
        var base := at + Vector2(x, r * (0.2 + _rand(i + 3.0) * 0.6))
        var tip := base + Vector2((_rand(i + 4.0) - 0.5) * r * 0.5, -r * (0.7 + _rand(i + 5.0) * 0.7) * grow)
        var side := Vector2(r * 0.12, 0.0)
        c.draw_colored_polygon(PackedVector2Array([base - side, tip, base + side]), _tint(color, fade))
        c.draw_line(base, tip, _tint(accent, fade), 1.0, true)


func m_clouds(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.15, 0.3)
    var sky := at + Vector2(0.0, -r * 1.5)
    for i in range(5):
        _glow(c, sky + Vector2((i - 2) * r * 0.5, (_rand(i) - 0.5) * r * 0.3), r * 0.75, color, fade)
    if fposmod(t * 3.0, 1.0) < 0.5:
        var frame := floorf(t * 6.0)
        for strand in range(amount - 1):
            var points := _bolt_points(sky + Vector2((strand - 0.5) * r * 0.6, 0.0), at, 6, r * 0.28, frame + strand * 5.0)
            c.draw_polyline(points, _tint(color.lightened(0.4), fade), maxf(r * 0.13, 1.0), true)
            c.draw_polyline(points, _tint(accent, fade), maxf(r * 0.04, 1.0), true)
        _glow(c, at, r * 0.9, accent, fade)


func m_tide(c: CanvasItem, at: Vector2, r: float, t: float) -> void:
    var fade := _fade(t, 0.1, 0.3)
    var push := lerpf(-r * 1.3, r * 1.3, t)
    for band in range(amount):
        var points := PackedVector2Array()
        var front := PackedVector2Array()
        var y := at.y + (float(band) / amount - 0.5) * r * 2.0
        for j in range(15):
            var x := at.x + (j / 14.0 - 0.5) * r * 2.4
            var surge := sin(j * 0.7 + t * 14.0 + band * 1.3) * r * 0.16
            points.append(Vector2(x + push * 0.3, y + surge))
            front.append(Vector2(x + push * 0.3, y + surge - r * 0.08))
        c.draw_polyline(points, _tint(color, fade), maxf(r * 0.32, 1.0), true)
        c.draw_polyline(front, _tint(accent, fade), maxf(r * 0.06, 1.0), true)


# Draws one cast from what the native view supplies: squares become pixels here.
func draw_effect(c: CanvasItem, effect: Dictionary, tile: float, zoom: float) -> void:
    var time: float = effect["time"]
    if time < 0.0:
        return
    restore_transform = Transform2D(0.0, Vector2.ONE * zoom, 0.0, Vector2.ZERO)
    color = effect["color"]
    accent = effect["accent"]
    amount = effect["amount"]
    seed_value = effect["seed"]
    var size: float = effect["size"]
    var travel: String = effect["travel"]
    var travel_seconds: float = effect["travel_seconds"]
    var from: Vector2 = effect["from"]
    var to: Vector2 = effect["to"]
    if not travel.is_empty() and time < travel_seconds:
        call("t_" + travel, c, from * tile, to * tile, time / travel_seconds, tile)
    if effect["missed"] or time < travel_seconds:
        return
    var arrive := "m_" + String(effect["arrive"])
    var seconds: float = effect["seconds"]
    var base_seed := seed_value
    var arrivals: PackedVector3Array = effect["arrivals"]
    for index in range(arrivals.size()):
        var arrival := arrivals[index]
        var progress := (time - travel_seconds - arrival.z) / seconds
        if progress >= 0.0 and progress < 1.0:
            seed_value = base_seed + index
            call(arrive, c, Vector2(arrival.x, arrival.y) * tile, tile * 0.5 * size, progress)
    seed_value = base_seed
    _restore(c)
