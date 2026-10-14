extends RefCounted

# Technical slot dimensions and draw positions from exploration_view.cpp, scaled 4x.
# Source art is never loaded by this projector.
const SCALE := 4
const CANVAS := 88 * SCALE
const VIEW_NAMES := [
    "Far front", "Far left", "Far right", "Middle front", "Middle left",
    "Middle right", "Near front", "Near left", "Near right", "Far extension"
]
const VIEW_SIZES := [
    [8, 16], [8, 32], [8, 32], [24, 32], [16, 64],
    [16, 64], [56, 64], [16, 88], [16, 88], [8, 16]
]

static func build(base_path: String, feature_path: String, kind: String) -> Array[Image]:
    var pieces: Array[Image] = []
    var base := _load_png(base_path)
    if base == null or base.is_empty():
        return pieces
    base.convert(Image.FORMAT_RGBA8)
    var front := base.duplicate() as Image
    front.resize(56 * SCALE, 64 * SCALE, Image.INTERPOLATE_NEAREST)
    if kind == "opening":
        _cut_arch(front)
    elif kind == "rect_opening":
        front.fill_rect(Rect2i(58, 36, 108, 220), Color.TRANSPARENT)
    elif kind == "cave_opening":
        _cut_cave(front)
    if not feature_path.is_empty():
        var feature := _load_png(feature_path)
        if feature == null or feature.is_empty():
            return pieces
        feature.convert(Image.FORMAT_RGBA8)
        var box := _feature_box(kind)
        feature.resize(box.size.x, box.size.y, Image.INTERPOLATE_NEAREST)
        front.blend_rect(feature, Rect2i(Vector2i.ZERO, feature.get_size()), box.position)
    for view in VIEW_SIZES.size():
        var size := Vector2i(VIEW_SIZES[view][0], VIEW_SIZES[view][1]) * SCALE
        if view == 9:
            var extension := base.duplicate() as Image
            extension.resize(size.x, size.y, Image.INTERPOLATE_NEAREST)
            pieces.append(extension)
        elif view == 1 or view == 2 or view == 4 or view == 5 or view == 7 or view == 8:
            pieces.append(_project_side(front, size, view))
        else:
            var piece := front.duplicate() as Image
            piece.resize(size.x, size.y, Image.INTERPOLATE_NEAREST)
            pieces.append(piece)
    return pieces

static func _load_png(path: String) -> Image:
    if path.is_empty() or not FileAccess.file_exists(path):
        return null
    var image := Image.new()
    if image.load_png_from_buffer(FileAccess.get_file_as_bytes(path)) != OK:
        return null
    return image

static func _feature_box(kind: String) -> Rect2i:
    match kind:
        "door":
            return Rect2i(36, 4, 152, 248)
        "opening":
            return Rect2i(30, 4, 164, 248)
        "landmark":
            return Rect2i(54, 35, 116, 172)
        "facade", "rect_opening", "cave_opening":
            return Rect2i(0, 0, 56 * SCALE, 64 * SCALE)
    return Rect2i(0, 0, 0, 0)

static func _cut_cave(front: Image) -> void:
    for y in range(26, 234):
        for x in range(35, 189):
            var dx := (float(x) - 112.0) / 76.0
            var dy := (float(y) - 132.0) / 106.0
            if dx * dx + dy * dy < 1.0:
                front.set_pixel(x, y, Color.TRANSPARENT)

static func _cut_arch(front: Image) -> void:
    # Cut independent geometry behind the generated arch overlay. Its center
    # remains transparent so a later wall can be seen through it.
    for y in range(58, 256):
        var half_width := 40.0
        if y < 102:
            var offset := float(y - 102) / 44.0
            half_width *= sqrt(maxf(0.0, 1.0 - offset * offset))
        for x in range(maxi(0, ceili(112.0 - half_width)), mini(224, floori(112.0 + half_width))):
            front.set_pixel(x, y, Color.TRANSPARENT)

static func _project_side(front: Image, size: Vector2i, view: int) -> Image:
    var side := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
    side.fill(Color.TRANSPARENT)
    var left := view == 1 or view == 4 or view == 7
    var inset := 8 if view == 1 or view == 2 else 16 if view == 4 or view == 5 else 8
    var bottom_inset := 16 if view == 7 or view == 8 else inset
    for x in size.x:
        var depth_ratio := (float(x) + 0.5) / size.x
        if not left:
            depth_ratio = 1.0 - depth_ratio
        var top := depth_ratio * inset * SCALE
        var bottom := size.y - depth_ratio * bottom_inset * SCALE
        for y in range(ceili(top), floori(bottom)):
            var u := (float(x) + 0.5) / size.x
            if not left:
                u = 1.0 - u
            var v := (float(y) + 0.5 - top) / (bottom - top)
            var sx := clampi(int(u * front.get_width()), 0, front.get_width() - 1)
            var sy := clampi(int(v * front.get_height()), 0, front.get_height() - 1)
            side.set_pixel(x, y, front.get_pixel(sx, sy))
    return side

static func preview(pieces: Array[Image], depth: int) -> Image:
    var canvas := Image.create(CANVAS, CANVAS, false, Image.FORMAT_RGBA8)
    canvas.fill(Color("39474a"))
    canvas.fill_rect(Rect2i(0, CANVAS / 2, CANVAS, CANVAS / 2), Color("503f2e"))
    if pieces.size() != 10:
        return canvas
    # Draw distant surfaces before nearer ones, as the game compositor does.
    _draw(canvas, pieces[0], 40, 32)
    _draw(canvas, pieces[9], 32, 32)
    _draw(canvas, pieces[1], 32, 24)
    _draw(canvas, pieces[2], 48, 24)
    if depth >= 1:
        _draw(canvas, pieces[3], 32, 24)
        _draw(canvas, pieces[4], 16, 8)
        _draw(canvas, pieces[5], 56, 8)
    if depth >= 2:
        _draw(canvas, pieces[6], 16, 8)
        _draw(canvas, pieces[7], 0, 0)
        _draw(canvas, pieces[8], 72, 0)
    return canvas

static func _draw(canvas: Image, piece: Image, x: int, y: int) -> void:
    canvas.blend_rect(piece, Rect2i(Vector2i.ZERO, piece.get_size()), Vector2i(x, y) * SCALE)
