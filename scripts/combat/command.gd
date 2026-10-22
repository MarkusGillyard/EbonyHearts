class_name Command
extends RefCounted
## A move command in arcade notation, relative to the fighter:
##   "HP"   "D+HP"   "B+LK"   "D,F+HP"   "B,B+LP"   "F,D,F+HK"   "B+LP+BL+LK"   "D,U+HK"
## A single direction must be held when the buttons are pressed. Two or more directions are a
## motion: tapped in order shortly before the buttons. Buttons are HP LP HK LK BL RN.

const DIRS := {"F": 6, "B": 4, "U": 8, "D": 2, "DF": 3, "DB": 1, "UF": 9, "UB": 7}
const BUTTONS := {"HP": InputBuffer.HP, "LP": InputBuffer.LP, "HK": InputBuffer.HK, "LK": InputBuffer.LK,
	"BL": InputBuffer.BL, "RN": InputBuffer.RN}
## Which held directions satisfy a held-direction command. Forward and back exclude the
## diagonals: down-forward and down-back are crouching, up-forward and up-back are jumps.
const HELD := {6: [6], 4: [4], 2: [1, 2, 3], 8: [7, 8, 9], 3: [3], 1: [1], 9: [9], 7: [7]}
## Which directions satisfy one step of a motion (lenient: a rolled diagonal counts).
const STEP := {6: [3, 6, 9], 4: [1, 4, 7], 2: [1, 2, 3], 8: [7, 8, 9], 3: [3], 1: [1], 9: [9], 7: [7]}
## The last direction of a motion may come this many ticks before the buttons (or one after).
const LAST_GAP := 10
## Consecutive motion steps may be this many ticks apart.
const STEP_GAP := 14
## The whole motion must fit in this many ticks.
const MOTION_WINDOW := 32

var text := ""
var steps: Array[int] = []
var mask := 0
var move := ""


static func parse(t: String) -> Command:
	var c := Command.new()
	c.text = t
	var s := t.strip_edges().replace(" ", "")
	var plus := s.find("+")
	var dir_part := s
	var btn_part := ""
	if plus >= 0:
		dir_part = s.substr(0, plus)
		btn_part = s.substr(plus + 1)
	if BUTTONS.has(dir_part):
		# No direction at all: "HP" or "HP+LP".
		btn_part = s
		dir_part = ""
	if dir_part != "":
		for d in dir_part.split(","):
			if not DIRS.has(d):
				push_error("Command '%s': unknown direction '%s'" % [t, d])
				return c
			c.steps.append(DIRS[d])
	if btn_part != "":
		for b in btn_part.split("+"):
			if not BUTTONS.has(b):
				push_error("Command '%s': unknown button '%s'" % [t, b])
				return c
			c.mask |= BUTTONS[b]
	if c.mask == 0:
		push_error("Command '%s' has no buttons" % t)
	return c


func is_motion() -> bool:
	return steps.size() >= 2


## Sort key: longer motions first, then chords, then held directions, then plain buttons.
func priority() -> int:
	return steps.size() * 100 + bit_count(mask) * 10 + (1 if steps.size() == 1 else 0)


## True when this command was entered with its buttons pressed at `age` ticks ago. Motions are
## read both with the facing stored on each tick and with the current facing, so a motion begun
## before a crossover and finished after it still comes out.
func matches(buf: InputBuffer, age: int, facing_now: int) -> bool:
	if steps.is_empty():
		return true
	if steps.size() == 1:
		var d := buf.dir_as(age, facing_now)
		return d in HELD[steps[0]]
	return _motion(buf, age, 0) or _motion(buf, age, facing_now)


## `facing` 0 reads each tick with the facing stored for it. Gaps are measured in effective
## ticks (hit-stop takes no time).
func _motion(buf: InputBuffer, press: int, facing: int) -> bool:
	var n := steps.size()
	var eff := buf.eff_ages(press + MOTION_WINDOW * 3)
	var limit := eff.size()
	if press >= limit:
		return false
	var last: Array = STEP[steps[n - 1]]
	var at := -1
	for a in range(maxi(press - 1, 0), limit):
		if eff[a] - eff[press] > LAST_GAP:
			break
		if _rd(buf, a, facing) in last:
			at = a
			break
	if at < 0:
		return false
	# The last direction must have been entered shortly before the press, not merely held.
	while at + 1 < limit and _rd(buf, at + 1, facing) in last:
		at += 1
	if eff[at] - eff[press] > LAST_GAP:
		return false
	for k in range(n - 2, -1, -1):
		var want: Array = STEP[steps[k]]
		var nxt: Array = STEP[steps[k + 1]]
		var same := want == nxt
		var found := -1
		var released := false
		# Leave the later step first: its direction may still be held for a while.
		var a := at + 1
		while a < limit and eff[a] - eff[at] <= STEP_GAP and eff[a] - eff[press] <= MOTION_WINDOW:
			var d := _rd(buf, a, facing)
			if same:
				if not (d in want):
					released = true
				elif released:
					found = a
					break
			elif d in want and not (d in nxt):
				found = a
				break
			a += 1
		if found < 0:
			return false
		at = found
	return true


static func _rd(buf: InputBuffer, age: int, facing: int) -> int:
	return buf.dir(age) if facing == 0 else buf.dir_as(age, facing)


static func bit_count(m: int) -> int:
	var n := 0
	while m:
		n += m & 1
		m >>= 1
	return n


## Readable notation for menus: "D,F+HP" becomes "↓ → + HP".
static func pretty(t: String) -> String:
	var arrows := {"F": "→", "B": "←", "U": "↑", "D": "↓", "DF": "↘", "DB": "↙", "UF": "↗", "UB": "↖"}
	var s := t.replace(" ", "")
	var plus := s.find("+")
	var dirs := s.substr(0, plus) if plus >= 0 else s
	if BUTTONS.has(dirs):
		return s.replace("+", " + ")
	var out: PackedStringArray = []
	for d in dirs.split(","):
		out.append(arrows.get(d, d))
	var rest := s.substr(plus + 1).replace("+", " + ") if plus >= 0 else ""
	return " ".join(out) + (" + " + rest if rest != "" else "")
