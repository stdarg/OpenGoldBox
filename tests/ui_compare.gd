extends SceneTree

# Compares two sets of screenshots taken by tests/ui_audit.gd --capture and
# lists the screens that changed, how much, and screens that appeared or went.
# For each changed screen it writes NAME.png to --out: the new screenshot
# dimmed, with the changed pixels in red. Run by tools/ui_snapshots.py.
#   godot --headless --script tests/ui_compare.gd -- --before=DIR --after=DIR --out=DIR

# A channel must differ by more than this (0-255) for a pixel to count as changed,
# so dithering and font smoothing noise do not.
const TOLERANCE := 8

var before := ""
var after := ""
var out := ""

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--before="): before = arg.trim_prefix("--before=")
        if arg.begins_with("--after="): after = arg.trim_prefix("--after=")
        if arg.begins_with("--out="): out = arg.trim_prefix("--out=")
    call_deferred("run")

func screenshots(folder: String) -> PackedStringArray:
    var names := PackedStringArray()
    for file in DirAccess.get_files_at(folder):
        if file.ends_with(".png"): names.append(file)
    return names

# The share of pixels that changed, and an image showing where.
func difference(old: Image, new: Image) -> Array:
    old.convert(Image.FORMAT_RGBA8); new.convert(Image.FORMAT_RGBA8)
    var a := old.get_data()
    var b := new.get_data()
    var marked := new.duplicate() as Image
    marked.adjust_bcs(0.35, 0.3, 1.0)
    var changed := 0
    var width := new.get_width()
    for i in range(0, a.size(), 4):
        if absi(a[i] - b[i]) > TOLERANCE or absi(a[i + 1] - b[i + 1]) > TOLERANCE or \
                absi(a[i + 2] - b[i + 2]) > TOLERANCE:
            changed += 1
            var pixel := i / 4
            marked.set_pixel(pixel % width, pixel / width, Color(1, 0.1, 0.1))
    return [float(changed) / (a.size() / 4), marked]

func run() -> void:
    if before.is_empty() or after.is_empty() or out.is_empty():
        printerr("--before, --after and --out are required"); quit(2); return
    DirAccess.make_dir_recursive_absolute(out)
    var old_names := screenshots(before)
    var new_names := screenshots(after)
    var report := PackedStringArray()
    var unchanged := 0
    for name in new_names:
        if not old_names.has(name):
            report.append("new:     " + name); continue
        var old := Image.load_from_file(before.path_join(name))
        var new := Image.load_from_file(after.path_join(name))
        if old.get_size() != new.get_size():
            report.append("resized: %s (%s -> %s)" % [name, old.get_size(), new.get_size()]); continue
        if old.get_data() == new.get_data():
            unchanged += 1; continue
        var result := difference(old, new)
        if result[0] == 0.0:
            unchanged += 1; continue
        (result[1] as Image).save_png(out.path_join(name))
        report.append("changed: %s (%.2f%% of pixels)" % [name, result[0] * 100])
    for name in old_names:
        if not new_names.has(name): report.append("gone:    " + name)
    report.sort()
    report.append("%d unchanged, %d differ" % [unchanged, report.size()])
    var file := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    file.store_string("\n".join(report) + "\n"); file.close()
    print("\n".join(report))
    quit(0)
