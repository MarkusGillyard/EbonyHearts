class_name Rig
extends Node3D
## A fighter's skeleton: a joint hierarchy posed every frame from a Pose by forward kinematics
## for the spine and two-bone IK for the limbs. The sculpted body (CharacterModel) follows these
## joints. Rig space has x right, y up and -z toward the opponent; the node itself sits at the
## fighter's root on the ground.

const JOINTS := ["hips", "spine", "chest", "neck", "head", "clav_l", "upperarm_l", "forearm_l", "hand_l",
	"clav_r", "upperarm_r", "forearm_r", "hand_r", "thigh_l", "shin_l", "foot_l", "thigh_r", "shin_r", "foot_r"]
const PARENT := {"hips": "", "spine": "hips", "chest": "spine", "neck": "chest", "head": "neck",
	"clav_l": "chest", "upperarm_l": "clav_l", "forearm_l": "upperarm_l", "hand_l": "forearm_l",
	"clav_r": "chest", "upperarm_r": "clav_r", "forearm_r": "upperarm_r", "hand_r": "forearm_r",
	"thigh_l": "hips", "shin_l": "thigh_l", "foot_l": "shin_l", "thigh_r": "hips", "shin_r": "thigh_r", "foot_r": "shin_r"}

var h := 1.8
var joints := {}
var rest := {}
## Rig-space transforms from the last apply().
var xf := {}
var upper_len := 0.3
var fore_len := 0.27
var thigh_len := 0.39
var shin_len := 0.43
var ankle_h := 0.08
var look: Dictionary = {}
## How closed each hand is, left and right: 1 a fist, 0 an open palm.
var fist := [1.0, 1.0]
var _mats := {}


func build(def: FighterDef) -> void:
	h = def.height
	look = def.look
	var b: Dictionary = look.get("build", {})
	var sw := float(b.get("shoulders", 1.0))
	var hw := float(b.get("hips", 1.0))
	upper_len = 0.172 * h
	fore_len = 0.152 * h
	thigh_len = 0.232 * h
	shin_len = 0.238 * h
	ankle_h = 0.042 * h
	rest = {
		"hips": Vector3(0, 0.53 * h, 0), "spine": Vector3(0, 0.05 * h, 0), "chest": Vector3(0, 0.08 * h, 0),
		"neck": Vector3(0, 0.185 * h, 0), "head": Vector3(0, 0.04 * h, 0),
		"clav_l": Vector3(-0.022 * h, 0.15 * h, 0.005 * h), "upperarm_l": Vector3(-0.093 * h * sw, 0, 0),
		"forearm_l": Vector3(0, -upper_len, 0), "hand_l": Vector3(0, -fore_len, 0),
		"clav_r": Vector3(0.022 * h, 0.15 * h, 0.005 * h), "upperarm_r": Vector3(0.093 * h * sw, 0, 0),
		"forearm_r": Vector3(0, -upper_len, 0), "hand_r": Vector3(0, -fore_len, 0),
		"thigh_l": Vector3(-0.055 * h * hw, -0.025 * h, 0), "shin_l": Vector3(0, -thigh_len, 0), "foot_l": Vector3(0, -shin_len, 0),
		"thigh_r": Vector3(0.055 * h * hw, -0.025 * h, 0), "shin_r": Vector3(0, -thigh_len, 0), "foot_r": Vector3(0, -shin_len, 0),
	}
	for j in JOINTS:
		var n := Node3D.new()
		n.name = j
		joints[j] = n
		var p: String = PARENT[j]
		(self if p == "" else joints[p]).add_child(n)
		n.position = rest[j]
		xf[j] = Transform3D(Basis.IDENTITY, rest[j])


# --- Materials for props and cloth ---------------------------------------------------------

func mat(key: String) -> StandardMaterial3D:
	if _mats.has(key):
		return _mats[key]
	var spec: Dictionary = look.get("materials", {}).get(key, {})
	var c := Color(str(spec.get("color", "#c08a62")))
	var m := Shapes.material(c, float(spec.get("roughness", 0.7)), float(spec.get("metallic", 0.0)), float(spec.get("emission", 0.0)), float(spec.get("rim", 0.35)))
	if spec.get("two_sided", false):
		m.cull_mode = BaseMaterial3D.CULL_DISABLED
	_mats[key] = m
	return m


# --- Posing ------------------------------------------------------------------------------------

## Poses the body. `feet` holds the final rig-space ankle targets and `foot_yaw` the yaw of a
## planted foot (the animator handles planting); a foot whose pose marks it free follows its
## shin instead. `extra` adds per-joint rotations on top of the pose.
func apply(p: Pose, feet: Array, foot_yaw: Array, extra: Dictionary) -> void:
	_set_local("hips", Transform3D(Basis(p.rot["hips_rot"] * _q(extra, "hips")), rest["hips"] + p.pos["hips"]))
	for j: String in ["spine", "chest", "neck", "head", "clav_l", "clav_r"]:
		_set_local(j, Transform3D(Basis(p.rot[j] * _q(extra, j)), rest[j]))
	_arm("l", p)
	_arm("r", p)
	_leg("l", feet[0], p.pos["knee_l"], foot_yaw[0], p.val["free_l"], p.val["toe_l"])
	_leg("r", feet[1], p.pos["knee_r"], foot_yaw[1], p.val["free_r"], p.val["toe_r"])
	fist[0] = p.val["fist_l"]
	fist[1] = p.val["fist_r"]


static func _q(extra: Dictionary, k: String) -> Quaternion:
	return extra.get(k, Quaternion.IDENTITY)


func _set_local(j: String, local: Transform3D) -> void:
	var n: Node3D = joints[j]
	n.transform = local
	var p: String = PARENT[j]
	xf[j] = local if p == "" else xf[p] * local


func _set_global(j: String, g: Transform3D) -> void:
	var p: String = PARENT[j]
	var local: Transform3D = g if p == "" else (xf[p] as Transform3D).affine_inverse() * g
	(joints[j] as Node3D).transform = local
	xf[j] = g


func _arm(s: String, p: Pose) -> void:
	var up := "upperarm_" + s
	var parent_xf: Transform3D = xf["clav_" + s]
	var shoulder: Vector3 = parent_xf * rest[up]
	var target: Vector3 = p.pos["hand_" + s]
	var pole: Vector3 = p.pos["elbow_" + s]
	var sol := two_bone(shoulder, target, pole, upper_len, fore_len)
	var elbow: Vector3 = sol[0]
	var wrist: Vector3 = sol[1]
	var axis: Vector3 = sol[2]
	_set_global(up, Transform3D(aim_basis(elbow - shoulder, axis), shoulder))
	_set_global("forearm_" + s, Transform3D(aim_basis(wrist - elbow, axis), elbow))
	var hb: Basis = (xf["forearm_" + s] as Transform3D).basis * Basis(p.rot["wrist_" + s])
	var aim: float = p.val["aim_" + s]
	if aim > 0.0:
		var y := -(p.pos["fingers_" + s] as Vector3).normalized()
		var z: Vector3 = p.pos["palm_" + s]
		z = (z - y * z.dot(y)).normalized()
		if z.length() > 0.5:
			var explicit := Basis(y.cross(z), y, z)
			hb = Basis(Quaternion(hb.orthonormalized()).slerp(Quaternion(explicit), clampf(aim, 0.0, 1.0)))
	_set_global("hand_" + s, Transform3D(hb, wrist))


func _leg(s: String, ankle_target: Vector3, pole: Vector3, yaw: float, free: float, toe_deg: float) -> void:
	var th := "thigh_" + s
	var hip: Vector3 = (xf["hips"] as Transform3D) * rest[th]
	var sol := two_bone(hip, ankle_target, pole, thigh_len, shin_len)
	var knee: Vector3 = sol[0]
	var ankle: Vector3 = sol[1]
	var axis: Vector3 = sol[2]
	var shin_b := aim_basis(ankle - knee, axis)
	_set_global(th, Transform3D(aim_basis(knee - hip, axis), hip))
	_set_global("shin_" + s, Transform3D(shin_b, knee))
	var flat := Quaternion(Vector3.UP, yaw)
	var pointed := Quaternion(shin_b * Basis(Vector3.RIGHT, deg_to_rad(-toe_deg)))
	_set_global("foot_" + s, Transform3D(Basis(flat.slerp(pointed, clampf(free, 0.0, 1.0))), ankle))


## Two-bone IK. Returns [middle joint, end point, bend axis] in the same space as the inputs.
static func two_bone(root: Vector3, target: Vector3, pole: Vector3, a: float, b: float) -> Array:
	var to := target - root
	var d := clampf(to.length(), absf(a - b) + 0.001, a + b - 0.0005)
	var dir := to.normalized() if to.length() > 1e-5 else Vector3.DOWN
	var cos_a := clampf((a * a + d * d - b * b) / (2.0 * a * d), -1.0, 1.0)
	var sin_a := sqrt(1.0 - cos_a * cos_a)
	var pv := pole - root
	var perp := pv - dir * pv.dot(dir)
	if perp.length() < 1e-5:
		perp = Vector3.BACK - dir * Vector3.BACK.dot(dir)
	perp = perp.normalized()
	var mid := root + dir * (a * cos_a) + perp * (a * sin_a)
	var end := root + dir * d
	var axis := perp.cross(dir).normalized()
	return [mid, end, axis]


## Basis whose -Y points along `along` and whose X is the bend axis.
static func aim_basis(along: Vector3, bend_axis: Vector3) -> Basis:
	var y := -along.normalized()
	var x := bend_axis - y * bend_axis.dot(y)
	if x.length() < 1e-5:
		x = Vector3.RIGHT - y * y.x
	x = x.normalized()
	var z := x.cross(y)
	return Basis(x, y, z)


## Rig-space centre of a fist or palm, for checking strikes against their hitboxes.
func hand_tip(s: String) -> Vector3:
	var t: Transform3D = xf["hand_" + s]
	return t * Vector3(0, -0.045 * h, 0)


func foot_tip(s: String) -> Vector3:
	var t: Transform3D = xf["foot_" + s]
	return t * Vector3(0, -0.015 * h, -0.06 * h)


func joint_pos(j: String) -> Vector3:
	return (xf[j] as Transform3D).origin
