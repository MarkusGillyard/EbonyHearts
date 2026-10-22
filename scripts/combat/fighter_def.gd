class_name FighterDef
extends RefCounted
## Static description of one fighter, loaded from data/fighters/<id>.json on top of the shared
## basic moves in data/basics.json (authored for a 1.8 m body and scaled to this fighter).

const BASE_HEIGHT := 1.8

var id := ""
var name := ""
var title := ""
## "him", "her" or "it", for the FINISH prompt.
var pronoun := "him"
var style := ""
var blurb := ""
var height := 1.8
var health := 1000
var walk_f := 2.4
var walk_b := 1.9
var run_speed := 5.2
var jump_vy := 8.6
var jump_vx := 2.6
var gravity := 20.0
var push_w := 0.28
var throw_range := 0.95
var boss := false
## Not part of the ordinary roster (bosses, the stranger).
var hidden := false
var moves: Dictionary = {}
## Ground commands, highest priority first.
var commands: Array[Command] = []
## Air commands, highest priority first.
var air_commands: Array[Command] = []
var finisher_cmd: Command
var finisher_range := "close"
var poses: Dictionary = {}
var look: Dictionary = {}
var ai: Dictionary = {}
var raw: Dictionary = {}


static func from_dict(d: Dictionary, basics: Dictionary = {}) -> FighterDef:
	var f := FighterDef.new()
	f.raw = d
	f.id = str(d["id"])
	f.name = str(d.get("name", f.id))
	f.title = str(d.get("title", ""))
	f.pronoun = str(d.get("pronoun", "him"))
	f.style = str(d.get("style", ""))
	f.blurb = str(d.get("blurb", ""))
	f.boss = bool(d.get("boss", false))
	f.hidden = bool(d.get("hidden", f.boss))
	var s: Dictionary = d.get("stats", {})
	f.height = float(s.get("height", f.height))
	f.health = int(s.get("health", f.health))
	f.walk_f = float(s.get("walk_f", f.walk_f))
	f.walk_b = float(s.get("walk_b", f.walk_b))
	f.run_speed = float(s.get("run_speed", f.run_speed))
	f.jump_vy = float(s.get("jump_vy", f.jump_vy))
	f.jump_vx = float(s.get("jump_vx", f.jump_vx))
	f.gravity = float(s.get("gravity", f.gravity))
	var k := f.height / BASE_HEIGHT
	f.push_w = float(s.get("push_w", f.push_w * k))
	f.throw_range = float(s.get("throw_range", f.throw_range * k))
	var skip: Array = d.get("no_basics", [])
	var bm: Dictionary = basics.get("moves", {})
	var own: Dictionary = d.get("moves", {})
	var tweaks: Dictionary = d.get("basic_tweaks", {})
	for id in bm:
		if id in skip or own.has(id):
			continue
		var src: Dictionary = bm[id]
		if tweaks.has(id):
			src = src.duplicate(true)
			src.merge(tweaks[id], true)
		f.moves[id] = MoveDef.from_dict(id, src, k)
	for id in own:
		f.moves[id] = MoveDef.from_dict(id, own[id], 1.0)
	var ground: Array = []
	for pair in d.get("commands", []):
		ground.append(pair)
	for pair in basics.get("commands", []):
		if f.moves.has(str(pair[1])) and not str(pair[1]) in skip:
			ground.append(pair)
	f.commands = _sorted(ground)
	var air: Array = []
	for pair in d.get("air_commands", []):
		air.append(pair)
	for pair in basics.get("air_commands", []):
		if f.moves.has(str(pair[1])) and not str(pair[1]) in skip:
			air.append(pair)
	f.air_commands = _sorted(air)
	var fin: Dictionary = d.get("finisher", {})
	if not fin.is_empty():
		f.finisher_cmd = Command.parse(str(fin["input"]))
		f.finisher_cmd.move = str(fin["move"])
		f.finisher_range = str(fin.get("range", "close"))
	f.poses = d.get("poses", {})
	f.look = d.get("look", {})
	f.ai = d.get("ai", {})
	return f


## Parses [notation, move] pairs and orders them so the most specific input wins; ties keep
## their data order.
static func _sorted(pairs: Array) -> Array[Command]:
	var tagged := []
	for i in pairs.size():
		var c := Command.parse(str(pairs[i][0]))
		c.move = str(pairs[i][1])
		tagged.append([c.priority(), -i, c])
	tagged.sort_custom(func(a, b): return a[0] > b[0] or (a[0] == b[0] and a[1] > b[1]))
	var out: Array[Command] = []
	for t in tagged:
		out.append(t[2])
	return out


func move(move_id: String) -> MoveDef:
	return moves.get(move_id)


## The notation a player types for a move (first ground or air command that starts it).
func notation_of(move_id: String) -> String:
	for c in commands:
		if c.move == move_id:
			return c.text
	for c in air_commands:
		if c.move == move_id:
			return "Air " + c.text
	if finisher_cmd and finisher_cmd.move == move_id:
		return finisher_cmd.text
	return ""
