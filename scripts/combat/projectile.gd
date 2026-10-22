class_name Projectile
extends RefCounted
## A thrown or conjured attack that travels on its own: fireballs, flares, sickles, harpoons,
## bubbles, falling anchors. Spawned by a move's "projectile" block on a set frame.

var owner := 0
var move: MoveDef
var kind := ""
var pos := Vector2.ZERO
var vel := Vector2.ZERO
var gravity := 0.0
## Speed lost per second (bubbles slow to a drift).
var decel := 0.0
var min_speed := 0.0
var size := Vector2(0.3, 0.3)
var facing := 1
var life := 90
var age := 0
var hits_left := 1
## Projectiles destroy each other; a stronger one survives a weaker one.
var strength := 1
var level: int = MoveDef.Level.HIGH
var damage := 60
var chip := 10
var hitstun := 24
var blockstun := 14
var hitstop := 8
var effect := "stagger"
var launch := Vector2(1.5, 0.0)
var juggle := Vector2(1.8, 4.0)
var push_hit := 0.5
var push_block := 0.45
## "die" on touching the floor, "roll" along it, "bounce".
var ground := "die"
## Out-and-back: after half its life it flies home and vanishes on reaching its owner.
var returns := false
## Drawn as a chain from the owner's hand (harpoon); dies if the owner is hit.
var tether := false
var dead := false


static func spawn(owner_f: Fighter, m: MoveDef) -> Projectile:
	var d: Dictionary = m.projectile
	var k: float = float(d.get("_scale", 1.0))
	var p := Projectile.new()
	p.owner = owner_f.index
	p.move = m
	p.kind = str(d.get("kind", "orb"))
	p.facing = owner_f.facing
	var off: Array = d.get("offset", [0.6, 1.3])
	p.pos = owner_f.pos + Vector2(float(off[0]) * k * owner_f.facing, float(off[1]) * k)
	if d.has("at_opponent"):
		# Appears over the opponent's position (falling anchors), clamped to a reach.
		var opp := owner_f.opponent()
		var reach: Array = d["at_opponent"]
		var dx := clampf((opp.pos.x - owner_f.pos.x) * owner_f.facing, float(reach[0]), float(reach[1]))
		p.pos.x = owner_f.pos.x + dx * owner_f.facing
	p.vel = Vector2(float(d.get("speed", 6.0)) * owner_f.facing, float(d.get("vy", 0.0)))
	p.gravity = float(d.get("gravity", 0.0))
	p.decel = float(d.get("decel", 0.0))
	p.min_speed = float(d.get("min_speed", 0.0))
	var sz: Array = d.get("size", [0.3, 0.3])
	p.size = Vector2(float(sz[0]), float(sz[1])) * k
	p.life = int(d.get("life", 90))
	p.hits_left = int(d.get("hits", 1))
	p.strength = int(d.get("strength", 1))
	p.level = MoveDef.LEVELS.get(str(d.get("level", "high")), MoveDef.Level.HIGH)
	p.damage = int(d.get("damage", 60))
	p.chip = int(d.get("chip", int(p.damage * 0.2)))
	p.hitstun = int(d.get("hitstun", 24))
	p.blockstun = int(d.get("blockstun", 14))
	p.hitstop = int(d.get("hitstop", 8))
	p.effect = str(d.get("effect", "stagger"))
	p.launch = MoveDef._v2(d.get("launch", [1.5, 0.0]))
	p.juggle = MoveDef._v2(d.get("juggle", [1.8, 4.0]))
	p.push_hit = float(d.get("push_hit", 0.5))
	p.push_block = float(d.get("push_block", 0.45))
	p.ground = str(d.get("ground", "die"))
	p.returns = bool(d.get("returns", false))
	p.tether = bool(d.get("tether", false))
	return p


func rect() -> Rect2:
	return Rect2(pos - size * 0.5, size)


## Advances one tick; `home` is the owner's hand height position for returning projectiles.
func step(dt: float, home: Vector2) -> void:
	age += 1
	if returns and age > life / 2:
		var to := home - pos
		var speed := absf(vel.x) if absf(vel.x) > 0.1 else 8.0
		if to.length() < speed * dt * 1.5:
			dead = true
			return
		vel = to.normalized() * speed
	else:
		vel.y -= gravity * dt
		if decel > 0.0 and absf(vel.x) > min_speed:
			vel.x = move_toward(vel.x, min_speed * facing, decel * dt)
	pos += vel * dt
	if pos.y - size.y * 0.5 < 0.0 and vel.y <= 0.0:
		match ground:
			"die":
				if gravity > 0.0:
					dead = true
			"roll":
				pos.y = size.y * 0.5
				vel.y = 0.0
			"bounce":
				pos.y = size.y * 0.5
				vel.y = absf(vel.y) * 0.45
	if age >= life:
		dead = true
