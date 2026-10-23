class_name CpuBrain
extends RefCounted
## A CPU opponent. It sees the other fighter only through a delayed copy of the match (its
## reaction time), so it can never answer an input before that input is visible on screen.
## Difficulty sets the delay and how often it blocks, anti-airs, punishes and finishes combos.
## It plays its fighter's real moves through PadIntent, the same input path a player's keys take.

const LEVELS := [
	{"react": 24, "block": 0.25, "anti_air": 0.2, "punish": 0.25, "combo": 0.35, "aggr": 0.45, "tech": 0.1, "think": 16, "mistake": 0.25, "finish": 0.35},
	{"react": 16, "block": 0.45, "anti_air": 0.5, "punish": 0.55, "combo": 0.7, "aggr": 0.6, "tech": 0.35, "think": 10, "mistake": 0.1, "finish": 0.7},
	{"react": 11, "block": 0.62, "anti_air": 0.75, "punish": 0.85, "combo": 0.92, "aggr": 0.7, "tech": 0.6, "think": 7, "mistake": 0.03, "finish": 0.95},
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
var _was_blocking := false
var _reach_cache := {}
var _recent: Array = []
## Tick at which to start an anti-air timed to meet a jump seen coming in (-1: none planned).
var _aa_at := -1
var _aa_ready := true
## How the CPU was hit lately ("low", "jump" or "other", newest first), so it adapts to habits.
var _hit_by: Array = []
var _last_damage := 0


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
	_note_hit(m)
	if not m.is_actionable() and m.state != Fighter.S.AIR:
		_was_blocking = m.state == Fighter.S.BLOCKSTUN
		return _continue_combo(m)
	var o := opp()
	var dist := absf(o.pos.x - m.pos.x)
	# A jump is read from its start, as a player reads it: the arc is predictable, so the
	# anti-air is timed to meet the opponent on the way down (from what was seen, not inputs).
	_plan_anti_air(m, seen)
	if _aa_at >= 0 and sim.tick_count >= _aa_at:
		_aa_at = -1
		if o.is_airborne() and dist < 2.6:
			var aas: Array = ai.get("anti_air", ["uppercut"])
			intent.command(_notation(aas[rng.randi_range(0, aas.size() - 1)]))
			return intent.next(m.facing)
	# A stunned, dazed or reeled-in opponent is an opening: take it.
	if o.state == Fighter.S.HITSTUN and o.stun_kind in ["dazed", "pulled", "break"] and o.stun > 8 and dist < 1.6:
		if rng.randf() < float(level["combo"]):
			_start_combo(true)
			return intent.next(m.facing)
	# Just blocked something that is still recovering: punish it.
	if _was_blocking and o.state == Fighter.S.MOVE and o.move and o.mf > o.move.last_active() and o.move.total - o.mf > 8 and dist < 1.5:
		_was_blocking = false
		if rng.randf() < float(level["punish"]):
			_start_combo(true)
			return intent.next(m.facing)
	_was_blocking = false
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
	# Swept again and again: crouch and guard while the sweeper is in range, which in turn
	# leaves the CPU open to overheads and jump-ins (the habit fades as other hits replace it).
	var lows := _hit_by.count("low")
	if lows >= 4 and o.is_actionable() and dist < 2.3 and rng.randf() < float(level["block"]) * 0.015 * lows:
		_block_t = rng.randi_range(14, 26)
		_block_low = true
		return PadIntent.pad(2, m.facing, 0, InputBuffer.BL)
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
		var lows := _hit_by.count("low")
		if r < float(level["block"]) * (0.35 + 0.08 * lows) and opp().is_actionable():
			# Expecting the opponent to come in: guard for a moment, low if they keep sweeping.
			_block_t = rng.randi_range(12, 24)
			_block_low = rng.randf() < 0.3 + 0.12 * lows
			return
		if r < 0.18 * aggr + 0.05:
			intent.hold(9, 3)
			intent.hold(5, 18)
			intent.queue.append([5, InputBuffer.HK, 0])
			return
		var rush: Array = _moves_tagged("rush")
		if not rush.is_empty() and r < 0.3 * aggr + 0.1:
			intent.command(_notation(rush[rng.randi_range(0, rush.size() - 1)]))
			return
		if r < 0.45:
			var pick := _attack_for(dist)
			if pick != "":
				intent.command(pick)
				return
		_walk = 6
		_walk_t = rng.randi_range(10, 24)
		return
	# Close.
	if o.state in [Fighter.S.CROUCH_BLOCK, Fighter.S.BLOCK] and dist < m.def.throw_range and rng.randf() < 0.35:
		# Someone sitting in their guard gets thrown.
		intent.command("F+LP")
		return
	var lows_close := _hit_by.count("low")
	if lows_close >= 2 and opp().is_actionable() and rng.randf() < float(level["block"]) * 0.12 * lows_close:
		# Swept again and again: expect another and guard low for a moment.
		_block_t = rng.randi_range(12, 22)
		_block_low = true
		return
	var r2 := rng.randf()
	if r2 < 0.12 and dist < m.def.throw_range:
		intent.command("F+LP")
		return
	if r2 < 0.12 + 0.45 * aggr and dist < _reach("hp") + 0.05:
		_start_combo(false)
		return
	if r2 < 0.8:
		var pick := _attack_for(dist)
		if pick != "":
			intent.command(pick)
			return
		_walk = 6
		_walk_t = 6
		return
	if rng.randf() < 0.5:
		_block_t = rng.randi_range(10, 26)
		_block_low = rng.randf() < 0.4 + 0.1 * _hit_by.count("low")
	else:
		_walk = 4
		_walk_t = rng.randi_range(8, 18)


## Plans an anti-air for a jump seen rising toward the CPU: when the seen arc will come down
## through head height near it, less the move's startup. More likely the more jump-ins have
## landed lately.
func _plan_anti_air(m: Fighter, seen: Dictionary) -> void:
	if not seen["air"]:
		_aa_ready = true
		return
	if not _aa_ready or _aa_at >= 0:
		return
	var o := opp()
	var p: Vector2 = seen["pos"]
	var v: Vector2 = seen["vel"]
	var toward := signf(m.pos.x - p.x)
	if v.y <= 0.0 or signf(v.x) != toward or absf(m.pos.x - p.x) > 3.6:
		return
	_aa_ready = false
	var chance := minf(0.95, float(level["anti_air"]) + 0.12 * _hit_by.count("jump"))
	if rng.randf() > chance:
		return
	var g := o.def.gravity
	var h := 1.4 * o.scale()
	var disc := v.y * v.y + 2.0 * g * (p.y - h)
	if disc < 0.0:
		return
	var t := (v.y + sqrt(disc)) / g
	var aas: Array = ai.get("anti_air", ["uppercut"])
	var mv := m.def.move(str(aas[0]))
	var startup := mv.startup if mv else 8
	var ticks_left := int(t * 60.0) - (sim.tick_count - int(seen["tick"]))
	_aa_at = sim.tick_count + maxi(0, ticks_left - startup + 1)


## Remembers the kind of attack that last hit the CPU.
func _note_hit(m: Fighter) -> void:
	var taken := m.damage_taken
	var hit := taken > _last_damage
	_last_damage = taken
	if not hit or m.last_hit_move == null:
		return
	var kind := "other"
	if m.last_hit_move.air:
		kind = "jump"
	elif m.last_hit_move.level == MoveDef.Level.LOW:
		kind = "low"
	_hit_by.push_front(kind)
	if _hit_by.size() > 8:
		_hit_by.pop_back()


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


## How far a move can reach, root to root: its farthest hitbox edge plus its forward motion plus
## the depth of the opponent's body.
func _reach(move_id: String) -> float:
	if _reach_cache.has(move_id):
		return _reach_cache[move_id]
	var mv := me().def.move(move_id)
	var r := 0.0
	if mv:
		for hb in mv.hitboxes:
			r = maxf(r, (hb["rect"] as Rect2).end.x)
		if not mv.motion.is_empty():
			r += mv.motion[mv.motion.size() - 1].y
		if not mv.projectile.is_empty():
			r = 99.0
	r = r + 0.22 * opp().scale() if r > 0.0 else 0.0
	_reach_cache[move_id] = r
	return r


## A ground attack that can connect from `dist` now, weighted toward the fighter's habits; "" if
## none reaches.
func _attack_for(dist: float) -> String:
	var options := []
	var total := 0.0
	var weights: Dictionary = ai.get("weights", {})
	# Specials are a spice, not the meal: most decisions consider only normals, and walking in
	# when none reaches.
	var specials_ok := rng.randf() < 0.3
	for c in me().def.commands:
		var mv := me().def.move(c.move)
		if mv == null or mv.kind in ["throw", "grab", "finisher"] or not mv.is_attack() or mv.hitboxes.is_empty():
			continue
		if mv.kind == "special" and not specials_ok:
			continue
		var reach := _reach(c.move)
		if reach < dist or reach > dist + 1.4:
			continue
		var w := float(weights.get(c.move, 1.0)) * (1.6 if mv.kind == "normal" else 0.35)
		# Variety: a move just used is less likely to be picked again at once.
		w *= 0.3 if c.move in _recent else 1.0
		options.append([c.text, w])
		total += w
	if options.is_empty():
		return ""
	var pick := rng.randf() * total
	var chosen: String = options[0][0]
	for o in options:
		pick -= o[1]
		if pick <= 0.0:
			chosen = o[0]
			break
	for c in me().def.commands:
		if c.text == chosen:
			_recent.push_front(c.move)
			break
	if _recent.size() > 3:
		_recent.pop_back()
	return chosen


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
