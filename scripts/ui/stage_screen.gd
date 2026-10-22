extends Screen
## Stage select for Versus and Training. The Roost appears once arcade has been cleared.

const DESCRIPTIONS := {
	"rendering_yards": "Tallowmarket's iron floor between the great vats. Drizzle and furnace light. Something in the centre vat is still warm.",
	"drowned_chapel": "Lowmere's sunken church: ankle-deep water, a thousand candles and the moon through the rose window.",
	"stilt_market": "The night market on stilts over the bog. Paper lanterns, rope rails, black water underneath.",
	"roost": "Beneath the Sump, where the Slopbat sleeps on its hoard.",
}

var mode := "versus"
var _ids: Array = []
var _desc: Label
var _thumb: TextureRect


func configure(data: Dictionary) -> void:
	mode = str(data.get("mode", "versus"))


func build() -> void:
	backdrop()
	back_to = "select"
	back_data = {"mode": mode}
	heading("CHOOSE A STAGE", 76, 40)
	_ids = Game.ARENAS.duplicate()
	if Progress.is_unlocked("arena:roost"):
		_ids.append(Game.BOSS_ARENA)
	var items := []
	for id in _ids:
		var sid: String = id
		items.append([Arena.display_name(sid), func(): _pick(sid)])
	items.append(["Random", func(): _pick(_ids[randi() % _ids.size()])])
	var buttons := menu(items, Vector2(150, 260), 520, 30)
	for i in buttons.size():
		var sid: String = _ids[i] if i < _ids.size() else ""
		buttons[i].focus_entered.connect(func(): _show(sid))
	_thumb = TextureRect.new()
	_thumb.position = Vector2(760, 250)
	_thumb.size = Vector2(1000, 562)
	_thumb.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_thumb.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	add_child(_thumb)
	_desc = UiKit.label("", 26, UiKit.TEXT)
	_desc.position = Vector2(760, 840)
	_desc.size = Vector2(1000, 120)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(_desc)
	_show(_ids[0])
	hint("Enter to fight   ·   Backspace to go back")


func _show(id: String) -> void:
	_desc.text = DESCRIPTIONS.get(id, "Any stage, chosen by the Sump.")
	var path := "res://assets/stages/%s.jpg" % id
	_thumb.texture = load(path) if id != "" and ResourceLoader.exists(path) else null


func _pick(id: String) -> void:
	Game.config["stage"] = id
	Game.config["mode"] = mode
	Game.config["rounds_to_win"] = 2
	Game.config["mirror_dark"] = false
	Game.goto("versus" if mode == "versus" else "match", {})
