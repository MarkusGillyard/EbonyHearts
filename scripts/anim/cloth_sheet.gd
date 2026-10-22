class_name ClothSheet
extends Node
## A hanging sheet of cloth or hair: a grid of points simulated with Verlet integration, held to
## its rest shape by a soft shape-matching pull and pushed out of the body's capsules. The top
## row follows anchor points on a joint. Simulated in world space at a fixed 90 Hz.
##
## The sheet draws nothing itself: every point poses a bone of a skeleton, and the detailed
## garment or hair mesh from the character model is skinned to those bones.

const DT := 1.0 / 90.0

var cols := 12
var rows := 6
var closed := true
var seg_len := 0.12
var gravity := 9.8
var damping := 0.97
var shape_k := 0.08
var iterations := 3
## Rest shape: positions in the anchor joint's frame, rows * cols.
var rest_local := PackedVector3Array()
var anchor_joint: Node3D
var colliders: Callable
var margin := 0.012
## No point may drift further than this from its rest shape; keeps fast turns from flinging
## the sheet into shapes it cannot recover from.
var max_dev := 0.35
var _p := PackedVector3Array()
var _prev := PackedVector3Array()
var _rest_h := PackedFloat32Array()
var _target := PackedVector3Array()
var _acc := 0.0
var skeleton: Skeleton3D
## 1 when bone 0 of the skeleton follows the anchor joint (hair rooted on the scalp).
var _first := 0


## Rest shape of a cloth spec from fighter data, in the anchor joint's frame: a skirt is a ring
## of columns hanging from the joint (open at the back when it has a gap; "yaw" in degrees turns
## the ring, so 180 opens it at the front); hair is an arc around the back of the head that
## narrows toward the tips, so it reads as a mass rather than a sheet. A skirt may also shift its
## ring back by "cz" and replace the straight flare by a "profile" of [x, z] widenings spaced
## evenly down its length (veils and capes that drape over the head and shoulders).
## Returns {"cols", "rows", "closed", "points"} with points row by row.
static func rest_shape(spec: Dictionary, h: float) -> Dictionary:
	var cols := int(spec.get("cols", 14))
	var rows := int(spec.get("rows", 6))
	var kind := str(spec.get("kind", "skirt"))
	var top := float(spec.get("top", 0.0)) * h
	var length := float(spec.get("length", 0.3)) * h
	var gap := float(spec.get("gap", 0.0))
	var yaw := deg_to_rad(float(spec.get("yaw", 0.0)))
	var cz := float(spec.get("cz", 0.0)) * h
	var profile: Array = spec.get("profile", [])
	var pts := PackedVector3Array()
	pts.resize(rows * cols)
	for r in rows:
		var t := float(r) / (rows - 1)
		for col in cols:
			var p: Vector3
			if kind == "hair":
				var width := float(spec.get("width", 0.1)) * h
				var back := float(spec.get("back", 0.06)) * h
				var a := (float(col) / (cols - 1) - 0.5) * PI * 0.9
				var rad := width * 0.62 * lerpf(1.0, 0.45, t * t)
				p = Vector3(sin(a) * rad, top - length * t, back * 0.4 + cos(a) * rad * 0.8 + 0.02 * h * t)
			else:
				var rx := float(spec.get("rx", 0.1)) * h
				var rz := float(spec.get("rz", 0.075)) * h
				var fl := float(spec.get("flare", 0.05)) * h * t
				var flx := fl
				var flz := fl
				if profile.size() >= 2:
					var u := t * (profile.size() - 1)
					var i0 := mini(int(u), profile.size() - 2)
					var a0: Array = profile[i0]
					var a1: Array = profile[i0 + 1]
					flx = lerpf(float(a0[0]), float(a1[0]), u - i0) * h
					flz = lerpf(float(a0[1]), float(a1[1]), u - i0) * h
				var span := TAU * (1.0 - gap)
				var a := PI * 0.5 + TAU * gap * 0.5 + span * (float(col) / (cols - 1 if gap > 0.0 else cols))
				var q := Vector2(cos(a) * (rx + flx), sin(a) * (rz + flz)).rotated(yaw)
				p = Vector3(q.x, top - length * t, q.y + cz)
			pts[r * cols + col] = p
	return {"cols": cols, "rows": rows, "closed": kind != "hair" and gap <= 0.0, "points": pts}


## `points` holds the rest position of each point, row by row, in the joint frame.
func setup(joint: Node3D, c: int, r: int, is_closed: bool, points: PackedVector3Array) -> void:
	anchor_joint = joint
	cols = c
	rows = r
	closed = is_closed
	rest_local = points
	seg_len = 0.0
	for j in cols:
		seg_len += rest_local[cols + j].distance_to(rest_local[j])
	seg_len /= cols
	_rest_h.resize(rows)
	for i in rows:
		var j2 := 1
		_rest_h[i] = rest_local[i * cols].distance_to(rest_local[i * cols + j2])
	reset()


func reset() -> void:
	_p.resize(rows * cols)
	_prev.resize(rows * cols)
	var g := anchor_joint.global_transform
	for k in rows * cols:
		_p[k] = g * rest_local[k]
		_prev[k] = _p[k]
	_acc = 0.0
	_pose_bones()


## Drives `sk`: bone _first + k follows point k (sk must sit at the world origin).
func drive(sk: Skeleton3D, anchored: bool) -> void:
	skeleton = sk
	_first = 1 if anchored else 0
	_pose_bones()


func step(delta: float) -> void:
	if anchor_joint == null:
		return
	_acc = minf(_acc + delta, DT * 4.0)
	var caps: Array = colliders.call() if colliders.is_valid() else []
	var g := anchor_joint.global_transform
	while _acc >= DT:
		_acc -= DT
		_simulate(g, caps)
	_pose_bones()


func _simulate(g: Transform3D, caps: Array) -> void:
	var grav := Vector3.DOWN * gravity * DT * DT
	var n := rows * cols
	if _target.size() != n:
		_target.resize(n)
	for k in n:
		_target[k] = g * rest_local[k]
	for j in cols:
		_p[j] = _target[j]
		_prev[j] = _p[j]
	for k in range(cols, n):
		var v := (_p[k] - _prev[k]) * damping
		_prev[k] = _p[k]
		_p[k] += v + grav
		_p[k] = _p[k].lerp(_target[k], shape_k)
	for it in iterations:
		for i in range(1, rows):
			for j in cols:
				var k := i * cols + j
				_constrain(k, k - cols, seg_len, 1.0)
				var jn := j + 1
				if jn >= cols:
					if not closed:
						continue
					jn = 0
				_constrain(k, i * cols + jn, _rest_h[i], 0.5)
	# Collisions and the drift limit once per step, after the constraints have settled.
	for k in range(cols, n):
		var q := _p[k]
		for cap in caps:
			q = _push_out(q, cap[0], cap[1], float(cap[2]) + margin)
		var dev := q - _target[k]
		if dev.length_squared() > max_dev * max_dev:
			q = _target[k] + dev.normalized() * max_dev
		if q.y < 0.01:
			q.y = 0.01
		_p[k] = q


func _constrain(k: int, other: int, rest_len: float, w: float) -> void:
	var d := _p[k] - _p[other]
	var l := d.length()
	if l < 1e-6:
		return
	var corr := d * ((l - rest_len) / l)
	if other < cols:
		_p[k] -= corr
	else:
		_p[k] -= corr * 0.5 * w
		_p[other] += corr * 0.5 * w


static func _push_out(q: Vector3, a: Vector3, b: Vector3, r: float) -> Vector3:
	var ab := b - a
	var t := 0.0
	var l2 := ab.length_squared()
	if l2 > 1e-8:
		t = clampf((q - a).dot(ab) / l2, 0.0, 1.0)
	var c := a + ab * t
	var d := q - c
	var dl := d.length()
	if dl < r and dl > 1e-6:
		return c + d * (r / dl)
	return q


## Each point's frame: -y down its column, x along its row (the sculptor binds the same way).
func _pose_bones() -> void:
	if skeleton == null:
		return
	if _first == 1:
		skeleton.set_bone_pose(0, anchor_joint.global_transform)
	for i in rows:
		var up := maxi(i - 1, 0)
		var dn := mini(i + 1, rows - 1)
		for j in cols:
			var jl := (j - 1 + cols) % cols if closed else maxi(j - 1, 0)
			var jr := (j + 1) % cols if closed else mini(j + 1, cols - 1)
			var k := i * cols + j
			var y := -(_p[dn * cols + j] - _p[up * cols + j]).normalized()
			var side := _p[i * cols + jr] - _p[i * cols + jl]
			var x := (side - y * side.dot(y)).normalized()
			skeleton.set_bone_pose(_first + k, Transform3D(Basis(x, y, x.cross(y)), _p[k]))
