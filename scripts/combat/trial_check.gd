class_name TrialCheck
extends RefCounted
## Watches a fight's events for one combo trial: the listed hits, landed by one fighter as a
## single combo, in order. Pure and deterministic, so tests can drive it; the Trials screen
## shows its state. A hit out of order, or the combo ending, starts the trial over.

var moves: Array = []
var who := 0
## Hits landed so far in the current attempt.
var progress := 0
var done := false


func _init(move_ids: Array, attacker := 0) -> void:
	moves = move_ids.duplicate()
	who = attacker


## Feeds one simulation event; returns "hit" when the attempt advanced, "done" when the trial is
## complete, "dropped" when a started attempt failed, "" otherwise.
func feed(e: Dictionary) -> String:
	if done:
		return ""
	match e["type"]:
		"hit":
			if int(e.get("by", -1)) != who:
				return ""
			var n := int(e.get("combo", 1))
			if n == progress + 1 and str(e.get("move", "")) == str(moves[progress]):
				progress += 1
				if progress >= moves.size():
					done = true
					return "done"
				return "hit"
			# The wrong move, or a hit after the combo had already been dropped: start over, and
			# count this hit as the first of a new attempt when it fits.
			var started := progress > 0
			progress = 1 if (n == 1 and str(e.get("move", "")) == str(moves[0])) else 0
			if progress == 1 and moves.size() == 1:
				done = true
				return "done"
			return "dropped" if started else ""
		"combo_end":
			if int(e.get("who", -1)) == 1 - who and progress > 0:
				progress = 0
				return "dropped"
	return ""


func reset() -> void:
	progress = 0
	done = false
