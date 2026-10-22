class_name BossBrain
extends RefCounted
## The Slopbat's mind. It fights in readable patterns rather than reflexes: claws and bites up
## close, tar and wind from afar, the Dive on a cooldown (its shadow gives it away), and below half
## health the Storm Shroud, a long telegraphed charge that can be escaped or interrupted. Every big
## attack leaves it open afterwards. Difficulty shortens its cooldowns and reaction time, but it
## never reacts to anything before it has been on screen for a while.

const LEVELS := [
	{"react": 22, "dive_cd": 720, "storm_cd": 660, "aa": 0.25, "think": 26, "block": 0.15},
	{"react": 16, "dive_cd": 560, "storm_cd": 520, "aa": 0.45, "think": 20, "block": 0.3},
	{"react": 12, "dive_cd": 440, "storm_cd": 420, "aa": 0.65, "think": 15, "block": 0.45},
]

var sim: FightSim
var index := 1
var level: Dictionary
var rng := RandomNumberGenerator.new()
var intent := PadIntent.new()
var _seen: Array = []
var _think := 60
var _dive_cd := 420
var _storm_cd := 240
var _walk := 5
var _walk_t := 0


func _init(s: FightSim, i: int, difficulty: int) -> void:
	sim = s
	index = i
	level = LEVELS[clampi(difficulty, 0, LEVELS.size() - 1)]
	rng.seed = 4242 + difficulty
	_dive_cd = int(level["dive_cd"]) / 2


func me() -> Fighter:
	return sim.fighters[index]


func opp() -> Fighter:
	return sim.fighters[1 - index]


func think() -> Vector3i:
	var m := me()
	var o := opp()
	_seen.push_front({"air": o.is_airborne(), "state": o.state, "x": o.pos.x})
	if _seen.size() > 40:
		_seen.pop_back()
	var seen: Dictionary = _seen[mini(int(level["react"]), _seen.size() - 1)]
	if not sim.can_act():
		intent.clear()
		return Vector3i.ZERO
	_dive_cd -= 1
	_storm_cd -= 1
	if intent.busy():
		return intent.next(m.facing)
	if not m.is_actionable():
		return Vector3i.ZERO
	var dist := absf(o.pos.x - m.pos.x)
	var low := float(m.health) / m.def.health < 0.5
	if seen["air"] and dist < 2.4 and rng.randf() < float(level["aa"]) * 0.2:
		intent.command("D+HP")
		return intent.next(m.facing)
	_think -= 1
	if _think > 0:
		if _walk_t > 0:
			_walk_t -= 1
			return PadIntent.pad(_walk, m.facing)
		return PadIntent.pad(5, m.facing)
	_think = int(level["think"]) + rng.randi_range(0, int(level["think"]))
	if low and _storm_cd <= 0 and dist < 3.0:
		_storm_cd = int(level["storm_cd"]) + rng.randi_range(0, 120)
		intent.command("B,B+HP")
		return intent.next(m.facing)
	if _dive_cd <= 0 and o.state not in [Fighter.S.DOWN, Fighter.S.WAKEUP]:
		_dive_cd = int(level["dive_cd"]) + rng.randi_range(0, 180) - (120 if low else 0)
		intent.command("D,U+HK")
		return intent.next(m.facing)
	var r := rng.randf()
	if dist > 3.2:
		if r < 0.45:
			intent.command("D,F+HP")
		elif r < 0.65:
			intent.command("B,F+HK")
		else:
			_walk = 6
			_walk_t = rng.randi_range(20, 40)
	elif dist > 1.5:
		if r < 0.3:
			intent.command("D,F+HP")
		elif r < 0.5:
			intent.command("B,F+HK")
		else:
			_walk = 6
			_walk_t = rng.randi_range(12, 26)
	else:
		if r < 0.18:
			intent.command("F,F+LP")
		elif r < 0.55:
			intent.command("HP")
			intent.hold(5, 9)
			intent.command("HP")
		elif r < 0.75:
			intent.command("D+LK")
		elif r < 0.85:
			intent.command("B,F+HK")
		else:
			_walk = 4
			_walk_t = rng.randi_range(10, 20)
	return intent.next(m.facing) if intent.busy() else PadIntent.pad(5, m.facing)
