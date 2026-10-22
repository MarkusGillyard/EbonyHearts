extends Screen
## Between arcade fights after a flawless win: a stranger in white is watching. Approaching him
## is optional; walking on costs nothing and the ladder continues.


func build() -> void:
	backdrop(Color(0.7, 0.55, 1.0))
	var st: Dictionary = Content.story.get("stranger", {})
	var l := UiKit.label(str(st.get("offer", "")), 40, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	l.position = Vector2(0, 330)
	l.size = Vector2(1920, 60)
	add_child(l)
	menu([[str(st.get("approach", "Approach him")), func(): _approach()],
		[str(st.get("leave", "Walk on")), func(): _leave()]], Vector2(730, 520), 460, 32)
	Game.stranger_pending = false


func _approach() -> void:
	Game.start_stranger(true)
	Game.goto("match")


func _leave() -> void:
	Game.goto("versus", {})
