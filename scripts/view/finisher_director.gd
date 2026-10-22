class_name FinisherDirector
extends Node
## Stages finishers and the other set pieces: every Sloptality, the Rendering Yards' stage
## finisher, the Slopbat's defeat and the stranger's strike. Each is timed to its move's frames in
## the simulation, so skipping a finisher simply jumps to its end, and nothing here changes the
## outcome. Gore "off" swaps blood for ash, tallow and dust; reduced effects thin the particles.
## Everything is put back at the start of the next round.

var m: Match
var active := false
var move_id := ""
var who := 0
var _t := 0.0
var _fired := {}
var _pool: MeshInstance3D
var _boss_t := -1.0
var _boss := -1
var _crack_t := -1.0


func setup(match_node: Match) -> void:
	m = match_node
	_pool = MeshInstance3D.new()
	var disc := CylinderMesh.new()
	disc.top_radius = 1.0
	disc.bottom_radius = 1.0
	disc.height = 0.01
	disc.radial_segments = 40
	_pool.mesh = disc
	_pool.material_override = Arena.slop(Color(0.02, 0.04, 0.01), Color(0.5, 0.95, 0.15), 2.0, 0.6)
	_pool.visible = false
	add_child(_pool)


func on_event(e: Dictionary) -> void:
	match e["type"]:
		"finisher":
			active = true
			move_id = str(e["move"])
			who = int(e["who"])
			_t = 0.0
			_fired.clear()
			m.camera.release_zoom()
		"round_start":
			_reset()
		"ko":
			if e.get("boss", false):
				_boss = int(e["who"])
				_boss_t = 0.0
		"stranger_crack":
			_crack_t = 0.0
		"hit":
			if m.sim.fighters[e["by"]].def.id == "daxion":
				# The one blow lands in slow motion.
				m._ko_slow = 45
				m.camera.add_shake(0.06)
				m.vfx.flash(Vector3(e["point"].x, e["point"].y, 0.2), Color(0.85, 0.75, 1.0), 1.4, 0.25, 3.0, 1)
				Audio.play("palm_strike", 2.0)


func _reset() -> void:
	active = false
	_boss_t = -1.0
	_crack_t = -1.0
	_pool.visible = false
	for v in m.views:
		v.extra_scale = Vector3.ONE
		v.sink = 0.0
		v.lift_extra = Vector3.ZERO
		v.spin_extra = 0.0
		v.pose_override = ""
		v.set_dissolve(0.0)
		v.visible = true
		if not (m.config.get("mirror_dark", false) and v == m.views[1]):
			v.set_tint(Color.BLACK, 0.0)
	m.camera.clear_override()


func _once(key: String, at: float) -> bool:
	if _t >= at and not _fired.has(key):
		_fired[key] = true
		return true
	return false


static func _ease(a: float, b: float, t: float) -> float:
	return Pose.ease("inout", clampf((t - a) / maxf(b - a, 1.0), 0.0, 1.0))


func step(delta: float) -> void:
	_boss_step(delta)
	_crack_step(delta)
	if not active or m.sim.finisher.is_empty():
		return
	var f := m.sim.fighters[who]
	_t = float(f.mf)
	var v := m.views[1 - who]
	var vf := m.sim.fighters[1 - who]
	var a := m.views[who]
	var face := float(f.facing)
	var mid := Vector3((f.pos.x + vf.pos.x) * 0.5, 1.0, 0.0)
	var vpos := Vector3(vf.pos.x, vf.pos.y, 0.0)
	var gore: String = m.vfx.gore
	match move_id:
		"last_light":
			_close_up(mid, face, 3.4, 0.0, 200.0)
			if _t > 6.0 and _t < 40.0 and int(_t) % 3 == 0:
				m.vfx.burst(a.strike_point("hand_l"), Color(1.0, 0.55, 0.15), 6, 1.0, 0.4)
			if _once("ignite", 40.0):
				Audio.play("ignite", 0.0)
				m.vfx.flash(vpos + Vector3(0, 1.2, 0.3), Color(1.0, 0.6, 0.2), 1.6, 0.35, 4.0, who)
			if _t > 40.0:
				v.set_tint(Color(0.05, 0.035, 0.03), clampf((_t - 40.0) / 70.0, 0.0, 1.0))
				v.set_dissolve(clampf((_t - 85.0) / 90.0, 0.0, 1.0), Color(1.0, 0.45, 0.1))
				if int(_t) % 4 == 0 and _t < 190.0:
					m.vfx.burst(vpos + Vector3(randf_range(-0.2, 0.2), 0.3 + randf() * 1.5, 0.2), Color(1.0, 0.5, 0.15), 14, 2.2, 0.7)
			if _once("ash", 170.0):
				Audio.play("crumble", -2.0)
				m.vfx.dust(vpos)
		"rendered_down":
			_close_up(mid, face, 4.4, 0.0, 250.0)
			var over := Vector3(f.pos.x + 0.25 * face, f.pos.y + 2.1 * f.scale(), 0.0)
			var front := Vector3(f.pos.x + 1.0 * face * f.scale(), 0.0, 0.0)
			if _t < 30.0:
				v.lift_extra = (over - vpos) * _ease(8.0, 30.0, _t)
				v.pose_override = "thrown_hold" if _t < 14.0 else "thrown_lift"
			elif _t < 86.0:
				var k := _ease(70.0, 86.0, _t)
				v.lift_extra = (over - vpos).lerp(front - vpos, k)
				v.pose_override = "thrown_lift" if k < 0.6 else "down"
			else:
				v.lift_extra = front - vpos
				v.pose_override = "down"
			if _once("slam", 86.0):
				Audio.play("slam", 2.0)
				m.camera.add_shake(0.07)
				m.vfx.dust(front)
				if gore != "off":
					m.vfx.blood(front + Vector3(0, 0.3, 0), -face, 0.8)
			if _once("stomp", 168.0):
				Audio.play("stamp", 3.0)
				m.camera.add_shake(0.1)
				m.vfx.dust(front)
				m.vfx.dust(front + Vector3(0.3 * face, 0, 0))
				if gore != "off":
					m.vfx.blood(front + Vector3(0, 0.2, 0), face, 1.0)
					m.vfx.blood(front + Vector3(0, 0.2, 0), -face, 1.0)
				else:
					m.vfx.burst(front + Vector3(0, 0.2, 0), Color(0.95, 0.88, 0.7), 26, 3.0, 0.6)
			if _t >= 168.0:
				v.extra_scale = Vector3(1.25, lerpf(1.0, 0.07, _ease(168.0, 171.0, _t)), 1.25)
				v.lift_extra.y = 0.0
		"undertow_pool":
			_close_up(mid, face, 4.6, 0.0, 230.0)
			if _once("pool", 26.0):
				_pool.global_position = Vector3(vf.pos.x, 0.012, 0.0)
				_pool.visible = true
				Audio.play("splash", -2.0)
			if _pool.visible:
				var r := 0.05 + 0.85 * _ease(26.0, 60.0, _t)
				_pool.scale = Vector3(r, 1.0, r * 0.8)
			if _t > 40.0:
				v.pose_override = "air_hit"
			if _t > 60.0:
				v.sink = vf.def.height * 1.3 * _ease(60.0, 190.0, _t)
				if int(_t) % 5 == 0 and _t < 200.0:
					m.vfx.burst(Vector3(vf.pos.x, 0.05, 0.1), Color(0.5, 0.9, 0.2), 10, 1.5, 0.6)
			if _once("sink", 70.0):
				Audio.play("sink", 0.0)
			if _once("gone", 196.0):
				v.visible = false
				Audio.play("splash", -6.0, 0.8)
		"last_note":
			_close_up(mid, face, 4.0, 0.0, 240.0)
			if _once("sing", 30.0):
				Audio.play("scream", 0.0, 0.85)
			if _t > 30.0 and _t < 200.0:
				v.lift_extra = Vector3(sin(_t * 1.9) * 0.012, 0.0, 0.0)
				v.set_tint(Color(0.86, 0.85, 0.8), clampf((_t - 40.0) / 90.0, 0.0, 1.0), 0.2)
				v.pose_override = "dazed"
			if _t > 140.0:
				v.set_dissolve(clampf((_t - 140.0) / 80.0, 0.0, 1.0), Color(0.95, 0.95, 0.9))
				if int(_t) % 3 == 0 and _t < 225.0:
					m.vfx.burst(vpos + Vector3(randf_range(-0.25, 0.25), 0.4 + randf() * 1.2, 0.2), Color(0.92, 0.92, 0.88), 12, 1.0, 0.8)
			if _once("crumble", 150.0):
				Audio.play("crumble", 0.0)
			if _once("heap", 215.0):
				m.vfx.dust(vpos)
		"lights_out":
			if _t < 70.0:
				_close_up(mid, face, 4.2, 0.0, 70.0)
			if _once("crack", 70.0):
				Audio.play("hit_heavy", 3.0)
				Audio.play("far_whoosh", 0.0)
				m.camera.add_shake(0.06)
				m.vfx.flash(vpos + Vector3(0, 1.3, 0.2), Color(1, 0.95, 0.8), 1.2, 0.2, 3.0, who)
				if gore != "off":
					m.vfx.blood(vpos + Vector3(0, 1.4, 0), face, 0.7)
			if _t >= 70.0:
				var u := clampf((_t - 70.0) / 90.0, 0.0, 1.0)
				v.lift_extra = Vector3(face * 26.0 * u, 12.0 * sin(u * PI * 0.8) + 4.0 * u, -60.0 * u)
				v.spin_extra = (_t - 70.0) * 22.0
				v.pose_override = "air_hit"
				m.camera.set_override(Vector3(f.pos.x - face * 0.5, 1.6, 6.0), vpos + v.lift_extra * 0.45 + Vector3(0, 1.0, 0), 1.0)
			if _once("twinkle", 160.0):
				v.visible = false
				m.vfx.flash(vpos + Vector3(face * 26.0, 16.0, -60.0), Color(1, 1, 0.9), 4.0, 0.5, 0.0, who)
				Audio.play("ui_select", -2.0, 1.6)
			if _t > 175.0:
				m.camera.clear_override()
		"crush_depth":
			_close_up(mid, face, 4.4, 0.0, 250.0)
			var helmet := Vector3(f.pos.x + 0.15 * face, f.def.height * 0.9, 0.0)
			if _once("fire", 24.0):
				Audio.play("cast_harpoon", 0.0)
			if _t > 60.0 and _t < 140.0:
				var k := _ease(60.0, 100.0, _t)
				v.lift_extra = Vector3((f.pos.x + 0.6 * face - vf.pos.x) * k, 0.0, 0.0)
				v.pose_override = "thrown_hold"
			if _once("reel", 60.0):
				Audio.play("chain", 0.0)
			if _t >= 100.0:
				var k2 := _ease(100.0, 140.0, _t)
				var start := Vector3(f.pos.x + 0.6 * face, 0.0, 0.0)
				v.lift_extra = start.lerp(helmet, k2) - vpos
				v.extra_scale = Vector3.ONE * lerpf(1.0, 0.04, k2)
				v.pose_override = "air_hit"
			if _once("gulp", 140.0):
				v.visible = false
				Audio.play("gulp", 2.0)
				m.vfx.burst(helmet, Color(0.6, 0.9, 0.8), 20, 1.6, 0.8)
		"stage_vat":
			var vat := Vector3(0.0, 3.3, -6.2)
			if _t < 14.0:
				_close_up(mid, face, 4.4, 0.0, 14.0)
			if _once("upper", 14.0):
				Audio.play("hit_heavy", 3.0)
				m.camera.add_shake(0.06)
			if _t >= 14.0:
				var u := clampf((_t - 14.0) / 56.0, 0.0, 1.0)
				var arc := vpos.lerp(vat, u) + Vector3(0, 4.0 * sin(u * PI), 0)
				v.lift_extra = arc - vpos
				v.spin_extra = (_t - 14.0) * 14.0
				v.pose_override = "air_hit"
				m.camera.set_override(Vector3(vpos.x * 0.5, 3.0, 7.0), arc + Vector3(0, 0.5, 0), 1.0)
			if _once("splash", 70.0):
				v.visible = false
				Audio.play("splash", 3.0)
				Audio.play("bell", -2.0)
				m.vfx.burst(vat + Vector3(0, 0.2, 0), Color(0.5, 0.95, 0.2), 30, 4.0, 0.9)
			if _t > 120.0:
				m.camera.clear_override()
		_:
			_close_up(mid, face, 4.0, 0.0, 180.0)
			if _t > 50.0:
				v.set_dissolve(clampf((_t - 50.0) / 100.0, 0.0, 1.0), Color(0.6, 1.0, 0.3))


## Holds a closer, lower framing of the pair between frames `from` and `to`.
func _close_up(mid: Vector3, face: float, dist: float, from: float, to: float) -> void:
	if _t >= from and _t <= to:
		m.camera.set_override(mid + Vector3(-face * 0.6, 0.3, dist), mid + Vector3(0, 0.1, 0), 1.0)
	elif _t > to:
		m.camera.clear_override()


## The Slopbat's defeat: it blackens into slop and sinks into its pool.
func _boss_step(delta: float) -> void:
	if _boss_t < 0.0:
		return
	_boss_t += delta
	var v := m.views[_boss]
	var f := m.sim.fighters[_boss]
	if _boss_t > 0.6:
		var k := clampf((_boss_t - 0.6) / 3.0, 0.0, 1.0)
		v.set_tint(Color(0.04, 0.07, 0.02), k, 0.8)
		v.sink = f.def.height * 1.2 * Pose.ease("in", k)
		if randf() < 0.4:
			m.vfx.burst(Vector3(f.pos.x + randf_range(-0.6, 0.6), 0.1 + randf() * 1.2, 0.2), Color(0.5, 0.95, 0.2), 10, 2.0, 0.7)
	if _boss_t > 0.6 and _boss_t - delta <= 0.6:
		Audio.play("screech", 0.0, 0.8)
	if _boss_t > 2.0 and _boss_t - delta <= 2.0:
		Audio.play("splash", 2.0, 0.7)
	if _boss_t > 3.8:
		v.visible = false


## The stranger's Find the Crack: a slow close-up of the touch, the moment, and the blow.
func _crack_step(delta: float) -> void:
	if _crack_t < 0.0:
		return
	_crack_t += delta
	var d := m.sim.fighters[1]
	var p := m.sim.fighters[0]
	var mid := Vector3((d.pos.x + p.pos.x) * 0.5, 1.25, 0.0)
	if _crack_t < 1.9:
		m.camera.set_override(mid + Vector3(-float(d.facing) * 1.4, 0.25, 2.6), mid + Vector3(0, 0.05, 0), 1.0)
	elif _crack_t > 3.2:
		m.camera.clear_override()
		_crack_t = -1.0
