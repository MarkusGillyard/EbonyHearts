class_name InputBuffer
extends RefCounted
## One fighter's input history, one entry per simulation tick. Directions are kept as seen on
## screen (numpad notation: 6 right, 4 left, 8 up, 2 down) together with the side the fighter
## faced on that tick, so commands can be read relative to the fighter even across a crossover.
## Ticks spent frozen in hit-stop are recorded but take no time: buffer and motion windows count
## only the ticks the fighter could move in, so a press made during hit-stop is never lost.

const SIZE := 120
const HP := 1
const LP := 2
const HK := 4
const LK := 8
const BL := 16
const RN := 32
const ATTACKS := HP | LP | HK | LK
const ALL := ATTACKS | BL | RN
## Buttons pressed this many ticks apart still count as one chord.
const CHORD_TICKS := 3
## A press is remembered this long while the fighter cannot act yet.
const BUFFER_TICKS := 6

var _dir := PackedInt32Array()
var _face := PackedInt32Array()
var _held := PackedInt32Array()
var _pressed := PackedInt32Array()
var _frozen := PackedByteArray()
var _head := -1
var _count := 0


func _init() -> void:
	_dir.resize(SIZE)
	_face.resize(SIZE)
	_held.resize(SIZE)
	_pressed.resize(SIZE)
	_frozen.resize(SIZE)
	clear()


func clear() -> void:
	_dir.fill(5)
	_face.fill(1)
	_held.fill(0)
	_pressed.fill(0)
	_frozen.fill(0)
	_head = -1
	_count = 0


func push(screen: int, facing: int, held: int, pressed: int, frozen := false) -> void:
	_head = (_head + 1) % SIZE
	_dir[_head] = screen
	_face[_head] = facing
	_held[_head] = held
	_pressed[_head] = pressed
	_frozen[_head] = 1 if frozen else 0
	_count = mini(_count + 1, SIZE)


## Effective ages for raw ages 0..n-1: how many unfrozen ticks lie between now and each entry.
func eff_ages(n: int) -> PackedInt32Array:
	var out := PackedInt32Array()
	n = mini(n, _count)
	out.resize(n)
	var e := 0
	for a in n:
		out[a] = e
		if _frozen[_idx(a)] == 0:
			e += 1
	return out


func size() -> int:
	return _count


func _idx(age: int) -> int:
	return (_head - age + SIZE * 2) % SIZE


func screen_dir(age := 0) -> int:
	return 5 if age >= _count else _dir[_idx(age)]


## Direction relative to the fighter as it faced on that tick (6 toward the opponent).
func dir(age := 0) -> int:
	if age >= _count:
		return 5
	var i := _idx(age)
	return rel(_dir[i], _face[i])


## Direction relative to the fighter as if it had faced `facing` on that tick.
func dir_as(age: int, facing: int) -> int:
	return 5 if age >= _count else rel(_dir[_idx(age)], facing)


func facing_at(age: int) -> int:
	return 1 if age >= _count else _face[_idx(age)]


func held(age := 0) -> int:
	return 0 if age >= _count else _held[_idx(age)]


func pressed(age := 0) -> int:
	return 0 if age >= _count else _pressed[_idx(age)]


## Age of the most recent press of any button in `button` within `window` ticks, or -1.
func press_age(button: int, window := BUFFER_TICKS) -> int:
	var e := 0
	for a in _count:
		var i := _idx(a)
		if _pressed[i] & button:
			return a
		if _frozen[i] == 0:
			e += 1
			if e >= window:
				break
	return -1


## Age of the latest press when every button in `mask` was pressed within CHORD_TICKS of the
## others inside `window`; -1 when the chord is incomplete.
func chord_age(mask: int, window := BUFFER_TICKS) -> int:
	var latest := -1
	var earliest := -1
	var b := 1
	while b <= RN:
		if mask & b:
			var a := press_age(b, window)
			if a < 0:
				return -1
			latest = a if latest < 0 else mini(latest, a)
			earliest = maxi(earliest, a)
		b <<= 1
	if earliest - latest > CHORD_TICKS:
		return -1
	return latest


## Marks presses of `mask` inside the window as used, so one press starts one move.
func consume(mask: int, window := BUFFER_TICKS) -> void:
	var e := 0
	for a in _count:
		var i := _idx(a)
		_pressed[i] = _pressed[i] & ~mask
		if _frozen[i] == 0:
			e += 1
			if e >= window:
				break


## Ticks the relative direction has continuously satisfied `group` (see Command.GROUPS).
func held_ticks(group: Array) -> int:
	var n := 0
	while n < _count and dir(n) in group:
		n += 1
	return n


static func rel(screen: int, facing: int) -> int:
	if facing >= 0:
		return screen
	match screen:
		6:
			return 4
		4:
			return 6
		3:
			return 1
		1:
			return 3
		9:
			return 7
		7:
			return 9
	return screen


static func screen_from(right: bool, left: bool, up: bool, down: bool) -> int:
	var x := 0
	if right and not left:
		x = 1
	elif left and not right:
		x = -1
	var y := 0
	if up and not down:
		y = 1
	elif down and not up:
		y = -1
	return 5 + x + 3 * y


static func is_down(d: int) -> bool:
	return d <= 3


static func is_up(d: int) -> bool:
	return d >= 7


static func is_fwd(d: int) -> bool:
	return d == 3 or d == 6 or d == 9


static func is_back(d: int) -> bool:
	return d == 1 or d == 4 or d == 7
