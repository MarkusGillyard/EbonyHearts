class_name FighterView
extends Node3D
## Draws one Fighter on the fighting plane. Every rendered frame it picks the pose the simulation
## state calls for, crossfades from what was on screen, mirrors it when the fighter faces left
## (so the same side of the body always faces the camera, as in a sprite game), plants the feet,
## adds hit-reaction springs, flips and spins airborne bodies and steps the cloth. It only reads
## the simulation, never changes it.

const WAKE_KEYS := [[8, "rise_1", "out"], [16, "rise_2", "inout"], [24, "stance", "inout"]]
const REACTIONS := {"high": "hit_head", "mid": "hit_body", "crouch": "hit_low", "guard": "block_hit",
	"crouch_guard": "block_low_hit", "dazed": "dazed", "pulled": "hit_body", "break": "throw_break", "crumple": "hit_gut"}

var fighter: Fighter
var def: FighterDef
var lib: PoseLibrary
var rig: Rig
var model: CharacterModel
var cloths: Array[ClothSheet] = []
var props: Array[Node3D] = []
var smoothing := true
## Time spent stepping cloth, for the benchmark readout.
var cloth_usec := 0

var _prev_pos := Vector2.ZERO
var _cur_pos := Vector2.ZERO
var _facing := 1
var _out := Pose.new()
var _mirror := Pose.new()
var _snap := Pose.new()
var _src := Pose.new()
var _key := ""
var _blend_t := 0.0
var _blend_len := 0.0
var _keys: Array = []
var _feet: Array[Dictionary] = []
var _lean := Vector3.ZERO
var _lean_v := Vector3.ZERO
var _spring := {"chest": [Vector3.ZERO, Vector3.ZERO], "head": [Vector3.ZERO, Vector3.ZERO], "hips": [Vector3.ZERO, Vector3.ZERO]}
var _time := 0.0
var _vis_vel := Vector3.ZERO
var _last_root := Vector3.ZERO
var _shake := 0.0
var _move_tick := 0
var _last_state := -1
var _last_move: MoveDef = null
## Whole-body rotation about the hips while airborne (degrees, + pitches backward).
var _air_pitch := 0.0
var _air_pitch_v := 0.0
var _flip := 0.0
var _run_phase := 0.0
var _mats: Array[ShaderMaterial] = []
var _body_mat: ShaderMaterial
var _eye_mat: ShaderMaterial
var _hair_mat: ShaderMaterial
var _brow_mat: ShaderMaterial
var _eye_base := 0.0
var _flash := 0.0
## Presentation overrides set by the finisher director: scale, sink depth, hidden.
var extra_scale := Vector3.ONE
var sink := 0.0
var lift_extra := Vector3.ZERO
var spin_extra := 0.0
## A pose the finisher director shows instead of the simulation's ("" for none).
var pose_override := ""
var _whip: MeshInstance3D


func setup(f: Fighter, common_poses: Dictionary) -> void:
	fighter = f
	def = f.def
	lib = PoseLibrary.new()
	lib.build(def, common_poses)
	rig = Rig.new()
	rig.name = "Rig"
	add_child(rig)
	rig.build(def)
	_build_model()
	_build_props()
	_whip = MeshInstance3D.new()
	var wm := CylinderMesh.new()
	wm.top_radius = 0.009
	wm.bottom_radius = 0.009
	wm.height = 1.0
	wm.radial_segments = 6
	_whip.mesh = wm
	_whip.material_override = Shapes.material(Color(0.33, 0.33, 0.31), 0.45, 0.85)
	_whip.top_level = true
	_whip.visible = false
	_whip.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_whip)
	_out = lib.get_pose("stance").duplicate_pose()
	for i in 2:
		_feet.append({"pos": Vector3.ZERO, "yaw": 0.0, "step": -1.0, "from": Vector3.ZERO, "from_yaw": 0.0, "dur": 0.15, "valid": false})
	sync_from_sim(true)
	update_visual(1.0, 0.0)
	_build_cloth()


# --- Model, props and cloth ---------------------------------------------------------------------

func _build_model() -> void:
	_body_mat = ShaderMaterial.new()
	_body_mat.shader = preload("res://assets/shaders/character.gdshader")
	_eye_mat = ShaderMaterial.new()
	_eye_mat.shader = preload("res://assets/shaders/eye.gdshader")
	var mats: Dictionary = def.look.get("materials", {})
	var eyes: Dictionary = mats.get("eyes", {})
	_eye_mat.set_shader_parameter("iris_color", Color(str(eyes.get("color", "#6a5a48"))))
	_eye_base = float(eyes.get("emission", 0.0)) * 0.5
	var hair_col := Color(str(mats.get("hair", {}).get("color", "#201810")))
	_hair_mat = _hair_material(hair_col, hair_col.lightened(0.18), 7.0, 0.05, float(mats.get("hair", {}).get("roughness", 0.42)))
	var brow_col := Color(str(mats.get("brows", {}).get("color", hair_col.to_html())))
	_brow_mat = _hair_material(brow_col, brow_col, 2.0, 0.0)
	model = CharacterModel.new()
	model.name = "Model"
	rig.add_child(model)
	var model_id := str(def.look.get("model", def.id))
	if not FileAccess.file_exists(CharacterModel.path_for(model_id)):
		# Only during development, before a fighter has been sculpted; tests require every model.
		push_warning("no model for %s yet; showing ashka" % model_id)
		model_id = "ashka"
	model.build(rig, model_id, {"body": _body_mat, "eye": _eye_mat, "hair": _hair_mat, "brows": _brow_mat})
	_mats.append_array([_body_mat, _eye_mat, _hair_mat, _brow_mat])


static func _hair_material(col: Color, tip: Color, strands: float, root_fade: float, roughness := 0.42) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = preload("res://assets/shaders/hair.gdshader")
	m.set_shader_parameter("roughness", roughness)
	m.set_shader_parameter("color", col)
	m.set_shader_parameter("tip_color", tip)
	m.set_shader_parameter("strands", strands)
	m.set_shader_parameter("root_fade", root_fade)
	return m


func _build_props() -> void:
	for spec: Dictionary in def.look.get("props", []):
		var node := Props.build(spec, rig)
		if node == null:
			continue
		var joint := str(spec.get("joint", "hand_r"))
		(rig.joints[joint] as Node3D).add_child(node)
		node.set_meta("spec", spec)
		props.append(node)
		for mi in node.find_children("*", "MeshInstance3D", true, false):
			var m: Material = (mi as MeshInstance3D).material_override
			if m is ShaderMaterial:
				_mats.append(m)


func _build_cloth() -> void:
	for spec in def.look.get("cloth", []):
		var c := ClothSheet.new()
		add_child(c)
		var kind := str(spec.get("kind", "skirt"))
		var shape := ClothSheet.rest_shape(spec, rig.h)
		c.setup(rig.joints[str(spec.get("joint", "hips"))], shape["cols"], shape["rows"], shape["closed"], shape["points"])
		c.shape_k = float(spec.get("shape", 0.08))
		c.damping = float(spec.get("damping", 0.97))
		c.max_dev = float(spec.get("max_dev", 0.3 if kind == "hair" else 0.35)) * rig.h / 1.8
		c.colliders = _colliders.bind(kind == "hair")
		var i := cloths.size()
		if i < model.cloth_skeletons.size():
			c.drive(model.cloth_skeletons[i], model.cloth_anchored[i])
		cloths.append(c)


## Body capsules in world space for cloth collision.
func _colliders(for_hair: bool) -> Array:
	var g := rig.global_transform
	var out := []
	var hh := rig.h
	var pairs := [["thigh_l", "shin_l", 0.062], ["thigh_r", "shin_r", 0.062], ["shin_l", "foot_l", 0.045], ["shin_r", "foot_r", 0.045]]
	if for_hair:
		pairs = [["spine", "neck", 0.09], ["neck", "head", 0.04], ["hips", "spine", 0.085]]
		out.append([g * (rig.joint_pos("head") + Vector3(0, 0.05 * hh, 0)), g * (rig.joint_pos("head") + Vector3(0, 0.05 * hh, 0)), 0.068 * hh])
	else:
		out.append([g * rig.joint_pos("hips"), g * rig.joint_pos("spine"), 0.085 * hh])
	for p in pairs:
		out.append([g * rig.joint_pos(p[0]), g * rig.joint_pos(p[1]), float(p[2]) * hh])
	return out


# --- Effects --------------------------------------------------------------------------------------

func set_shader(param: String, value: Variant) -> void:
	for m in _mats:
		m.set_shader_parameter(param, value)


func set_tint(color: Color, amount: float, gloss := 0.0) -> void:
	set_shader("tint_color", color)
	set_shader("tint", amount)
	set_shader("tint_gloss", gloss)


func set_dissolve(amount: float, color := Color(1.0, 0.45, 0.1)) -> void:
	set_shader("dissolve_color", color)
	set_shader("dissolve", amount)
	if model.parts.has("eye_l"):
		for e in ["eye_l", "eye_r"]:
			(model.parts[e] as Node3D).visible = amount < 0.55
	for p in props:
		p.visible = amount < 0.4


## Menu previews render into transparent viewports, where Godot cannot scatter light under the
## skin (it switches it off with a warning); they draw with a copy of the shader without it.
func disable_subsurface() -> void:
	var flat := Shader.new()
	flat.code = _body_mat.shader.code.replace("SSS_STRENGTH =", "float sss_unused =")
	_body_mat.shader = flat


func set_costume(hue: float, sat: float) -> void:
	_body_mat.set_shader_parameter("cloth_hue", hue)
	_body_mat.set_shader_parameter("cloth_sat", sat)


func flash(amount: float) -> void:
	_flash = maxf(_flash, amount)


# --- Per tick and per frame ---------------------------------------------------------------------

## Call right after every simulation tick.
func sync_from_sim(teleport := false) -> void:
	_prev_pos = fighter.pos if teleport else _cur_pos
	_cur_pos = fighter.pos
	if fighter.state in [Fighter.S.MOVE, Fighter.S.FINISHER] and (fighter.move != _last_move or _last_state != fighter.state):
		_move_tick = fighter.sim.tick_count if fighter.sim else 0
	_last_state = fighter.state
	_last_move = fighter.move
	if teleport:
		for ft in _feet:
			ft["valid"] = false
		for c in cloths:
			c.reset()


func reset_cloth() -> void:
	for c in cloths:
		c.reset()


## `frac` is how far rendering is between the previous tick and the current one.
func update_visual(frac: float, delta: float) -> void:
	var frozen := fighter.hitstop > 0
	if not smoothing or frozen:
		frac = 1.0
	_time += delta
	var p2 := _prev_pos.lerp(_cur_pos, frac)
	var p := Vector3(p2.x, p2.y - sink - _move_sink(frac), 0.0)
	if fighter.facing != _facing:
		_facing = fighter.facing
		for ft in _feet:
			ft["valid"] = false
	if frozen and _shake > 0.0:
		p.x += sin(_time * 95.0) * _shake
	if delta > 0.0:
		_vis_vel = _vis_vel.lerp((p - _last_root) / delta, clampf(delta * 12.0, 0.0, 1.0))
	_last_root = p
	var basis := Basis.looking_at(Vector3(_facing, 0, 0), Vector3.UP)
	basis = Basis.from_scale(extra_scale) * basis
	global_transform = Transform3D(basis, p + lift_extra) * _air_tilt(delta, frozen)
	_evaluate_source(frac)
	if delta > 0.0:
		_blend_t += delta * 60.0 * (0.0 if frozen else 1.0)
	var w := 1.0
	if _blend_len > 0.0:
		w = Pose.ease("inout", _blend_t / _blend_len)
	_out.blend_into(_snap, _src, w)
	var shown := _out
	if _facing < 0:
		_out.mirror_into(_mirror)
		shown = _mirror
	var extra := _procedural(delta, frozen)
	var feet := _plant_feet(delta, shown)
	rig.apply(shown, feet[0], feet[1], extra)
	model.sync(_look_target())
	_update_props(shown)
	if _flash > 0.0:
		set_shader("flash", _flash)
		_flash = maxf(0.0, _flash - delta * 8.0)
		if _flash <= 0.0:
			set_shader("flash", 0.0)
	var t_cloth := Time.get_ticks_usec()
	if not frozen:
		for c in cloths:
			c.step(delta)
	cloth_usec += Time.get_ticks_usec() - t_cloth


func _set_source(key: String, blend_ticks: float) -> bool:
	if key == _key:
		return false
	_snap = _out.duplicate_pose()
	_key = key
	_blend_t = 0.0
	_blend_len = blend_ticks
	return true


func _hold(name: String, blend: float) -> void:
	var nm := name if lib.has(name) else "stance"
	_set_source("pose:" + nm, blend)
	var pz := lib.get_pose(nm)
	_src.blend_into(pz, pz, 0.0)


func _evaluate_source(frac: float) -> void:
	var f := fighter
	var t := float(f.sf) - 1.0 + frac
	if pose_override != "":
		_hold(pose_override, 6.0)
		return
	match f.state:
		Fighter.S.IDLE, Fighter.S.WALK_F, Fighter.S.WALK_B:
			_hold("stance", 7.0)
		Fighter.S.INTRO:
			_hold("intro", 10.0)
		Fighter.S.WIN:
			_hold("win", 14.0)
		Fighter.S.CROUCH, Fighter.S.PREJUMP, Fighter.S.LAND:
			_hold("crouch", 3.0 if f.state != Fighter.S.CROUCH else 4.0)
		Fighter.S.BLOCK:
			_hold("block", 3.0)
		Fighter.S.CROUCH_BLOCK:
			_hold("block_low", 3.0)
		Fighter.S.RUN:
			_run_cycle()
		Fighter.S.AIR:
			if f.jump_dir != 0:
				_hold("jump_tuck", 4.0)
			else:
				_hold("jump", 5.0)
		Fighter.S.MOVE, Fighter.S.FINISHER:
			if f.throw_victim != null:
				if _set_source("throw:%d" % _move_tick, 0.0):
					_keys = _anim_keys(f.move, "hold")
				PoseLibrary.sample(_keys, float(f.throw_t) - 1.0 + frac, _snap, _src)
			else:
				if _set_source("move:%s:%d" % [f.move.id, _move_tick], 0.0):
					_keys = lib.move_keys.get(f.move.id, [])
				PoseLibrary.sample(_keys, float(f.mf) - 1.0 + frac, _snap, _src)
		Fighter.S.BLOCKSTUN, Fighter.S.HITSTUN:
			var react := str(REACTIONS.get(f.stun_kind, "hit_body"))
			var total := float(f.stun + f.sf)
			if _set_source("stun:%d:%s" % [f.sim.tick_count - f.sf if f.sim else 0, f.stun_kind], 0.0):
				var back := "crouch" if f.crouch_stun else "stance"
				if f.stun_kind in ["guard", "crouch_guard"]:
					back = "block_low" if f.crouch_stun else "block"
				if f.stun_kind == "dazed":
					_keys = [{"f": 4.0, "pose": lib.get_pose("dazed"), "ease": "out"}, {"f": total, "pose": lib.get_pose("dazed"), "ease": "linear"}]
				elif f.stun_kind == "crumple":
					_keys = [{"f": 3.0, "pose": lib.get_pose("hit_gut"), "ease": "out"}, {"f": total, "pose": lib.get_pose("crumple"), "ease": "inout"}]
				else:
					# Recoil from the current pose, then ease straight back; bigger stuns rock further.
					var peak := _reaction_pose(react, back, total)
					var settle := Pose.new()
					settle.blend_into(peak, lib.get_pose(back), 0.45)
					_keys = [{"f": 2.0, "pose": peak, "ease": "snap"},
						{"f": maxf(3.0, total * 0.4), "pose": settle, "ease": "out"},
						{"f": total, "pose": lib.get_pose(back), "ease": "inout"}]
			PoseLibrary.sample(_keys, t, _snap, _src)
		Fighter.S.JUGGLE:
			_hold("air_hit" if f.vel.y > 0.0 else "air_fall", 5.0)
		Fighter.S.FALL:
			var swept := f.last_hit_move != null and f.last_hit_move.effect == "sweep"
			_hold("swept" if swept else "air_fall", 4.0)
		Fighter.S.DOWN, Fighter.S.KO:
			_hold("down", 6.0)
		Fighter.S.WAKEUP:
			if _set_source("wake:%d" % (f.sim.tick_count - f.sf if f.sim else 0), 0.0):
				_keys = []
				for k in WAKE_KEYS:
					_keys.append({"f": float(k[0]), "pose": lib.get_pose(str(k[1])), "ease": str(k[2])})
			PoseLibrary.sample(_keys, t, _snap, _src)
		Fighter.S.THROWN:
			var th := f.thrown_by
			if th and th.move:
				if _set_source("thrown:%s:%d" % [th.move.id, f.sim.tick_count - f.sf if f.sim else 0], 0.0):
					_keys = _anim_keys(th.move, "victim")
				PoseLibrary.sample(_keys, float(th.throw_t) - 1.0 + frac, _snap, _src)
		Fighter.S.DIZZY, Fighter.S.VICTIM:
			_hold("dizzy", 12.0)


func _anim_keys(m: MoveDef, part: String) -> Array:
	var out := []
	for k in m.anim.get(part, []):
		out.append({"f": float(k[0]), "pose": lib.get_pose(str(k[1])), "ease": str(k[2]) if k.size() > 2 else "inout"})
	return out


## A two-pose stride cycle timed to the distance covered, so the feet keep pace with the ground.
func _run_cycle() -> void:
	_set_source("run", 4.0)
	var stride := 1.25 * rig.h / 1.8
	_run_phase = fposmod(absf(fighter.pos.x) / stride, 1.0)
	var a := lib.get_pose("run_a")
	var b := lib.get_pose("run_b")
	var u := 0.5 - 0.5 * cos(_run_phase * TAU)
	_src.blend_into(a, b, u)


## The reaction pose for a fresh stun, only partly reached for short stuns.
func _reaction_pose(react: String, back: String, stun_total: float) -> Pose:
	var r := lib.get_pose(react)
	var out := Pose.new()
	out.blend_into(lib.get_pose(back), r, clampf(0.45 + stun_total / 40.0, 0.55, 1.0))
	return out


## Shows a named pose directly, bypassing the simulation (authoring tools). "bind" shows the pose
## the character model is sculpted in.
func preview(pose_name: String) -> void:
	if pose_name == "bind":
		var bp := CharacterModel.bind_pose(rig)
		rig.apply(bp[0], bp[1], [0.0, 0.0], {})
		model.sync()
		return
	var p := lib.get_pose(pose_name)
	_key = "preview:" + pose_name
	_snap = p.duplicate_pose()
	_src = p.duplicate_pose()
	_out = p.duplicate_pose()
	_blend_len = 0.0
	for ft in _feet:
		ft["valid"] = false
	var feet := _plant_feet(0.0, _out)
	rig.apply(_out, feet[0], feet[1], {})
	model.sync()
	_update_props(_out)


# --- Procedural layers --------------------------------------------------------------------------

func _procedural(delta: float, frozen: bool) -> Dictionary:
	var f := fighter
	var target := Vector3.ZERO
	var local_v := global_transform.basis.inverse() * _vis_vel
	if f.state in [Fighter.S.WALK_F, Fighter.S.WALK_B]:
		target = Vector3(clampf(-local_v.z * 0.045, -0.2, 0.25), 0.0, 0.0)
	if not frozen and delta > 0.0:
		var acc := (target - _lean) * 160.0 - _lean_v * 22.0
		_lean_v += acc * delta
		_lean += _lean_v * delta
		for k in _spring:
			var s: Array = _spring[k]
			var a: Vector3 = -(s[0] as Vector3) * 260.0 - (s[1] as Vector3) * 16.0
			s[1] = (s[1] as Vector3) + a * delta
			s[0] = (s[0] as Vector3) + (s[1] as Vector3) * delta
	var breath := sin(_time * TAU * 0.3) * 0.02 if f.state in [Fighter.S.IDLE, Fighter.S.INTRO, Fighter.S.CROUCH, Fighter.S.WIN, Fighter.S.BLOCK] else 0.0
	var sway := Vector3.ZERO
	if f.state in [Fighter.S.DIZZY, Fighter.S.VICTIM] or (f.state == Fighter.S.HITSTUN and f.stun_kind == "dazed"):
		# Unsteady on their feet: a slow figure-eight of the upper body.
		sway = Vector3(sin(_time * 2.1) * 0.12, sin(_time * 1.05) * 0.18, cos(_time * 2.1) * 0.08)
	var extra := {}
	var side := 1.0 if _facing > 0 else -1.0
	extra["hips"] = Quaternion.from_euler((_spring["hips"][0] as Vector3) * 0.6)
	extra["spine"] = Quaternion.from_euler(Vector3(-_lean.x * 0.5 + sway.x * 0.5, sway.y * 0.4 * side, sway.z * 0.5 * side))
	extra["chest"] = Quaternion.from_euler(Vector3(-_lean.x * 0.5 + breath + sway.x * 0.5, sway.y * 0.3 * side, sway.z * 0.4 * side) + _spring["chest"][0])
	extra["head"] = Quaternion.from_euler(_spring["head"][0] + Vector3(sway.x * 0.6, 0, sway.z * side))
	return extra


## Whole-body rotation about the hips: somersaults on angled jumps, backward tumbles when juggled
## or knocked down, spinning for heavy knockdowns. Returns the transform to append to the root.
func _air_tilt(delta: float, frozen: bool) -> Transform3D:
	var f := fighter
	var pivot := Vector3(0, 0.5 * rig.h, 0)
	var spin := 0.0
	if f.state == Fighter.S.AIR and f.jump_dir != 0:
		# A full forward somersault over the jump, timed by the vertical speed.
		var u := clampf(1.0 - (f.vel.y + f.def.jump_vy) / (2.0 * f.def.jump_vy), 0.0, 1.0)
		_flip = -360.0 * Pose.ease("inout", clampf((u - 0.08) / 0.8, 0.0, 1.0)) * float(f.jump_dir)
	elif delta > 0.0:
		_flip = move_toward(_flip, 0.0 if absf(_flip) < 180.0 else signf(_flip) * 360.0, delta * 900.0)
		if absf(_flip) >= 359.0:
			_flip = 0.0
	if f.state in [Fighter.S.JUGGLE, Fighter.S.FALL]:
		if delta > 0.0 and not frozen:
			var target := clampf(-f.vel.y * 6.0, -20.0, 40.0)
			if f.state == Fighter.S.FALL and absf(f.vel.x) > 3.0:
				target = 80.0
			var acc := (target - _air_pitch) * 80.0 - _air_pitch_v * 11.0
			_air_pitch_v += acc * delta
			_air_pitch += _air_pitch_v * delta
		pivot = Vector3(0, 0.35 * rig.h, 0)
	else:
		_air_pitch_v = 0.0
		_air_pitch = move_toward(_air_pitch, 0.0, delta * 300.0)
	spin = _flip + _air_pitch + spin_extra
	if absf(spin) < 0.01:
		return Transform3D.IDENTITY
	return Transform3D(Basis.IDENTITY, pivot) * Transform3D(Basis(Vector3.RIGHT, deg_to_rad(spin)), Vector3.ZERO) * Transform3D(Basis.IDENTITY, -pivot)


## Kicks the hit-reaction springs. `dir` is +1 when the blow pushes toward +x in the world.
func impact(dir: float, strength: float, level: int, blocked: bool) -> void:
	if fighter.is_airborne():
		_air_pitch_v += 180.0 * clampf(strength, 0.3, 1.5)
	# Pushed away from the attacker: in rig space that is backward (+z) when the attacker is in front.
	var back := 1.0 if signf(dir) == float(_facing) * -1.0 else -1.0
	var s := strength * (0.35 if blocked else 1.0) * back
	var head_k := 1.6 if level == MoveDef.Level.HIGH else 0.7
	var chest_k := 0.4 if level == MoveDef.Level.LOW else 1.0
	(_spring["head"] as Array)[1] += Vector3(-s * head_k * 9.0, 0, 0)
	(_spring["chest"] as Array)[1] += Vector3(-s * chest_k * 6.0, 0, 0)
	(_spring["hips"] as Array)[1] += Vector3(-s * 2.0 * (1.6 if level == MoveDef.Level.LOW else 0.6), 0, 0)
	_shake = clampf(0.004 + strength * 0.012, 0.0, 0.02)
	if not blocked and strength > 0.6:
		flash(0.22)


# --- Feet ---------------------------------------------------------------------------------------

## Returns [[ankle_l, ankle_r] rig space, [yaw_l, yaw_r] rig space]. Planted feet stay put in the
## world until the pose's target drifts too far, then take a short arcing step.
func _plant_feet(delta: float, pose: Pose) -> Array:
	var g := global_transform
	var gi := g.affine_inverse()
	var root_yaw := atan2(-g.basis.z.x, -g.basis.z.z)
	var speed := absf(_vis_vel.x)
	var ankles := []
	var yaws := []
	var f := fighter
	var loose := f.state in [Fighter.S.AIR, Fighter.S.JUGGLE, Fighter.S.FALL, Fighter.S.DOWN, Fighter.S.KO, Fighter.S.THROWN,
		Fighter.S.RUN, Fighter.S.WAKEUP] or f.is_airborne() or absf(_flip) > 1.0 or absf(_air_pitch) > 1.0
	var thresh := 0.085 * rig.h
	for i in 2:
		var s := "l" if i == 0 else "r"
		var ft: Dictionary = _feet[i]
		var want_local: Vector3 = pose.pos["foot_" + s] + Vector3(0, rig.ankle_h, 0)
		var want: Vector3 = g * want_local
		var want_yaw: float = root_yaw + deg_to_rad(pose.val["yaw_" + s])
		var free: bool = pose.val["free_" + s] >= 0.5 or loose or want_local.y > rig.ankle_h + 0.06
		if free or not ft["valid"] or delta <= 0.0:
			ft["pos"] = want
			ft["yaw"] = want_yaw
			ft["step"] = -1.0
			ft["valid"] = not free
		elif ft["step"] >= 0.0:
			ft["step"] += delta / float(ft["dur"])
			var k := clampf(ft["step"], 0.0, 1.0)
			var e := Pose.ease("inout", k)
			var lead := Vector3(_vis_vel.x, 0, 0) * float(ft["dur"]) * (1.0 - k) * 0.5
			var p: Vector3 = (ft["from"] as Vector3).lerp(want + lead, e)
			p.y = want.y + sin(k * PI) * minf(0.05 + speed * 0.02, 0.1) * rig.h / 1.8
			ft["pos"] = p
			ft["yaw"] = lerp_angle(ft["from_yaw"], want_yaw, e)
			if k >= 1.0:
				ft["step"] = -1.0
				ft["pos"] = Vector3(p.x, want.y, p.z)
		else:
			var err := Vector2(want.x - ft["pos"].x, want.z - ft["pos"].z).length()
			var yaw_err := absf(angle_difference(ft["yaw"], want_yaw))
			var other: Dictionary = _feet[1 - i]
			var other_busy: bool = other["step"] >= 0.0
			if (err > thresh or yaw_err > 0.6) and (not other_busy or err > thresh * 1.6):
				ft["step"] = 0.0
				ft["from"] = ft["pos"]
				ft["from_yaw"] = ft["yaw"]
				ft["dur"] = clampf(0.17 - speed * 0.03, 0.08, 0.17) if f.state != Fighter.S.MOVE else 0.09
			ft["pos"] = Vector3(ft["pos"].x, want.y, ft["pos"].z)
		ankles.append(gi * (ft["pos"] as Vector3))
		yaws.append(float(ft["yaw"]) - root_yaw)
	return [ankles, yaws]


# --- Props, eyes --------------------------------------------------------------------------------

func _update_props(pose: Pose) -> void:
	for node in props:
		Props.update(node, pose, self)
		var spec: Dictionary = node.get_meta("spec")
		if spec.has("thrown"):
			# A weapon that is also a projectile leaves the hand while it flies.
			var out := false
			if fighter.sim:
				for pr in fighter.sim.projectiles:
					out = out or (pr.owner == fighter.index and pr.kind == str(spec["thrown"]))
			node.visible = not out
	_update_whip()
	_eye_mat.set_shader_parameter("glow", _eye_base + pose.val["glow"] * 3.0)


## Moves that sink into the floor ("sink": [from, to] frames) lower the whole body, then raise it.
func _move_sink(frac: float) -> float:
	var f := fighter
	if f.state != Fighter.S.MOVE or f.move == null or not f.move.effects.has("sink"):
		return 0.0
	var r: Array = f.move.effects["sink"]
	var t := float(f.mf) - 1.0 + frac
	var a := float(r[0])
	var b := float(r[1])
	if t < a or t > b:
		return 0.0
	var span := maxf(b - a, 1.0)
	var u := (t - a) / span
	var depth := rig.h * 1.05
	return depth * clampf(minf(u * 4.0, (1.0 - u) * 4.0), 0.0, 1.0)


## A chain from the hand to the far end of the move's hitbox while it lashes out ("whip").
func _update_whip() -> void:
	var f := fighter
	var show := f.state == Fighter.S.MOVE and f.move != null and f.move.effects.has("whip")
	if show:
		var r: Array = f.move.effects["whip"]
		show = f.mf >= int(r[0]) and f.mf <= int(r[1]) and not f.move.hitboxes.is_empty()
	_whip.visible = show
	if not show:
		return
	var hb: Rect2 = f.move.hitboxes[0]["rect"]
	var reach := hb.end.x
	var u := clampf(float(f.mf - int(f.move.effects["whip"][0]) + 1) / 3.0, 0.0, 1.0)
	var hand := strike_point("hand_r")
	var tip := Vector3(f.pos.x + reach * u * f.facing, hb.position.y + hb.size.y * 0.5, 0.0)
	var to := tip - hand
	var len := to.length()
	if len < 0.05:
		_whip.visible = false
		return
	var y := to / len
	var x := y.cross(Vector3.BACK).normalized()
	_whip.global_transform = Transform3D(Basis(x, y, x.cross(y)).scaled_local(Vector3(1, len, 1)), hand + to * 0.5)


## Where the eyes look: the opponent's face.
func _look_target() -> Vector3:
	if fighter.sim == null or fighter.sim.fighters.size() < 2:
		return Vector3.INF
	var other: Fighter = fighter.opponent()
	return Vector3(other.pos.x, other.pos.y + other.def.height * 0.93, 0.0)


## World-space striking points used to check animation against hitboxes.
func strike_point(which: String) -> Vector3:
	var g := rig.global_transform
	match which:
		"hand_l":
			return g * rig.hand_tip("l")
		"hand_r":
			return g * rig.hand_tip("r")
		"foot_l":
			return g * rig.foot_tip("l")
		"foot_r":
			return g * rig.foot_tip("r")
		"knee_l":
			return g * rig.joint_pos("shin_l")
		"knee_r":
			return g * rig.joint_pos("shin_r")
	for node in props:
		if str((node.get_meta("spec") as Dictionary).get("kind", "")) == which:
			return Props.tip(node)
	return global_position
