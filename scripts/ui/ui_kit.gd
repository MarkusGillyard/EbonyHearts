class_name UiKit
extends RefCounted
## Shared look for menus and the HUD: bone-white type on near-black teal, a slop-green accent for
## focus and selection, rust and amber for warnings and titles. New Rocker for display text,
## Oswald for everything else.

const ACCENT := Color(0.66, 0.86, 0.22)
const AMBER := Color(0.96, 0.66, 0.24)
const RUST := Color(0.72, 0.24, 0.12)
const TEXT := Color(0.93, 0.9, 0.82)
const DIM := Color(0.6, 0.62, 0.56)
const BG := Color(0.02, 0.035, 0.03)
const PANEL := Color(0.04, 0.06, 0.05, 0.9)

static var _theme: Theme
static var _display: Font
static var _body: Font
static var _body_bold: Font


static func display_font() -> Font:
	if _display == null:
		_display = load("res://assets/fonts/NewRocker-Regular.ttf")
	return _display


static func body_font(bold := false) -> Font:
	if bold:
		if _body_bold == null:
			var v := FontVariation.new()
			v.base_font = load("res://assets/fonts/Oswald-Variable.ttf")
			v.variation_opentype = {"wght": 600}
			_body_bold = v
		return _body_bold
	if _body == null:
		var v := FontVariation.new()
		v.base_font = load("res://assets/fonts/Oswald-Variable.ttf")
		v.variation_opentype = {"wght": 400}
		_body = v
	return _body


static func theme() -> Theme:
	if _theme:
		return _theme
	var t := Theme.new()
	t.default_font = body_font()
	t.default_font_size = 24
	var normal := _box(Color(0.05, 0.07, 0.06, 0.82), Color(0.2, 0.24, 0.2))
	var hover := _box(Color(0.08, 0.11, 0.08, 0.95), ACCENT.darkened(0.35))
	var focus := _box(Color(0.1, 0.14, 0.08, 0.96), ACCENT)
	var pressed := _box(Color(0.14, 0.18, 0.08, 0.96), ACCENT)
	for cls in ["Button", "OptionButton"]:
		t.set_stylebox("normal", cls, normal)
		t.set_stylebox("hover", cls, hover)
		t.set_stylebox("focus", cls, focus)
		t.set_stylebox("pressed", cls, pressed)
		t.set_stylebox("disabled", cls, normal)
		t.set_color("font_color", cls, TEXT)
		t.set_color("font_hover_color", cls, ACCENT)
		t.set_color("font_focus_color", cls, ACCENT)
		t.set_color("font_pressed_color", cls, ACCENT)
		t.set_color("font_disabled_color", cls, DIM.darkened(0.3))
	t.set_color("font_color", "Label", TEXT)
	t.set_stylebox("panel", "PanelContainer", _box(PANEL, Color(0.18, 0.22, 0.18)))
	var slider_bg := StyleBoxFlat.new()
	slider_bg.bg_color = Color(0.15, 0.17, 0.15)
	slider_bg.content_margin_top = 4
	slider_bg.content_margin_bottom = 4
	t.set_stylebox("slider", "HSlider", slider_bg)
	var fill := StyleBoxFlat.new()
	fill.bg_color = ACCENT.darkened(0.2)
	t.set_stylebox("grabber_area", "HSlider", fill)
	t.set_stylebox("grabber_area_highlight", "HSlider", fill)
	_theme = t
	return t


static func _box(bg: Color, border: Color) -> StyleBoxFlat:
	var s := StyleBoxFlat.new()
	s.bg_color = bg
	s.border_color = border
	s.set_border_width_all(2)
	s.set_content_margin_all(8)
	s.content_margin_left = 22
	s.content_margin_right = 22
	s.corner_radius_top_left = 2
	s.corner_radius_bottom_right = 2
	return s


static func label(text: String, size := 24, color := TEXT, align := HORIZONTAL_ALIGNMENT_LEFT, font: Font = null) -> Label:
	var l := Label.new()
	l.text = text
	l.horizontal_alignment = align
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	if font:
		l.add_theme_font_override("font", font)
	return l


## Makes a label wrap within `box`. A control grows to its minimum size when sized, and a label's
## minimum stays its whole unwrapped line until it is in the tree, so the box is applied then.
static func wrap(l: Label, box: Vector2) -> void:
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.size = box
	if not l.is_inside_tree():
		l.tree_entered.connect(func(): l.set_deferred("size", box), CONNECT_ONE_SHOT)


## Big display text with a dark outline, for titles and announcements.
static func title(text: String, size := 72, color := TEXT, align := HORIZONTAL_ALIGNMENT_CENTER) -> Label:
	var l := label(text, size, color, align, display_font())
	l.add_theme_color_override("font_outline_color", Color(0.02, 0.02, 0.02))
	l.add_theme_constant_override("outline_size", maxi(4, size / 9))
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.6))
	l.add_theme_constant_override("shadow_offset_y", maxi(2, size / 18))
	return l


static func button(text: String, on_press: Callable, size := 26) -> Button:
	var b := Button.new()
	b.text = text
	b.add_theme_font_size_override("font_size", size)
	b.add_theme_font_override("font", body_font(true))
	b.focus_mode = Control.FOCUS_ALL
	b.pressed.connect(func():
		Audio.play("ui_select")
		on_press.call())
	b.focus_entered.connect(func(): Audio.play("ui_move", -6.0))
	b.mouse_entered.connect(func(): b.grab_focus())
	return b


## A full-screen backdrop: near-black with a faint green haze rising from below.
static func backdrop(parent: Control, haze := ACCENT) -> void:
	var bg := ColorRect.new()
	bg.color = BG
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(bg)
	var tex := GradientTexture2D.new()
	var g := Gradient.new()
	g.colors = PackedColorArray([Color(haze.r, haze.g, haze.b, 0.16), Color(haze.r, haze.g, haze.b, 0.0)])
	tex.gradient = g
	tex.fill = GradientTexture2D.FILL_RADIAL
	tex.fill_from = Vector2(0.5, 1.15)
	tex.fill_to = Vector2(0.5, 0.2)
	var glow := TextureRect.new()
	glow.texture = tex
	glow.set_anchors_preset(Control.PRESET_FULL_RECT)
	glow.stretch_mode = TextureRect.STRETCH_SCALE
	glow.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(glow)


## Links a vertical list of controls for keyboard and controller focus, wrapping around.
static func chain_focus(controls: Array, wrap := true) -> void:
	var n := controls.size()
	for i in n:
		var c: Control = controls[i]
		var prev: Control = controls[(i - 1 + n) % n] if (wrap or i > 0) else c
		var nxt: Control = controls[(i + 1) % n] if (wrap or i < n - 1) else c
		c.focus_neighbor_top = c.get_path_to(prev)
		c.focus_neighbor_bottom = c.get_path_to(nxt)
		c.focus_previous = c.get_path_to(prev)
		c.focus_next = c.get_path_to(nxt)
