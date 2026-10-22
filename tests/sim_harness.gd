class_name SimHarness
extends RefCounted
## Builds deterministic matches for tests and drives them with scripted device states.

const R := FightSim.PAD_RIGHT
const L := FightSim.PAD_LEFT
const U := FightSim.PAD_UP
const D := FightSim.PAD_DOWN
const HP := InputBuffer.HP
const LP := InputBuffer.LP
const HK := InputBuffer.HK
const LK := InputBuffer.LK
const BL := InputBuffer.BL
const RN := InputBuffer.RN

var sim: FightSim
## Every event emitted while stepping, oldest first.
var log: Array = []
## Buttons held down for each side (for block and run), on top of each tick's presses.
var hold := [0, 0]


## A plain fighter made only of the shared basic moves plus a projectile and a grab.
static func dummy_def(id := "dummy", extra := {}) -> FighterDef:
	var d := {
		"id": id, "name": id.capitalize(), "stats": {"height": 1.8, "health": 1000},
		"moves": {
			"orb": {"kind": "special", "startup": 10, "active": 0, "recovery": 20, "level": "none",
				"projectile": {"frame": 10, "kind": "orb", "offset": [0.6, 1.3], "speed": 8.0, "size": [0.3, 0.3], "life": 100,
					"level": "high", "damage": 60, "chip": 10}},
			"low_orb": {"kind": "special", "startup": 10, "active": 0, "recovery": 20, "level": "none",
				"projectile": {"frame": 10, "kind": "orb", "offset": [0.6, 0.25], "speed": 8.0, "size": [0.3, 0.3], "life": 100,
					"level": "low", "damage": 50, "chip": 10}},
			"grab": {"kind": "grab", "startup": 6, "active": 3, "recovery": 30, "level": "throw",
				"hitboxes": [{"rect": [0.2, 0.8, 0.6, 0.6]}],
				"throw": {"frames": 30, "damage_frame": 20, "damage": 150, "release": [2.0, 3.0]}},
			"fin": {"kind": "finisher", "total": 120},
		},
		"commands": [["D,F+HP", "orb"], ["D,B+LP", "low_orb"], ["B,F+LP", "grab"]],
		"finisher": {"input": "F,F,D+HP", "range": "close", "move": "fin"},
	}
	d.merge(extra, true)
	return FighterDef.from_dict(d, Content.basics)


func _init(d1: FighterDef = null, d2: FighterDef = null, training := true) -> void:
	sim = FightSim.new()
	sim.training = training
	sim.intro = false
	sim.setup(d1 if d1 else dummy_def("p1"), d2 if d2 else dummy_def("p2"))
	# Skip the round announcement.
	while sim.phase != FightSim.Phase.FIGHT:
		sim.step([Vector3i.ZERO, Vector3i.ZERO])


func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE and sim:
		sim.dispose()


func f(i: int) -> Fighter:
	return sim.fighters[i]


## Places both fighters `gap` metres apart around `centre`, facing each other, at rest.
func place(gap: float, centre := 0.0) -> void:
	f(0).pos = Vector2(centre - gap * 0.5, 0)
	f(1).pos = Vector2(centre + gap * 0.5, 0)
	f(0).facing = 1
	f(1).facing = -1
	for x in sim.fighters:
		x.move = null
		x.set_state(Fighter.S.IDLE)
		x.vel = Vector2.ZERO
		x.push_v = 0.0
		x.hitstop = 0
		x.stun = 0
		x.input.clear()
	sim.projectiles.clear()
	for x in sim.fighters:
		x.projectiles_out = 0


## One tick: directions held as bits and buttons pressed this tick (plus `hold`).
func step(p1_dirs := 0, p1_press := 0, p2_dirs := 0, p2_press := 0) -> void:
	sim.step([Vector3i(p1_dirs, p1_press | hold[0], p1_press), Vector3i(p2_dirs, p2_press | hold[1], p2_press)])
	log.append_array(sim.events)


func idle(n: int, p1_dirs := 0, p2_dirs := 0) -> void:
	for i in n:
		step(p1_dirs, 0, p2_dirs, 0)


## Taps a sequence of screen directions for fighter `who`, one tick each, then presses `btn`
## while holding the last direction.
func motion(who: int, dirs: Array, btn: int) -> void:
	for dd in dirs:
		if who == 0:
			step(dd, 0, 0, 0)
		else:
			step(0, 0, dd, 0)
	var last: int = dirs[dirs.size() - 1] if not dirs.is_empty() else 0
	if who == 0:
		step(last, btn, 0, 0)
	else:
		step(0, 0, last, btn)


## The first event of `type` in the log after index `since`, or {}.
func find(type: String, since := 0) -> Dictionary:
	for i in range(since, log.size()):
		if log[i]["type"] == type:
			return log[i]
	return {}


func count(type: String, since := 0) -> int:
	var n := 0
	for i in range(since, log.size()):
		if log[i]["type"] == type:
			n += 1
	return n


## Steps until `cond` holds or `limit` ticks pass; returns whether it held.
func until(cond: Callable, limit := 400, p1_dirs := 0, p2_dirs := 0) -> bool:
	for i in limit:
		if cond.call():
			return true
		step(p1_dirs, 0, p2_dirs, 0)
	return cond.call()
