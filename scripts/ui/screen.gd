class_name Screen
extends Control
## Base for every menu screen: the moving backdrop, a title, focus handling for keyboard and
## controller, and "back" on Escape, Backspace or the controller's B button.

var back_to := ""
var back_data := {}
var _focus_first: Control


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	build()
	if _focus_first:
		_focus_first.grab_focus.call_deferred()


## Subclasses build their contents here.
func build() -> void:
	pass


func backdrop(glow := UiKit.ACCENT) -> void:
	var bg := ColorRect.new()
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var m := ShaderMaterial.new()
	m.shader = preload("res://assets/shaders/menu_backdrop.gdshader")
	m.set_shader_parameter("glow", glow)
	bg.material = m
	add_child(bg)


func heading(text: String, size := 84, y := 60.0) -> Label:
	var t := UiKit.title(text, size, UiKit.TEXT)
	t.position = Vector2(0, y)
	t.size = Vector2(1920, size * 1.4)
	add_child(t)
	return t


## A vertical list of buttons at `pos`; returns them. Items: [label, Callable] or [label, Callable, enabled].
func menu(items: Array, pos: Vector2, width := 460.0, size := 30) -> Array[Button]:
	var box := VBoxContainer.new()
	box.position = pos
	box.size = Vector2(width, 0)
	box.add_theme_constant_override("separation", 10)
	add_child(box)
	var buttons: Array[Button] = []
	for it in items:
		var b := UiKit.button(it[0], it[1], size)
		b.custom_minimum_size = Vector2(width, size * 1.9)
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		if it.size() > 2 and not it[2]:
			b.disabled = true
			b.focus_mode = Control.FOCUS_ALL
		box.add_child(b)
		buttons.append(b)
	UiKit.chain_focus(buttons)
	if not buttons.is_empty() and _focus_first == null:
		_focus_first = buttons[0]
	return buttons


func hint(text: String) -> Label:
	var l := UiKit.label(text, 22, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
	l.position = Vector2(0, 1080 - 60)
	l.size = Vector2(1920, 40)
	add_child(l)
	return l


func go_back() -> void:
	if back_to != "":
		Audio.play("ui_back")
		Game.goto(back_to, back_data)


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel") or (event is InputEventKey and event.pressed and not event.echo and event.physical_keycode == KEY_ESCAPE):
		if back_to != "":
			go_back()
			get_viewport().set_input_as_handled()


## Text for a key or button prompt, e.g. "Enter / J".
static func accept_prompt() -> String:
	return "Enter"
