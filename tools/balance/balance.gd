extends Node
## CPU-against-CPU matches for every pairing of the roster, run headless through the real
## simulation and CPU brains. Prints win rates, average match length, how often each move landed,
## and any match that stalled or reached an impossible state.
##   godot --headless --path . res://tools/balance/balance.tscn -- --matches=6 --difficulty=2 [--boss]

var matches := 6
var difficulty := 2
var boss := false
var _counts := {}


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--matches="):
			matches = int(a.trim_prefix("--matches="))
		elif a.begins_with("--difficulty="):
			difficulty = int(a.trim_prefix("--difficulty="))
		elif a == "--boss":
			boss = true
	_run.call_deferred()


func _run() -> void:
	var ids: Array = Content.roster()
	var wins := {}
	var games := {}
	var hits := {}
	var problems := []
	var total_ticks := 0
	var rounds := 0
	var timeouts := 0
	var n := 0
	var t0 := Time.get_ticks_msec()
	var opponents: Array = ["slopbat"] if boss else ids
	for a in ids:
		for b in opponents:
			for k in matches:
				var r := _play(a, b, k)
				n += 1
				total_ticks += r["ticks"]
				rounds += int(r["rounds"])
				timeouts += int(r["timeouts"])
				games[a] = int(games.get(a, 0)) + 1
				games[b] = int(games.get(b, 0)) + 1
				if r["winner"] == 0:
					wins[a] = int(wins.get(a, 0)) + 1
				elif r["winner"] == 1:
					wins[b] = int(wins.get(b, 0)) + 1
				for mv in r["hits"]:
					hits[mv] = int(hits.get(mv, 0)) + int(r["hits"][mv])
				problems.append_array(r["problems"])
	print("\n%d matches in %.1f s, average %.1f s of game time each; %d rounds, %d timeouts, %.1f s per round" % [n, (Time.get_ticks_msec() - t0) / 1000.0, total_ticks / 60.0 / maxi(n, 1), rounds, timeouts, total_ticks / 60.0 / maxi(rounds, 1)])
	for id in ids + (["slopbat"] if boss else []):
		print("%-10s won %3d of %3d  (%.0f%%)" % [id, int(wins.get(id, 0)), int(games.get(id, 0)), 100.0 * int(wins.get(id, 0)) / maxi(1, int(games.get(id, 0)))])
	var top := hits.keys()
	top.sort_custom(func(x, y): return hits[x] > hits[y])
	print("most landed: ", ", ".join(top.slice(0, 16).map(func(x): return "%s %d" % [x, hits[x]])))
	print("per round: %.1f moves, %.1f hits, %.1f blocks, %.1f projectiles, %.0f damage" % [float(_counts.get("move", 0)) / rounds, float(_counts.get("hit", 0)) / rounds, float(_counts.get("block", 0)) / rounds, float(_counts.get("projectile", 0)) / rounds, float(_counts.get("damage", 0)) / rounds])
	print("problems: %d" % problems.size())
	for p in problems.slice(0, 20):
		print("  ", p)
	get_tree().quit(1 if not problems.is_empty() else 0)


func _play(a: String, b: String, seed: int) -> Dictionary:
	var sim := FightSim.new()
	sim.intro = false
	sim.rounds_to_win = 1 if b == "slopbat" else 2
	sim.setup(Content.fighter(a), Content.fighter(b))
	if b == "slopbat":
		# As in the arcade: the boss's health depends on the tower.
		sim.fighters[1].health_max = Game.BOSS_HEALTH[clampi(difficulty, 0, Game.BOSS_HEALTH.size() - 1)]
		sim.fighters[1].health = sim.fighters[1].max_health()
	var brains := [CpuBrain.new(sim, 0, difficulty, 100 + seed), BossBrain.new(sim, 1, difficulty) if b == "slopbat" else CpuBrain.new(sim, 1, difficulty, 200 + seed)]
	var hits := {}
	var problems := []
	var ticks := 0
	var limit := 60 * 60 * 6
	var idle_streak := 0
	var rounds := 0
	var timeouts := 0
	while sim.phase != FightSim.Phase.MATCH_END and ticks < limit:
		ticks += 1
		var pads := [brains[0].think(), brains[1].think()]
		sim.step(pads)
		for e in sim.events:
			if e["type"] == "round_end":
				rounds += 1
				if e["how"] == "timeout":
					timeouts += 1
			if e["type"] in ["hit", "block", "move", "projectile"]:
				_counts[e["type"]] = int(_counts.get(e["type"], 0)) + 1
				if e["type"] == "hit":
					_counts["damage"] = int(_counts.get("damage", 0)) + int(e["damage"])
			if e["type"] == "hit" and e["damage"] > 0:
				var key := "%s.%s" % [sim.fighters[e["by"]].def.id, e["move"]]
				hits[key] = int(hits.get(key, 0)) + 1
		for f in sim.fighters:
			if absf(f.pos.x) > FightSim.STAGE_HALF + 0.01 or f.pos.y < -0.01 or f.pos.y > 12.0 or f.health < 0:
				problems.append("%s vs %s t%d: %s out of bounds (%s, hp %d, %s)" % [a, b, ticks, f.def.id, f.pos, f.health, f.state_name()])
		if sim.phase == FightSim.Phase.FIGHT and sim.fighters[0].state == Fighter.S.IDLE and sim.fighters[1].state == Fighter.S.IDLE:
			idle_streak += 1
		else:
			idle_streak = 0
		if idle_streak > 60 * 20:
			problems.append("%s vs %s: both idle for 20 s" % [a, b])
			break
	if ticks >= limit:
		problems.append("%s vs %s: match did not finish" % [a, b])
	var w := sim.match_winner
	sim.dispose()
	return {"winner": w, "ticks": ticks, "hits": hits, "problems": problems, "rounds": rounds, "timeouts": timeouts}
