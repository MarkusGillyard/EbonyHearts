class_name PadIntent
extends RefCounted
## Turns intentions written relative to a fighter (forward, back, motions, buttons) into the
## screen-space pad states FightSim.step() reads, one tick at a time. CPU brains, the training
## dummy, the tutorial and the stranger all drive their fighters through the same input path as
## a player's keys.

## Queued ticks: each [relative numpad direction, pressed buttons, held buttons].
var queue: Array = []
var held_buttons := 0


static func to_screen(rel_dir: int, facing: int) -> int:
	var bits := 0
	var x := (rel_dir - 1) % 3 - 1
	var y := (rel_dir - 1) / 3 - 1
	if facing < 0:
		x = -x
	if x > 0:
		bits |= FightSim.PAD_RIGHT
	elif x < 0:
		bits |= FightSim.PAD_LEFT
	if y > 0:
		bits |= FightSim.PAD_UP
	elif y < 0:
		bits |= FightSim.PAD_DOWN
	return bits


static func pad(rel_dir: int, facing: int, press := 0, held := 0) -> Vector3i:
	return Vector3i(to_screen(rel_dir, facing), press | held, press)


func busy() -> bool:
	return not queue.is_empty()


func clear() -> void:
	queue.clear()


## Queues a command in arcade notation ("D,F+HP", "B+LK", "HP"): motion directions one tick
## each, then the buttons pressed while holding the last direction.
func command(notation: String, hold_ticks := 0) -> void:
	var c := Command.parse(notation)
	if c.steps.size() >= 2:
		for i in c.steps.size():
			var d: int = c.steps[i]
			queue.append([d, 0, 0])
			# Roll through the diagonal between down and forward or back, as a player would.
			if i + 1 < c.steps.size():
				var nxt: int = c.steps[i + 1]
				if d == 2 and nxt == 6:
					queue.append([3, 0, 0])
				elif d == 2 and nxt == 4:
					queue.append([1, 0, 0])
				elif d == nxt:
					queue.append([5, 0, 0])
		queue.append([c.steps[c.steps.size() - 1], c.mask, 0])
	elif c.steps.size() == 1:
		queue.append([c.steps[0], c.mask, 0])
	else:
		queue.append([5, c.mask, 0])
	for i in hold_ticks:
		queue.append([5, 0, 0])


## Holds a direction (and buttons) for `ticks` ticks.
func hold(rel_dir: int, ticks: int, buttons := 0) -> void:
	for i in ticks:
		queue.append([rel_dir, 0, buttons])


## The pad state for this tick; `idle_dir` and `idle_held` apply when nothing is queued.
func next(facing: int, idle_dir := 5, idle_held := 0) -> Vector3i:
	if queue.is_empty():
		return pad(idle_dir, facing, 0, idle_held | held_buttons)
	var q: Array = queue.pop_front()
	return pad(q[0], facing, q[1], q[2] | held_buttons)
