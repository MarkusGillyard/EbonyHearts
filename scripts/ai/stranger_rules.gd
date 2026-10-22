class_name StrangerRules
extends RefCounted
## The optional encounter with the stranger in white (docs/DESIGN.md). He cannot be hurt and he
## does not fight; he answers. His memory is one stage in the progress file that only moves
## forward, so the encounter changes over many meetings:
##
##   0 never met   1 met   2 warned   3 struck down   4 asked   5 finished properly   6 recognised
##
## The first blow that would land he steps away from ("Careful, child."); the next he answers with
## one palm. Meeting again after that: "What have you learned, child?", and the next blow gets the
## whole of Find the Crack. After that he recognises the player as a fellow immortal. Holding Block
## bows out; standing idle long enough, he sends the player on. Losing to him never costs anything.

const BOW_TICKS := 50
const IDLE_TICKS := 25 * 60

var sim: FightSim
var match_node: Node
var stage := 0
var _lines: Array = []
var _t := 0
var _bow := 0
var _idle := 0
var _ended := false
var last_line := ""
var _busy := false


func _init(s: FightSim, m: Node) -> void:
	sim = s
	match_node = m
	stage = int(Progress.data.get("stranger", 0))
	match stage:
		0:
			_say(60, "Yes?")
			Progress.set_stranger(1)
		3:
			_say(60, "What have you learned, child?")
			Progress.set_stranger(4)
			stage = 4
		5:
			_say(50, "Interesting.")
			_say(150, "(You remember?)")
			_say(250, "So do you.")
			_say(380, "It seems I'm not the only immortal bastard around here.")
			Progress.set_stranger(6)
			stage = 6
		_:
			_say(60, "You again.")
	_say(240 if stage != 6 else 520, str(Content.story.get("stranger", {}).get("bow", "")), true)


func _say(at: int, text: String, hint := false) -> void:
	_lines.append([at, text, hint])


func dax() -> Fighter:
	return sim.fighters[1]


func player() -> Fighter:
	return sim.fighters[0]


func think() -> Vector3i:
	return Vector3i.ZERO


func on_contact(c: Dictionary) -> String:
	var d: Fighter = c["def"]
	if d.index != 1:
		return ""
	var p: Projectile = c.get("proj")
	if p:
		p.dead = true
	if _busy or _ended:
		return "ignore"
	_idle = 0
	match stage:
		0, 1:
			_evade("Careful, child.")
			Progress.set_stranger(2)
			stage = 2
		2:
			_strike("")
			Progress.set_stranger(3)
			stage = 3
		3, 4:
			_crack()
			Progress.set_stranger(5)
			stage = 5
		_:
			_strike("Apparently, very little.")
	return "ignore"


func _evade(line: String) -> void:
	var d := dax()
	d.start_move(d.def.move("dax_evade"))
	sim.emit({"type": "line", "who": 1, "text": line})
	sim.emit({"type": "stranger_evade"})
	last_line = line


## Steps in beside the attacker and catches them, so nothing they were doing can carry on.
func _catch(stun: int) -> void:
	var a := player()
	var d := dax()
	var dir := signf(a.pos.x - d.pos.x)
	if dir == 0.0:
		dir = 1.0
	d.pos.x = clampf(a.pos.x - dir * 0.78 * d.scale(), -FightSim.STAGE_HALF + d.def.push_w, FightSim.STAGE_HALF - d.def.push_w)
	d.facing = 1 if a.pos.x > d.pos.x else -1
	a.move = null
	a.vel = Vector2.ZERO
	a.pos.y = 0.0
	a.take_stun(stun, "dazed")
	for pr in sim.projectiles:
		pr.dead = true


func _strike(line: String) -> void:
	_busy = true
	_catch(40)
	var d := dax()
	d.start_move(d.def.move("dax_palm"))
	if line != "":
		sim.emit({"type": "line", "who": 1, "text": line})
		last_line = line
	sim.emit({"type": "stranger_strike"})


func _crack() -> void:
	_busy = true
	_catch(110)
	var d := dax()
	d.start_move(d.def.move("dax_crack"))
	sim.emit({"type": "stranger_crack"})


func on_tick(s: FightSim) -> void:
	if _ended or s.phase != FightSim.Phase.FIGHT:
		return
	_t += 1
	_idle += 1
	for l in _lines:
		if l[0] == _t:
			s.emit({"type": "line", "who": 1, "text": l[1], "hint": l[2]})
			if not l[2]:
				last_line = l[1]
	var a := player()
	if not _busy and a.is_actionable() and a.input.held() & InputBuffer.BL and dax().is_actionable():
		_bow += 1
		if _bow >= BOW_TICKS:
			_end("Go.")
	else:
		_bow = 0
	if not _busy and _idle >= IDLE_TICKS:
		_end("Go on.")


func _end(line: String) -> void:
	_ended = true
	sim.emit({"type": "line", "who": 1, "text": line})
	last_line = line
	sim.phase = FightSim.Phase.MATCH_END
	sim.phase_t = 0
	sim.emit({"type": "match_end", "winner": -1, "wins": [0, 0], "finisher": "", "flawless": false, "stranger_line": line})


func on_event(e: Dictionary) -> void:
	if e["type"] == "match_end" and not e.has("stranger_line"):
		e["stranger_line"] = last_line
