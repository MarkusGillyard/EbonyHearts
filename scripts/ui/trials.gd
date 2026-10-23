extends Node
## Combo trials, run inside a training match against the fighter's rival: one trial at a time,
## its inputs on screen, each hit of the combo ticked off as it lands (TrialCheck). A dropped
## combo starts the trial over; a completed one is saved and the next begins. Tab skips, F4
## resets the positions.

var m: Match
var sim: FightSim
var fighter := ""
var trials: Array = []
var index := 0
var check: TrialCheck
var _done := false
var _busy := false


func setup(match_node: Match) -> void:
	m = match_node
	sim = m.sim
	fighter = str(m.config["p1"])
	trials = Content.trials.get(fighter, [])
	sim.refill_health = true
	sim.phase = FightSim.Phase.FIGHT
	for f in sim.fighters:
		f.set_state(Fighter.S.IDLE)
	# Start at the first trial not yet completed.
	var done := Progress.trials_done(fighter)
	index = 0
	while index < trials.size() and str(trials[index]["name"]) in done:
		index += 1
	if index >= trials.size():
		index = 0
	_begin()


func _begin() -> void:
	if trials.is_empty():
		m.hud.prompt("No trials for this fighter yet.")
		return
	var t: Dictionary = trials[index]
	check = TrialCheck.new(t["moves"], 0)
	_show()
	Audio.play("ui_select", -4.0)


## The prompt: trial number and name, its inputs, and a tick for each hit landed so far.
func _show(note := "") -> void:
	var t: Dictionary = trials[index]
	var steps: Array = t["steps"]
	var pretty := []
	for s in steps:
		pretty.append(Command.pretty(str(s)))
	var ticks := ""
	for i in (t["moves"] as Array).size():
		ticks += "●" if i < check.progress else "○"
	var done_mark := "  ✓" if str(t["name"]) in Progress.trials_done(fighter) else ""
	m.hud.prompt("Trial %d of %d:  %s%s\n%s     %s%s   (Tab: next trial, F4: reset)" % [index + 1, trials.size(), t["name"], done_mark,
		"   ".join(pretty), ticks, ("   " + note) if note != "" else ""])


func on_event(e: Dictionary) -> void:
	if _done or check == null or _busy:
		return
	match check.feed(e):
		"hit":
			_show()
		"dropped":
			_show("dropped")
		"done":
			_complete()


func _complete() -> void:
	_busy = true
	var t: Dictionary = trials[index]
	var first := Progress.trial_done(fighter, str(t["name"]))
	_show()
	m.hud.announce("TRIAL COMPLETE", t["name"] if first else "", 2.0, UiKit.ACCENT, 104)
	Audio.play("victory", -6.0)
	await get_tree().create_timer(2.2).timeout
	if m == null or not is_instance_valid(m):
		return
	_busy = false
	_advance()


func _advance() -> void:
	if trials.is_empty():
		return
	if Progress.trials_done(fighter).size() >= trials.size() and index == trials.size() - 1:
		m.hud.announce("ALL TRIALS COMPLETE", "", 3.0, UiKit.ACCENT, 104)
	index = (index + 1) % trials.size()
	m.reset_positions()
	_begin()


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("training_dummy") and not _done and not _busy:
		get_viewport().set_input_as_handled()
		Audio.play("ui_move", -6.0)
		_advance()
