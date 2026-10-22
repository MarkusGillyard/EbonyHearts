class_name PadReader
extends RefCounted
## Turns one side's input actions into the per-tick device state FightSim.step() expects.
## Presses are latched from events, so a tap shorter than one tick is never lost.

const BUTTON_BITS := {"hp": InputBuffer.HP, "lp": InputBuffer.LP, "hk": InputBuffer.HK, "lk": InputBuffer.LK,
	"bl": InputBuffer.BL, "rn": InputBuffer.RN}
const DIR_BITS := {"right": FightSim.PAD_RIGHT, "left": FightSim.PAD_LEFT, "up": FightSim.PAD_UP, "down": FightSim.PAD_DOWN}

var side := 0
var _latched := 0
var _latched_dirs := 0
var _prev_held := 0


func _init(s: int) -> void:
	side = s


func _action(a: String) -> String:
	return Settings.action_name(side, a)


## Call with every input event the match receives.
func feed(event: InputEvent) -> void:
	if event.is_echo():
		return
	for a in BUTTON_BITS:
		if event.is_action_pressed(_action(a)):
			_latched |= BUTTON_BITS[a]
	for a in DIR_BITS:
		if event.is_action_pressed(_action(a)):
			_latched_dirs |= DIR_BITS[a]


## Device state for this tick: Vector3i(direction bits, held buttons, newly pressed buttons).
func poll() -> Vector3i:
	var dirs := 0
	for a in DIR_BITS:
		if Input.is_action_pressed(_action(a)):
			dirs |= DIR_BITS[a]
	# A direction tapped and released within one tick still counts for that tick.
	dirs |= _latched_dirs
	var held := 0
	for a in BUTTON_BITS:
		if Input.is_action_pressed(_action(a)):
			held |= BUTTON_BITS[a]
	var pressed := (_latched | (held & ~_prev_held)) & InputBuffer.ALL
	_prev_held = held
	_latched = 0
	_latched_dirs = 0
	return Vector3i(dirs, held | pressed, pressed)


## Forgets held and latched state (focus loss, pause, device change), so nothing sticks.
func reset() -> void:
	_latched = 0
	_latched_dirs = 0
	_prev_held = 0
