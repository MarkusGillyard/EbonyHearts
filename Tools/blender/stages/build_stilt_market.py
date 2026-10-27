"""Lowmere Stilt Market at night, Bram Ketch's beat: a plank boardwalk on stilts over the bog. Rope
rails behind the fighting strip under strings of amber paper lanterns; stalls with striped awnings
close both ends and line a second pier across the water, joined by a footbridge; reeds and black
water below, stilt houses with lit windows across the bog, a low moon and mist. On the right a
raised walkway juts out over the water, lit by a single pale lamp: where the stranger is met.

Composition from scripts/view/arenas/market.gd (Godot (x, y, z) -> Blender (x, -z, y)); the tall
rail poles moved to X 0, +-7.2, +-14.4 so the ones at +-7.2 mark the ends of the floor.
Run: Tools/blender/stages/run.sh build_stilt_market.py [-- --no-render | --quick]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402
from mathutils import Vector  # noqa: E402

A = S.args()
st = S.Stage("stilt_market", "Lowmere Stilt Market", seed=11, ev100=1.0)
R = st.rng
st.music = "music_market"
st.footstep = "wood"

DECK_BACK = 2.55
DECK_FRONT = -4.65
WATER_Z = -1.7
PIER_Y = 12.0
PIER_D = 2.6
WALK_Z = 1.25
WALK_Y = 15.5
RAIL_Y = DECK_BACK - 0.12
POLE_X = (-14.4, -7.2, 0.0, 7.2, 14.4)

PLANK = st.surface("weathered_planks", tint=(0.62, 0.58, 0.54), rough=1.0)
PLANK_D = st.surface("weathered_planks__dark", tint=(0.32, 0.29, 0.27), rough=1.0)
PILE = st.surface("moss_wood", tint=(0.45, 0.47, 0.4), rough=1.0)
WOOD = st.surface("Wood051", tint=(0.5, 0.42, 0.36), rough=1.2)
ROPE = st.surface("Rope003", tint=(0.75, 0.68, 0.55), rough=1.0)
HESS = st.surface("hessian_380", tint=(0.72, 0.66, 0.52), rough=1.0)
WICKER = st.surface("hessian_380__wicker", tint=(0.27, 0.23, 0.17), rough=1.0, tiling=1.0)
RED = st.surface("Fabric040", tint=(0.85, 0.55, 0.5), rough=1.0, two_sided=True)
TEAL = st.surface("Fabric040__teal", tint=(0.18, 0.55, 0.52), rough=1.0, two_sided=True)
OCHRE = st.surface("Fabric040__ochre", tint=(1.0, 0.78, 0.3), rough=1.0, two_sided=True)
PLUM = st.surface("Fabric040__plum", tint=(0.5, 0.3, 0.45), rough=1.0, two_sided=True)
CREAM = st.surface("hessian_380__awning", tint=(0.95, 0.9, 0.78), rough=1.0, two_sided=True)
IRON = st.surface("Metal055A", tint=(0.22, 0.21, 0.2), rough=1.0, metal=1.0)
FISH = st.surface("Metal055A__fish", tint=(0.6, 0.66, 0.68), rough=0.5, metal=0.5)
LEATHER = st.surface("Leather021", tint=(0.8, 0.7, 0.6), rough=1.0)
CLAY = st.surface("Ivory001B__clay", tint=(0.42, 0.4, 0.26), rough=0.7)
GOURD = st.surface("Ivory001B__gourd", tint=(0.85, 0.45, 0.12), rough=0.6)
BEET = st.surface("Fabric040__beet", tint=(0.5, 0.12, 0.2), rough=0.6)
ROOF = st.surface("moss_wood__roof", tint=(0.22, 0.23, 0.2), rough=1.0)
BANK = st.surface("Ground051", tint=(0.12, 0.13, 0.12), rough=1.0)
REED = st.surface("Ground037__reed", tint=(0.42, 0.42, 0.22), rough=1.0, two_sided=True)
MUD = st.surface("brown_mud_03", tint=(0.35, 0.33, 0.3), rough=0.6)
AMBER = st.glow("GLOW_lantern_amber", (1.0, 0.42, 0.1), 1.5, "paper lanterns: horizontal paper ribs about 3 cm apart, slow sway", pattern="ribs", scale=0.032)
RED_L = st.glow("GLOW_lantern_red", (1.0, 0.16, 0.05), 1.3, "red paper lanterns, ribbed likewise", pattern="ribs", scale=0.032)
WIN = st.glow("GLOW_window", (1.0, 0.56, 0.24), 1.6, "stilt-house windows")
JAR = st.glow("GLOW_jar", (0.5, 0.95, 0.2), 1.8, "jars of slop at the jar stall")
PALE = st.glow("GLOW_lamp_pale", (0.75, 0.8, 1.0), 6.0, "the stranger's lamp")
MOON = st.glow("GLOW_moon", (0.85, 0.9, 0.82), 3.0, "the low moon disc (delete if the sky carries one)")
BOG = st.water("WATER_bog", (0.008, 0.012, 0.013), 0.05, 1.0, "black bog water with a little green scum; gentle swell; reflections matter")

deck = st.part("deck", PLANK, floor=True, uv_rot=True, sharp=40)
dark = st.part("beams", PLANK_D, sharp=40)
piles = st.part("piles", PILE, sharp=50)
wood = st.part("posts", WOOD, sharp=45)
rope = st.part("ropes", ROPE, sharp=70, shadows=False)
lan = st.part("lanterns", AMBER, shadows=False, sharp=60)
caps = st.part("lantern_caps", IRON, shadows=False, sharp=60)
goods = st.part("goods", WICKER, sharp=50)


def planks(part, x0, x1, y0, y1, z, w=2 / 7, thick=0.05, lmin=1.8, lmax=3.6, along="x", jitter=0.0):
    """Boards with gaps and staggered joints covering [x0,x1] x [y0,y1], tops at z."""
    rows = int(round((y1 - y0) / w)) if along == "x" else int(round((x1 - x0) / w))
    for i in range(rows):
        a = x0 if along == "x" else y0
        b = x1 if along == "x" else y1
        t = a - R.uniform(0, lmax * 0.6)
        while t < b:
            L = R.uniform(lmin, lmax)
            s0, s1 = max(t, a), min(t + L, b)
            if s1 - s0 > 0.15:
                dz = R.uniform(-jitter, 0) if jitter else 0.0
                if along == "x":
                    yc = y0 + (i + 0.5) * w
                    part.box((s1 - s0 - 0.012, w - 0.014, thick), ((s0 + s1) * 0.5, yc, z - thick * 0.5 + dz), bevel=0.01, drop=("-z",))
                else:
                    xc = x0 + (i + 0.5) * w
                    part.box((w - 0.014, s1 - s0 - 0.012, thick), (xc, (s0 + s1) * 0.5, z - thick * 0.5 + dz), bevel=0.01, drop=("-z",))
            t += L


def pile(x, y, top, r=0.11, lean=2.0):
    piles.cyl(r, top - WATER_Z + 0.5, (x, y, WATER_Z - 0.5), (R.uniform(-lean, lean), R.uniform(-lean, lean), 0), segs=8, bevel=0.02)


def lantern(p, mat=AMBER, scale=1.0):
    """A ribbed paper lantern hanging from `p` (its hook), with dark caps."""
    p = Vector(p)
    s = scale
    # bottom to top, so the normals face outward
    prof = [(0.06 * s, -0.3 * s), (0.1 * s, -0.26 * s), (0.125 * s, -0.2 * s), (0.13 * s, -0.15 * s), (0.125 * s, -0.1 * s), (0.1 * s, -0.04 * s), (0.06 * s, 0)]
    lan.lathe(prof, p + Vector((0, 0, -0.13 * s)), segs=10, cap_bottom=False, cap_top=False, mat=mat)
    caps.cyl(0.065 * s, 0.04 * s, p + Vector((0, 0, -0.15 * s)), segs=8, caps=(False, True))
    caps.cyl(0.065 * s, 0.04 * s, p + Vector((0, 0, -0.47 * s)), segs=8, caps=(True, False))
    caps.cyl(0.006, 0.12 * s, p + Vector((0, 0, -0.12 * s)), segs=4, caps=(False, False))
    caps.cyl(0.012, 0.08 * s, p + Vector((0, 0, -0.55 * s)), segs=4, caps=(True, False))


def lantern_string(a, c, sag, spacing=0.9, part=None):
    a, c = Vector(a), Vector(c)
    rope.rope(a, c, sag, 0.012, 14, 5)
    n = int((a - c).length / spacing)
    for k in range(1, n):
        t = k / n
        p = a.lerp(c, t) - Vector((0, 0, sag * 4 * t * (1 - t) + 0.02))
        lantern(p, RED_L if R.random() < 0.22 else AMBER, R.uniform(0.9, 1.1))


def basket(p, r, h, lid=False):
    p = Vector(p)
    goods.lathe([(r * 0.8, 0), (r * 0.88, 0.02), (r, h * 0.9), (r * 1.03, h), (r * 0.96, h), (r * 0.9, h * 0.75), (0, h * 0.72)], p, segs=12, cap_bottom=True)
    if lid:
        goods.lathe([(r * 1.02, 0), (r * 0.8, r * 0.18), (0, r * 0.26)], p + Vector((0, 0, h)), segs=12, cap_bottom=False)


def coil(p):
    p = Vector(p)
    for k in range(4):
        rope.ring(0.18 - k * 0.025, 0.022, p + Vector((0, 0, 0.022 + k * 0.04)), segs=16, rsegs=5)


def trap(p, yaw):
    M = S.xform(p, (0, 0, yaw))
    goods.add(S.geo_lathe([(0.2, 0), (0.19, 0.25), (0.15, 0.5), (0.11, 0.7), (0.0, 0.72)], 12, cap_bottom=True), m=M @ S.xform((0, -0.35, 0.2), (-90, 0, 0)))
    for k in range(3):
        caps.add(S.geo_ring(0.205 - k * 0.04, 0.008, 12, 3), m=M @ S.xform((0, -0.35 + k * 0.25, 0.2), (90, 0, 0)))


def net(p, w, h, yaw=0.0, sag=0.06, cells=6.5):
    """A drying net: knotted strands on a grid, sagging between its top corners."""
    M = S.xform(p, (0, 0, yaw))
    nx, nz = max(2, int(w * cells)), max(2, int(h * cells))
    def pt(u, v):
        x = -w / 2 + w * u
        z = -h * v - sag * 4 * u * (1 - u) * (1 - 0.5 * v)
        return M @ Vector((x, 0.01 * math.sin(u * 9 + v * 5), z))
    for i in range(nx + 1):
        rope.tube([pt(i / nx, j / 4) for j in range(5)], 0.004, 3, caps=False)
    for j in range(1, nz + 1):
        rope.tube([pt(i / 6, j / nz) for i in range(7)], 0.004, 3, caps=False)
    rope.tube([pt(i / 8, 0) for i in range(9)], 0.008, 4, caps=False)
    for k in range(3):
        st.part("floats", WOOD).cyl(0.045, 0.06, pt(0.17 + k * 0.33, 0.05), (90, 0, yaw), segs=8, bevel=0.01)


def fish(p, rot, s=1.0):
    """A fish along local Z (head down, tail up), flattened so its broad side faces local X."""
    g = S.geo_lathe([(0, -0.085 * s), (0.012 * s, -0.07 * s), (0.026 * s, -0.03 * s), (0.03 * s, 0.02 * s), (0.018 * s, 0.06 * s), (0, 0.08 * s)], 8)
    for v in g.v:
        v.x *= 0.45
    M = S.xform(p, rot)
    st.part("fish", FISH, sharp=70).add(g, m=M)
    tail = S.geo_prism([(0, 0), (0.035 * s, 0.045 * s), (-0.035 * s, 0.045 * s)], 0.006 * s, axis="x")
    st.part("fish", FISH).add(tail, m=M @ S.xform((0, 0, 0.075 * s)), uv="box")


def stall(o, w, d, colours, goods_kind, back_open=False):
    """A market stall at `o` (front centre, on the boards) facing -Y."""
    o = Vector(o)
    for sx in (-0.5, 0.5):
        wood.box((0.1, 0.1, 2.1), o + Vector((sx * w, 0, 1.05)), bevel=0.015)
        wood.box((0.1, 0.1, 2.55), o + Vector((sx * w, d, 1.275)), bevel=0.015)
        dark.box((0.05, d, 1.1), o + Vector((sx * w, d * 0.5, 0.55)), bevel=0.008)
        wood.box((0.07, d + 0.1, 0.07), o + Vector((sx * w, d * 0.5, 2.08)), (math.degrees(math.atan2(0.47, d)), 0, 0), bevel=0.01)
    dark.box((w, 0.06, 2.5), o + Vector((0, d, 1.25)), bevel=0.01)
    wood.box((w + 0.12, 0.56, 0.06), o + Vector((0, 0.25, 0.88)), bevel=0.015)
    dark.box((w, 0.05, 0.85), o + Vector((0, 0.02, 0.425)), bevel=0.008)
    # striped awning sagging from the back wall top out past the front posts, scalloped valance
    run, drop = d + 0.55, 0.55
    n = 7
    for k in range(n):
        mat = colours[k % 2]
        sx = -w * 0.5 + (k + 0.5) * w / n
        pts = []
        for j in range(6):
            t = j / 5
            pts.append(o + Vector((sx, d - run * t, 2.55 - drop * t - 0.06 * math.sin(math.pi * t))))
        for j in range(5):
            a, b = pts[j], pts[j + 1]
            mid = (a + b) * 0.5
            L = (b - a).length
            ang = math.degrees(math.atan2(a.z - b.z, a.y - b.y))
            st.part("awnings", colours[0], sharp=60).box((w / n + 0.003, L + 0.01, 0.018), mid, (ang, 0, 0), bevel=0, mat=mat)
        edge = pts[-1]
        val = [(-w / n / 2, 0), (w / n / 2, 0), (w / n / 2, -0.16), (w / n * 0.25, -0.2), (0, -0.23), (-w / n * 0.25, -0.2), (-w / n / 2, -0.16)]
        st.part("awnings", colours[0]).prism(val, 0.016, edge + Vector((0, 0, 0)), bevel=0, mat=mat)
    wood.box((w * 0.55, 0.04, 0.3), o + Vector((0, d - 0.05, 2.72)), bevel=0.01)
    lantern(o + Vector((0, d * 0.45, 2.3)), AMBER if R.random() < 0.7 else RED_L)
    top = o + Vector((0, 0.25, 0.91))
    if goods_kind == "fish":
        st.part("crates_ice", HESS).box((w * 0.7, 0.42, 0.1), top + Vector((0, 0, 0.05)), bevel=0.02)
        for k in range(7):
            fish(top + Vector((-w * 0.3 + k * w * 0.1, R.uniform(-0.1, 0.1), 0.125)), (0, 90, 90 + R.uniform(-30, 30)), 0.9)
        for k in range(int(w / 0.28)):
            fx = -w * 0.5 + 0.2 + k * 0.28
            rope.tube([o + Vector((fx, -0.3, 1.98)), o + Vector((fx, -0.3, 1.72))], 0.005, 3, caps=False)
            fish(o + Vector((fx, -0.3, 1.62)), (0, 0, 90 + R.uniform(-20, 20)), 1.2)
        wood.box((w, 0.05, 0.05), o + Vector((0, -0.3, 2.0)), bevel=0.01)
    elif goods_kind == "produce":
        for k in range(3):
            bp = top + Vector((-w * 0.3 + k * w * 0.3, 0, 0))
            basket(bp, 0.24, 0.18)
            m = (BEET, GOURD, CLAY)[k]
            for j in range(8):
                a = math.tau * j / 8
                st.part("produce", m).add(S.geo_rock(j + k * 9, 1, 0.15, None), bp + Vector((math.cos(a) * 0.11, math.sin(a) * 0.11, 0.19)), scale=0.065, mat=m)
            st.part("produce", m).add(S.geo_rock(k * 5 + 3, 1, 0.15, None), bp + Vector((0, 0, 0.24)), scale=0.07, mat=m)
    elif goods_kind == "jars":
        for k in range(8):
            jp = top + Vector((-w * 0.38 + k * w * 0.11, R.uniform(-0.1, 0.1), 0.0))
            prof = [(0, 0), (0.045, 0), (0.055, 0.04), (0.055, 0.15), (0.035, 0.19), (0.03, 0.22), (0, 0.22)]
            if k % 3 == 1:
                st.part("jars_glow", JAR, shadows=False).lathe(prof, jp, segs=10)
            else:
                st.part("jars", CLAY, sharp=60).lathe(prof, jp, segs=10)
        wood.box((w * 0.9, 0.3, 0.04), o + Vector((0, d - 0.18, 1.5)), bevel=0.01)
        for k in range(6):
            prof = [(0, 0), (0.04, 0), (0.05, 0.05), (0.05, 0.16), (0.03, 0.22), (0, 0.22)]
            st.part("jars", CLAY).lathe(prof, o + Vector((-w * 0.35 + k * w * 0.14, d - 0.18, 1.52)), segs=10)
    elif goods_kind == "cloth":
        for k, m in enumerate((RED, TEAL, CREAM, PLUM)):
            st.part("bolts", m).add(S.geo_cyl(0.09, 0.55, 10, bevel=0.02, centred=True), top + Vector((-w * 0.3 + k * w * 0.2, 0, 0.09)), (90, 0, 15 * k), mat=m)
        net(o + Vector((0, d - 0.06, 2.0)), w * 0.8, 1.0)
        for k in range(3):
            st.part("leather", LEATHER, sharp=50).box((0.28, 0.08, 0.36), o + Vector((-w * 0.3 + k * w * 0.3, d - 0.08, 1.3)), bevel=0.03)


# ------------------------------------------------------------------ water, far bank, moon
st.part("water", BOG, floor=True, smooth=False).box((420, 320, 0.001), (0, 150, WATER_Z), bevel=0, drop=("-z", "-x", "+x", "-y", "+y"))
bank = st.part("far_bank", BANK, sharp=35)
for i in range(16):
    x = -130 + i * 17 + R.uniform(-5, 5)
    bank.rock((x, 100 + R.uniform(0, 18), WATER_Z - 3.0), (R.uniform(14, 22), R.uniform(6, 9), R.uniform(4.5, 7.0)), i + 1, subdiv=2, rough=0.25)
trees = st.part("dead_trees", ROOF, sharp=60)
for tx, ty, th in [(-34, 62, 7), (-21, 70, 6), (16, 58, 6.5), (41, 66, 8), (-48, 55, 5.5), (27, 75, 7.5), (58, 60, 6), (-62, 68, 7)]:
    base = Vector((tx, ty, WATER_Z))
    trees.tube([base, base + Vector((R.uniform(-0.4, 0.4), 0, th * 0.5)), base + Vector((R.uniform(-0.6, 0.6), 0, th))], [0.32, 0.2, 0.05], 6)
    for k in range(4):
        y_ = th * (0.45 + k * 0.12)
        sd = 1 if k % 2 == 0 else -1
        L = th * (0.35 - k * 0.05)
        p0 = base + Vector((0, 0, y_))
        trees.tube([p0, p0 + Vector((sd * L * 0.5, R.uniform(-0.5, 0.5), L * 0.4)), p0 + Vector((sd * L, R.uniform(-0.5, 0.5), L * 0.55))], [0.1, 0.05, 0.015], 5)
moon_dir = Vector((0.3, 1.0, 0.2)).normalized()
st.part("moon", MOON, shadows=False, smooth=False).add(S.geo_cyl(3.2, 0.01, 24), moon_dir * 240, (90, 0, math.degrees(math.atan2(moon_dir.x, moon_dir.y)) * -1))

# ------------------------------------------------------------------ the boardwalk
planks(deck, -15.0, 15.0, DECK_FRONT, DECK_BACK, 0.0)
dark.box((30.2, 0.16, 0.3), (0, DECK_BACK + 0.02, -0.2), bevel=0.02)
dark.box((30.2, 0.16, 0.3), (0, DECK_FRONT - 0.05, -0.2), bevel=0.02)
for k in range(8):
    dark.box((30.0, 0.12, 0.18), (0, DECK_FRONT + 0.5 + k * 0.95, -0.14), bevel=0.015)
for i in range(18):
    x = -14.85 + i * 1.75
    for y in (DECK_BACK + 0.05, DECK_FRONT - 0.05, -1.05):
        pile(x, y, -0.05)
for i in range(17):
    x = -14.85 + i * 1.75 + 0.875
    a = 28 if i % 2 == 0 else -28
    piles.box((1.95, 0.06, 0.09), (x, DECK_BACK + 0.12, WATER_Z + 0.85), (0, a, 0), bevel=0.01)
# mud and weed collars where the piles meet the water
for i in range(0, 18, 2):
    st.part("mud", MUD, sharp=30).rock((-14.85 + i * 1.75 + R.uniform(-0.3, 0.3), DECK_BACK + 0.3, WATER_Z - 0.18), (0.6, 0.4, 0.25), i + 40, subdiv=1, rough=0.3)

# rope rail: short posts every 1.8 m, tall lantern poles at 0, +-7.2, +-14.4
for i in range(17):
    x = -14.4 + i * 1.8
    tall = any(abs(x - px) < 0.01 for px in POLE_X)
    if tall:
        wood.box((0.16, 0.16, 3.5), (x, RAIL_Y, 1.75), bevel=0.02, segs=2)
        wood.box((0.8, 0.1, 0.1), (x, RAIL_Y, 3.32), bevel=0.015)
        for sx in (-1, 1):
            wood.box((0.06, 0.06, 0.45), (x + sx * 0.18, RAIL_Y, 3.1), (0, sx * 40, 0), bevel=0.01)
        caps.cyl(0.03, 0.12, (x, RAIL_Y, 3.5), segs=6)
    else:
        wood.box((0.12, 0.12, 1.15), (x, RAIL_Y, 0.575), bevel=0.02)
    wood.box((0.18, 0.18, 0.06), (x, RAIL_Y, 1.13), bevel=0.015)
    if i < 16:
        rope.rope((x, RAIL_Y, 1.04), (x + 1.8, RAIL_Y, 1.04), 0.14, 0.018, 8, 6)
        rope.rope((x, RAIL_Y, 0.56), (x + 1.8, RAIL_Y, 0.56), 0.1, 0.014, 8, 6)
    for z in (1.04, 0.56):
        rope.ring(0.075, 0.018, (x, RAIL_Y, z), (0, 0, 0), segs=10, rsegs=4)
net((-9.0, RAIL_Y + 0.06, 1.04), 2.2, 0.95)
net((4.5, RAIL_Y + 0.06, 1.04), 1.6, 0.8)
for i in range(len(POLE_X) - 1):
    lantern_string((POLE_X[i], RAIL_Y, 3.28), (POLE_X[i + 1], RAIL_Y, 3.28), 0.55)
for a, c in [(-7.2, -11.6), (0.0, -5.3), (7.2, 0.4)]:
    lantern_string((a, RAIL_Y, 3.28), (c, PIER_Y + 0.2, 2.7), 0.85)

# clutter on the boards, low and to the sides
basket((-4.6, 1.95, 0), 0.28, 0.35, True)
basket((-4.05, 2.1, 0), 0.22, 0.28)
basket((5.2, 2.0, 0), 0.3, 0.3, True)
coil((3.3, 2.05, 0))
coil((-5.6, -2.4, 0))
basket((-2.9, 2.05, 0), 0.2, 0.42, True)       # tall lidded creels where the traps stood
basket((6.1, 1.9, 0), 0.19, 0.38, True)
coil((6.75, 2.0, 0))
basket((-6.2, -2.9, 0), 0.25, 0.3, True)
basket((5.4, -3.3, 0), 0.22, 0.24)
st.prop("wooden_lantern_01", (-3.4, 1.75, 0.0), rot_z=20)
st.prop("wooden_bucket_01", (2.6, 2.05, 0.0), rot_z=-30)

# ------------------------------------------------------------------ the stall pier across the water
X0, X1 = -22.0, 1.6
planks(deck, X0, X1, PIER_Y, PIER_Y + PIER_D, 0.1, lmin=2.0, lmax=4.0)
dark.box((X1 - X0, 0.14, 0.26), ((X0 + X1) * 0.5, PIER_Y - 0.05, -0.08), bevel=0.02)
x = X0 + 0.6
while x < X1:
    for yy in (PIER_Y + 0.05, PIER_Y + PIER_D - 0.05):
        pile(x, yy, 0.05, 0.1)
    x += 1.9
STALLS = [(-19.6, 2.4, "jars", (OCHRE, PLUM)), (-16.6, 2.6, "fish", (RED, CREAM)), (-13.5, 2.5, "produce", (TEAL, CREAM)),
          (-10.4, 2.6, "cloth", (PLUM, CREAM)), (-7.2, 2.4, "fish", (OCHRE, RED)), (-4.0, 2.7, "produce", (RED, CREAM)), (-0.9, 2.2, "jars", (TEAL, OCHRE))]
for sx, w, kind, cols in STALLS:
    stall((sx, PIER_Y + 0.35, 0.1), w, 1.6, cols, kind)
# footbridge from the boardwalk to the pier, on the left
bx = -12.6
planks(deck, bx - 0.7, bx + 0.7, DECK_BACK, PIER_Y, -0.02, along="y", lmin=1.5, lmax=3.0)
span = PIER_Y - DECK_BACK
for sd in (-0.7, 0.7):
    for k in range(4):
        yy = DECK_BACK + k * span / 3.0
        wood.box((0.09, 0.09, 1.0), (bx + sd, yy, 0.45), bevel=0.012)
        pile(bx + sd, yy, -0.1, 0.09)
    rope.rope((bx + sd, DECK_BACK, 0.92), (bx + sd, PIER_Y, 0.92), 0.25, 0.016, 14, 6)
dark.box((1.5, span, 0.12), (bx, (DECK_BACK + PIER_Y) * 0.5, -0.13), bevel=0.01)

# the stranger's walk: raised, empty, lit by one pale lamp
WX0, WX1 = 3.4, 15.5
planks(deck, WX0, WX1, WALK_Y - 0.8, WALK_Y + 0.8, WALK_Z + 0.08, lmin=2.0, lmax=3.5)
x = WX0 + 0.4
while x < WX1:
    for yy in (WALK_Y - 0.75, WALK_Y + 0.75):
        pile(x, yy, WALK_Z, 0.1)
    wood.box((0.09, 0.09, 1.0), (x, WALK_Y + 0.75, WALK_Z + 0.58), bevel=0.012)
    if x + 1.8 < WX1:
        rope.rope((x, WALK_Y + 0.75, WALK_Z + 1.03), (x + 1.8, WALK_Y + 0.75, WALK_Z + 1.03), 0.12, 0.015, 8, 6)
    x += 1.8
for k in range(6):
    wood.box((0.5, 1.2, 0.08), (1.8 + k * 0.3, WALK_Y - 0.2, 0.1 + k * WALK_Z / 6.0), bevel=0.012)
planks(deck, 0.9, 2.3, PIER_Y + PIER_D, PIER_Y + PIER_D + 2.6, 0.1, along="y")
wood.box((0.12, 0.12, 3.2), (13.6, WALK_Y + 0.6, WALK_Z + 1.68), bevel=0.02)
wood.box((0.6, 0.07, 0.07), (13.35, WALK_Y + 0.6, WALK_Z + 3.23), bevel=0.01)
caps.cyl(0.006, 0.18, (13.1, WALK_Y + 0.6, WALK_Z + 3.05), segs=4)
st.part("pale_lamp", PALE, shadows=False).lathe([(0, -0.17), (0.1, -0.12), (0.14, 0.0), (0.1, 0.12), (0, 0.17)], (13.1, WALK_Y + 0.6, WALK_Z + 2.85), segs=12)
caps.cyl(0.06, 0.04, (13.1, WALK_Y + 0.6, WALK_Z + 3.0), segs=8)

# ------------------------------------------------------------------ the ends: stalls, crates, barrels
stall((-8.95, -0.35, 0.0), 2.6, 1.45, (OCHRE, CREAM), "produce")
stall((8.95, -0.35, 0.0), 2.6, 1.45, (TEAL, CREAM), "fish")
st.prop("wooden_crate_02", (-7.95, 1.75, 0.0), rot_z=88)
st.prop("wooden_crate_02", (-8.0, 1.7, 0.455), rot_z=95)
for bx_, by_, bz_, s_, yaw in [(-7.98, -0.68, 0.0, 0.5, -8.0), (-8.1, 0.45, 0.0, 0.46, 20.0)]:
    wood.box((s_, s_, s_), (bx_, by_, bz_ + s_ * 0.5), (0, 0, yaw), bevel=0.02)
    for e in (-0.4, 0.4):
        dark.box((s_ + 0.02, s_ + 0.02, 0.05), (bx_, by_, bz_ + s_ * 0.5 + e * s_), (0, 0, yaw), bevel=0.01)
st.prop("wine_barrel_01", (8.2, 1.65, 0.0), rot_z=30)
for bx_, by_ in [(7.95, 1.0), (8.0, 0.35)]:
    goods.lathe([(0, 0), (0.27, 0), (0.3, 0.06), (0.32, 0.43), (0.3, 0.8), (0.27, 0.86), (0, 0.86)], (bx_, by_, 0), segs=14)
    for z in (0.12, 0.74):
        caps.lathe([(0.305, z - 0.03), (0.318, z - 0.02), (0.318, z + 0.02), (0.305, z + 0.03)], (bx_, by_, 0), segs=14, cap_bottom=False, cap_top=False)
net((8.95, 1.0, 1.95), 1.4, 1.2)

# ------------------------------------------------------------------ stilt houses across the bog
walls_h = st.part("houses", PLANK_D, sharp=40)
roofs = st.part("roofs", ROOF, sharp=40)
wins = st.part("windows", WIN, shadows=False)
for hx, hy in [(-44, 52), (-33, 40), (-25, 47), (-16, 36), (-9, 50), (-1, 42), (7, 54), (12, 38), (20, 46), (28, 36), (35, 50),
               (46, 42), (-52, 38), (55, 56), (-60, 50), (64, 44)]:
    x = hx + R.uniform(-2, 2)
    y = hy
    w, h, d = R.uniform(3.0, 5.5), R.uniform(2.2, 3.4), R.uniform(2.6, 3.6)
    legs = R.uniform(1.6, 2.6)
    base = WATER_Z + legs
    tilt = R.uniform(-3, 3)
    for lx in (-0.42, 0.42):
        for ly in (-0.4, 0.4):
            piles.cyl(0.12, legs + 0.4, (x + lx * w, y + ly * d, WATER_Z - 0.3), segs=6)
    walls_h.box((w + 0.8, d + 0.8, 0.15), (x, y, base), bevel=0.02)
    walls_h.box((w, d, h), (x, y, base + h * 0.5), (0, tilt, 0), bevel=0.03)
    rr = max(w, d) * 0.62
    if R.random() < 0.5:
        roofs.prism([(-d / 2 - 0.35, 0), (d / 2 + 0.35, 0), (0, rr * 0.75)], w + 0.5, (x, y, base + h - 0.05), (0, 0, 90), axis="y", bevel=0.03)
    else:
        roofs.prism([(-w / 2 - 0.35, 0), (w / 2 + 0.35, 0), (0, rr * 0.75)], d + 0.5, (x, y, base + h - 0.05), axis="y", bevel=0.03)
    walls_h.box((0.45, 0.45, 1.4), (x + w * 0.3, y - 0.3, base + h + 0.7), bevel=0.03)
    for k in range(R.randint(1, 3)):
        wx = x - w * 0.35 + k * w * 0.35
        wz = base + h * 0.5 + R.uniform(-0.2, 0.3)
        if R.random() < 0.72:
            wins.box((0.5, 0.06, 0.65), (wx, y - d * 0.5 - 0.02, wz), bevel=0)
        else:
            dark.box((0.5, 0.06, 0.65), (wx, y - d * 0.5 - 0.02, wz), bevel=0)
    walls_h.box((w * 0.5, 0.9, 0.08), (x, y - d * 0.5 - 0.45, base - 0.02), bevel=0.01)
    if R.random() < 0.45:
        lantern((x + w * 0.5 + 0.2, y - d * 0.5, base + 1.9), AMBER, 1.4)

# ------------------------------------------------------------------ reeds
reeds = st.part("reeds", REED, shadows=False, sharp=80)
def reed_clump(p, s):
    p = Vector(p)
    rr = S.random.Random(int(p.x * 13 + p.y * 7))
    for k in range(7):
        a = rr.uniform(0, math.tau)
        base = p + Vector((math.cos(a), math.sin(a), 0)) * rr.uniform(0, 0.22) * s
        hgt = rr.uniform(1.1, 2.3) * s
        lean = Vector((math.cos(a), math.sin(a), 0)) * rr.uniform(0.1, 0.45) * s
        side = Vector((-math.sin(a + 1.2), math.cos(a + 1.2), 0)) * 0.022 * s
        g = S.Geo()
        prev = [g.vert(base - side), g.vert(base + side)]
        for sgm in range(1, 5):
            t = sgm / 4
            c = base + Vector((0, 0, hgt * t)) + lean * t * t
            wv = side * (1.0 - t * 0.9)
            cur = [g.vert(c - wv), g.vert(c + wv)]
            g.face([prev[0], prev[1], cur[1], cur[0]])
            prev = cur
        reeds.add(g, uv="box")
        if k % 3 == 0:
            top = base + Vector((0, 0, hgt * 0.8)) + lean * 0.64
            reeds.add(S.geo_cyl(0.032 * s, 0.2 * s, 5), top, mat=MUD)
for i in range(70):
    p = (R.uniform(-16, 16), R.uniform(DECK_BACK + 0.4, PIER_Y - 0.3), WATER_Z)
    if abs(p[0] + 12.6) < 1.2:
        continue
    reed_clump(p, R.uniform(0.7, 1.15))
for i in range(40):
    reed_clump((R.uniform(-24, 24), R.uniform(PIER_Y + PIER_D + 0.3, 22.0), WATER_Z), R.uniform(0.8, 1.3))
for i in range(40):
    reed_clump((R.uniform(-60, 60), R.uniform(24.0, 60.0), WATER_Z), R.uniform(1.0, 1.8))

# ------------------------------------------------------------------ light
st.set_fog((0.1, 0.125, 0.135), 0.02, 0.25, base_z=WATER_Z,
           note="heavy low mist over the water (height fog), thinner above the boardwalk; volumetric fog for lantern halos")
# sky_rot turns the HDRI so the open-lake stretch (u ~0.36) faces the camera: Blender's equirect u for
# a view direction is 0.25 - rot/360 at +Y; the shore trees and the dead tree baked into the HDRI
# stay out of the gameplay and wide frames
st.set_ambient((0.32, 0.5, 0.6), 0.3, sky="lakeside_night", sky_strength=0.2, sky_rot=-40)
st.light("key_warm", "directional", (-6, -6, 10), (1.0, 0.84, 0.68), 2.6, direction=(0.429, 0.686, -0.588), shadows=True, source_radius=2.0,
         note="warm key from the upper left front")
st.light("rim_moon", "directional", (6, 30, 10), (0.6, 0.76, 1.0), 3.0, direction=(-0.441, -0.765, -0.469), affects="not_floor", specular=1.5,
         note="cool moonlit rim from behind right")
st.light("fill_camera", "directional", (0, -10, 3), (0.75, 0.82, 0.9), 1.4, direction=(-0.136, 0.968, -0.208), affects="fighters_only", specular=0.25)
for i, lx in enumerate((-10.8, -3.6, 3.6, 10.8)):
    st.light(f"lantern_string_{i + 1}", "point", (lx, RAIL_Y - 0.25, 2.8), (1.0, 0.58, 0.28), 20.0, radius=7.0, shadows=(i == 2),
             flicker=0.08, flicker_speed=3.0, source_radius=0.3, specular=0.4, note="signature: strings of amber lanterns")
for i, lx in enumerate((-15.0, -8.0, -2.0)):
    st.light(f"pier_stalls_{i + 1}", "point", (lx, PIER_Y + 0.4, 2.0), (1.0, 0.56, 0.26), 18.0, radius=6.0, flicker=0.1, flicker_speed=4.0,
             source_radius=0.3, specular=0.4)
for i, sd in enumerate((-1, 1)):
    st.light(f"end_stall_{i + 1}", "point", (sd * 8.95, 0.3, 1.9), (1.0, 0.58, 0.28), 14.0, radius=4.5, flicker=0.1, flicker_speed=4.0,
             source_radius=0.2, specular=0.3)
st.light("set_down_lantern", "point", (-3.4, 1.75, 0.3), (1.0, 0.62, 0.3), 3.0, radius=2.2, flicker=0.15, flicker_speed=9.0, source_radius=0.05)
st.light("stranger_lamp", "point", (13.1, WALK_Y + 0.4, WALK_Z + 2.7), (0.72, 0.78, 1.0), 16.0, radius=6.0, flicker=0.04, flicker_speed=1.5,
         source_radius=0.12, note="the pale lamp over the empty walk")

st.animated("stilt_market_lanterns", "sway", 4.0, 1.1,
            note="many lanterns in one mesh: sway in the material (WPO) about each lantern's hook, phase by world position; hook = top of each lantern")
st.fx += [
    {"kind": "motes", "note": "fireflies / bog lights over the water", "box_center": [0, 14.0, WATER_Z + 1.0], "box_extent": [20, 9, 1.0],
     "count": 40, "color": [0.7, 1.0, 0.4, 0.9], "size_m": 0.06},
    {"kind": "mist", "center": [0, 8.0, WATER_Z + 0.35], "size": [60, 14], "color": [0.26, 0.31, 0.34], "density": 0.35},
    {"kind": "mist", "center": [0, 24.0, WATER_Z + 0.5], "size": [90, 20], "color": [0.24, 0.29, 0.32], "density": 0.45},
    {"kind": "mist", "center": [0, 50.0, WATER_Z + 0.8], "size": [160, 30], "color": [0.22, 0.27, 0.3], "density": 0.55},
]
st.notes += [
    "Tall rail poles at X 0, +-7.2, +-14.4 (the +-7.2 pair marks the ends of the floor); end stalls start at X +-7.65.",
    "Lanterns are one merged object for the draw count; the strings are in stilt_market_ropes.",
    "The raised walkway on the right (X 3.4-15.5, Y 15.5, Z 1.25) is kept empty for the stranger.",
]

st.finish()
st.facing_report()
if A["render"]:
    # review-only low mist over the bog
    st.preview_volume((0, 30, WATER_Z - 0.3), 40.0, 1.8, (0.55, 0.62, 0.68), 0.14, 0.35)
    st.render_reviews(samples=24 if A["quick"] else 64)
if A["export"]:
    path, rows, tris = st.export()
    for r_ in rows:
        print("OBJ", r_[0], r_[1], r_[2])
    print("TRIS", tris, "OBJECTS", len(rows))
