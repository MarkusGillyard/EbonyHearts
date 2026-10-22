class_name FightSim
extends RefCounted
## A match on the fighting plane. Deterministic: step() takes both players' device state for one
## 60 Hz tick and advances everything; nothing reads the clock, the scene or input devices.
## Every outcome is reported as an event dictionary in `events` for presentation and tests.

enum Phase { INTRO, READY, FIGHT, FINISH, FINISHER, ROUND_END, MATCH_END }

const DT := 1.0 / 60.0
const PAD_RIGHT := 1
const PAD_LEFT := 2
const PAD_UP := 4
const PAD_DOWN := 8
## Half the width of the walkable floor, in metres.
const STAGE_HALF := 7.0
## The camera never lets the fighters drift further apart than this.
const MAX_SEP := 5.6
const START_GAP := 3.0
const INTRO_TICKS := 110
const READY_TICKS := 100
const ROUND_TICKS := 90 * 60
const ROUND_END_TICKS := 170
const FINISH_TICKS := 420
const MAX_ROUNDS := 9
const THROW_BREAK_WINDOW := 8
const RANGES := {"close": [0.0, 1.15], "sweep": [1.15, 2.3], "far": [2.6, 99.0]}

var fighters: Array[Fighter] = []
var projectiles: Array[Projectile] = []
var events: Array = []
var tick_count := 0
var phase: int = Phase.INTRO
var phase_t := 0
var timer := ROUND_TICKS
var round_num := 1
var rounds_to_win := 2
var wins := [0, 0]
var round_winner := -1
var round_how := ""
var match_winner := -1
## Which fighter won the deciding round and may finish the other.
var finish_winner := -1
var finish_dizzy := false
var finish_t0 := 0
var finisher := {}
var training := false
var infinite_run := false
## Training: health refills once a combo ends.
var refill_health := false
var finishers_enabled := true
## The arena's own finisher, when it has one (an id in data/basics.json).
var stage_finisher := ""
var intro := true
## Optional rule hooks (the stranger encounter): on_contact(contact) -> String, on_tick(sim).
var hooks: Object = null
var flawless := [false, false]


func setup(d1: FighterDef, d2: FighterDef) -> void:
	fighters.clear()
	for i in 2:
		var f := Fighter.new()
		f.setup(self, i, d1 if i == 0 else d2)
		fighters.append(f)
	_start_round(intro)


func dispose() -> void:
	for f in fighters:
		f.sim = null
		f.throw_victim = null
		f.thrown_by = null
	fighters.clear()
	projectiles.clear()
	hooks = null


func emit(e: Dictionary) -> void:
	e["t"] = tick_count
	events.append(e)


func can_act() -> bool:
	return phase == Phase.FIGHT or phase == Phase.FINISH


func distance() -> float:
	return absf(fighters[1].pos.x - fighters[0].pos.x)


func timer_seconds() -> int:
	return int(ceil(timer / 60.0))


# --- Rounds --------------------------------------------------------------------------------------

func _start_round(with_intro: bool) -> void:
	projectiles.clear()
	for f in fighters:
		f.health = f.max_health()
		f.run_meter = Fighter.RUN_MAX
		f.run_idle = 0
		f.pos = Vector2(START_GAP * (-0.5 if f.index == 0 else 0.5), 0.0)
		f.facing = 1 if f.index == 0 else -1
		f.vel = Vector2.ZERO
		f.move = null
		f.mf = 0
		f.stun = 0
		f.hitstop = 0
		f.push_v = 0.0
		f.invuln = 0
		f.juggle_points = 0
		f.combo_hits = 0
		f.combo_damage = 0
		f.projectiles_out = 0
		f.damage_taken = 0
		f.throw_victim = null
		f.thrown_by = null
		f.buff = ""
		f.buff_ticks = 0
		f.input.clear()
		f.set_state(Fighter.S.INTRO)
		f.sf = 0
	timer = ROUND_TICKS
	round_winner = -1
	round_how = ""
	finish_winner = -1
	finish_dizzy = false
	finisher = {}
	flawless = [false, false]
	phase = Phase.INTRO if with_intro else Phase.READY
	phase_t = 0
	if not with_intro:
		emit({"type": "round_start", "round": round_num})


## Starts the current round over (training reset, rematch tools).
func restart_round() -> void:
	_start_round(false)


func step(pads: Array) -> void:
	events.clear()
	tick_count += 1
	phase_t += 1
	for i in 2:
		var f := fighters[i]
		var p: Vector3i = pads[i]
		var screen := InputBuffer.screen_from(p.x & PAD_RIGHT, p.x & PAD_LEFT, p.x & PAD_UP, p.x & PAD_DOWN)
		f.input.push(screen, f.facing, p.y, p.z, f.hitstop > 0)
	_phase_tick()
	_update_facing()
	if phase == Phase.FINISHER:
		_finisher_tick()
	else:
		for f in fighters:
			f.tick()
		for f in fighters:
			if f.throw_victim != null and f.hitstop <= 0:
				_throw_tick(f)
	_step_projectiles()
	_separate()
	_clamp()
	if phase == Phase.FIGHT or phase == Phase.FINISH:
		_resolve()
	if hooks and hooks.has_method("on_tick"):
		hooks.on_tick(self)
	_check_round()
	if refill_health:
		for f in fighters:
			if f.health < f.max_health() and f.is_actionable() and f.sf > 30 and f.opponent().is_actionable():
				f.health = f.max_health()


func _phase_tick() -> void:
	match phase:
		Phase.INTRO:
			if phase_t >= INTRO_TICKS:
				phase = Phase.READY
				phase_t = 0
				emit({"type": "round_start", "round": round_num})
		Phase.READY:
			for f in fighters:
				if f.state == Fighter.S.INTRO and phase_t > 20:
					f.set_state(Fighter.S.IDLE)
			if phase_t >= READY_TICKS:
				phase = Phase.FIGHT
				phase_t = 0
				for f in fighters:
					f.set_state(Fighter.S.IDLE)
				emit({"type": "fight"})
		Phase.FIGHT:
			if not training:
				timer = maxi(0, timer - 1)


func _update_facing() -> void:
	for f in fighters:
		if f.state in [Fighter.S.IDLE, Fighter.S.WALK_F, Fighter.S.WALK_B, Fighter.S.CROUCH, Fighter.S.BLOCK,
				Fighter.S.CROUCH_BLOCK, Fighter.S.LAND, Fighter.S.INTRO, Fighter.S.DIZZY, Fighter.S.PREJUMP]:
			var dx := f.opponent().pos.x - f.pos.x
			if absf(dx) > 0.02:
				var want := 1 if dx > 0.0 else -1
				if want != f.facing:
					f.facing = want
					if f.state == Fighter.S.WALK_F or f.state == Fighter.S.WALK_B:
						f.set_state(Fighter.S.IDLE)


# --- Movement constraints ------------------------------------------------------------------------

func _separate() -> void:
	var a := fighters[0]
	var b := fighters[1]
	if not a.has_pushbox() or not b.has_pushbox():
		return
	var ra := a.pushbox()
	var rb := b.pushbox()
	if not ra.intersects(rb):
		return
	var overlap := minf(ra.end.x, rb.end.x) - maxf(ra.position.x, rb.position.x)
	if overlap <= 0.0:
		return
	var dir := signf(b.pos.x - a.pos.x)
	if dir == 0.0:
		dir = float(a.facing)
	# A grounded body holds its place against one landing on it, which slides off over a few
	# ticks rather than popping aside; two grounded bodies share the push.
	var share_a := 0.5
	if a.is_airborne() != b.is_airborne():
		share_a = 1.0 if a.is_airborne() else 0.0
		overlap = minf(overlap, 0.07)
	a.pos.x -= dir * overlap * share_a
	b.pos.x += dir * overlap * (1.0 - share_a)


func _clamp() -> void:
	for f in fighters:
		var lo := -STAGE_HALF + f.def.push_w
		var hi := STAGE_HALF - f.def.push_w
		var want := f.pos.x
		f.pos.x = clampf(f.pos.x, lo, hi)
		var overflow := want - f.pos.x
		if overflow != 0.0 and f.push_v != 0.0 and f.push_from >= 0 and f.push_from != f.index:
			# Pushed into the wall: the attacker is pushed back instead.
			var att := fighters[f.push_from]
			if not att.is_airborne():
				att.pos.x -= overflow
	var a := fighters[0]
	var b := fighters[1]
	var sep := absf(b.pos.x - a.pos.x)
	if sep > MAX_SEP:
		var excess := sep - MAX_SEP
		var dir := signf(b.pos.x - a.pos.x)
		var a_away := signf(a.vel.x + a.push_v) == -dir
		var b_away := signf(b.vel.x + b.push_v) == dir
		var ka := 0.5
		if a_away and not b_away:
			ka = 1.0
		elif b_away and not a_away:
			ka = 0.0
		a.pos.x += dir * excess * ka
		b.pos.x -= dir * excess * (1.0 - ka)
		for f in fighters:
			f.pos.x = clampf(f.pos.x, -STAGE_HALF + f.def.push_w, STAGE_HALF - f.def.push_w)


# --- Projectiles ---------------------------------------------------------------------------------

func spawn_projectile(owner: Fighter, m: MoveDef) -> void:
	var p := Projectile.spawn(owner, m)
	projectiles.append(p)
	owner.projectiles_out += 1
	emit({"type": "projectile", "who": owner.index, "move": m.id, "kind": p.kind})


func _step_projectiles() -> void:
	for p in projectiles:
		if p.dead:
			continue
		var o := fighters[p.owner]
		if p.tether and o.state in [Fighter.S.HITSTUN, Fighter.S.JUGGLE, Fighter.S.FALL, Fighter.S.THROWN, Fighter.S.BLOCKSTUN]:
			p.dead = true
			continue
		p.step(DT, o.pos + Vector2(0.35 * o.facing, 1.3 * o.scale()))
		if absf(p.pos.x) > STAGE_HALF + 2.0 or p.pos.y > 12.0:
			p.dead = true
	var kept: Array[Projectile] = []
	for p in projectiles:
		if p.dead:
			if p.owner < fighters.size():
				fighters[p.owner].projectiles_out = maxi(0, fighters[p.owner].projectiles_out - 1)
			emit({"type": "projectile_end", "who": p.owner, "kind": p.kind, "pos": p.pos})
		else:
			kept.append(p)
	projectiles = kept


# --- Contact -------------------------------------------------------------------------------------

func _resolve() -> void:
	var contacts := []
	for a in fighters:
		var d := a.opponent()
		var hb := a.active_hitboxes()
		if hb.is_empty():
			continue
		var hurt := d.hurtboxes()
		var found := false
		for h in hb:
			for r: Rect2 in hurt:
				if (h[1] as Rect2).intersects(r):
					contacts.append({"att": a, "def": d, "move": a.move, "group": h[0], "point": _contact_point(h[1], r)})
					found = true
					break
			if found:
				break
	for p in projectiles:
		if p.dead:
			continue
		var d := fighters[1 - p.owner]
		if d.is_proj_invulnerable():
			continue
		for r: Rect2 in d.hurtboxes():
			if p.rect().intersects(r):
				contacts.append({"att": fighters[p.owner], "def": d, "proj": p, "point": p.pos})
				break
	_clash_projectiles()
	for c in contacts:
		if hooks and hooks.has_method("on_contact") and str(hooks.on_contact(c)) == "ignore":
			continue
		_apply(c)
	_resolve_throws()


static func _contact_point(a: Rect2, b: Rect2) -> Vector2:
	var i := a.intersection(b)
	return i.get_center()


func _clash_projectiles() -> void:
	for i in projectiles.size():
		var p := projectiles[i]
		for j in range(i + 1, projectiles.size()):
			var q := projectiles[j]
			if p.dead or q.dead or p.owner == q.owner or not p.rect().intersects(q.rect()):
				continue
			var sp := p.strength
			p.strength -= q.strength
			q.strength -= sp
			p.dead = p.strength <= 0
			q.dead = q.strength <= 0
			emit({"type": "clash", "pos": (p.pos + q.pos) * 0.5})


func can_block(d: Fighter, level: int) -> bool:
	if not d.is_blocking():
		return false
	match level:
		MoveDef.Level.LOW:
			return d.is_crouching()
		MoveDef.Level.OVERHEAD:
			return not d.is_crouching()
		MoveDef.Level.UNBLOCKABLE, MoveDef.Level.THROW:
			return false
	return true


func _apply(c: Dictionary) -> void:
	var a: Fighter = c["att"]
	var d: Fighter = c["def"]
	var p: Projectile = c.get("proj")
	var m: MoveDef = p.move if p else c["move"]
	var level: int = p.level if p else m.level
	var damage: int = p.damage if p else m.damage
	var chip: int = p.chip if p else m.chip
	var effect: String = p.effect if p else m.effect
	var hs: int = p.hitstop if p else m.hitstop
	if hs < 0:
		hs = 5 if damage < 50 else (8 if damage < 95 else 11)
	if p:
		p.hits_left -= 1
		p.dead = p.hits_left <= 0
	else:
		a.hit_groups[c["group"]] = true
	var dir := signf(d.pos.x - a.pos.x)
	if dir == 0.0:
		dir = float(a.facing)
	if d.state == Fighter.S.DIZZY:
		# Any blow on a dizzy fighter ends it.
		d.health = 0
		d.knock(Vector2(1.6, 3.0), "fall", int(dir))
		if not p:
			a.move_contact = "hit"
			a.hitstop = hs
		d.hitstop = hs
		emit({"type": "hit", "who": d.index, "by": a.index, "move": m.id, "damage": 0, "level": level, "point": c["point"], "combo": 1, "proj": p != null})
		return
	if not p and d.state == Fighter.S.MOVE and d.move.effects.has("parry") and level in [MoveDef.Level.HIGH, MoveDef.Level.MID, MoveDef.Level.OVERHEAD]:
		var pr: Dictionary = d.move.effects["parry"]
		var fr: Array = pr.get("frames", [3, 20])
		if d.mf >= int(fr[0]) and d.mf <= int(fr[1]):
			# Caught: the attacker is left open and the parrier answers at once.
			a.move = null
			a.take_stun(int(pr.get("stun", 34)), "dazed")
			a.hitstop = 10
			d.hitstop = 10
			var counter := d.def.move(str(pr.get("counter", "")))
			if counter:
				d.start_move(counter)
			emit({"type": "parry", "who": d.index, "by": a.index, "move": m.id, "point": c["point"]})
			return
	var buffed := d.buff == "armor" and d.buff_hits > 0 and level != MoveDef.Level.THROW
	if buffed or (d.state == Fighter.S.MOVE and MoveDef.in_range(d.move.armor, d.mf) and d.armor_left > 0):
		if buffed:
			d.buff_hits -= 1
		else:
			d.armor_left -= 1
		var absorbed := int(damage * 0.5)
		d.health = maxi(0, d.health - absorbed)
		d.damage_taken += absorbed
		d.hitstop = 6
		if not p:
			a.hitstop = 6
			a.move_contact = "block"
		emit({"type": "armor", "who": d.index, "by": a.index, "move": m.id, "damage": absorbed, "point": c["point"]})
		if d.health <= 0:
			d.knock(Vector2(1.5, 3.5), "fall", int(dir))
		return
	if can_block(d, level):
		var chip_dmg := chip if (p or m.kind != "normal") else 0
		if chip_dmg > 0:
			d.health = maxi(0, d.health - chip_dmg)
			d.damage_taken += chip_dmg
		d.take_stun(p.blockstun if p else m.blockstun, "crouch_guard" if d.is_crouching() else "guard")
		d.push((p.push_block if p else m.push_block) * dir, a.index)
		d.hitstop = maxi(4, hs - 2)
		if not p:
			a.hitstop = d.hitstop
			a.move_contact = "block"
		emit({"type": "block", "who": d.index, "by": a.index, "move": m.id, "damage": chip_dmg, "point": c["point"], "proj": p != null})
		if d.health <= 0:
			d.knock(Vector2(1.6, 3.0), "fall", int(dir))
		return
	var n := d.combo_hits + 1
	var k := 1.0 if n <= 2 else maxf(0.4, 1.0 - 0.1 * (n - 2))
	var dmg := maxi(1, int(round(damage * k)))
	d.health = maxi(0, d.health - dmg)
	d.damage_taken += dmg
	d.combo_hits = n
	d.combo_damage += dmg
	d.last_hit_move = m
	d.last_hit_level = level
	var launch: Vector2 = p.launch if p else m.launch
	var jug: Vector2 = p.juggle if p else m.juggle
	if d.is_airborne():
		d.juggle_points += 1 if p else m.juggle_cost
		if not p and m.effects.get("air_knock", false):
			# Air-to-air strikes knock the other jumper out of the sky.
			effect = "knockdown"
			launch = Vector2(2.0, 2.5)
		match effect:
			"launch":
				d.knock(launch, "juggle", int(dir))
			"knockdown", "sweep":
				d.knock(launch, "fall", int(dir))
			_:
				d.knock(jug, "juggle", int(dir))
	else:
		match effect:
			"launch":
				d.juggle_points += m.juggle_cost if not p else 1
				d.knock(launch, "juggle", int(dir))
			"knockdown", "sweep":
				d.knock(launch, "fall", int(dir))
			"crumple":
				d.take_stun(p.hitstun if p else m.hitstun, "crumple")
			"pull":
				d.take_stun(p.hitstun if p else m.hitstun, "pulled")
			"daze":
				d.take_stun(p.hitstun if p else m.hitstun, "dazed")
			_:
				var kind := "crouch" if d.is_crouching() else ("high" if level == MoveDef.Level.HIGH else "mid")
				d.take_stun(p.hitstun if p else m.hitstun, kind)
				d.push((p.push_hit if p else m.push_hit) * dir, a.index)
	d.hitstop = hs
	if not p:
		a.hitstop = hs
		a.move_contact = "hit"
	emit({"type": "hit", "who": d.index, "by": a.index, "move": m.id, "damage": dmg, "level": level, "effect": effect,
		"point": c["point"], "combo": n, "proj": p != null, "kind": p.kind if p else ""})


func _resolve_throws() -> void:
	var attempts := []
	for a in fighters:
		if a.state != Fighter.S.MOVE or a.throw_victim != null or a.move_contact != "":
			continue
		var m := a.move
		if not (m.kind in ["throw", "grab"]) or not m.in_active(a.mf):
			continue
		var d := a.opponent()
		if d.is_invulnerable() or d.is_airborne() or d.def.raw.get("stats", {}).get("unthrowable", false):
			continue
		if d.state in [Fighter.S.HITSTUN, Fighter.S.BLOCKSTUN, Fighter.S.THROWN]:
			continue
		var ok := false
		if m.kind == "throw":
			ok = absf(d.pos.x - a.pos.x) <= a.def.throw_range + 0.05
		else:
			for hb in m.hitboxes:
				if a.mf < hb["from"] or a.mf > hb["to"]:
					continue
				for r: Rect2 in d.hurtboxes():
					if a.to_world(hb["rect"]).intersects(r):
						ok = true
		if ok:
			attempts.append(a)
	if attempts.size() == 2:
		# Both grabbed at once: they break apart.
		_break_throw(attempts[0], attempts[1])
		return
	for a: Fighter in attempts:
		var d := a.opponent()
		if hooks and hooks.has_method("on_contact") and str(hooks.on_contact({"att": a, "def": d, "move": a.move, "throw": true, "point": d.pos})) == "ignore":
			continue
		a.throw_victim = d
		a.throw_t = 0
		a.move_contact = "hit"
		d.thrown_by = a
		d.move = null
		d.vel = Vector2.ZERO
		d.set_state(Fighter.S.THROWN)
		emit({"type": "throw", "who": d.index, "by": a.index, "move": a.move.id})


func _break_throw(a: Fighter, d: Fighter) -> void:
	var dir := signf(d.pos.x - a.pos.x)
	if dir == 0.0:
		dir = float(a.facing)
	a.throw_victim = null
	d.thrown_by = null
	for f in [a, d]:
		f.take_stun(16, "break")
	a.push(-0.7 * dir, d.index)
	d.push(0.7 * dir, a.index)
	emit({"type": "throw_break", "who": d.index, "by": a.index})


func _throw_tick(a: Fighter) -> void:
	var d := a.throw_victim
	var td := a.move.throw_data
	a.throw_t += 1
	if a.throw_t <= THROW_BREAK_WINDOW and d.input.press_age(InputBuffer.LP | InputBuffer.HP, 1) == 0 and a.move.kind == "throw":
		_break_throw(a, d)
		return
	var path: Array = td.get("path", [[0, 0.7, 0.0]])
	var off := _path_at(path, a.throw_t) * a.scale()
	d.pos = Vector2(a.pos.x + off.x * a.facing, maxf(0.0, a.pos.y + off.y))
	d.facing = -a.facing
	if a.throw_t == int(td.get("damage_frame", 20)):
		var dmg := int(td.get("damage", 100))
		d.health = maxi(0, d.health - dmg)
		d.damage_taken += dmg
		d.combo_hits += 1
		d.combo_damage += dmg
		emit({"type": "hit", "who": d.index, "by": a.index, "move": a.move.id, "damage": dmg, "level": MoveDef.Level.THROW,
			"point": d.pos + Vector2(0, 1.0), "combo": d.combo_hits, "proj": false, "throw": true})
	if a.throw_t >= int(td.get("frames", 40)):
		var rel: Array = td.get("release", [2.0, 3.0])
		var away := -a.facing if td.get("swap", false) else a.facing
		a.throw_victim = null
		d.thrown_by = null
		d.knock(Vector2(float(rel[0]), float(rel[1])), "fall", away)
		a.move = null
		a.set_state(Fighter.S.LAND)
		a.land_frames = int(td.get("recover", 10))


static func _path_at(path: Array, t: int) -> Vector2:
	var prev: Array = path[0]
	for k in path:
		if t <= int(k[0]):
			var span := maxf(float(k[0]) - float(prev[0]), 1.0)
			var u := clampf((t - float(prev[0])) / span, 0.0, 1.0)
			return Vector2(lerpf(prev[1], k[1], u), lerpf(prev[2], k[2], u))
		prev = k
	return Vector2(float(prev[1]), float(prev[2]))


# --- KO, finish and round flow -----------------------------------------------------------------

func _check_round() -> void:
	match phase:
		Phase.FIGHT:
			var dead := [fighters[0].health <= 0, fighters[1].health <= 0]
			if dead[0] and dead[1]:
				for f in fighters:
					if not f.state in [Fighter.S.FALL, Fighter.S.JUGGLE, Fighter.S.KO, Fighter.S.DOWN]:
						f.knock(Vector2(1.2, 3.0), "fall")
				_end_round(-1, "double_ko")
			elif dead[0] or dead[1]:
				var loser := 0 if dead[0] else 1
				var winner := 1 - loser
				var lf := fighters[loser]
				if wins[winner] + 1 >= rounds_to_win and _finish_allowed(winner):
					finish_winner = winner
					phase = Phase.FINISH
					phase_t = 0
					emit({"type": "ko", "who": loser, "by": winner, "finish": true})
				else:
					if lf.state in [Fighter.S.HITSTUN, Fighter.S.BLOCKSTUN, Fighter.S.IDLE, Fighter.S.MOVE, Fighter.S.THROWN] and lf.thrown_by == null:
						lf.knock(Vector2(1.4, 3.2), "fall")
					emit({"type": "ko", "who": loser, "by": winner, "finish": false, "boss": lf.def.boss})
					_end_round(winner, "ko")
			elif timer <= 0:
				var h0 := float(fighters[0].health) / fighters[0].max_health()
				var h1 := float(fighters[1].health) / fighters[1].max_health()
				var w := -1 if is_equal_approx(h0, h1) else (0 if h0 > h1 else 1)
				emit({"type": "timeout", "winner": w})
				_end_round(w, "timeout")
		Phase.FINISH:
			var victim := fighters[1 - finish_winner]
			victim.health = 0
			if not finish_dizzy:
				# The beaten fighter gets back up, unsteady, before the winner may finish them.
				var standing := victim.state in [Fighter.S.IDLE, Fighter.S.CROUCH, Fighter.S.WALK_F, Fighter.S.WALK_B, Fighter.S.BLOCK, Fighter.S.CROUCH_BLOCK, Fighter.S.LAND]
				var risen := victim.state == Fighter.S.DOWN and victim.sf >= Fighter.DOWN_FRAMES
				if (standing or risen) and victim.hitstop <= 0:
					victim.move = null
					victim.pos.y = 0.0
					victim.set_state(Fighter.S.DIZZY)
					finish_dizzy = true
					finish_t0 = phase_t
					emit({"type": "finish_prompt", "who": finish_winner, "victim": victim.index, "pronoun": victim.def.pronoun})
				elif phase_t > 600:
					_end_round(finish_winner, "ko")
			elif victim.state == Fighter.S.DIZZY:
				if phase_t - finish_t0 > FINISH_TICKS:
					victim.knock(Vector2(0.3, 1.0), "fall")
					emit({"type": "finish_timeout", "who": finish_winner})
					_end_round(finish_winner, "ko")
			else:
				_end_round(finish_winner, "ko")
		Phase.ROUND_END:
			_round_end_tick()


## A beaten fighter who should get back up dizzy rather than stay down.
func awaiting_dizzy(f: Fighter) -> bool:
	return phase == Phase.FINISH and not finish_dizzy and f.index != finish_winner


func _finish_allowed(winner: int) -> bool:
	if not finishers_enabled or training:
		return false
	return not fighters[1 - winner].def.boss and not fighters[winner].def.boss


func _end_round(winner: int, how: String) -> void:
	phase = Phase.ROUND_END
	phase_t = 0
	round_winner = winner
	round_how = how
	if winner >= 0:
		wins[winner] += 1
		if fighters[winner].damage_taken == 0:
			flawless[winner] = true
	emit({"type": "round_end", "winner": winner, "how": how, "round": round_num, "flawless": winner >= 0 and flawless[winner]})


func _round_end_tick() -> void:
	var settled := true
	for f in fighters:
		if f.state in [Fighter.S.MOVE, Fighter.S.AIR, Fighter.S.JUGGLE, Fighter.S.FALL, Fighter.S.HITSTUN, Fighter.S.BLOCKSTUN, Fighter.S.PREJUMP] or f.hitstop > 0 or f.throw_victim != null:
			settled = false
	if round_winner >= 0:
		var w := fighters[round_winner]
		if settled and w.state != Fighter.S.WIN and phase_t > 40:
			w.move = null
			w.set_state(Fighter.S.WIN)
			emit({"type": "win_pose", "who": round_winner, "flawless": flawless[round_winner]})
			var l := w.opponent()
			if round_how == "timeout" and l.state in [Fighter.S.IDLE, Fighter.S.CROUCH, Fighter.S.BLOCK, Fighter.S.CROUCH_BLOCK, Fighter.S.WALK_F, Fighter.S.WALK_B]:
				l.set_state(Fighter.S.DIZZY)
	else:
		for f in fighters:
			if settled and f.health > 0 and f.state != Fighter.S.DIZZY and f.is_actionable():
				f.set_state(Fighter.S.DIZZY)
	if phase_t < ROUND_END_TICKS + (40 if not settled else 0):
		return
	if not settled and phase_t < ROUND_END_TICKS + 240:
		return
	var decided: bool = wins[0] >= rounds_to_win or wins[1] >= rounds_to_win
	if decided or round_num >= MAX_ROUNDS:
		match_winner = -1
		if wins[0] >= rounds_to_win:
			match_winner = 0
		elif wins[1] >= rounds_to_win:
			match_winner = 1
		phase = Phase.MATCH_END
		phase_t = 0
		emit({"type": "match_end", "winner": match_winner, "wins": wins.duplicate(), "finisher": finisher.get("move", ""),
			"flawless": match_winner >= 0 and flawless[match_winner]})
		return
	round_num += 1
	_start_round(false)


## Called by a fighter in neutral during the FINISH phase: performs its finisher (or the arena's)
## when the input and range match. Returns true when one began.
func try_finisher(f: Fighter) -> bool:
	if phase != Phase.FINISH or f.index != finish_winner:
		return false
	var v := f.opponent()
	if v.state != Fighter.S.DIZZY:
		return false
	var gap := absf(v.pos.x - f.pos.x)
	var candidates := []
	if f.def.finisher_cmd:
		candidates.append([f.def.finisher_cmd, f.def.finisher_range])
	if stage_finisher != "" and f.def.move(stage_finisher):
		var sc := Command.parse(str(f.def.move(stage_finisher).effects.get("input", "D,D,D+LP")))
		sc.move = stage_finisher
		candidates.append([sc, "close"])
	for cand in candidates:
		var c: Command = cand[0]
		var age := f.input.chord_age(c.mask)
		if age < 0 or not c.matches(f.input, age, f.facing):
			continue
		var r: Array = RANGES.get(cand[1], RANGES["close"])
		if gap < r[0] or gap > r[1]:
			emit({"type": "finisher_range", "who": f.index, "gap": gap, "want": cand[1]})
			continue
		f.input.consume(c.mask)
		begin_finisher(f, c.move)
		return true
	return false


func begin_finisher(f: Fighter, move_id: String) -> void:
	var v := f.opponent()
	phase = Phase.FINISHER
	phase_t = 0
	finisher = {"who": f.index, "move": move_id, "victim": v.index, "stage": move_id == stage_finisher}
	f.move = f.def.move(move_id)
	f.mf = 0
	f.vel = Vector2.ZERO
	v.vel = Vector2.ZERO
	f.set_state(Fighter.S.FINISHER)
	v.set_state(Fighter.S.VICTIM)
	projectiles.clear()
	for x in fighters:
		x.projectiles_out = 0
	emit({"type": "finisher", "who": f.index, "victim": v.index, "move": move_id})


func _finisher_tick() -> void:
	var f := fighters[int(finisher["who"])]
	f.mf += 1
	f.sf += 1
	f.opponent().sf += 1
	if f.mf >= f.move.total:
		f.set_state(Fighter.S.WIN)
		f.opponent().set_state(Fighter.S.KO)
		emit({"type": "finisher_end", "who": f.index, "move": finisher["move"]})
		_end_round(f.index, "finisher")
		# The finisher already showed the outcome; the results follow promptly.
		phase_t = ROUND_END_TICKS - 30


## Ends a finisher early (the player skipped it); the round result stands.
func skip_finisher() -> void:
	if phase != Phase.FINISHER:
		return
	var f := fighters[int(finisher["who"])]
	f.mf = f.move.total - 1
