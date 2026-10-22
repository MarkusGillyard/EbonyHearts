class_name FinisherDirector
extends Node
## Stages finishers: camera, the victim's fate (burning to ash, sinking, flattening, flying off,
## turning to salt) and the effects around it, timed to the finisher move's frames in the
## simulation, so a skipped finisher simply jumps to its end. Gore "off" swaps blood for ash and
## dust; "reduced" effects thin the particles out. Everything is restored for the next round.

var m: Match
var active := false
var move_id := ""
var who := 0
var _t := 0.0
var _fired := {}


func setup(match_node: Match) -> void:
	m = match_node


func on_event(e: Dictionary) -> void:
	match e["type"]:
		"finisher":
			active = true
			move_id = str(e["move"])
			who = int(e["who"])
			_t = 0.0
			_fired.clear()
			m.camera.release_zoom()
		"finisher_end":
			_finish_effects()
		"round_start":
			_reset()


func _reset() -> void:
	active = false
	for v in m.views:
		v.extra_scale = Vector3.ONE
		v.sink = 0.0
		v.lift_extra = Vector3.ZERO
		v.spin_extra = 0.0
		v.set_dissolve(0.0)
		v.visible = true
		if not (m.config.get("mirror_dark", false) and v == m.views[1]):
			v.set_tint(Color.BLACK, 0.0)
	m.camera.clear_override()


func _finish_effects() -> void:
	# The finisher's last frame: the victim's final state holds through the result.
	m.camera.clear_override()


func _once(key: String, at: float) -> bool:
	if _t >= at and not _fired.has(key):
		_fired[key] = true
		return true
	return false


func step(_delta: float) -> void:
	if not active or m.sim.finisher.is_empty():
		return
	var f := m.sim.fighters[who]
	_t = float(f.mf)
	var a := m.views[who]
	var v := m.views[1 - who]
	var vf := m.sim.fighters[1 - who]
	var mid := Vector3((f.pos.x + vf.pos.x) * 0.5, 1.0, 0.0)
	var face := float(f.facing)
	var gore: String = m.vfx.gore
	match move_id:
		"last_light":
			_close_up(mid, face, 0.0, 200.0)
			if _once("ignite", 40.0):
				Audio.play("ignite", 0.0)
				m.vfx.flash(Vector3(vf.pos.x, 1.2, 0.3), Color(1.0, 0.6, 0.2), 1.4, 0.3, 4.0, who)
			if _t > 40.0:
				var k := clampf((_t - 40.0) / 110.0, 0.0, 1.0)
				v.set_tint(Color(0.05, 0.035, 0.03), clampf(k * 1.6, 0.0, 1.0))
				v.set_dissolve(clampf((_t - 80.0) / 90.0, 0.0, 1.0), Color(1.0, 0.45, 0.1))
				if int(_t) % 6 == 0:
					m.vfx.burst(Vector3(vf.pos.x, 0.4 + randf() * 1.4, 0.2), Color(1.0, 0.5, 0.15), 12, 2.0, 0.6)
		_:
			_close_up(mid, face, 0.0, 180.0)
			if _t > 50.0:
				v.set_dissolve(clampf((_t - 50.0) / 100.0, 0.0, 1.0), Color(0.6, 1.0, 0.3))
	if gore == "off":
		pass


## Holds a closer, lower framing of the pair between frames `from` and `to`.
func _close_up(mid: Vector3, face: float, from: float, to: float) -> void:
	if _t >= from and _t <= to:
		var pos := mid + Vector3(-face * 0.6, 0.35, 3.6)
		m.camera.set_override(pos, mid + Vector3(0, 0.15, 0), 1.0)
	elif _t > to:
		m.camera.clear_override()
