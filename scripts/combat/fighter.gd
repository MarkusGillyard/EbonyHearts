class_name Fighter
extends RefCounted
## One fighter's state in a match: position on the fighting plane, the state machine, health,
## run meter and the move in progress. Driven by FightSim at 60 Hz; reads only its InputBuffer
## and the simulation, never the scene.

enum S { INTRO, IDLE, WALK_F, WALK_B, CROUCH, RUN, BLOCK, CROUCH_BLOCK, PREJUMP, AIR, LAND, MOVE,
	BLOCKSTUN, HITSTUN, JUGGLE, FALL, DOWN, WAKEUP, THROWN, DIZZY, KO, WIN, FINISHER, VICTIM }

const DT := 1.0 / 60.0
const PREJUMP_FRAMES := 3
const LAND_FRAMES := 3
const LAND_FRAMES_ATTACK := 5
const DOWN_FRAMES := 38
const WAKEUP_FRAMES := 24
const RUN_MAX := 100.0
## Running empties a full meter in about 1.7 seconds.
const RUN_DRAIN := 1.0
const RUN_REGEN := 1.6
## The meter starts refilling once it has been left alone this long (ticks).
const RUN_REGEN_DELAY := 30
const RUN_MIN_START := 12.0
## Juggle points a body can absorb before further hits pass through it.
const JUGGLE_LIMIT := 7
## Hurtboxes for a 1.8 m fighter: [back, up, width, height] relative to the root, facing right.
const BOX_STAND := [Rect2(-0.2, 0.0, 0.42, 0.95), Rect2(-0.22, 0.9, 0.46, 0.62), Rect2(-0.13, 1.45, 0.28, 0.38)]
const BOX_CROUCH := [Rect2(-0.24, 0.0, 0.5, 1.12)]
const BOX_AIR := [Rect2(-0.22, 0.25, 0.46, 1.25)]

var sim: FightSim
var index := 0
var def: FighterDef
var input := InputBuffer.new()
var pos := Vector2.ZERO
var vel := Vector2.ZERO
## +1 facing right (toward +x), -1 facing left.
var facing := 1
var state: int = S.IDLE
## Ticks spent in the current state, 1 on the first.
var sf := 0
var move: MoveDef
## Frame of the move in progress, 1-based.
var mf := 0
var move_cmd_priority := 0
var move_origin := Vector2.ZERO
## "", "hit" or "block" for the move in progress.
var move_contact := ""
var hit_groups := {}
var health := 1000
## Full health for this match: the definition's, unless the match sets it (the arcade's boss).
var health_max := 0
var run_meter := RUN_MAX
var run_idle := 0
var stun := 0
## "high", "low", "crouch", "guard", "crouch_guard", "crumple", "dazed", "pulled", "dizzy"
var stun_kind := ""
var hitstop := 0
var push_v := 0.0
var push_dec := 0.0
## Index of the fighter whose hit is pushing this one (gets the overflow at a wall).
var push_from := -1
var juggle_points := 0
var combo_hits := 0
var combo_damage := 0
var air_attack_used := false
## Relative direction of the jump: -1 back, 0 straight up, 1 forward.
var jump_dir := 0
var invuln := 0
var armor_left := 0
var projectiles_out := 0
var damage_taken := 0
## Throw in progress (thrower side): the victim and the frame of the throw sequence.
var throw_victim: Fighter = null
var thrown_by: Fighter = null
var throw_t := 0
## The move that last hit this fighter, for the reaction pose.
var last_hit_move: MoveDef
var last_hit_level := MoveDef.Level.MID
var crouch_stun := false
var land_frames := LAND_FRAMES
## A temporary power-up from a move (Gorm's Tallow Hide): its name, ticks left, hits it absorbs.
var buff := ""
var buff_ticks := 0
var buff_hits := 0
## Training: the dummy holds block automatically.
var auto_block := ""


func setup(s: FightSim, i: int, d: FighterDef) -> void:
	sim = s
	index = i
	def = d
	health = d.health


func opponent() -> Fighter:
	return sim.fighters[1 - index]


func set_state(s: int) -> void:
	if state == s:
		return
	state = s
	sf = 0


func state_name() -> String:
	return S.keys()[state]


# --- Queries --------------------------------------------------------------------------------

func is_airborne() -> bool:
	match state:
		S.AIR, S.JUGGLE, S.FALL:
			return true
		S.MOVE:
			return move.air or MoveDef.in_range(move.airborne, mf) or pos.y > 0.05
		S.THROWN, S.VICTIM:
			return pos.y > 0.05
	return false


func is_crouching() -> bool:
	match state:
		S.CROUCH, S.CROUCH_BLOCK:
			return true
		S.BLOCKSTUN, S.HITSTUN:
			return crouch_stun
		S.MOVE:
			return "crouch" in move.tags or MoveDef.in_range(move.low_profile, mf)
	return false


func is_blocking() -> bool:
	return state == S.BLOCK or state == S.CROUCH_BLOCK or (state == S.BLOCKSTUN and stun_kind.ends_with("guard"))


## Not hittable at all right now.
func is_invulnerable() -> bool:
	if invuln > 0:
		return true
	match state:
		S.DOWN, S.WAKEUP, S.KO, S.INTRO, S.WIN, S.FINISHER, S.VICTIM, S.THROWN:
			return true
		S.FALL:
			return true
		S.JUGGLE:
			return juggle_points >= JUGGLE_LIMIT
		S.MOVE:
			return MoveDef.in_range(move.invuln, mf)
	return false


func is_proj_invulnerable() -> bool:
	return is_invulnerable() or (state == S.MOVE and MoveDef.in_range(move.proj_invuln, mf))


## Free to start an action (also used by the CPU and tests).
func is_actionable() -> bool:
	return state in [S.IDLE, S.WALK_F, S.WALK_B, S.CROUCH, S.BLOCK, S.CROUCH_BLOCK, S.RUN]


func max_health() -> int:
	return health_max if health_max > 0 else def.health


func scale() -> float:
	return def.height / FighterDef.BASE_HEIGHT


## Local [forward, up] rect to world space.
func to_world(r: Rect2) -> Rect2:
	var x0 := pos.x + (r.position.x if facing > 0 else -(r.position.x + r.size.x))
	return Rect2(Vector2(x0, pos.y + r.position.y), r.size)


func hurtboxes() -> Array[Rect2]:
	var out: Array[Rect2] = []
	if is_invulnerable():
		return out
	var k := scale()
	var src: Array = BOX_STAND
	if is_airborne():
		src = BOX_AIR
	elif is_crouching():
		src = BOX_CROUCH
	for r: Rect2 in src:
		out.append(to_world(Rect2(r.position * k, r.size * k)))
	if state == S.MOVE:
		for hb in move.hurtboxes:
			if mf >= hb["from"] and mf <= hb["to"]:
				out.append(to_world(hb["rect"]))
	return out


## Active hitboxes this frame that have not connected yet: [[group index, world rect], ...].
func active_hitboxes() -> Array:
	var out := []
	if state != S.MOVE or not move.is_attack() or move.level == MoveDef.Level.THROW:
		return out
	for i in move.hitboxes.size():
		var hb: Dictionary = move.hitboxes[i]
		if mf >= hb["from"] and mf <= hb["to"] and not hit_groups.has(i):
			out.append([i, to_world(hb["rect"])])
	return out


func pushbox() -> Rect2:
	var k := scale()
	var w := def.push_w
	if is_airborne():
		return Rect2(pos.x - w * 0.85, pos.y + 0.3 * k, w * 1.7, 0.75 * k)
	var h := (1.05 if is_crouching() else 1.55) * k
	return Rect2(pos.x - w, pos.y, w * 2.0, h)


func has_pushbox() -> bool:
	return not state in [S.DOWN, S.KO, S.THROWN, S.FINISHER, S.VICTIM]


# --- Tick -------------------------------------------------------------------------------------

func tick() -> void:
	if hitstop > 0:
		hitstop -= 1
		return
	sf += 1
	if invuln > 0:
		invuln -= 1
	if buff_ticks > 0:
		buff_ticks -= 1
		if buff_ticks == 0 or buff_hits <= 0:
			buff_ticks = 0
			buff = ""
			sim.emit({"type": "buff_end", "who": index})
	_regen_run()
	match state:
		S.INTRO, S.WIN, S.DIZZY:
			vel = Vector2.ZERO
			_gravity_settle()
		S.IDLE, S.WALK_F, S.WALK_B, S.CROUCH, S.BLOCK, S.CROUCH_BLOCK:
			_neutral()
		S.RUN:
			_run()
		S.PREJUMP:
			# Motions ending in up (D,U+HK) arrive a tick or two after the jump began.
			if sim.can_act() and try_commands(def.commands):
				return
			if sf >= PREJUMP_FRAMES:
				_jump()
		S.AIR:
			_air()
		S.LAND:
			if sf >= land_frames:
				to_neutral()
		S.MOVE:
			_move_tick()
		S.BLOCKSTUN, S.HITSTUN:
			_stun_tick()
		S.JUGGLE, S.FALL:
			_fall_tick()
		S.DOWN:
			if sf >= DOWN_FRAMES and health > 0:
				set_state(S.WAKEUP)
		S.WAKEUP:
			if sf >= WAKEUP_FRAMES:
				to_neutral()
		S.KO:
			_gravity_settle()
	_apply_push()


func _regen_run() -> void:
	if state == S.RUN:
		run_idle = 0
		return
	run_idle += 1
	if sim.infinite_run:
		run_meter = RUN_MAX
	elif run_idle > RUN_REGEN_DELAY:
		run_meter = minf(RUN_MAX, run_meter + RUN_REGEN)


func spend_run(amount: float) -> void:
	run_meter = maxf(0.0, run_meter - amount)
	run_idle = 0


func _gravity_settle() -> void:
	if pos.y > 0.0:
		vel.y -= def.gravity * DT
		pos.y = maxf(0.0, pos.y + vel.y * DT)


func _apply_push() -> void:
	if push_v != 0.0:
		pos.x += push_v * DT
		push_v = move_toward(push_v, 0.0, push_dec)


## Pushes this fighter `dist` metres along world x (sign gives direction) over about 8 ticks.
func push(dist: float, from: int) -> void:
	push_v = dist * 15.0
	push_dec = absf(push_v) / 8.0
	push_from = from


func to_neutral() -> void:
	move = null
	mf = 0
	vel.x = 0.0
	combo_reset()
	var d := input.dir()
	if input.held() & InputBuffer.BL and sim.can_act():
		set_state(S.CROUCH_BLOCK if InputBuffer.is_down(d) else S.BLOCK)
	elif InputBuffer.is_down(d) and sim.can_act():
		set_state(S.CROUCH)
	else:
		set_state(S.IDLE)


func combo_reset() -> void:
	juggle_points = 0
	combo_hits = 0
	combo_damage = 0


# --- Neutral, walking, running, jumping ---------------------------------------------------------

func _neutral() -> void:
	var d := input.dir()
	var held := input.held()
	if not sim.can_act() or health <= 0:
		vel.x = 0.0
		set_state(S.IDLE)
		return
	if sim.try_finisher(self):
		return
	if try_commands(def.commands):
		return
	if auto_block != "":
		held |= InputBuffer.BL
		d = 2 if auto_block == "crouch" else 5
	if held & InputBuffer.BL:
		vel.x = 0.0
		set_state(S.CROUCH_BLOCK if InputBuffer.is_down(d) else S.BLOCK)
		return
	if InputBuffer.is_up(d):
		jump_dir = 1 if d == 9 else (-1 if d == 7 else 0)
		vel.x = 0.0
		set_state(S.PREJUMP)
		return
	if InputBuffer.is_down(d):
		vel.x = 0.0
		set_state(S.CROUCH)
		return
	if d == 6 and held & InputBuffer.RN and run_meter >= RUN_MIN_START:
		set_state(S.RUN)
		sim.emit({"type": "run", "who": index})
		_run()
		return
	if d == 6:
		set_state(S.WALK_F)
		vel.x = def.walk_f * facing
	elif d == 4:
		set_state(S.WALK_B)
		vel.x = -def.walk_b * facing
	else:
		set_state(S.IDLE)
		vel.x = 0.0
	pos.x += vel.x * DT


func _run() -> void:
	var d := input.dir()
	if try_commands(def.commands):
		return
	if d != 6 or not (input.held() & InputBuffer.RN) or run_meter <= 0.0 or input.held() & InputBuffer.BL:
		to_neutral()
		return
	if not sim.infinite_run:
		run_meter = maxf(0.0, run_meter - RUN_DRAIN)
	vel.x = def.run_speed * facing
	pos.x += vel.x * DT


func _jump() -> void:
	vel = Vector2(def.jump_vx * jump_dir * facing, def.jump_vy)
	air_attack_used = false
	set_state(S.AIR)
	sim.emit({"type": "jump", "who": index})
	_air()


func _air() -> void:
	if not air_attack_used and sim.can_act() and try_commands(def.air_commands):
		air_attack_used = true
		return
	_integrate_air(def.gravity)
	if _landed():
		_land(LAND_FRAMES)


func _integrate_air(g: float) -> void:
	vel.y -= g * DT
	pos += vel * DT


func _landed() -> bool:
	return pos.y <= 0.0 and vel.y <= 0.0


func _land(frames: int) -> void:
	pos.y = 0.0
	move = null
	set_state(S.LAND)
	land_frames = frames
	vel = Vector2.ZERO
	sim.emit({"type": "land", "who": index, "soft": true})


# --- Commands and moves ---------------------------------------------------------------------

## Starts the first command in `list` whose buttons were just pressed and whose direction and
## conditions are met. `min_priority` limits a cancel to more specific commands.
func try_commands(list: Array[Command], min_priority := -1, kinds: Array = []) -> bool:
	if input.press_age(InputBuffer.ALL) < 0:
		return false
	for c in list:
		if c.priority() <= min_priority:
			continue
		var m := def.move(c.move)
		if m == null or (not kinds.is_empty() and not m.kind in kinds):
			continue
		var age := input.chord_age(c.mask)
		if age < 0 or not c.matches(input, age, facing):
			continue
		if not can_use(m):
			continue
		input.consume(c.mask)
		move_cmd_priority = c.priority()
		start_move(m)
		return true
	return false


func can_use(m: MoveDef) -> bool:
	if m.finisher:
		return false
	if not m.projectile.is_empty() and projectiles_out > 0:
		return false
	if m.run_cost > 0.0 and run_meter < m.run_cost and not sim.infinite_run:
		return false
	if m.air != (state == S.AIR):
		return false
	if m.effects.has("health_below") and float(health) / max_health() >= float(m.effects["health_below"]):
		return false
	if m.effects.has("buff") and buff_ticks > 0:
		return false
	match m.requires:
		"close":
			var o := opponent()
			if absf(o.pos.x - pos.x) > def.throw_range or o.is_airborne() or o.is_invulnerable():
				return false
			if o.def.raw.get("stats", {}).get("unthrowable", false) and m.kind in ["throw", "grab"]:
				return false
			if o.state in [S.HITSTUN, S.BLOCKSTUN, S.THROWN, S.JUGGLE] or (o.state == S.MOVE and o.move.kind in ["throw", "grab"] and o.mf > o.move.startup):
				return false
		"straight":
			if jump_dir != 0:
				return false
		"angled":
			if jump_dir == 0:
				return false
	return true


func start_move(m: MoveDef) -> void:
	move = m
	mf = 1
	move_origin = pos
	move_contact = ""
	hit_groups.clear()
	armor_left = int(m.effects.get("armor_hits", 1)) if m.armor != Vector2i.ZERO else 0
	if m.run_cost > 0.0 and not sim.infinite_run:
		spend_run(m.run_cost)
	if not m.air:
		vel = Vector2.ZERO
	set_state(S.MOVE)
	sim.emit({"type": "move", "who": index, "move": m.id})
	_move_frame()


func _move_tick() -> void:
	if throw_victim != null:
		# FightSim drives the throw sequence.
		return
	mf += 1
	# A plain button that turns out to be the first half of a chord or a motion upgrades
	# to the more specific command within the chord window.
	if mf <= InputBuffer.CHORD_TICKS + 1 and move.kind == "normal" and not move.air:
		if try_commands(def.commands, move_cmd_priority):
			return
	if _try_chain():
		return
	if move.cancel != Vector2i.ZERO and move_contact != "" and MoveDef.in_range(move.cancel, mf):
		if try_commands(def.commands, -1, ["special"]):
			return
	if move.air:
		var g := def.gravity
		if move.lift.is_empty():
			_integrate_air(g)
		else:
			_move_frame()
		if _landed() and mf > 2:
			_land(LAND_FRAMES_ATTACK if move_contact == "" else LAND_FRAMES)
			return
		if mf > move.total:
			# The attack is spent; the rest of the jump continues without it.
			move = null
			set_state(S.AIR)
			air_attack_used = true
			return
		if not move.lift.is_empty():
			return
		_spawn_check()
		return
	if mf > move.total:
		var crouched := move.ends_crouched
		move = null
		to_neutral()
		if crouched and InputBuffer.is_down(input.dir()):
			set_state(S.CROUCH)
		return
	_move_frame()


func _try_chain() -> bool:
	for link in move.chain:
		if mf < link["from"] or mf > link["to"]:
			continue
		var age := input.chord_age(link["mask"])
		if age < 0:
			continue
		var steps: Array = link["steps"]
		if steps.size() == 1 and not input.dir_as(age, facing) in Command.HELD[steps[0]]:
			continue
		var cost: float = link["cost"]
		if run_meter < cost and not sim.infinite_run:
			continue
		var m := def.move(link["move"])
		if m == null:
			continue
		input.consume(link["mask"])
		if not sim.infinite_run:
			spend_run(cost)
		var contact := move_contact
		start_move(m)
		sim.emit({"type": "chain", "who": index, "move": m.id, "after": contact})
		return true
	return false


## Root motion, flight paths, tracking and projectile spawns for the current move frame.
func _move_frame() -> void:
	var m := move
	if not m.motion.is_empty():
		var dx := MoveDef.curve_at(m.motion, mf) - MoveDef.curve_at(m.motion, mf - 1)
		pos.x += dx * facing
	if not m.lift.is_empty():
		pos.y = maxf(0.0, move_origin.y + MoveDef.curve_at(m.lift, mf))
	if m.track != Vector3.ZERO and mf >= int(m.track.x) and mf <= int(m.track.y):
		var o := opponent()
		pos.x = move_toward(pos.x, o.pos.x, m.track.z * DT)
	var fx := m.effects
	if fx.has("teleport") and mf == int(fx["teleport"].get("frame", 20)):
		# Reappears on the far side of the opponent, facing them.
		var o := opponent()
		var d := float(fx["teleport"].get("dist", 0.9)) * scale()
		var side := -o.facing
		pos.x = clampf(o.pos.x + side * d, -FightSim.STAGE_HALF + def.push_w, FightSim.STAGE_HALF - def.push_w)
		if absf(pos.x - o.pos.x) < d * 0.5:
			pos.x = clampf(o.pos.x - side * d, -FightSim.STAGE_HALF + def.push_w, FightSim.STAGE_HALF - def.push_w)
		facing = 1 if o.pos.x > pos.x else -1
		sim.emit({"type": "teleport", "who": index})
	if fx.has("air_vel") and mf == int(fx["air_vel"][0]):
		vel = Vector2(float(fx["air_vel"][1]) * facing, float(fx["air_vel"][2]))
	if fx.has("buff") and mf == int(fx.get("buff_frame", m.startup)):
		buff = str(fx["buff"])
		buff_ticks = int(fx.get("buff_ticks", 240))
		buff_hits = int(fx.get("buff_hits", 2))
		sim.emit({"type": "buff", "who": index, "buff": buff})
	_spawn_check()


func _spawn_check() -> void:
	if not move.projectile.is_empty() and mf == int(move.projectile.get("frame", move.startup)):
		sim.spawn_projectile(self, move)


# --- Being hit ----------------------------------------------------------------------------------

func _stun_tick() -> void:
	stun -= 1
	if state == S.BLOCKSTUN:
		# The guard can change height while it holds.
		crouch_stun = InputBuffer.is_down(input.dir()) or auto_block == "crouch"
		stun_kind = "crouch_guard" if crouch_stun else "guard"
	if stun_kind == "pulled":
		# Reeled in toward the puller until close, then held briefly.
		var o := opponent()
		var gap := absf(o.pos.x - pos.x)
		if gap > 0.85 * (scale() + o.scale()) * 0.5 + 0.35:
			pos.x = move_toward(pos.x, o.pos.x, 9.0 * DT)
	if pos.y > 0.0:
		_integrate_air(def.gravity)
		pos.y = maxf(pos.y, 0.0)
	if stun <= 0:
		if stun_kind == "crumple":
			knock(Vector2(0.6, 1.2), "fall")
			return
		to_neutral()


func _fall_tick() -> void:
	var g := def.gravity * (1.0 + 0.12 * juggle_points)
	_integrate_air(g)
	if _landed():
		pos.y = 0.0
		vel = Vector2.ZERO
		combo_hits = 0
		juggle_points = 0
		set_state(S.KO if health <= 0 and not sim.awaiting_dizzy(self) else S.DOWN)
		sim.emit({"type": "land", "who": index, "soft": false})


## A thrower who is hit lets go: the victim drops out of the throw and lands, unhurt by it.
func release_throw() -> void:
	if throw_victim == null:
		return
	var v := throw_victim
	throw_victim = null
	v.thrown_by = null
	v.knock(Vector2(1.0, 2.0), "fall", facing)


## Sends this fighter flying: `v` is [away from the hitter, up] in m/s. `how` is "juggle"
## (hittable on the way down) or "fall" (knocked down, untouchable).
func knock(v: Vector2, how: String, from_facing := 0) -> void:
	release_throw()
	var away := from_facing if from_facing != 0 else -facing
	vel = Vector2(v.x * away, v.y)
	if pos.y <= 0.0 and v.y > 0.0:
		pos.y = 0.01
	move = null
	set_state(S.JUGGLE if how == "juggle" else S.FALL)


func take_stun(frames: int, kind: String) -> void:
	release_throw()
	move = null
	stun = frames
	stun_kind = kind
	crouch_stun = kind in ["crouch", "crouch_guard"]
	set_state(S.BLOCKSTUN if kind.ends_with("guard") else S.HITSTUN)
