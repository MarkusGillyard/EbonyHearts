class_name TrainingDummy
extends RefCounted
## The training partner. Modes: stand still, crouch, block everything, block standing or
## crouching at random, block only low, jump in place, or fight back as the CPU at Warrior.

const MODES := ["Stand", "Crouch", "Block All", "Block Random", "Crouch Block", "Jump", "CPU"]

var sim: FightSim
var index := 1
var mode := 0
var _cpu: CpuBrain
var _rng := RandomNumberGenerator.new()
var _low := false
var _switch_t := 0


func _init(s: FightSim, i: int) -> void:
	sim = s
	index = i
	_rng.seed = 77


func next_mode() -> void:
	set_mode((mode + 1) % MODES.size())


func set_mode(m: int) -> void:
	mode = m
	_cpu = CpuBrain.new(sim, index, 1) if MODES[mode] == "CPU" else null


func mode_name() -> String:
	return MODES[mode]


func think() -> Vector3i:
	var f := sim.fighters[index]
	match MODES[mode]:
		"Crouch":
			return PadIntent.pad(2, f.facing)
		"Block All":
			# Reads the attacker's move as it happens, so every block is correct.
			var o := sim.fighters[1 - index]
			var low := false
			if o.state == Fighter.S.MOVE and o.move:
				low = o.move.level == MoveDef.Level.LOW
			for p in sim.projectiles:
				if p.owner != index:
					low = low or p.level == MoveDef.Level.LOW
			return PadIntent.pad(2 if low else 5, f.facing, 0, InputBuffer.BL)
		"Block Random":
			_switch_t -= 1
			if _switch_t <= 0:
				_low = _rng.randf() < 0.5
				_switch_t = _rng.randi_range(20, 60)
			return PadIntent.pad(2 if _low else 5, f.facing, 0, InputBuffer.BL)
		"Crouch Block":
			return PadIntent.pad(2, f.facing, 0, InputBuffer.BL)
		"Jump":
			return PadIntent.pad(8 if f.is_actionable() else 5, f.facing)
		"CPU":
			return _cpu.think()
	return Vector3i.ZERO
