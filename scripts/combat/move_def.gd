class_name MoveDef
extends RefCounted
## One attack, special, throw or action, parsed from fighter data. Frames are 1-based: frame
## `startup` is the first active frame. Boxes are [forward, up, width, height] in metres from the
## fighter's root (the point between the feet on the floor), forward toward the opponent.

enum Level { HIGH, MID, LOW, OVERHEAD, UNBLOCKABLE, THROW, NONE }

const LEVELS := {"high": Level.HIGH, "mid": Level.MID, "low": Level.LOW, "overhead": Level.OVERHEAD,
	"unblockable": Level.UNBLOCKABLE, "throw": Level.THROW, "none": Level.NONE}
const LEVEL_NAMES := {Level.HIGH: "High", Level.MID: "Mid", Level.LOW: "Low", Level.OVERHEAD: "Overhead",
	Level.UNBLOCKABLE: "Unblockable", Level.THROW: "Throw", Level.NONE: ""}

var id := ""
var name := ""
var notation := ""
## "normal", "special", "throw", "grab", "finisher", "taunt", "air", "chain"
var kind := "normal"
var startup := 1
var active := 0
var recovery := 0
var total := 1
var level: int = Level.NONE
var damage := 0
var chip := 0
var hitstun := 18
var blockstun := 12
var hitstop := -1
## "stagger", "launch", "knockdown", "sweep", "crumple", "pull", "daze"
var effect := "stagger"
## Velocity given to the defender by launch, knockdown, sweep and juggle hits: [back, up] m/s.
var launch := Vector2(1.5, 0.0)
var juggle := Vector2(1.6, 4.2)
var juggle_cost := 1
var push_hit := 0.35
var push_block := 0.45
## Each: {"from": int, "to": int, "rect": Rect2} in forward/up metres.
var hitboxes: Array = []
## Extra hurtboxes while a limb is extended: {"from", "to", "rect"}.
var hurtboxes: Array = []
## Root motion: [frame, metres forward so far].
var motion := PackedVector2Array()
## Vertical root motion for hops and flights: [frame, metres up so far].
var lift := PackedVector2Array()
## Steering toward the opponent on frames [from, to] at this many m/s (dives, homing rushes).
var track := Vector3.ZERO
var air := false
## Frame ranges with special properties ([from, to], zero when absent).
var invuln := Vector2i.ZERO
var proj_invuln := Vector2i.ZERO
var armor := Vector2i.ZERO
var low_profile := Vector2i.ZERO
var airborne := Vector2i.ZERO
var ends_crouched := false
## Chain links (dial-a-combo): {"mask", "dir", "input", "move", "from", "to", "cost"}.
var chain: Array = []
## Frames on which a special move may cancel this one after it has made contact.
var cancel := Vector2i.ZERO
var projectile: Dictionary = {}
var throw_data: Dictionary = {}
## Range condition: "", "close" (throws, grabs), "sweep", "far" (finishers).
var requires := ""
var run_cost := 0.0
var finisher := false
var effects: Dictionary = {}
var anim: Dictionary = {}
var sfx: Dictionary = {}
var tags: PackedStringArray = PackedStringArray()


## `scale` converts boxes and distances authored for a 1.8 m body (the shared basic moves).
static func from_dict(move_id: String, d: Dictionary, scale := 1.0) -> MoveDef:
	var m := MoveDef.new()
	m.id = move_id
	m.name = str(d.get("name", move_id))
	m.notation = str(d.get("notation", ""))
	m.kind = str(d.get("kind", "normal"))
	m.startup = int(d.get("startup", 1))
	m.active = int(d.get("active", 0))
	m.recovery = int(d.get("recovery", 0))
	m.total = maxi(1, m.startup - 1 + m.active + m.recovery)
	if d.has("total"):
		m.total = int(d["total"])
	m.level = LEVELS.get(str(d.get("level", "none")), Level.NONE)
	m.damage = int(d.get("damage", 0))
	m.chip = int(d.get("chip", 0))
	m.hitstun = int(d.get("hitstun", 18))
	m.blockstun = int(d.get("blockstun", 12))
	m.hitstop = int(d.get("hitstop", -1))
	m.effect = str(d.get("effect", "stagger"))
	m.launch = _v2(d.get("launch", [1.5, 0.0]))
	m.juggle = _v2(d.get("juggle", [1.6, 4.2]))
	m.juggle_cost = int(d.get("juggle_cost", 1))
	m.push_hit = float(d.get("push_hit", 0.35)) * scale
	m.push_block = float(d.get("push_block", 0.45)) * scale
	for hb in d.get("hitboxes", []):
		var fr: Array = hb.get("frames", [m.startup, m.startup + m.active - 1])
		m.hitboxes.append({"from": int(fr[0]), "to": int(fr[1]), "rect": _rect(hb["rect"], scale)})
	for hb in d.get("hurtboxes", []):
		var fr: Array = hb.get("frames", [m.startup, m.startup + m.active - 1])
		m.hurtboxes.append({"from": int(fr[0]), "to": int(fr[1]), "rect": _rect(hb["rect"], scale)})
	m.motion = _curve(d.get("motion", []), scale)
	m.lift = _curve(d.get("lift", []), scale)
	var tr: Array = d.get("track", [])
	if tr.size() == 3:
		m.track = Vector3(float(tr[0]), float(tr[1]), float(tr[2]))
	m.air = bool(d.get("air", false))
	var props: Dictionary = d.get("props", {})
	m.invuln = _range(props.get("invuln"))
	m.proj_invuln = _range(props.get("proj_invuln"))
	m.armor = _range(props.get("armor"))
	m.low_profile = _range(props.get("low_profile"))
	m.airborne = _range(props.get("airborne"))
	m.ends_crouched = bool(d.get("ends_crouched", false))
	for c in d.get("chain", []):
		var cmd := Command.parse(str(c["input"]))
		var w: Array = c.get("window", [m.startup, m.total])
		m.chain.append({"input": str(c["input"]), "mask": cmd.mask, "steps": cmd.steps, "move": str(c["move"]),
			"from": int(w[0]), "to": int(w[1]), "cost": float(c.get("cost", 12.0))})
	m.cancel = _range(d.get("cancel"))
	m.projectile = d.get("projectile", {})
	if not m.projectile.is_empty():
		m.projectile = m.projectile.duplicate(true)
		m.projectile["_scale"] = scale
	m.throw_data = d.get("throw", {})
	m.requires = str(d.get("requires", ""))
	m.run_cost = float(d.get("run_cost", 0.0))
	m.finisher = m.kind == "finisher"
	m.effects = d.get("effects", {})
	m.anim = d.get("anim", {})
	m.sfx = d.get("sfx", {})
	m.tags = PackedStringArray(d.get("tags", []))
	return m


static func _v2(a: Array) -> Vector2:
	return Vector2(float(a[0]), float(a[1]))


static func _rect(a: Array, scale: float) -> Rect2:
	return Rect2(float(a[0]) * scale, float(a[1]) * scale, float(a[2]) * scale, float(a[3]) * scale)


static func _range(v: Variant) -> Vector2i:
	if v is Array and v.size() == 2:
		return Vector2i(int(v[0]), int(v[1]))
	return Vector2i.ZERO


static func _curve(points: Array, scale: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in points:
		out.append(Vector2(float(p[0]), float(p[1]) * scale))
	return out


## Cumulative distance along a [frame, distance] curve at `frame`, linear between keys.
static func curve_at(curve: PackedVector2Array, frame: float) -> float:
	if curve.is_empty():
		return 0.0
	if frame <= curve[0].x:
		return curve[0].y * clampf(frame / maxf(curve[0].x, 1.0), 0.0, 1.0)
	for i in range(1, curve.size()):
		if frame <= curve[i].x:
			var t := (frame - curve[i - 1].x) / maxf(curve[i].x - curve[i - 1].x, 1e-5)
			return lerpf(curve[i - 1].y, curve[i].y, t)
	return curve[curve.size() - 1].y


static func in_range(r: Vector2i, frame: int) -> bool:
	return r != Vector2i.ZERO and frame >= r.x and frame <= r.y


func is_attack() -> bool:
	return level != Level.NONE


func last_active() -> int:
	return startup + active - 1


func in_active(frame: int) -> bool:
	return active > 0 and frame >= startup and frame <= last_active()


## Frame advantage on block when the first active frame connects (for move lists and tests).
func block_advantage() -> int:
	return blockstun - (active - 1 + recovery)


func hit_advantage() -> int:
	if effect in ["launch", "knockdown", "sweep", "crumple"]:
		return 999
	return hitstun - (active - 1 + recovery)
