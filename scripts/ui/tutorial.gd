extends Node
## The tutorial, run inside a training match (Ashka against Gorm): one prompt at a time, each
## checked against what actually happens in the simulation. Gorm attacks on cue for the blocking
## steps; the last step lets the player knock him out and perform a Sloptality. Tab skips a step.

var m: Match
var sim: FightSim
var step := -1
var _count := 0
var _seen := {}
var _t := 0
var _done := false

const STEPS := [
	{"id": "walk_f", "text": "Walk toward Gorm: hold %s."},
	{"id": "walk_b", "text": "Walk away: hold %s."},
	{"id": "jump", "text": "Jump with %s (twice). Up-forward or up-back jumps are somersaults."},
	{"id": "crouch", "text": "Crouch: hold %s. High attacks pass over a crouching fighter."},
	{"id": "strikes", "text": "Try all four strikes: High Punch %s, Low Punch %s, High Kick %s, Low Kick %s."},
	{"id": "block", "text": "Gorm will swing at you. Hold Block %s to stop his punches (3 blocks)."},
	{"id": "block_low", "text": "Sweeps hit low: hold Down %s and Block %s together to block them (2 blocks)."},
	{"id": "uppercut", "text": "The uppercut: Down + High Punch, up close. Land one on Gorm."},
	{"id": "special", "text": "A special move. Ember Shot: Down, Forward, High Punch, in quick succession."},
	{"id": "chain", "text": "A dial-a-combo: High Punch, High Punch, Low Punch, then Back + High Kick, quickly. Each link spends run meter."},
	{"id": "run", "text": "Run: hold Run %s and Forward. Running empties the blue meter under your health."},
	{"id": "throw", "text": "Throw: get right up to Gorm and press Forward + Low Punch."},
	{"id": "finish", "text": "Knock Gorm out. When FINISH HIM appears, stand a sweep's distance away (about two steps) and enter Forward, Forward, Down + High Punch."},
]


func setup(match_node: Match) -> void:
	m = match_node
	sim = m.sim
	sim.refill_health = true
	sim.phase = FightSim.Phase.FIGHT
	for f in sim.fighters:
		f.set_state(Fighter.S.IDLE)
	_next()


func _keys(id: String) -> String:
	return Settings.prompt(id)


func _next() -> void:
	step += 1
	_count = 0
	_seen.clear()
	_t = 0
	if step >= STEPS.size():
		_complete()
		return
	var s: Dictionary = STEPS[step]
	var text: String = s["text"]
	match s["id"]:
		"walk_f":
			text = text % _keys("right")
		"walk_b":
			text = text % _keys("left")
		"jump":
			text = text % _keys("up")
		"crouch":
			text = text % _keys("down")
		"strikes":
			text = text % [_keys("hp"), _keys("lp"), _keys("hk"), _keys("lk")]
		"block":
			text = text % _keys("bl")
		"block_low":
			text = text % [_keys("down"), _keys("bl")]
		"run":
			text = text % _keys("rn")
		"finish":
			_arm_finish()
	m.hud.prompt("Step %d of %d.  %s   (Tab: skip)" % [step + 1, STEPS.size(), text])
	Audio.play("ui_select", -4.0)


func _arm_finish() -> void:
	sim.training = false
	sim.refill_health = false
	sim.finishers_enabled = true
	sim.rounds_to_win = 1
	sim.wins = [0, 0]
	sim.timer = FightSim.ROUND_TICKS
	sim.fighters[1].health = 140


func _complete() -> void:
	_done = true
	m.hud.prompt("")
	Progress.data["tutorial_done"] = true
	Progress.save()
	if sim.phase in [FightSim.Phase.FINISHER, FightSim.Phase.ROUND_END, FightSim.Phase.MATCH_END]:
		# The finished match leads to the results by itself.
		return
	m.hud.announce("WELL FOUGHT", "The Sump awaits.", 2.5, UiKit.ACCENT, 110)
	await get_tree().create_timer(2.8).timeout
	if is_instance_valid(m):
		m.finished.emit({"winner": 0, "mode": "tutorial", "p1": "ashka", "p2": "gorm", "finisher": "", "flawless": false})


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("training_dummy") and not _done:
		get_viewport().set_input_as_handled()
		_next()


## Gorm's part: still, except when a blocking step needs him to attack.
func filter_pads(p: Array) -> Array:
	_t += 1
	var g := sim.fighters[1]
	var out := p.duplicate()
	out[1] = Vector3i.ZERO
	if step < 0 or step >= STEPS.size() or _done:
		return out
	var id: String = STEPS[step]["id"]
	if id in ["block", "block_low"] and g.is_actionable():
		# He closes in until the swing would actually land, so a held block has something to stop.
		var swing := "hp" if id == "block" else "sweep"
		if sim.distance() > _reach(g, swing) - 0.1:
			out[1] = PadIntent.pad(6, g.facing)
		elif _t % 70 == 0:
			out[1] = PadIntent.pad(5 if id == "block" else 4, g.facing, InputBuffer.HP if id == "block" else InputBuffer.LK)
	return out


## How far a move reaches: its furthest hitbox plus about half the other fighter's body.
func _reach(f: Fighter, move_id: String) -> float:
	var r := 0.0
	for hb in f.def.move(move_id).hitboxes:
		r = maxf(r, (hb["rect"] as Rect2).end.x)
	return r + 0.22 * sim.fighters[1 - f.index].scale()


func on_event(e: Dictionary) -> void:
	if _done or step < 0 or step >= STEPS.size():
		return
	var id: String = STEPS[step]["id"]
	var p := sim.fighters[0]
	match id:
		"jump":
			if e["type"] == "jump" and e["who"] == 0:
				_count += 1
				if _count >= 2:
					_next()
		"strikes":
			if e["type"] == "move" and e["who"] == 0 and e["move"] in ["hp", "lp", "hk", "lk", "hp2", "lp2"]:
				_seen[str(e["move"]).trim_suffix("2")] = true
				if _seen.size() >= 4:
					_next()
		"block":
			if e["type"] == "block" and e["who"] == 0:
				_count += 1
				if _count >= 3:
					_next()
		"block_low":
			if e["type"] == "block" and e["who"] == 0 and p.is_crouching():
				_count += 1
				if _count >= 2:
					_next()
		"uppercut":
			if e["type"] == "hit" and e["by"] == 0 and e["move"] == "uppercut":
				_next()
		"special":
			if e["type"] == "projectile" and e["who"] == 0:
				_next()
		"chain":
			if e["type"] == "chain" and e["who"] == 0 and e["move"] == "ashka_spin_chain":
				_next()
		"throw":
			if e["type"] == "throw" and e["by"] == 0:
				_next()
		"finish":
			if e["type"] == "finisher_end" and e["who"] == 0:
				_next()
			elif e["type"] == "round_end" and e.get("how", "") != "finisher":
				m.hud.notice("Not quite: knock him out, then enter the finisher at sweep distance. Again!")
				_retry_finish()


func _retry_finish() -> void:
	await get_tree().create_timer(1.6).timeout
	if _done or m == null or not is_instance_valid(m):
		return
	sim.restart_round()
	sim.phase = FightSim.Phase.FIGHT
	for f in sim.fighters:
		f.set_state(Fighter.S.IDLE)
	for v in m.views:
		v.sync_from_sim(true)
	_arm_finish()


func _physics_process(_delta: float) -> void:
	if _done or step < 0 or step >= STEPS.size() or m == null:
		return
	var p := sim.fighters[0]
	match STEPS[step]["id"]:
		"walk_f":
			if p.state == Fighter.S.WALK_F:
				_count += 1
				if _count > 40:
					_next()
		"walk_b":
			if p.state == Fighter.S.WALK_B:
				_count += 1
				if _count > 40:
					_next()
		"crouch":
			if p.state == Fighter.S.CROUCH:
				_count += 1
				if _count > 40:
					_next()
		"run":
			if p.state == Fighter.S.RUN:
				_count += 1
				if _count > 25:
					_next()
