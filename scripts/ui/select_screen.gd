extends Screen
## Character select. A grid of portraits; each player moves their own cursor with their own
## controls and confirms with a punch (or Enter), and may cancel with Block (or Backspace).
## High Kick toggles the alternate colours once they are unlocked. In Training the player then
## picks the dummy; in Versus both players pick at once.

const COLS := 3
const TILE := Vector2(210, 250)

var mode := "arcade"
var _ids: Array = []
var _cursor := [0, 1]
var _locked := [false, false]
var _costume := [false, false]
var _tiles: Array[Panel] = []
var _previews: Array[FighterPreview] = []
var _names: Array[Label] = []
var _infos: Array[Label] = []
var _picking := 0
var _grid_origin := Vector2.ZERO
var _done := false


func configure(data: Dictionary) -> void:
	mode = str(data.get("mode", Game.config.get("mode", "arcade")))


func build() -> void:
	backdrop()
	Settings.set_solo(mode != "versus")
	back_to = "menu" if mode != "versus" else "devices"
	heading("CHOOSE YOUR FIGHTER", 72, 30)
	_ids = Content.roster()
	_grid_origin = Vector2(960 - TILE.x * COLS * 0.5, 300)
	for i in _ids.size():
		var tile := Panel.new()
		var sb := StyleBoxFlat.new()
		sb.bg_color = Color(0.05, 0.07, 0.06, 0.92)
		sb.border_color = Color(0.2, 0.24, 0.2)
		sb.set_border_width_all(4)
		tile.add_theme_stylebox_override("panel", sb)
		tile.position = _grid_origin + Vector2((i % COLS) * TILE.x, (i / COLS) * TILE.y)
		tile.size = TILE - Vector2(12, 12)
		add_child(tile)
		var tex_path := "res://assets/portraits/%s.png" % _ids[i]
		if ResourceLoader.exists(tex_path):
			var tr := TextureRect.new()
			tr.texture = load(tex_path)
			tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
			tr.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
			tr.position = Vector2(4, 4)
			tr.size = tile.size - Vector2(8, 8)
			tr.mouse_filter = Control.MOUSE_FILTER_IGNORE
			tile.add_child(tr)
		var nm := UiKit.label(Content.fighter(_ids[i]).name.to_upper(), 22, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER, UiKit.body_font(true))
		nm.position = Vector2(0, tile.size.y - 40)
		nm.size = Vector2(tile.size.x, 34)
		nm.add_theme_color_override("font_outline_color", Color.BLACK)
		nm.add_theme_constant_override("outline_size", 6)
		tile.add_child(nm)
		_tiles.append(tile)
	for side in 2:
		var pv := FighterPreview.new(Vector2i(560, 700))
		pv.facing = 1 if side == 0 else -1
		pv.position = Vector2(30 if side == 0 else 1920 - 590, 150)
		add_child(pv)
		_previews.append(pv)
		var n := UiKit.title("", 54, UiKit.ACCENT if side == 0 else UiKit.AMBER, HORIZONTAL_ALIGNMENT_CENTER)
		n.position = Vector2(30 if side == 0 else 1920 - 590, 840)
		n.size = Vector2(560, 70)
		add_child(n)
		_names.append(n)
		var info := UiKit.label("", 22, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
		info.position = Vector2(30 if side == 0 else 1920 - 590, 910)
		UiKit.wrap(info, Vector2(560, 120))
		add_child(info)
		_infos.append(info)
	if mode == "versus":
		_cursor = [0, mini(1, _ids.size() - 1)]
	else:
		_cursor = [_ids.find(Game.config.get("p1", _ids[0])) if Game.config.get("p1", "") in _ids else 0, 1]
	if mode == "arcade":
		_previews[1].visible = false
		_names[1].text = "?"
		_infos[1].text = "The Slopbat's chosen await."
	_refresh()
	hint("Move: arrows / WASD   ·   Select: punch or Enter   ·   Colours: High Kick   ·   Back: Block or Backspace")


func _active_sides() -> Array:
	if mode == "versus":
		return [0, 1]
	return [_picking]


func _unhandled_input(event: InputEvent) -> void:
	if _done or event.is_echo() or not event.is_pressed():
		return
	for side in _active_sides():
		var who: int = side if mode == "versus" else 0
		var pre := "p%d_" % (who + 1)
		var move := Vector2i.ZERO
		if event.is_action_pressed(pre + "left") or (who == 0 and mode != "versus" and event.is_action_pressed("ui_left")):
			move.x = -1
		elif event.is_action_pressed(pre + "right") or (who == 0 and mode != "versus" and event.is_action_pressed("ui_right")):
			move.x = 1
		elif event.is_action_pressed(pre + "up") or (who == 0 and mode != "versus" and event.is_action_pressed("ui_up")):
			move.y = -1
		elif event.is_action_pressed(pre + "down") or (who == 0 and mode != "versus" and event.is_action_pressed("ui_down")):
			move.y = 1
		if move != Vector2i.ZERO and not _locked[side]:
			var c: int = _cursor[side]
			var col := c % COLS + move.x
			var row := c / COLS + move.y
			var rows := int(ceil(float(_ids.size()) / COLS))
			col = (col + COLS) % COLS
			row = (row + rows) % rows
			_cursor[side] = mini(row * COLS + col, _ids.size() - 1)
			Audio.play("ui_move", -6.0)
			_refresh()
			get_viewport().set_input_as_handled()
			return
		var accept: bool = event.is_action_pressed(pre + "hp") or event.is_action_pressed(pre + "lp") or (who == 0 and mode != "versus" and event.is_action_pressed("ui_accept"))
		if mode == "versus" and who == 0 and event is InputEventKey and (event.physical_keycode == KEY_ENTER or event.physical_keycode == KEY_KP_ENTER):
			accept = true
		var cancel: bool = event.is_action_pressed(pre + "bl")
		if event.is_action_pressed(pre + "hk") and not _locked[side]:
			var id: String = _ids[_cursor[side]]
			if Progress.is_unlocked("costume:" + id):
				_costume[side] = not _costume[side]
				Audio.play("ui_move")
				_refresh()
			else:
				Audio.play("ui_locked", -4.0)
			get_viewport().set_input_as_handled()
			return
		if accept and not _locked[side]:
			_locked[side] = true
			Audio.play("select_fighter")
			_previews[side].celebrate()
			_refresh()
			get_viewport().set_input_as_handled()
			_after_lock(side)
			return
		if cancel and _locked[side]:
			_locked[side] = false
			Audio.play("ui_back")
			_refresh()
			get_viewport().set_input_as_handled()
			return
		if cancel and mode == "training" and _picking == 1:
			_picking = 0
			_locked[0] = false
			Audio.play("ui_back")
			_refresh()
			get_viewport().set_input_as_handled()
			return
	if event.is_action_pressed("ui_cancel") or (event is InputEventKey and event.physical_keycode == KEY_ESCAPE):
		go_back()
		get_viewport().set_input_as_handled()


func _after_lock(side: int) -> void:
	match mode:
		"arcade":
			Game.config["p1"] = _ids[_cursor[0]]
			Game.config["costume_p1"] = _costume[0]
			_finish("tower", {"fighter": _ids[_cursor[0]]})
		"training":
			if side == 0:
				_picking = 1
				_cursor[1] = _cursor[0]
				_refresh()
			else:
				Game.config["p1"] = _ids[_cursor[0]]
				Game.config["p2"] = _ids[_cursor[1]]
				Game.config["costume_p1"] = _costume[0]
				Game.config["costume_p2"] = _costume[1]
				_finish("stage", {"mode": "training"})
		"versus":
			if _locked[0] and _locked[1]:
				Game.config["p1"] = _ids[_cursor[0]]
				Game.config["p2"] = _ids[_cursor[1]]
				Game.config["costume_p1"] = _costume[0]
				Game.config["costume_p2"] = _costume[1]
				_finish("stage", {"mode": "versus"})


func _finish(screen: String, data: Dictionary) -> void:
	_done = true
	await get_tree().create_timer(0.9).timeout
	Game.goto(screen, data)


func _refresh() -> void:
	for i in _tiles.size():
		var sb: StyleBoxFlat = _tiles[i].get_theme_stylebox("panel")
		var c0: bool = _cursor[0] == i
		var c1: bool = _cursor[1] == i and (mode == "versus" or (mode == "training" and _picking == 1))
		sb.border_color = UiKit.ACCENT if c0 and not c1 else (UiKit.AMBER if c1 and not c0 else (Color(0.9, 0.9, 0.6) if c0 and c1 else Color(0.2, 0.24, 0.2)))
		sb.set_border_width_all(6 if c0 or c1 else 3)
	for side in 2:
		if mode == "arcade" and side == 1:
			continue
		if mode == "training" and side == 1 and _picking == 0:
			_previews[1].visible = false
			_names[1].text = "DUMMY"
			_infos[1].text = "Choose who you will practise against next."
			continue
		_previews[side].visible = true
		var id: String = _ids[_cursor[side]]
		var def := Content.fighter(id)
		_previews[side].show_fighter(id, _costume[side])
		_names[side].text = def.name.to_upper() + ("  ✓" if _locked[side] else "")
		var bio: String = Content.story.get("fighters", {}).get(id, {}).get("bio", def.blurb)
		_infos[side].text = "%s\n%s" % [def.title, def.style] + ("\n" + bio if mode != "versus" else "")
