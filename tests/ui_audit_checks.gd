extends RefCounted

# Layout checks for any screen: walks the visible controls under a node and
# reports what a player would see as broken. Used by tests/ui_audit.gd and by
# the play-tests, which audit the screens they reach.
#
#   overlap      two controls showing text or taking input cover each other
#   text cut     a label, button or log does not fit its text in its box
#   off-window   a control draws outside the window
#   no room      a scrolling text area is shorter than one line
#
# Overlays that open over the screen on purpose (the hover panel, menus and
# dialogs) are left out of overlap checks.

# Controls a player reads or uses; frames and backgrounds are left out.
const CONTENT := ["Button", "OptionButton", "CheckBox", "CheckButton", "Label", "LineEdit",
    "RichTextLabel", "ItemList", "TextEdit", "SpinBox"]

static func is_content(control: Control) -> bool:
    for kind in CONTENT:
        if control.is_class(kind): return true
    return false

static func visible_controls(node: Node, out: Array) -> void:
    for child in node.get_children():
        if child is Window and not (child as Window).visible: continue
        if child is Control:
            var control := child as Control
            if not control.is_visible_in_tree(): continue
            out.append(control)
        if child is CanvasItem and not (child as CanvasItem).visible: continue
        visible_controls(child, out)

# Where a control draws: a label's text, not the empty rest of its box; the
# whole box for anything else. Empty for a label with no text.
static func drawn_rect(control: Control) -> Rect2:
    var rect := control.get_global_rect()
    if control is Label:
        var label := control as Label
        if label.text.strip_edges().is_empty(): return Rect2()
        var spacing := label.get_theme_constant("line_spacing")
        var pitch := label.get_theme_font("font").get_height(label.get_theme_font_size("font_size")) + spacing
        var height: float = min(rect.size.y, label.get_visible_line_count() * pitch - spacing)
        # Its widest line, wrapped at the label's width.
        var width: float = min(rect.size.x, label.get_theme_font("font").get_multiline_string_size(
            label.text, label.horizontal_alignment, rect.size.x,
            label.get_theme_font_size("font_size")).x)
        var x := rect.position.x + (rect.size.x - width) * \
            (0.5 if label.horizontal_alignment == HORIZONTAL_ALIGNMENT_CENTER else \
             1.0 if label.horizontal_alignment == HORIZONTAL_ALIGNMENT_RIGHT else 0.0)
        var y := rect.position.y + (rect.size.y - height) * \
            (0.5 if label.vertical_alignment == VERTICAL_ALIGNMENT_CENTER else \
             1.0 if label.vertical_alignment == VERTICAL_ALIGNMENT_BOTTOM else 0.0)
        return Rect2(x, y, width, height)
    if control is RichTextLabel:
        var log := control as RichTextLabel
        if log.get_parsed_text().strip_edges().is_empty(): return Rect2()
        return Rect2(rect.position, Vector2(rect.size.x, min(rect.size.y, log.get_content_height())))
    return rect

# The part of a control's drawing that ancestors clipping their contents
# (scroll areas) let show.
static func shown_rect(control: Control) -> Rect2:
    var rect := drawn_rect(control)
    var parent := control.get_parent()
    while parent is Control:
        var ancestor := parent as Control
        if ancestor.clip_contents or ancestor is ScrollContainer:
            rect = rect.intersection(ancestor.get_global_rect())
        parent = parent.get_parent()
    return rect

# True for controls drawn over the screen on purpose, and everything in them.
static func in_overlay(control: Control) -> bool:
    var node: Node = control
    while node != null:
        if node is Popup or node is AcceptDialog: return true
        if node is Control and ((node as Control).top_level or node.name == "HoverInfo"): return true
        node = node.get_parent()
    return false

static func inside_scroll(control: Control) -> bool:
    var node := control.get_parent()
    while node != null:
        if node is ScrollContainer: return true
        node = node.get_parent()
    return false

static func related(a: Node, b: Node) -> bool:
    return a.is_ancestor_of(b) or b.is_ancestor_of(a)

static func describe(control: Control, root: Node) -> String:
    var text := ""
    if "text" in control and control.text is String and not (control.text as String).is_empty():
        text = " \"" + (control.text as String).replace("\n", " ").left(40) + "\""
    return str(root.get_path_to(control)) + text

static func text_problem(control: Control) -> String:
    if control is Label:
        var label := control as Label
        if label.text.is_empty() or label.clip_text or \
                label.text_overrun_behavior != TextServer.OVERRUN_NO_TRIMMING: return ""
        if label.get_visible_line_count() < label.get_line_count():
            return "text cut: %d of %d lines fit" % [label.get_visible_line_count(), label.get_line_count()]
        if label.autowrap_mode == TextServer.AUTOWRAP_OFF and label.get_minimum_size().x > label.size.x + 1:
            return "text cut: needs %d px, has %d" % [label.get_minimum_size().x, label.size.x]
    elif control is Button:
        var button := control as Button
        if button.text.is_empty() or button.clip_text or \
                button.text_overrun_behavior != TextServer.OVERRUN_NO_TRIMMING: return ""
        if button.get_minimum_size().x > button.size.x + 1:
            return "text cut: needs %d px, has %d" % [button.get_minimum_size().x, button.size.x]
    elif control is RichTextLabel:
        var log := control as RichTextLabel
        if log.get_parsed_text().strip_edges().is_empty(): return ""
        var line := log.get_theme_font("normal_font").get_height(log.get_theme_font_size("normal_font_size"))
        if log.scroll_active:
            if log.size.y < line: return "no room: %d px for %d px lines" % [log.size.y, line]
        elif log.get_content_height() > log.size.y + 1:
            return "text cut: needs %d px, has %d" % [log.get_content_height(), log.size.y]
    return ""

# Findings for the screen under `root`, each "<path> "<text>": <problem>".
static func audit(root: Node, window: Vector2) -> PackedStringArray:
    var findings := PackedStringArray()
    var controls := []
    visible_controls(root, controls)
    var bounds := Rect2(Vector2.ZERO, window).grow(1)
    for control in controls:
        var problem := text_problem(control)
        if not problem.is_empty():
            findings.append(describe(control, root) + ": " + problem)
        # Where it draws: a label's empty box may hang past the window unseen.
        var drawn := drawn_rect(control)
        if not inside_scroll(control) and drawn.size.x > 0 and drawn.size.y > 0 and \
                not bounds.encloses(drawn):
            findings.append(describe(control, root) + ": off-window at " + str(drawn))
    var content := controls.filter(func(c): return is_content(c) and not in_overlay(c))
    for i in range(content.size()):
        for j in range(i + 1, content.size()):
            var a: Control = content[i]
            var b: Control = content[j]
            if related(a, b): continue
            var shared := shown_rect(a).intersection(shown_rect(b))
            if shared.size.x > 2 and shared.size.y > 2:
                findings.append(describe(a, root) + " overlaps " + describe(b, root))
    return findings

# The visible control tree as data: path, class, rectangle and text.
static func dump(root: Node) -> Array:
    var rows := []
    var controls := []
    visible_controls(root, controls)
    for control in controls:
        var rect: Rect2 = control.get_global_rect()
        rows.append({
            "path": str(root.get_path_to(control)),
            "class": control.get_class(),
            "rect": [rect.position.x, rect.position.y, rect.size.x, rect.size.y],
            "text": control.text if "text" in control and control.text is String else ""})
    return rows
