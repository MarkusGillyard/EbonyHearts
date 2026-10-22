extends Screen
## Move lists, generated from the same data the fights run on: specials, dial-a-combos, the
## finisher, throws and the shared basics, with level, damage, startup and advantage on block.
## Left and right change fighter. Opened from the pause menu it closes back into the match.

const ARROWS_NOTE := "Directions are written for a fighter facing right: → is toward the opponent."

var fighter := "ashka"
var overlay := false
var _ids: Array = []
var _box: VBoxContainer
var _title: Label
var _scroll: ScrollContainer


func configure(data: Dictionary) -> void:
	fighter = str(data.get("fighter", Game.config.get("p1", "ashka")))
	overlay = data.get("overlay", false)


func build() -> void:
	if overlay:
		var dim := ColorRect.new()
		dim.color = Color(0, 0, 0, 0.88)
		dim.set_anchors_preset(Control.PRESET_FULL_RECT)
		add_child(dim)
	else:
		backdrop()
		back_to = "extras"
	_ids = Content.roster()
	if not fighter in _ids:
		_ids.append(fighter)
	_title = heading("", 64, 24)
	_scroll = ScrollContainer.new()
	_scroll.position = Vector2(160, 140)
	_scroll.size = Vector2(1600, 840)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	add_child(_scroll)
	_box = VBoxContainer.new()
	_box.custom_minimum_size = Vector2(1580, 0)
	_box.add_theme_constant_override("separation", 4)
	_scroll.add_child(_box)
	hint("Left / Right: fighter   ·   Up / Down: scroll   ·   Backspace or Esc: close   ·   " + ARROWS_NOTE)
	_fill()


func _fill() -> void:
	for c in _box.get_children():
		c.queue_free()
	var def := Content.fighter(fighter)
	_title.text = "%s  ·  MOVE LIST" % def.name.to_upper()
	var specials := []
	var chains := []
	var throws := []
	var basics := []
	for c in def.commands:
		var m := def.move(c.move)
		if m == null:
			continue
		if m.kind in ["special", "grab"]:
			specials.append([m, c.text])
		elif m.kind == "throw":
			throws.append([m, c.text])
		else:
			basics.append([m, c.text])
	for c in def.air_commands:
		var m := def.move(c.move)
		if m == null:
			continue
		if m.id.begins_with("jump_"):
			basics.append([m, "In the air: " + c.text])
		else:
			specials.append([m, "In the air: " + c.text])
	for path in _chains(def):
		chains.append(path)
	_section("SPECIAL MOVES")
	for s in specials:
		_row(s[0], s[1])
	_section("DIAL-A-COMBOS  (each link spends run meter)")
	for ch in chains:
		_text_row(ch[0], ch[1], "")
	if def.finisher_cmd:
		_section("SLOPTALITY  (when FINISH appears)")
		var fm := def.move(def.finisher_cmd.move)
		_text_row(fm.name if fm else "Sloptality", Command.pretty(def.finisher_cmd.text), "%s range" % def.finisher_range)
		if def.move("stage_vat"):
			_text_row("Stage Sloptality (Rendering Yards)", Command.pretty("D,D,D+LP"), "close")
	_section("THROWS  (press a punch just after being grabbed to break free)")
	for t in throws:
		_row(t[0], t[1])
	_section("BASICS")
	for b in basics:
		_row(b[0], b[1])
	_section("RUN AND BLOCK")
	_text_row("Run", "Hold Run with →", "Drains the run meter")
	_text_row("Block", "Hold Block", "Standing stops highs, mids and overheads; crouching stops lows and mids")
	_scroll.scroll_vertical = 0


## Every chain path starting from a basic move: [names, notation].
func _chains(def: FighterDef) -> Array:
	var out := []
	for c in def.commands:
		var m := def.move(c.move)
		if m == null or m.kind != "normal" or m.chain.is_empty():
			continue
		_walk(def, m, [c.text], [m.name], out, 0)
	return out


func _walk(def: FighterDef, m: MoveDef, inputs: Array, names: Array, out: Array, depth: int) -> void:
	var extended := false
	for link in m.chain:
		var nm := def.move(link["move"])
		if nm == null or nm.id in ["hp", "lp"] or depth > 4:
			continue
		extended = true
		_walk(def, nm, inputs + [link["input"]], names + [nm.name], out, depth + 1)
	if not extended and inputs.size() >= 3:
		var pretty := []
		for i in inputs:
			pretty.append(Command.pretty(str(i)))
		out.append([str(names[names.size() - 1]).split(":")[0], ",  ".join(pretty)])


func _section(text: String) -> void:
	var l := UiKit.label(text, 26, UiKit.ACCENT, HORIZONTAL_ALIGNMENT_LEFT, UiKit.body_font(true))
	l.custom_minimum_size = Vector2(1580, 52)
	l.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
	_box.add_child(l)


func _row(m: MoveDef, notation: String) -> void:
	var info := ""
	if m.is_attack() and m.level != MoveDef.Level.THROW:
		info = "%s · %d dmg · startup %d · %+d on block" % [MoveDef.LEVEL_NAMES[m.level], m.damage, m.startup, m.block_advantage()]
	elif not m.projectile.is_empty():
		var p := m.projectile
		info = "projectile · %s · %d dmg · startup %d" % [str(p.get("level", "high")).capitalize(), int(p.get("damage", 0)), m.startup]
	elif m.kind in ["throw", "grab"]:
		info = "%s · %d dmg" % ["unbreakable grab" if m.kind == "grab" else "throw", int(m.throw_data.get("damage", 0))]
	_text_row(m.name, Command.pretty(notation) if not notation.begins_with("In the air") else "In the air: " + Command.pretty(notation.trim_prefix("In the air: ")), info)


func _text_row(name: String, notation: String, info: String) -> void:
	var h := HBoxContainer.new()
	h.custom_minimum_size = Vector2(1580, 40)
	var a := UiKit.label(name, 24, UiKit.TEXT)
	a.custom_minimum_size = Vector2(520, 40)
	var b := UiKit.label(notation, 24, UiKit.AMBER, HORIZONTAL_ALIGNMENT_LEFT, UiKit.body_font(true))
	b.custom_minimum_size = Vector2(440, 40)
	var c := UiKit.label(info, 20, UiKit.DIM)
	c.custom_minimum_size = Vector2(620, 40)
	h.add_child(a)
	h.add_child(b)
	h.add_child(c)
	_box.add_child(h)


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
		return
	var i := _ids.find(fighter)
	if event.is_action_pressed("ui_left") or event.is_action_pressed("p1_left"):
		fighter = _ids[(i + _ids.size() - 1) % _ids.size()]
		Audio.play("ui_move", -6.0)
		_fill()
	elif event.is_action_pressed("ui_right") or event.is_action_pressed("p1_right"):
		fighter = _ids[(i + 1) % _ids.size()]
		Audio.play("ui_move", -6.0)
		_fill()
	elif event.is_action_pressed("ui_down") or event.is_action_pressed("p1_down"):
		_scroll.scroll_vertical += 120
	elif event.is_action_pressed("ui_up") or event.is_action_pressed("p1_up"):
		_scroll.scroll_vertical -= 120
	elif event.is_action_pressed("ui_cancel") or event.is_action_pressed("pause") or (event is InputEventKey and event.physical_keycode == KEY_ESCAPE):
		Audio.play("ui_back")
		if overlay:
			get_tree().current_scene.close_overlay()
		else:
			go_back()
	else:
		return
	get_viewport().set_input_as_handled()
