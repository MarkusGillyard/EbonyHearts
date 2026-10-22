class_name CpuBrain
extends RefCounted
## A CPU opponent. It sees the other fighter only through a delayed copy of the match (its
## reaction time), so it can never answer an input before that input is visible on screen.
## Difficulty sets the delay and how often it blocks, anti-airs, punishes and finishes combos.
## It plays its fighter's real moves through PadIntent, the same input path a player's keys take.

const LEVELS := [
	{"react": 24, "block": 0.28, "anti_air": 0.2, "punish": 0.2, "combo": 0.35, "aggr": 0.35, "tech": 0.1, "think": 24, "mistake": 0.25, "finish": 0.35},
	{"react": 16, "block": 0.55, "anti_air": 0.5, "punish": 0.5, "combo": 0.7, "aggr": 0.5, "tech": 0.35, "think": 15, "mistake": 0.1, "finish": 0.7},
	{"react": 11, "block": 0.78, "anti_air": 0.75, "punish": 0.8, "combo": 0.92, "aggr": 0.6, "tech": 0.6, "think": 10, "mistake": 0.03, "finish": 0.95},
]

var sim: FightSim
var index := 1
var level: Dictionary
var rng := RandomNumberGenerator.new()
var intent := PadIntent.new()
var ai: Dictionary = {}
var _seen: Array = []
var _think_t := 0
var _block_t := 0
var _block_low := false
var _walk := 5
var _walk_t := 0
var _run_t := 0
var _last_hit_tick := -1000
var _combo_queued := false


func _init(s: FightSim, i: int, difficulty: int, seed := 0) -> void:
	sim = s
	index = i
	level = LEVELS[clampi(difficulty, 0, LEVELS.size() - 1)]
	rng.seed = seed if seed != 0 else 9001 + i * 31 + difficulty * 7
	ai = me().def.ai


func me() -> Fighter:
	return sim.fighters[index]


func opp() -> Fighter:
	return sim.fighters[1 - index]


## What the CPU has seen of the opponent: their state `react` ticks ago.
func _perceive() -> Dictionary:
	var o := opp()
	var snap := {"state": o.state, "move": o.move, "mf": o.mf, "pos": o.pos, "vel": o.vel, "air": o.is_airborne(),
		"crouch": o.is_crouching(), "contact": o.move_contact, "tick": sim.tick_count}
	_seen.push_front(snap)
	if _seen.size() > 48:
		_seen.pop_back()
	return _seen[mini(int(level["react"]), _seen.size() - 1)]


func think() -> Vector3i:
	var m := me()
	var seen := _perceive()
	if not sim.can_act():
		intent.clear()
		_block_t = 0
		return Vector3i.ZERO
	# Breaking a throw is a reaction too: only after the grab has been visible a while.
	if m.state == Fighter.S.THROWN:
		if m.thrown_by and m.thrown_by.throw_t == mini(int(level["react"]) / 3, 6) and rng.randf() < float(level["tech"]):
			return PadIntent.pad(5, m.facing, InputBuffer.LP)
		return Vector3i.ZERO
	if sim.phase == FightSim.Phase.FINISH:
		return _finish()
	if intent.busy():
		return intent.next(m.facing)
	if _block_t > 0:
		_block_t -= 1
		return PadIntent.pad(2 if _block_low else 5, m.facing, 0, InputBuffer.BL)
	if not m.is_actionable() and m.state != Fighter.S.AIR:
		return _continue_combo(m)
	var o := opp()
	var dist := absf(o.pos.x - m.pos.x)
	# --- Reactions to what has been seen ---
	var sm: MoveDef = seen["move"]
	if seen["state"] == Fighter.S.MOVE and sm and sm.is_attack() and int(seen["mf"]) <= sm.last_active() and dist < 2.4:
		if m.state != Fighter.S.AIR and rng.randf() < float(level["block"]):
			_block_t = maxi(8, sm.last_active() - int(seen["mf"]) + 10)
			_block_low = sm.level == MoveDef.Level.LOW or (sm.level != MoveDef.Level.OVERHEAD and rng.randf() < 0.2)
			return PadIntent.pad(2 if _block_low else 5, m.facing, 0, InputBuffer.BL)
	if seen["air"] and seen["state"] in [Fighter.S.AIR, Fighter.S.MOVE] and dist < 2.2 and (seen["vel"] as Vector2).y < 1.0:
		if rng.randf() < float(level["anti_air"]) * 0.25:
			var aa: Array = ai.get("anti_air", ["uppercut"])
			intent.command(_notation(aa[rng.randi_range(0, aa.size() - 1)]))
			return intent.next(m.facing)
	for p in sim.projectiles:
		if p.owner != index and p.age > int(level["react"]) and absf(p.pos.x - m.pos.x) < 3.0 and signf(m.pos.x - p.pos.x) == signf(p.vel.x):
			var r := rng.randf()
			if p.level == MoveDef.Level.HIGH and r < 0.4:
				return PadIntent.pad(2, m.facing)
			if r < 0.4 + float(level["block"]) * 0.5:
				_block_t = 14
				_block_low = p.level == MoveDef.Level.LOW
				return PadIntent.pad(2 if _block_low else 5, m.facing, 0, InputBuffer.BL)
			intent.hold(9, 2)
			return intent.next(m.facing)
	# Punish a whiffed or blocked attack still recovering.
	if seen["state"] == Fighter.S.MOVE and sm and int(seen["mf"]) > sm.last_active() and sm.total - int(seen["mf"]) > 14 and dist < 1.5:
		if rng.randf() < float(level["punish"]) * 0.4:
			_start_combo(true)
			return intent.next(m.facing)
	if o.state == Fighter.S.JUGGLE and dist < 1.8 and o.pos.y < 1.6 and o.vel.y < 0.0 and o.juggle_points < Fighter.JUGGLE_LIMIT - 2:
		if rng.randf() < float(level["combo"]):
			intent.command("D+HP")
			return intent.next(m.facing)
	# --- Plans ---
	_think_t -= 1
	if _think_t <= 0:
		_think_t = int(level["think"]) + rng.randi_range(0, int(level["think"]))
		_plan(m, o, dist)
		if intent.busy():
			return intent.next(m.facing)
	if _walk_t > 0:
		_walk_t -= 1
		if _run_t > 0:
			_run_t -= 1
			return PadIntent.pad(6, m.facing, 0, InputBuffer.RN)
		return PadIntent.pad(_walk, m.facing)
	return PadIntent.pad(5, m.facing)


func _plan(m: Fighter, o: Fighter, dist: float) -> void:
	var aggr := float(level["aggr"]) * (0.6 + float(ai.get("aggression", 0.5)) * 0.8)
	var want := float(ai.get("range", 1.1))
	if rng.randf() < float(level["mistake"]):
		_walk = [4, 6, 5][rng.randi_range(0, 2)]
		_walk_t = rng.randi_range(6, 20)
		return
	if o.state in [Fighter.S.DOWN, Fighter.S.WAKEUP, Fighter.S.KO]:
		_walk = 6 if dist > 1.6 else 5
		_walk_t = 10
		return
	if dist > 3.4:
		var zoner: Array = _moves_tagged("projectile")
		if not zoner.is_empty() and rng.randf() < 0.3 + float(ai.get("zoning", 0.2)):
			intent.command(_notation(zoner[rng.randi_range(0, zoner.size() - 1)]))
			return
		if rng.randf() < aggr * 0.5:
			_walk = 6
			_walk_t = 30
			_run_t = 30 if m.run_meter > 50.0 and rng.randf() < aggr else 0
			return
		if rng.randf() < 0.15:
			intent.hold(9, 3)
			intent.hold(5, 20)
			intent.queue.append([5, InputBuffer.HK, 0])
			return
		_walk = 6
		_walk_t = 20
		return
	if dist > want + 0.9:
		var r := rng.randf()
		if r < 0.18 * aggr + 0.05:
			intent.hold(9, 3)
			intent.hold(5, 18)
			intent.queue.append([5, InputBuffer.HK, 0])
			return
		var rush: Array = _moves_tagged("rush")
		if not rush.is_empty() and r < 0.3 * aggr + 0.1:
			intent.command(_notation(rush[rng.randi_range(0, rush.size() - 1)]))
			return
		var far: Array = ["HK", "B+LK", "B+HK"]
		if r < 0.45:
			intent.hold(6, rng.randi_range(4, 10))
			intent.command(far[rng.randi_range(0, far.size() - 1)])
			return
		_walk = 6
		_walk_t = rng.randi_range(10, 24)
		return
	# Close.
	var r2 := rng.randf()
	if r2 < 0.12:
		intent.command("F+LP")
		return
	if r2 < 0.12 + 0.45 * aggr:
		_start_combo(false)
		return
	if r2 < 0.75:
		var pokes := ["HP", "LP", "LK", "D+LK", "D+LP", "HK", "B+LK"]
		intent.command(pokes[rng.randi_range(0, pokes.size() - 1)])
		return
	if rng.randf() < 0.5:
		_block_t = rng.randi_range(10, 26)
		_block_low = rng.randf() < 0.4
	else:
		_walk = 4
		_walk_t = rng.randi_range(8, 18)


func _start_combo(punish: bool) -> void:
	var combos: Array = ai.get("combos", [])
	if combos.is_empty() or rng.randf() > float(level["combo"]):
		intent.command("D+HP" if punish else "HP")
		return
	var c: Array = combos[rng.randi_range(0, combos.size() - 1)]
	for i in c.size():
		intent.command(str(c[i]))
		# Space the links so each lands inside its chain window.
		intent.hold(5, 6)
	_combo_queued = true


func _continue_combo(m: Fighter) -> Vector3i:
	return PadIntent.pad(5, m.facing)


## Finishing a dizzy opponent: walk to the right distance and enter the finisher, or settle for
## a blow when the CPU is not sure of it.
func _finish() -> Vector3i:
	var m := me()
	if sim.finish_winner != index or opp().state != Fighter.S.DIZZY:
		return Vector3i.ZERO
	if intent.busy():
		return intent.next(m.facing)
	if m.def.finisher_cmd == null or rng.randf() > float(level["finish"]):
		if sim.phase_t - sim.finish_t0 > 90:
			intent.command("D+HP")
		return PadIntent.pad(5, m.facing)
	var r: Array = FightSim.RANGES.get(m.def.finisher_range, FightSim.RANGES["close"])
	var dist := sim.distance()
	var mid := (float(r[0]) + minf(float(r[1]), 3.2)) * 0.5
	if dist > float(r[1]) - 0.05 or (dist > mid + 0.3 and float(r[1]) < 5.0):
		return PadIntent.pad(6, m.facing)
	if dist < float(r[0]) + 0.05:
		return PadIntent.pad(4, m.facing)
	if sim.phase_t - sim.finish_t0 > 50:
		intent.command(m.def.finisher_cmd.text)
	return PadIntent.pad(5, m.facing)


func _moves_tagged(tag: String) -> Array:
	var out := []
	for c in me().def.commands:
		var mv := me().def.move(c.move)
		if mv and tag in mv.tags:
			out.append(c.move)
	return out


func _notation(move_id: String) -> String:
	var n := me().def.notation_of(move_id)
	return n if n != "" else "HP"
