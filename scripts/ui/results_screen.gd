extends Screen
## After a match: who won, then what next. Arcade advances the ladder (offering the stranger after
## a flawless win), counts down a continue after a loss, or leads to the ending after the boss.
## Versus and Training offer a rematch. Records and statistics are saved here.

var result := {}
var _countdown := -1.0
var _count_label: Label
var _auto := -1.0


func configure(data: Dictionary) -> void:
	result = data


func build() -> void:
	backdrop()
	var mode := str(result.get("mode", Game.config.get("mode", "versus")))
	var w := int(result.get("winner", -1))
	var p1 := str(result.get("p1", Game.config.get("p1", "")))
	var p2 := str(result.get("p2", Game.config.get("p2", "")))
	if mode not in ["tutorial", "stranger"]:
		Progress.count("matches")
		if w == 0 or (mode == "versus" and w >= 0):
			Progress.count("wins")
		if str(result.get("finisher", "")) != "":
			Progress.count("finishers")
		if result.get("flawless", false):
			Progress.count("flawless")
		Progress.save()
	match mode:
		"arcade":
			_arcade(w, p1)
		"survival":
			_survival(w, p1)
		"stranger":
			_stranger()
		"tutorial":
			heading("TUTORIAL COMPLETE", 80, 300)
			Progress.data["tutorial_done"] = true
			Progress.save()
			menu([["Main Menu", func(): Game.goto("menu")]], Vector2(730, 560), 460, 32)
		_:
			var text := "DRAW"
			if w >= 0:
				text = "%s WINS" % Content.fighter(p1 if w == 0 else p2).name.to_upper()
			heading(text, 96, 220)
			if mode == "versus" and w >= 0:
				var who := UiKit.label("Player %d" % (w + 1), 34, UiKit.ACCENT if w == 0 else UiKit.AMBER, HORIZONTAL_ALIGNMENT_CENTER)
				who.position = Vector2(0, 360)
				who.size = Vector2(1920, 50)
				add_child(who)
			_flourishes(420)
			menu([["Rematch", func(): Game.goto("match")],
				["Character Select", func(): Game.goto("select", {"mode": mode})],
				["Main Menu", func(): Game.goto("menu")]], Vector2(730, 560), 460, 32)
	hint("Enter to choose")


func _flourishes(y: float) -> void:
	var parts := []
	if result.get("flawless", false):
		parts.append("FLAWLESS VICTORY")
	var fin := str(result.get("finisher", ""))
	if fin != "":
		parts.append("STAGE SLOPTALITY" if fin.begins_with("stage_") else "SLOPTALITY")
	if parts.is_empty():
		return
	var l := UiKit.title("  ·  ".join(parts), 48, UiKit.ACCENT)
	l.position = Vector2(0, y)
	l.size = Vector2(1920, 70)
	add_child(l)


## "+12,300 points   ·   total 48,200" under the result.
func _score_line(pts: int, total: int, y: float) -> void:
	var l := UiKit.label("+%s points   ·   total %s" % [UiKit.commas(pts), UiKit.commas(total)], 30, UiKit.AMBER, HORIZONTAL_ALIGNMENT_CENTER, UiKit.body_font(true))
	l.position = Vector2(0, y)
	l.size = Vector2(1920, 40)
	add_child(l)


func _arcade(w: int, p1: String) -> void:
	if w == 0:
		var pts := Game.arcade_score(result)
		var total := int(Game.arcade.get("score", 0))
		var more := Game.arcade_advance(result.get("flawless", false))
		if more:
			heading("%s WINS" % Content.fighter(p1).name.to_upper(), 96, 220)
			_flourishes(360)
			_score_line(pts, total, 430)
			var next: Dictionary = Game.current_rung()
			var nxt := "Next: " + ("the Slopbat" if next.get("kind", "") == "boss" else ("the Reflection" if next.get("kind", "") == "mirror" else Content.fighter(next["opponent"]).name))
			var nl := UiKit.label(nxt, 30, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
			nl.position = Vector2(0, 480)
			nl.size = Vector2(1920, 40)
			add_child(nl)
			menu([["Continue", func(): Game.goto("stranger_offer" if Game.stranger_pending else "versus", {})],
				["Quit Arcade", func(): _quit_arcade()]], Vector2(730, 560), 460, 32)
		else:
			var secs: float = float(Game.arcade.get("seconds", 0.0))
			var tower := int(Game.arcade.get("tower", 0))
			var old_best := int(Progress.data["arcade"].get("%s:%d" % [p1, tower], {}).get("score", 0))
			Progress.record_arcade(p1, tower, secs, total)
			heading(str(Content.story.get("boss_defeated", "")), 44, 300)
			_score_line(pts, total, 390)
			var t := UiKit.label("Time %d:%02d   ·   continues %d%s" % [int(secs) / 60, int(secs) % 60, int(Game.arcade.get("continues", 0)),
				"   ·   NEW BEST SCORE" if total > old_best else ""], 30, UiKit.DIM if total <= old_best else UiKit.ACCENT, HORIZONTAL_ALIGNMENT_CENTER)
			t.position = Vector2(0, 440)
			t.size = Vector2(1920, 40)
			add_child(t)
			menu([["Continue", func(): Game.goto("ending", {"fighter": p1})]], Vector2(730, 560), 460, 32)
	else:
		heading("CONTINUE?", 110, 220)
		_count_label = UiKit.title("10", 140, UiKit.RUST.lightened(0.2))
		_count_label.position = Vector2(0, 360)
		_count_label.size = Vector2(1920, 170)
		add_child(_count_label)
		_countdown = 10.99
		menu([["Retry", func(): _retry()], ["Quit Arcade", func(): _quit_arcade()]], Vector2(730, 600), 460, 32)


func _survival(w: int, p1: String) -> void:
	var name := Content.fighter(p1).name
	if w == 0:
		var health: Array = result.get("health", [1, 0])
		Game.survival_advance(int(health[0]), result.get("flawless", false))
		var waves := Game.survival_waves()
		heading("WAVE %d CLEARED" % waves, 96, 220)
		_flourishes(360)
		var full := Content.fighter(p1).health
		var carried := int(Game.survival.get("health", full))
		var nxt := "the Slopbat" if Game.config["p2"] == "slopbat" else Content.fighter(Game.config["p2"]).name
		var nl := UiKit.label("Health carried on: %d%%   ·   Next: wave %d, %s (%s)" % [int(round(100.0 * carried / full)), waves + 1, nxt,
			Game.DIFFICULTIES[int(Game.config["difficulty"])]], 30, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
		nl.position = Vector2(0, 450)
		nl.size = Vector2(1920, 40)
		add_child(nl)
		menu([["Continue", func(): Game.goto("versus", {})], ["Quit Survival", func(): _quit_survival()]], Vector2(730, 560), 460, 32)
	else:
		var waves := Game.survival_waves()
		var new_best := Progress.record_survival(p1, waves)
		heading("%s SURVIVED %d WAVE%s" % [name.to_upper(), waves, "" if waves == 1 else "S"], 80, 220)
		var t := UiKit.label("NEW RECORD" if new_best and waves > 0 else "Best with %s: %d" % [name.split(" ")[0], Progress.best_survival(p1)],
			34, UiKit.ACCENT if new_best and waves > 0 else UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
		t.position = Vector2(0, 380)
		t.size = Vector2(1920, 50)
		add_child(t)
		menu([["Try Again", func():
			Game.start_survival(p1)
			Game.goto("versus", {})], ["Character Select", func(): Game.goto("select", {"mode": "survival"})],
			["Main Menu", func(): _quit_survival()]], Vector2(730, 560), 460, 32)


func _quit_survival() -> void:
	Game.survival = {}
	Game.goto("menu")


func _stranger() -> void:
	var line := str(result.get("stranger_line", ""))
	heading("THE STRANGER", 80, 200)
	if line != "":
		var l := UiKit.label("“%s”" % line, 36, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
		l.position = Vector2(0, 360)
		l.size = Vector2(1920, 60)
		add_child(l)
	var from_arcade: bool = Game.config.get("from_arcade", false) and not Game.arcade.is_empty()
	if from_arcade:
		menu([["Continue", func():
			Game.configure_rung()
			Game.goto("versus", {})]], Vector2(730, 560), 460, 32)
	else:
		menu([["Extras", func(): Game.goto("extras", {})], ["Main Menu", func(): Game.goto("menu")]], Vector2(730, 560), 460, 32)


func _retry() -> void:
	Game.arcade_retry()
	Game.goto("versus", {})


func _quit_arcade() -> void:
	Game.arcade = {}
	Game.goto("menu")


func _process(delta: float) -> void:
	if _countdown > 0.0:
		_countdown -= delta
		_count_label.text = str(maxi(0, int(_countdown)))
		if _countdown <= 0.0:
			_quit_arcade()
