extends TestCase
## The data holds together: every fighter loads, every command and chain names a real move,
## every model exists and matches its look, every sound the code names is present, and the
## story covers the whole roster.

const SOUNDS := ["whoosh_light", "whoosh_medium", "whoosh_heavy", "hit_light", "hit_medium", "hit_heavy", "block", "armor",
	"grab", "slam", "throw_break", "land", "step", "burst", "ko_hit", "gong", "fight", "finish", "ui_move", "ui_select",
	"ui_back", "ui_locked", "select_fighter", "ignite", "crumble", "splash", "sink", "stamp", "far_whoosh", "gulp", "bell",
	"scream", "chain", "steam", "thunder", "charge", "screech", "wing", "palm_strike",
	"music_menu", "music_select", "music_yards", "music_chapel", "music_market", "music_roost", "music_ending"]


func test_all_fighters_load() -> void:
	assert_true(Content.errors.is_empty(), "content errors: %s" % [Content.errors])
	for id in Content.all_ids():
		assert_true(Content.fighter(id) != null, "%s loads" % id)
	assert_eq(Content.roster().size(), 6, "six selectable fighters")


func test_commands_and_chains_point_at_real_moves() -> void:
	var bad := []
	for id in Content.all_ids():
		var def := Content.fighter(id)
		for c in def.commands + def.air_commands:
			if def.move(c.move) == null:
				bad.append("%s command %s -> %s" % [id, c.text, c.move])
			if c.mask == 0:
				bad.append("%s command %s has no buttons" % [id, c.text])
		for mid in def.moves:
			var m: MoveDef = def.moves[mid]
			for link in m.chain:
				if def.move(link["move"]) == null:
					bad.append("%s chain %s -> %s" % [id, mid, link["move"]])
			var parry: Dictionary = m.effects.get("parry", {})
			if not parry.is_empty() and def.move(str(parry.get("counter", ""))) == null:
				bad.append("%s parry counter %s" % [id, parry.get("counter", "")])
		if def.finisher_cmd and def.move(def.finisher_cmd.move) == null:
			bad.append("%s finisher %s" % [id, def.finisher_cmd.move])
	assert_true(bad.is_empty(), "broken references: %s" % [bad])


func test_every_roster_fighter_is_complete() -> void:
	for id in Content.roster():
		var def := Content.fighter(id)
		var specials := 0
		for c in def.commands:
			var m := def.move(c.move)
			if m and m.kind in ["special", "grab"]:
				specials += 1
		var chains := 0
		for mid in def.moves:
			if (def.moves[mid] as MoveDef).kind == "chain":
				chains += 1
		assert_true(specials >= 3, "%s has at least three specials (%d)" % [id, specials])
		assert_true(chains >= 2, "%s has dial-a-combo links (%d)" % [id, chains])
		assert_true(def.finisher_cmd != null, "%s has a finisher" % id)
		assert_true(def.move("throw_f") != null, "%s can throw" % id)
		var story: Dictionary = Content.story.get("fighters", {}).get(id, {})
		assert_true(story.has("ending") and story.has("bio") and story.has("rival_lines"), "%s has a story" % id)
		assert_true(Content.fighter(str(story.get("rival", ""))) != null, "%s has a rival" % id)


func test_models_exist_and_match_their_looks() -> void:
	for id in Content.all_ids():
		var def := Content.fighter(id)
		var path := CharacterModel.path_for(str(def.look.get("model", id)))
		assert_true(FileAccess.file_exists(path), "%s has a model" % id)
		var data := CharacterModel.read(path)
		if data.is_empty():
			continue
		assert_eq(str(data["header"].get("look_hash", "")), CharacterModel.look_hash(def), "%s's model is up to date with its look (regenerate it)" % id)


func test_sounds_exist() -> void:
	var missing := []
	for s in SOUNDS:
		if not ResourceLoader.exists("res://assets/audio/%s.wav" % s):
			missing.append(s)
	for id in Content.all_ids():
		var def := Content.fighter(id)
		for mid in def.moves:
			var sw := str((def.moves[mid] as MoveDef).sfx.get("swing", ""))
			if sw != "" and not sw in ["light", "medium", "heavy"] and not ResourceLoader.exists("res://assets/audio/%s.wav" % sw):
				missing.append("%s.%s: %s" % [id, mid, sw])
		for k in Vfx.KINDS:
			if not ResourceLoader.exists("res://assets/audio/cast_%s.wav" % k) and k in ["ember", "wave", "flare", "bubble", "tar", "sickle", "harpoon", "anchor"]:
				missing.append("cast_" + k)
	assert_true(missing.is_empty(), "missing sounds: %s" % [missing])


func test_every_stage_builds() -> void:
	for id in Arena.STAGES:
		var a := Arena.create(id)
		root.add_child(a)
		a.build()
		assert_true(a.get_child_count() > 5, "%s builds" % id)
		assert_true(a.music.begins_with("music_"), "%s has music" % id)
		a.queue_free()


func test_music_loops_and_effects_do_not() -> void:
	for name in ["music_menu", "music_select", "music_yards", "music_chapel", "music_market", "music_roost", "music_ending"]:
		var s: AudioStreamWAV = load("res://assets/audio/%s.wav" % name)
		assert_ne(s.loop_mode, AudioStreamWAV.LOOP_DISABLED, "%s loops" % name)
		assert_true(s.get_length() > 30.0, "%s is a real loop" % name)
	var hit: AudioStreamWAV = load("res://assets/audio/hit_heavy.wav")
	assert_eq(hit.loop_mode, AudioStreamWAV.LOOP_DISABLED, "effects play once")
