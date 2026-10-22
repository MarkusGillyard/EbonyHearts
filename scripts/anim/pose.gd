class_name Pose
extends RefCounted
## One full-body key pose. Spine and head are rotations (FK); hands and feet are IK targets in
## the fighter's root space, written in data as [forward, up, right] metres, angles in degrees.
## Data may name a "base" pose and override only what changes.

const ROT_KEYS := ["hips_rot", "spine", "chest", "neck", "head", "clav_l", "clav_r", "wrist_l", "wrist_r"]
const POS_KEYS := ["hips", "hand_l", "hand_r", "elbow_l", "elbow_r", "foot_l", "foot_r", "knee_l", "knee_r",
	"fingers_l", "fingers_r", "palm_l", "palm_r"]
## aim_l/aim_r weigh an explicit hand orientation (fingers and palm directions) against simply
## continuing the forearm; data that gives "fingers_l" sets "aim_l" to 1 automatically.
const FLOAT_KEYS := ["fist_l", "fist_r", "yaw_l", "yaw_r", "toe_l", "toe_r", "free_l", "free_r", "prop", "staff", "glow",
	"aim_l", "aim_r", "eye_alt"]
const DIR_KEYS := ["fingers_l", "fingers_r", "palm_l", "palm_r"]

var rot := {}
var pos := {}
var val := {}


func _init() -> void:
	for k in ROT_KEYS:
		rot[k] = Quaternion.IDENTITY
	pos["hips"] = Vector3.ZERO
	pos["hand_l"] = Vector3(-0.25, 0.85, 0.0)
	pos["hand_r"] = Vector3(0.25, 0.85, 0.0)
	pos["elbow_l"] = Vector3(-0.5, 1.1, 0.4)
	pos["elbow_r"] = Vector3(0.5, 1.1, 0.4)
	pos["foot_l"] = Vector3(-0.12, 0.0, 0.0)
	pos["foot_r"] = Vector3(0.12, 0.0, 0.0)
	pos["knee_l"] = Vector3(-0.15, 0.5, -0.8)
	pos["knee_r"] = Vector3(0.15, 0.5, -0.8)
	pos["fingers_l"] = Vector3.DOWN
	pos["fingers_r"] = Vector3.DOWN
	pos["palm_l"] = Vector3.RIGHT
	pos["palm_r"] = Vector3.LEFT
	for k in FLOAT_KEYS:
		val[k] = 0.0
	val["fist_l"] = 1.0
	val["fist_r"] = 1.0


func duplicate_pose() -> Pose:
	var p := Pose.new()
	p.rot = rot.duplicate()
	p.pos = pos.duplicate()
	p.val = val.duplicate()
	return p


## Data [forward, up, right] to rig space (x right, y up, -z forward).
static func data_to_local(a: Array, scale := 1.0) -> Vector3:
	return Vector3(float(a[2]), float(a[1]), -float(a[0])) * scale


static func euler_deg(a: Array) -> Quaternion:
	return Quaternion.from_euler(Vector3(deg_to_rad(float(a[0])), deg_to_rad(float(a[1])), deg_to_rad(float(a[2]))))


## Parses `d` on top of `base`. `scale` converts positions authored for another height.
static func from_dict(d: Dictionary, base: Pose, scale := 1.0) -> Pose:
	var p := base.duplicate_pose() if base else Pose.new()
	for k in ROT_KEYS:
		if d.has(k):
			p.rot[k] = euler_deg(d[k])
	for k in POS_KEYS:
		if d.has(k):
			p.pos[k] = data_to_local(d[k], 1.0 if k in DIR_KEYS else scale)
	for k in FLOAT_KEYS:
		if d.has(k):
			p.val[k] = float(d[k])
	for s in ["l", "r"]:
		if d.has("fingers_" + s) and not d.has("aim_" + s):
			p.val["aim_" + s] = 1.0
	return p


## Mirrors left and right, for poses authored for one side.
func mirrored() -> Pose:
	var p := Pose.new()
	mirror_into(p)
	return p


## Writes the left-right mirror of this pose into `p` (no allocation).
func mirror_into(p: Pose) -> void:
	for k in ROT_KEYS:
		var q: Quaternion = rot[k]
		var m := Quaternion(q.x, -q.y, -q.z, q.w)
		p.rot[_swap(k)] = m
	for k in POS_KEYS:
		var v: Vector3 = pos[k]
		p.pos[_swap(k)] = Vector3(-v.x, v.y, v.z)
	for k in FLOAT_KEYS:
		# A foot turned one way turns the other way in the mirror.
		p.val[_swap(k)] = -val[k] if k.begins_with("yaw_") else val[k]


static func _swap(k: String) -> String:
	if k.ends_with("_l"):
		return k.trim_suffix("_l") + "_r"
	if k.ends_with("_r"):
		return k.trim_suffix("_r") + "_l"
	return k


## Writes the blend of a and b at t into this pose (t = 0 gives a).
func blend_into(a: Pose, b: Pose, t: float) -> void:
	for k in ROT_KEYS:
		rot[k] = (a.rot[k] as Quaternion).slerp(b.rot[k], t)
	for k in POS_KEYS:
		pos[k] = (a.pos[k] as Vector3).lerp(b.pos[k], t)
	for k in FLOAT_KEYS:
		val[k] = lerpf(a.val[k], b.val[k], t)


static func ease(kind: String, t: float) -> float:
	t = clampf(t, 0.0, 1.0)
	match kind:
		"linear":
			return t
		"in":
			return t * t
		"out":
			return 1.0 - (1.0 - t) * (1.0 - t)
		"snap":
			return 1.0 - pow(1.0 - t, 3.0)
		"hold":
			return 0.0 if t < 1.0 else 1.0
	return t * t * (3.0 - 2.0 * t)
