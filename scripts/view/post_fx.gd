class_name PostFx
extends CanvasLayer
## The frame's finish, drawn under the HUD: a vignette and a faint grain (assets/shaders/post.gdshader).
## Off when Settings "post_fx" is off. Costs one full-screen quad.

var _rect: ColorRect


func _ready() -> void:
	layer = 5
	_rect = ColorRect.new()
	_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var m := ShaderMaterial.new()
	m.shader = preload("res://assets/shaders/post.gdshader")
	_rect.material = m
	add_child(_rect)
	visible = bool(Settings.get_value("post_fx"))
