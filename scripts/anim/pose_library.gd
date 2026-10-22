class_name PoseLibrary
extends RefCounted
## Every pose one fighter can show: shared poses from data/poses_common.json (authored for a
## 1.8 m body and scaled to the fighter) overridden by the fighter's own, plus the keyframe
## lists of its moves.

const COMMON_HEIGHT := 1.8
## Shared poses a fighter's "pose_patch" applies to (reactions where the arms only flinch).
const PATCHED := ["guard_hit", "guard_hit_low", "hit_head", "hit_body", "hit_gut", "hit_low", "hit_back", "parried",
	"crumple", "throw_break", "thrown_hold", "rise_2"]

var poses: Dictionary = {}
## move id -> Array of {"f": float, "pose": Pose, "ease": String}
var move_keys: Dictionary = {}
var victim_keys: Dictionary = {}
var missing: Array[String] = []
var _src: Dictionary = {}
var _scale := {}


func build(def: FighterDef, common: Dictionary) -> void:
	var s := def.height / COMMON_HEIGHT
	var patch: Dictionary = def.raw.get("pose_patch", {})
	for k in common:
		_src[k] = common[k]
		if not patch.is_empty() and k in PATCHED:
			var merged: Dictionary = (common[k] as Dictionary).duplicate()
			merged.merge(patch, true)
			_src[k] = merged
		_scale[k] = s
	for k in def.poses:
		_src[k] = def.poses[k]
		_scale[k] = 1.0
	for k in _src:
		get_pose(k)
	for id in def.moves:
		var m: MoveDef = def.moves[id]
		move_keys[id] = _keys(m.anim.get("keys", []))
		if m.anim.has("victim"):
			victim_keys[id] = _keys(m.anim["victim"])


func has(name: String) -> bool:
	return _src.has(name)


func get_pose(name: String) -> Pose:
	if poses.has(name):
		return poses[name]
	if not _src.has(name):
		if not missing.has(name):
			missing.append(name)
		return poses.get("stance", Pose.new())
	var d: Dictionary = _src[name]
	var p: Pose
	if d.has("mirror"):
		p = get_pose(str(d["mirror"])).mirrored()
		p = Pose.from_dict(d, p, _scale[name])
	else:
		var base: Pose = get_pose(str(d["base"])) if d.has("base") else null
		p = Pose.from_dict(d, base, _scale[name])
	poses[name] = p
	return p


func _keys(raw: Array) -> Array:
	var out := []
	for k in raw:
		out.append({"f": float(k[0]), "pose": get_pose(str(k[1])), "ease": str(k[2]) if k.size() > 2 else "inout"})
	return out


## Pose at fractional frame `t` of a key list, starting from `from` at frame 0.
static func sample(keys: Array, t: float, from: Pose, out: Pose) -> void:
	if keys.is_empty():
		out.blend_into(from, from, 0.0)
		return
	var prev_f := 0.0
	var prev_p := from
	for k in keys:
		if t <= k["f"]:
			var span: float = maxf(k["f"] - prev_f, 0.001)
			out.blend_into(prev_p, k["pose"], Pose.ease(k["ease"], (t - prev_f) / span))
			return
		prev_f = k["f"]
		prev_p = k["pose"]
	out.blend_into(prev_p, prev_p, 0.0)
