extends TestCase
## The animation agrees with the simulation: on every active frame of every strike, the striking
## part named by the move's "tip" (a fist, foot, knee, elbow or weapon) is inside the move's
## hitbox, give or take a hand's width; and every pose a move names exists.

const SLACK := 0.16
const TIP_JOINTS := {"hand_l": "hand_l", "hand_r": "hand_r", "foot_l": "foot_l", "foot_r": "foot_r", "knee_l": "knee_l", "knee_r": "knee_r"}

var _root3d: Node3D


func before_each() -> void:
	_root3d = Node3D.new()
	root.add_child(_root3d)


func after_each() -> void:
	_root3d.queue_free()


func test_every_strike_reaches_its_hitbox() -> void:
	var bad := []
	for id in Content.all_ids():
		var def := Content.fighter(id)
		if def == null:
			continue
		for mid in def.moves:
			var m: MoveDef = def.moves[mid]
			var tip := str(m.anim.get("tip", ""))
			if not m.is_attack() or m.hitboxes.is_empty() or not TIP_JOINTS.has(tip) or m.air:
				continue
			var miss := _check(def, m, tip)
			if miss != "":
				bad.append("%s.%s %s" % [id, mid, miss])
	assert_true(bad.is_empty(), "strikes whose animation misses the hitbox:\n      " + "\n      ".join(bad))


func _check(def: FighterDef, m: MoveDef, tip: String) -> String:
	var sim := FightSim.new()
	sim.training = true
	sim.intro = false
	sim.setup(def, def)
	sim.phase = FightSim.Phase.FIGHT
	var f := sim.fighters[0]
	f.pos = Vector2.ZERO
	f.facing = 1
	sim.fighters[1].pos = Vector2(30, 0)
	var v := FighterView.new()
	_root3d.add_child(v)
	v.setup(f, Content.common_poses)
	v.smoothing = false
	f.set_state(Fighter.S.IDLE)
	for i in 10:
		v.sync_from_sim()
		v.update_visual(1.0, 1.0 / 60.0)
	f.start_move(m)
	var worst := ""
	var worst_d := 0.0
	for fr in range(1, m.last_active() + 1):
		if f.state != Fighter.S.MOVE or f.move != m:
			break
		v.sync_from_sim()
		v.update_visual(1.0, 1.0 / 60.0)
		if m.in_active(f.mf):
			var p := v.strike_point(tip)
			var tip2 := Vector2(p.x, p.y)
			for hb in m.hitboxes:
				if f.mf < hb["from"] or f.mf > hb["to"]:
					continue
				var r: Rect2 = f.to_world(hb["rect"]).grow(SLACK)
				if not r.has_point(tip2):
					var d := _dist(r, tip2)
					if d > worst_d:
						worst_d = d
						worst = "f%d tip %s at (%.2f, %.2f) outside %s by %.2f m" % [f.mf, tip, tip2.x - f.pos.x, tip2.y, f.to_world(hb["rect"]), d]
		f.mf += 1
		f._move_frame()
	v.queue_free()
	sim.dispose()
	return worst


static func _dist(r: Rect2, p: Vector2) -> float:
	var dx := maxf(maxf(r.position.x - p.x, 0.0), p.x - r.end.x)
	var dy := maxf(maxf(r.position.y - p.y, 0.0), p.y - r.end.y)
	return Vector2(dx, dy).length()


func test_every_named_pose_exists() -> void:
	var missing := []
	for id in Content.all_ids():
		var def := Content.fighter(id)
		if def == null:
			continue
		var lib := PoseLibrary.new()
		lib.build(def, Content.common_poses)
		for name in ["stance", "crouch", "block", "block_low", "jump", "jump_tuck", "run_a", "run_b", "hit_head", "hit_body", "hit_low",
				"air_hit", "air_fall", "swept", "down", "rise_1", "rise_2", "dizzy", "dazed", "win", "intro", "thrown_hold", "thrown_lift"]:
			lib.get_pose(name)
		for k in lib.missing:
			missing.append("%s: %s" % [id, k])
	assert_true(missing.is_empty(), "missing poses: %s" % [missing])


func test_move_keys_run_forward_in_time() -> void:
	var bad := []
	for id in Content.all_ids():
		var def := Content.fighter(id)
		if def == null:
			continue
		for mid in def.moves:
			var m: MoveDef = def.moves[mid]
			for part in ["keys", "hold", "victim"]:
				var last := -1.0
				for k in m.anim.get(part, []):
					if float(k[0]) < last:
						bad.append("%s.%s.%s" % [id, mid, part])
					last = float(k[0])
	assert_true(bad.is_empty(), "keyframes out of order: %s" % [bad])
