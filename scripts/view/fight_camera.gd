class_name FightCamera
extends Camera3D
## Side-on fighting camera. It frames both fighters, pulls back as they separate, stays inside the
## stage and is smoothed with critically damped springs, so distances stay easy to judge. Shake is
## small and brief by design. Finishers and knockouts can take over the framing for a moment.

const VFOV := 30.0
## Horizontal margin kept around the fighters, in metres.
const MARGIN := 1.9
## Closest and farthest camera distances, in metres. At the closest, a 1.8 m fighter fills about
## two thirds of the screen's height.
const DIST_MIN := 4.8
const DIST_MAX := 8.4
## Height alone pulls back no further than this, which still fits a jumping fighter's head: the
## view should not breathe in and out by the full range on every jump.
const DIST_MAX_HEIGHT := 7.2
## Room kept below the lowest feet and above the highest head, in metres.
const FLOOR_ROOM := 0.3
const HEAD_ROOM := 0.35

var sim: FightSim
var shake_enabled := true
var _pos := Vector3.ZERO
var _pos_v := Vector3.ZERO
var _look := Vector3.ZERO
var _look_v := Vector3.ZERO
var _shake := 0.0
var _shake_t := 0.0
## A short displacement along the direction of a blow, sprung back: the camera takes the hit too.
var _kick := Vector3.ZERO
var _kick_v := Vector3.ZERO
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
	if Settings.get_value("graphics") != "low":
		# The stage softens a few metres behind the fighting plane: depth, and the fighters stand
		# out from whatever is behind them. The distance follows the camera in _apply().
		var attr := CameraAttributesPractical.new()
		attr.dof_blur_far_enabled = true
		attr.dof_blur_far_distance = 9.0
		attr.dof_blur_far_transition = 6.0
		attr.dof_blur_amount = 0.055
		attributes = attr


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


## Nudges the camera `amount` metres along `dir` (world space); it springs back within a few frames.
func kick(dir: Vector3, amount: float) -> void:
	if shake_enabled and dir.length() > 0.001:
		_kick_v += dir.normalized() * amount * 40.0


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
	var half_h_per_d := tan(deg_to_rad(VFOV * 0.5))
	var half_w_per_d := half_h_per_d * aspect
	# Both fighters whole, feet to head, jumps and juggles included: the camera pulls back as far
	# as it must (and as they separate), keeping the floor near the bottom of the frame.
	var top := -INF
	var bottom := INF
	for f in sim.fighters:
		top = maxf(top, f.pos.y + f.def.height * 1.05 + HEAD_ROOM)
		bottom = minf(bottom, f.pos.y - FLOOR_ROOM)
	var dist := maxf((sep + MARGIN) / (2.0 * half_w_per_d), minf((top - bottom) / (2.0 * half_h_per_d), DIST_MAX_HEIGHT))
	dist = clampf(dist, DIST_MIN, DIST_MAX)
	dist *= 1.0 - _zoom * 0.45
	var half_w := dist * half_w_per_d
	var half_h := dist * half_h_per_d
	var lim := maxf(0.0, FightSim.STAGE_HALF + 0.9 - half_w)
	var cx := clampf(mid, -lim, lim)
	# Always anchored to the lowest fighter: something higher than the farthest framing (the
	# Slopbat rising for its dive) leaves the top of the screen rather than the player the bottom.
	var ly := bottom + half_h
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
	var wk := 22.0
	var ak := -_kick * wk * wk - _kick_v * 2.0 * wk
	_kick_v += ak * delta
	_kick += _kick_v * delta
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
	var off := _kick
	if _shake > 0.0:
		off += Vector3(sin(_shake_t * 61.0), sin(_shake_t * 47.0 + 1.3), 0.0) * _shake
	global_transform = Transform3D(Basis.looking_at(_look - _pos, Vector3.UP), _pos + off)
	if attributes:
		(attributes as CameraAttributesPractical).dof_blur_far_distance = _pos.z + 3.4
