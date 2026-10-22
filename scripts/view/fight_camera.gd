class_name FightCamera
extends Camera3D
## Side-on fighting camera. It frames both fighters, pulls back as they separate, stays inside the
## stage and is smoothed with critically damped springs, so distances stay easy to judge. Shake is
## small and brief by design. Finishers and knockouts can take over the framing for a moment.

const VFOV := 30.0
## Horizontal margin kept around the fighters, in metres.
const MARGIN := 1.9

var sim: FightSim
var shake_enabled := true
var _pos := Vector3.ZERO
var _pos_v := Vector3.ZERO
var _look := Vector3.ZERO
var _look_v := Vector3.ZERO
var _shake := 0.0
var _shake_t := 0.0
var _zoom := 0.0
var _zoom_target := 0.0
var _zoom_focus := -1
## A scripted framing (finishers): [position, look target, weight 0..1].
var override_pos := Vector3.ZERO
var override_look := Vector3.ZERO
var override_w := 0.0
var override_target := 0.0


func _ready() -> void:
	fov = VFOV
	near = 0.1
	far = 300.0


func snap() -> void:
	var t := _targets()
	_pos = t[0]
	_look = t[1]
	_pos_v = Vector3.ZERO
	_look_v = Vector3.ZERO
	_apply()


## Closer framing for a moment (knockouts, finishers); `focus` is a fighter index or -1.
func punch_in(amount: float, focus := -1) -> void:
	_zoom_target = amount
	_zoom_focus = focus


func release_zoom() -> void:
	_zoom_target = 0.0
	_zoom_focus = -1


func add_shake(amount: float) -> void:
	if shake_enabled:
		_shake = maxf(_shake, amount)


func set_override(pos: Vector3, look: Vector3, weight := 1.0) -> void:
	override_pos = pos
	override_look = look
	override_target = weight


func clear_override() -> void:
	override_target = 0.0


func _targets() -> Array:
	var a := sim.fighters[0].pos
	var b := sim.fighters[1].pos
	var mid := (a.x + b.x) * 0.5
	var sep := absf(b.x - a.x)
	var aspect := 16.0 / 9.0
	var vp := get_viewport()
	if vp:
		var sz := vp.get_visible_rect().size
		if sz.y > 0.0:
			aspect = sz.x / sz.y
	var half_w_per_d := tan(deg_to_rad(VFOV * 0.5)) * aspect
	var dist := clampf((sep + MARGIN) / (2.0 * half_w_per_d), 4.3, 8.4)
	dist *= 1.0 - _zoom * 0.45
	var half_w := dist * half_w_per_d
	var lim := maxf(0.0, FightSim.STAGE_HALF + 0.9 - half_w)
	var cx := clampf(mid, -lim, lim)
	# Follow jumps and juggles upward so the airborne fighter stays in frame.
	var high := maxf(a.y, b.y)
	var ly := 1.0 + clampf(high - 0.3, 0.0, 3.0) * 0.55
	var focus := Vector3(cx, ly, 0.0)
	if _zoom_focus >= 0:
		var fp := sim.fighters[_zoom_focus].pos
		focus = focus.lerp(Vector3(fp.x, ly, 0.0), _zoom * 0.6)
	var pos := focus + Vector3(0.0, 0.18 + dist * 0.02, dist)
	return [pos, focus]


func step(delta: float) -> void:
	if sim == null:
		return
	_zoom = move_toward(_zoom, _zoom_target, delta * 2.2)
	override_w = move_toward(override_w, override_target, delta * 2.5)
	var t := _targets()
	var pos_t: Vector3 = t[0]
	var look_t: Vector3 = t[1]
	if override_w > 0.0:
		var k := Pose.ease("inout", override_w)
		pos_t = pos_t.lerp(override_pos, k)
		look_t = look_t.lerp(override_look, k)
	_spring(delta, pos_t, look_t)
	_shake_t += delta
	_shake = maxf(0.0, _shake - delta * 0.14)
	_apply()


func _spring(delta: float, pos_t: Vector3, look_t: Vector3) -> void:
	var w := 7.0
	var a := (pos_t - _pos) * w * w - _pos_v * 2.0 * w
	_pos_v += a * delta
	_pos += _pos_v * delta
	var wl := 9.0
	var al := (look_t - _look) * wl * wl - _look_v * 2.0 * wl
	_look_v += al * delta
	_look += _look_v * delta


func _apply() -> void:
	var off := Vector3.ZERO
	if _shake > 0.0:
		off = Vector3(sin(_shake_t * 61.0), sin(_shake_t * 47.0 + 1.3), 0.0) * _shake
	global_transform = Transform3D(Basis.looking_at(_look - _pos, Vector3.UP), _pos + off)
