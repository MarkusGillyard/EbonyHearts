extends Screen
## Choose your destiny: three towers of opponents, Novice, Warrior and Master, each ending at the
## roost. The tower also sets how hard the CPU fights. Left and right choose, Enter climbs.

var fighter := "ashka"
var _plans: Array = []
var _cols: Array[Control] = []
var _sel := 1
var _records: Array[Label] = []


func configure(data: Dictionary) -> void:
	fighter = str(data.get("fighter", Game.config.get("p1", "ashka")))


func build() -> void:
	backdrop(UiKit.AMBER)
	back_to = "select"
	back_data = {"mode": "arcade"}
	heading("CHOOSE YOUR DESTINY", 80, 40)
	var seed := int(Time.get_unix_time_from_system()) % 100000
	for t in Game.TOWERS.size():
		_plans.append(Game.plan_arcade(fighter, t, seed + t * 17))
	for t in Game.TOWERS.size():
		var col := VBoxContainer.new()
		col.position = Vector2(300 + t * 470, 190)
		col.size = Vector2(380, 760)
		col.add_theme_constant_override("separation", 8)
		add_child(col)
		var name := UiKit.title(Game.TOWERS[t]["name"].to_upper(), 46, UiKit.TEXT)
		col.add_child(name)
		var rungs: Array = _plans[t]
		for i in range(rungs.size() - 1, -1, -1):
			var r: Dictionary = rungs[i]
			var text := ""
			match r["kind"]:
				"boss":
					text = "THE SLOPBAT"
				"mirror":
					text = "THE REFLECTION"
				_:
					text = Content.fighter(r["opponent"]).name.to_upper()
			var row := Label.new()
			row.text = text
			row.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			row.add_theme_font_override("font", UiKit.body_font(true))
			row.add_theme_font_size_override("font_size", 26)
			row.add_theme_color_override("font_color", UiKit.RUST.lightened(0.25) if r["kind"] == "boss" else (UiKit.AMBER if r["kind"] == "rival" else UiKit.TEXT))
			var panel := PanelContainer.new()
			panel.add_child(row)
			col.add_child(panel)
		var you := UiKit.label(Content.fighter(fighter).name.to_upper(), 26, UiKit.ACCENT, HORIZONTAL_ALIGNMENT_CENTER, UiKit.body_font(true))
		col.add_child(you)
		var rec := UiKit.label("", 20, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
		var key := "%s:%d" % [fighter, t]
		var a: Dictionary = Progress.data["arcade"].get(key, {})
		rec.text = "Cleared %d×  ·  best %s" % [int(a.get("clears", 0)), _time(float(a.get("best", 0.0)))] if not a.is_empty() else "Not yet cleared"
		col.add_child(rec)
		_records.append(rec)
		_cols.append(col)
	_refresh()
	hint("Left / Right to choose a tower   ·   Enter or a punch to begin   ·   Backspace to go back")


static func _time(sec: float) -> String:
	if sec <= 0.0:
		return "--"
	return "%d:%02d" % [int(sec) / 60, int(sec) % 60]


func _refresh() -> void:
	for t in _cols.size():
		_cols[t].modulate = Color(1, 1, 1, 1) if t == _sel else Color(0.5, 0.5, 0.5, 0.75)
		_cols[t].scale = Vector2.ONE * (1.04 if t == _sel else 1.0)


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
		return
	if event.is_action_pressed("ui_left") or event.is_action_pressed("p1_left"):
		_sel = (_sel + Game.TOWERS.size() - 1) % Game.TOWERS.size()
		Audio.play("ui_move", -6.0)
		_refresh()
	elif event.is_action_pressed("ui_right") or event.is_action_pressed("p1_right"):
		_sel = (_sel + 1) % Game.TOWERS.size()
		Audio.play("ui_move", -6.0)
		_refresh()
	elif event.is_action_pressed("ui_accept") or event.is_action_pressed("p1_hp") or event.is_action_pressed("p1_lp"):
		Audio.play("select_fighter")
		Game.start_arcade(fighter, _sel, _plans[_sel])
		Game.goto("versus", {})
	else:
		super._unhandled_input(event)
		return
	get_viewport().set_input_as_handled()
