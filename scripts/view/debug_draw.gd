class_name DebugDraw
extends MeshInstance3D
## Outline overlay of the simulation's boxes on the fighting plane: hurtboxes (blue), active
## hitboxes (red), throw reach (orange), pushboxes (yellow) and projectiles (magenta), drawn
## through bodies.

const HURT := Color(0.3, 0.6, 1.0)
const HIT := Color(1.0, 0.15, 0.1)
const THROW := Color(1.0, 0.6, 0.1)
const PUSH := Color(1.0, 0.95, 0.3)
const PROJ := Color(1.0, 0.3, 0.9)

var sim: FightSim
var _im := ImmediateMesh.new()


func _ready() -> void:
	mesh = _im
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.vertex_color_use_as_albedo = true
	m.no_depth_test = true
	m.render_priority = 10
	material_override = m
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF


func redraw() -> void:
	_im.clear_surfaces()
	if sim == null or not visible:
		return
	_im.surface_begin(Mesh.PRIMITIVE_LINES)
	for f in sim.fighters:
		for r in f.hurtboxes():
			_rect(r, HURT)
		for hb in f.active_hitboxes():
			_rect(hb[1], HIT)
		if f.state == Fighter.S.MOVE and f.move and f.move.kind == "throw":
			var reach := Rect2(f.pos.x - f.def.throw_range, 0.02, f.def.throw_range * 2.0, 0.04)
			_rect(reach, THROW if f.move.in_active(f.mf) else THROW.darkened(0.6))
		_rect(f.pushbox(), PUSH)
	for p in sim.projectiles:
		_rect(p.rect(), PROJ)
	_im.surface_end()


func _rect(r: Rect2, c: Color) -> void:
	var z := 0.6
	var a := Vector3(r.position.x, r.position.y, z)
	var b := Vector3(r.end.x, r.position.y, z)
	var cc := Vector3(r.end.x, r.end.y, z)
	var d := Vector3(r.position.x, r.end.y, z)
	for seg in [[a, b], [b, cc], [cc, d], [d, a]]:
		_im.surface_set_color(c)
		_im.surface_add_vertex(seg[0])
		_im.surface_set_color(c)
		_im.surface_add_vertex(seg[1])
