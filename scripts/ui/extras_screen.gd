extends Screen
## Extras: the endings you have earned, your records, move lists, the stranger once you have met
## him, and the credits.

var _panel: Label
var _sub := ""


func build() -> void:
	backdrop(Color(0.6, 0.5, 0.9))
	back_to = "menu"
	heading("EXTRAS", 76, 40)
	var items := [["Endings", func(): _show_endings()], ["Records", func(): _show_records()],
		["Move Lists", func(): Game.goto("moves", {"fighter": Content.roster()[0]})]]
	var met := int(Progress.data.get("stranger", 0)) > 0
	items.append(["The Stranger" if met else "???", func(): _stranger(), met])
	items.append(["Credits", func(): _show_credits()])
	items.append(["Back", func(): go_back()])
	menu(items, Vector2(160, 240), 420, 30)
	var panel := PanelContainer.new()
	panel.position = Vector2(680, 240)
	panel.size = Vector2(1080, 720)
	add_child(panel)
	_panel = UiKit.label("", 26, UiKit.TEXT)
	_panel.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_panel.custom_minimum_size = Vector2(1040, 680)
	_panel.vertical_alignment = VERTICAL_ALIGNMENT_TOP
	panel.add_child(_panel)
	_show_endings()


func _show_endings() -> void:
	var lines := []
	for id in Content.roster():
		var def := Content.fighter(id)
		if ("ending:" + id) in Progress.data["endings"]:
			lines.append("%s\n%s\n" % [def.name.to_upper(), " ".join(Content.story["fighters"][id]["ending"])])
		else:
			lines.append("%s — clear any arcade tower with %s to see this ending.\n" % [def.name.to_upper(), def.name.split(" ")[0]])
	_panel.text = "\n".join(lines)


func _show_records() -> void:
	var lines := []
	var st: Dictionary = Progress.data["stats"]
	lines.append("Matches %d   ·   Wins %d   ·   Sloptalities %d   ·   Flawless victories %d\n" % [int(st.get("matches", 0)), int(st.get("wins", 0)), int(st.get("finishers", 0)), int(st.get("flawless", 0))])
	for id in Content.roster():
		var parts := []
		for t in Game.TOWERS.size():
			var a: Dictionary = Progress.data["arcade"].get("%s:%d" % [id, t], {})
			if a.is_empty():
				parts.append("%s —" % Game.TOWERS[t]["name"])
			else:
				var b := float(a.get("best", 0.0))
				parts.append("%s %d× best %d:%02d, %s pts" % [Game.TOWERS[t]["name"], int(a.get("clears", 0)), int(b) / 60, int(b) % 60, UiKit.commas(int(a.get("score", 0)))])
		lines.append("%s:  %s" % [Content.fighter(id).name, "   ".join(parts)])
	var surv := []
	for id in Content.roster():
		var waves := Progress.best_survival(id)
		if waves > 0:
			surv.append("%s %d" % [Content.fighter(id).name.split(" ")[0], waves])
	lines.append("\nSurvival, most waves cleared: %s" % ("   ·   ".join(surv) if not surv.is_empty() else "none yet"))
	var unl := []
	for id in Content.roster():
		if Progress.is_unlocked("costume:" + id):
			unl.append(Content.fighter(id).name.split(" ")[0])
	lines.append("\nAlternate colours unlocked: %s" % (", ".join(unl) if not unl.is_empty() else "none yet (clear arcade with a fighter)"))
	lines.append("The Roost as a versus stage: %s" % ("unlocked" if Progress.is_unlocked("arena:roost") else "clear arcade once"))
	_panel.text = "\n".join(lines)


func _stranger() -> void:
	if int(Progress.data.get("stranger", 0)) <= 0:
		Audio.play("ui_locked", -4.0)
		_panel.text = "Something is missing from this list. Perhaps a flawless victory would bring it out."
		return
	Game.arcade = {}
	Game.start_stranger(false)
	Game.goto("match")


func _show_credits() -> void:
	_panel.text = "MORTAL SLOPBAT\nA game by Markus Gillyard.\n\nCharacters, stages, animation, sound and music were made for this game.\n\nBuilt with the Godot Engine (MIT license).\nTypefaces: New Rocker by Pablo Impallari, Brenda Gallo and Rodrigo Fuenzalida; Oswald by the Oswald Project Authors; both under the SIL Open Font License 1.1.\n\nFull notices are in THIRD_PARTY_NOTICES.md and the licenses folder beside the game."
