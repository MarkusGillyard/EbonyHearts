"""The Drowned Chapel: Lowmere's sunken church, Mirela Sorrow's home. The fight crosses the nave
near its west end; the camera looks down the flooded nave between two arcades of clustered piers
to the altar and a great rose window full of moonlight. Ankle-deep water over stone flags, broken
pews, hundreds of candles on ledges, racks, the altar steps and little floating boards, censers on
long chains, tattered banners.

Composition from scripts/view/arenas/chapel.gd (Godot (x, y, z) -> Blender (x, -z, y)), shifted
0.8 m back so the first piers clear the walkable strip; their inner faces mark X = +-7.2.
Run: Tools/blender/stages/run.sh build_drowned_chapel.py [-- --no-render | --quick]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402
from mathutils import Vector  # noqa: E402

A = S.args()
st = S.Stage("drowned_chapel", "The Drowned Chapel", seed=7, ev100=1.0)
R = st.rng
st.music = "music_chapel"
st.footstep = "stone"

COL_X = 7.8
PIERS = [1.7 + 4.4 * k for k in range(8)]
BACK_Y = 32.6
SPRING = 6.2
WALL_TOP = 10.2
AISLE_X = 11.6
ROSE_Z = 6.7
ROSE_R = 3.0
WATER_Z = 0.035

STONE = st.surface("medieval_blocks_03", tint=(0.4, 0.43, 0.48), rough=1.0)
MOSS = st.surface("mossy_stone_wall", tint=(0.36, 0.4, 0.38), rough=0.9)
FLAGS = st.surface("monastery_stone_floor", tint=(0.4, 0.42, 0.45), rough=0.35)
WOOD = st.surface("Wood051", tint=(0.62, 0.48, 0.36), rough=1.0)
WAX = st.surface("Ivory001B__wax", tint=(0.8, 0.72, 0.56), rough=0.6, emissive=((1.0, 0.55, 0.25), 0.35))
IRON = st.surface("Metal055A", tint=(0.22, 0.21, 0.2), rough=1.0, metal=1.0)
BRASS = st.surface("Metal042A", tint=(0.78, 0.55, 0.28), rough=1.4, metal=1.0)
PALE = st.surface("Fabric001__banner", tint=(0.62, 0.58, 0.5), two_sided=True)
DARK = st.surface("Fabric066__banner", tint=(0.12, 0.1, 0.1), two_sided=True)
CLOTH = st.surface("Fabric001", tint=(0.72, 0.68, 0.58))
FLAME = st.glow("GLOW_candle", (1.0, 0.5, 0.14), 9.0, "candle flames: flicker, a soft additive halo helps")
ROSE_B = st.glow("GLOW_rose_blue", (0.22, 0.4, 1.0), 3.0, "rose window glass, moonlit", pattern=0.35, scale=6.0)
ROSE_V = st.glow("GLOW_rose_violet", (0.45, 0.28, 0.95), 2.4, "rose window glass", pattern=0.35, scale=6.0)
ROSE_R_ = st.glow("GLOW_rose_red", (0.95, 0.16, 0.12), 2.0, "rose window glass", pattern=0.35, scale=6.0)
ROSE_G = st.glow("GLOW_rose_gold", (1.0, 0.72, 0.32), 2.4, "rose window hub")
LANCET = st.glow("GLOW_lancet", (0.38, 0.5, 0.95), 1.5, "lancets beside the altar and the aisle windows", pattern=0.35, scale=5.0)
EMBER = st.glow("GLOW_ember", (1.0, 0.42, 0.1), 4.0, "censer embers")
FLOOD = st.water("WATER_flood", (0.03, 0.05, 0.06), 0.04, 0.35,
                 "a 3.5 cm sheet over the flags: clear, reflective, ripples and drip rings; no collision")


def candle(base, h, r, lit=True, segs=6):
    """Wax (with a melted dip at the top) and a teardrop flame at the wick."""
    base = Vector(base)
    wax.lathe([(r, 0), (r * 1.02, h * 0.6), (r, h), (r * 0.45, h * 0.97), (0, h * 0.95)], base, (0, 0, R.uniform(0, 60)), segs=segs,
              cap_bottom=False)
    if lit:
        k = max(0.75, min(1.5, r / 0.028)) * R.uniform(0.85, 1.15)
        flame.lathe([(0, 0), (0.0095 * k, 0.012 * k), (0.006 * k, 0.03 * k), (0, 0.046 * k)], base + Vector((0, 0, h + 0.004)),
                    segs=4, cap_bottom=False, cap_top=False)


def pew(at, yaw=0.0, tilt=(0.0, 0.0), L=3.4):
    M = S.xform(at, (tilt[0], tilt[1], yaw))
    def put(g, lp, lr=(0, 0, 0)):
        pews.add(g, m=M @ S.xform(lp, lr), uv="box")
    put(S.geo_box(L, 0.42, 0.06, bevel=0.012), (0, 0.0, 0.45))
    put(S.geo_box(L, 0.05, 0.5, bevel=0.01), (0, -0.2, 0.74))
    put(S.geo_box(L + 0.04, 0.1, 0.06, bevel=0.022, segs=2), (0, -0.2, 1.01))
    put(S.geo_box(L, 0.12, 0.05, bevel=0.01), (0, 0.3, 0.16))
    put(S.geo_box(L, 0.1, 0.025, bevel=0.008), (0, -0.27, 0.86))
    end = [(-0.27, 0), (0.27, 0), (0.27, 0.48), (0.2, 0.52), (-0.12, 0.55), (-0.16, 0.62), (-0.16, 1.0),
           (-0.21, 1.1), (-0.27, 1.12), (-0.31, 1.06), (-0.31, 0.56), (-0.27, 0.5)]
    for sx in (-0.5, 0.5):
        put(S.geo_prism(end, 0.07, bevel=0.012, axis="x"), (sx * L, 0, 0))


def blocks_chunk(loc, size, seed):
    """A fallen dressed stone: a bevelled block, tumbled."""
    rr = S.random.Random(seed)
    rubble.box(size, loc, (rr.uniform(-25, 25), rr.uniform(-25, 25), rr.uniform(0, 90)), bevel=0.03)


wax = st.part("candles_wax", WAX, sharp=60)
flame = st.part("candle_flames", FLAME, shadows=False, smooth=True)
pews = st.part("pews", WOOD, sharp=50)
rubble = st.part("rubble", STONE, sharp=40)

# ------------------------------------------------------------------ floor and water
floor = st.part("floor", FLAGS, floor=True)
floor.box((26, 46, 0.3), (0, 12.0, -0.15), bevel=0)
water = st.part("water", FLOOD, floor=True, smooth=False, shadows=False, surface_z=WATER_Z + 0.001)
water.box((25.0, 45.0, 0.001), (0, 11.7, WATER_Z), bevel=0, drop=("-z", "-x", "+x", "-y", "+y"))
# a few flags lifted and cracked off the strip, by the piers and walls
for k, (fx_, fy_) in enumerate([(-6.6, 3.2), (6.4, 4.6), (-5.2, 9.0), (6.9, 13.4), (-6.8, 18.6), (2.2, 22.5), (-1.5, 27.0),
                                 (9.6, 6.0), (-9.8, 14.0), (10.1, 22.0), (-4.6, -2.4), (5.6, -3.1)]):
    floor.box((R.uniform(0.7, 1.1), R.uniform(0.6, 0.9), 0.08), (fx_, fy_, -0.01), (R.uniform(-6, 6), R.uniform(-6, 6), R.uniform(0, 90)), bevel=0.02)

# ------------------------------------------------------------------ the arcades
piers = st.part("piers", STONE, sharp=45)
walls = st.part("arcade_walls", STONE, sharp=40)
for side in (-1, 1):
    x = side * COL_X
    for y in PIERS:
        piers.box((1.2, 1.2, 0.5), (x, y, 0.25), bevel=0.04, segs=2)
        piers.lathe([(0.6, 0.5), (0.6, 0.56), (0.52, 0.62), (0.5, 0.7), (0.42, 0.76), (0.38, 0.8)], (x, y, 0), segs=16,
                    cap_bottom=False, cap_top=False)
        piers.cyl(0.38, SPRING - 0.4 - 0.8, (x, y, 0.8), segs=16, caps=(False, False))
        for ox, oy in ((0.38, 0), (-0.38, 0), (0, 0.38), (0, -0.38)):
            piers.cyl(0.13, SPRING - 0.4 - 0.62, (x + ox, y + oy, 0.62), segs=10, caps=(False, False))
            piers.lathe([(0.17, 0.56), (0.17, 0.6), (0.14, 0.66), (0.13, 0.7)], (x + ox, y + oy, 0), segs=10, cap_bottom=False, cap_top=False)
            piers.lathe([(0.13, SPRING - 0.42), (0.16, SPRING - 0.36), (0.16, SPRING - 0.32)], (x + ox, y + oy, 0), segs=10,
                        cap_bottom=False, cap_top=False)
        piers.lathe([(0.38, SPRING - 0.42), (0.44, SPRING - 0.36), (0.5, SPRING - 0.3), (0.55, SPRING - 0.26), (0.55, SPRING - 0.22)],
                    (x, y, 0), segs=16, cap_bottom=False, cap_top=False)
        piers.box((1.2, 1.2, 0.26), (x, y, SPRING - 0.1), bevel=0.03, segs=2)
        # moss and tide line low on the plinth
        st.part("damp", MOSS, sharp=40).box((1.26, 1.26, 0.22), (x, y, 0.08), bevel=0.03)
    for i in range(len(PIERS) - 1):
        y0, y1 = PIERS[i], PIERS[i + 1]
        bay = y1 - y0
        span = bay - 1.2
        yc = (y0 + y1) * 0.5
        arch = S.arch_points(-span / 2, span / 2, SPRING, "pointed", 12)
        walls.band([(p[0], WALL_TOP) for p in arch], arch, 0.7, (x, yc, 0), (0, 0, 90))
        ring = S.arch_points(-span / 2 - 0.22, span / 2 + 0.22, SPRING, "pointed", 12)
        walls.band(ring, arch, 0.9, (x, yc, 0), (0, 0, 90))
        hood = S.arch_points(-span / 2 - 0.34, span / 2 + 0.34, SPRING, "pointed", 12)
        walls.band(hood, ring, 0.8, (x, yc, 0), (0, 0, 90))
    for y in PIERS:
        walls.box((0.7, 1.2, WALL_TOP - SPRING), (x, y, (SPRING + WALL_TOP) * 0.5), bevel=0)
    walls.box((0.95, PIERS[-1] - PIERS[0] + 1.2, 0.3), (x, (PIERS[0] + PIERS[-1]) * 0.5, WALL_TOP + 0.15), bevel=0.04)
    walls.box((0.7, PIERS[-1] - PIERS[0] + 1.2, 4.2), (x, (PIERS[0] + PIERS[-1]) * 0.5, WALL_TOP + 2.4), bevel=0)
    walls.box((0.9, PIERS[-1] - PIERS[0] + 1.2, 0.22), (x, (PIERS[0] + PIERS[-1]) * 0.5, WALL_TOP + 4.5), bevel=0.03)
    # wall shafts rising from the capitals to the clerestory
    for y in PIERS:
        piers.cyl(0.12, WALL_TOP + 4.0 - SPRING, (x - side * 0.4, y, SPRING), segs=8, caps=(False, True))

# tie beams across the nave: where the censers hang from (out of frame)
for y in PIERS[1:6]:
    pews.box((2 * COL_X, 0.4, 0.45), (0, y, 11.2), bevel=0.03)

# ------------------------------------------------------------------ the aisles
aisle = st.part("aisle_walls", MOSS, sharp=40)
ledges = st.part("ledges", STONE, sharp=40)
lan = st.part("lancets", LANCET, shadows=False)
for side in (-1, 1):
    x = side * AISLE_X
    aisle.box((0.8, 34.6, 8.0), (x + side * 0.4, 16.3, 4.0), bevel=0)
    walls.box((4.6, 34.6, 0.4), (side * (COL_X + 2.1), 16.3, 7.8), bevel=0.03)
    ledges.box((0.36, 33.0, 0.12), (x - side * 0.18, 16.5, 1.3), bevel=0.025)
    ledges.box((0.2, 33.0, 0.22), (x - side * 0.1, 16.5, 1.13), bevel=0.02)
    for i in range(len(PIERS) - 1):
        yc = (PIERS[i] + PIERS[i + 1]) * 0.5
        warch = S.arch_points(-0.5, 0.5, 4.6, "pointed", 8)
        poly = [(-0.5, 2.5)] + warch + [(0.5, 2.5)]
        lan.prism(poly, 0.02, (x - side * 0.01, yc, 0), (0, 0, 90))
        frame_o = S.arch_points(-0.66, 0.66, 4.6, "pointed", 8)
        ledges.band(frame_o, warch, 0.12, (x - side * 0.06, yc, 0), (0, 0, 90))
        for sx in (-1, 1):
            ledges.box((0.12, 0.16, 2.1), (x - side * 0.06, yc + sx * 0.58, 3.55), bevel=0.02)
        ledges.box((0.3, 1.3, 0.12), (x - side * 0.12, yc, 2.45), bevel=0.02)
        ledges.box((0.1, 0.06, 2.5), (x - side * 0.04, yc, 3.6), bevel=0.01)
        # candles along the ledge below each window, unevenly spaced, some burned out
        yy = yc - 2.0
        while yy < yc + 2.0:
            candle((x - side * 0.2 + R.uniform(-0.06, 0.06), yy, 1.36), R.uniform(0.06, 0.3), R.uniform(0.02, 0.035), R.random() < 0.88, segs=5)
            yy += R.uniform(0.18, 0.45)

# ------------------------------------------------------------------ the east end: rose window, lancets, altar
east = st.part("east_wall", STONE, sharp=40)
tr = st.part("tracery", STONE, sharp=50)
EY = BACK_Y + 0.5
N = 48
circ = [(math.cos(math.tau * k / N) * ROSE_R, ROSE_Z + math.sin(math.tau * k / N) * ROSE_R) for k in range(N)]
def sq(a, h):
    c, s_ = math.cos(a), math.sin(a)
    t = h / max(abs(c), abs(s_))
    return (c * t, ROSE_Z + s_ * t)
east.band([sq(math.tau * k / N, 3.5) for k in range(N)], circ, 1.0, (0, EY, 0), closed=True)
east.box((7.0, 1.0, ROSE_Z - 3.5), (0, EY, (ROSE_Z - 3.5) * 0.5), bevel=0)
east.box((7.0, 1.0, 16 - ROSE_Z - 3.5), (0, EY, (16 + ROSE_Z + 3.5) * 0.5), bevel=0)
LX, LW, LZ0, LZ1 = 4.6, 1.3, 1.5, 4.6
for side in (-1, 1):
    cx = side * LX
    east.box((LX - LW / 2 - 3.5, 1.0, 16), (side * (3.5 + (LX - LW / 2 - 3.5) * 0.5), EY, 8), bevel=0)
    east.box((13.5 - LX - LW / 2, 1.0, 16), (side * (LX + LW / 2 + (13.5 - LX - LW / 2) * 0.5), EY, 8), bevel=0)
    east.box((LW, 1.0, LZ0), (cx, EY, LZ0 * 0.5), bevel=0)
    la = S.arch_points(-LW / 2, LW / 2, LZ1, "pointed", 10)
    east.band([(p[0], 6.0) for p in la], la, 1.0, (cx, EY, 0))
    east.box((LW, 1.0, 10.0), (cx, EY, 11.0), bevel=0)
    poly = [(-LW / 2, LZ0)] + la + [(LW / 2, LZ0)]
    lan.prism(poly, 0.02, (cx, BACK_Y + 0.45, 0))
    tr.band(S.arch_points(-LW / 2 - 0.16, LW / 2 + 0.16, LZ1, "pointed", 10), la, 0.3, (cx, BACK_Y - 0.05, 0))
    for sx in (-1, 1):
        tr.box((0.16, 0.3, LZ1 - LZ0 + 0.1), (cx + sx * (LW / 2 + 0.08), BACK_Y - 0.05, (LZ0 + LZ1) * 0.5), bevel=0.02)
    tr.box((0.08, 0.14, 4.3), (cx, BACK_Y + 0.38, LZ0 + 2.1), bevel=0.01)
    for zz in (2.4, 3.4):
        tr.box((LW, 0.14, 0.07), (cx, BACK_Y + 0.38, zz), bevel=0.01)
    tr.box((LW + 0.5, 0.4, 0.16), (cx, BACK_Y - 0.1, LZ0 - 0.08), bevel=0.03)
# rose tracery: roll and chamfer mouldings, 12 primary spokes to the hub, 12 secondary ones from
# the middle ring, foils in every light; coloured glass cells behind in a designed rhythm
def ring_pts(r, n=N):
    return [(math.cos(math.tau * k / n) * r, ROSE_Z + math.sin(math.tau * k / n) * r) for k in range(n)]
tr.band(ring_pts(3.6, 64), ring_pts(3.36, 64), 0.22, (0, BACK_Y - 0.1, 0), closed=True)
tr.band(ring_pts(3.36, 64), ring_pts(ROSE_R - 0.02, 64), 0.5, (0, BACK_Y - 0.06, 0), closed=True)
tr.band(ring_pts(1.8, 48), ring_pts(1.7, 48), 0.2, (0, BACK_Y + 0.2, 0), closed=True)
tr.band(ring_pts(0.62, 24), ring_pts(0.5, 24), 0.24, (0, BACK_Y + 0.2, 0), closed=True)
NS = 24
for k in range(NS):
    a = math.tau * k / NS
    r0 = 0.6 if k % 2 == 0 else 1.75
    rmid = (r0 + ROSE_R) * 0.5
    tr.box((ROSE_R - r0 + 0.04, 0.18 if k % 2 == 0 else 0.15, 0.075 if k % 2 == 0 else 0.055),
           (math.cos(a) * rmid, BACK_Y + 0.2, ROSE_Z + math.sin(a) * rmid), (0, -math.degrees(a), 0), bevel=0.012)
    af = a + math.pi / NS
    tr.ring(0.2, 0.028, (math.cos(af) * 2.55, BACK_Y + 0.2, ROSE_Z + math.sin(af) * 2.55), (90, 0, 0), segs=12, rsegs=4)
    # a pointed cusp closing the light at the rim
    cx_, cz_ = math.cos(af) * 2.88, math.sin(af) * 2.88
    tr.box((0.05, 0.12, 0.26), (cx_, BACK_Y + 0.2, ROSE_Z + cz_), (0, -math.degrees(af) + 90, 0), bevel=0.01)
for k in range(12):
    af = math.tau * k / 12 + math.pi / 12
    tr.ring(0.24, 0.03, (math.cos(af) * 1.2, BACK_Y + 0.2, ROSE_Z + math.sin(af) * 1.2), (90, 0, 0), segs=12, rsegs=4)
# hub quatrefoil
for k in range(4):
    a = math.tau * k / 4 + math.pi / 4
    tr.ring(0.17, 0.025, (math.cos(a) * 0.2, BACK_Y + 0.22, ROSE_Z + math.sin(a) * 0.2), (90, 0, 0), segs=12, rsegs=4)

def cell(a0, a1, r0, r1, mat, steps=3):
    g = S.Geo()
    inner, outer = [], []
    for j in range(steps + 1):
        a = a0 + (a1 - a0) * j / steps
        inner.append(g.vert((math.cos(a) * r0, 0, ROSE_Z + math.sin(a) * r0)))
        outer.append(g.vert((math.cos(a) * r1, 0, ROSE_Z + math.sin(a) * r1)))
    for j in range(steps):
        g.face([inner[j], inner[j + 1], outer[j + 1], outer[j]], expect=(0, -1, 0))
    st.part("rose_glass", ROSE_B, shadows=False).add(g, (0, BACK_Y + 0.36, 0), mat=mat)
for k in range(NS):
    a0, a1 = math.tau * k / NS, math.tau * (k + 1) / NS
    cell(a0, a1, 1.75, 2.35, ROSE_R_ if k % 3 == 0 else (ROSE_V if k % 3 == 1 else ROSE_B))
    cell(a0, a1, 2.35, ROSE_R + 0.05, ROSE_B if k % 2 == 0 else ROSE_V)
for k in range(12):
    a0, a1 = math.tau * k / 12, math.tau * (k + 1) / 12
    cell(a0, a1, 0.5, 1.12, ROSE_V if k % 2 else ROSE_B, 4)
    cell(a0, a1, 1.12, 1.75, ROSE_G if k % 4 == 0 else (ROSE_R_ if k % 4 == 2 else ROSE_B), 4)
hub = S.Geo()
c0 = hub.vert((0, 0, ROSE_Z))
rim = [hub.vert((math.cos(math.tau * k / 24) * 0.52, 0, ROSE_Z + math.sin(math.tau * k / 24) * 0.52)) for k in range(24)]
for k in range(24):
    hub.face([c0, rim[k], rim[(k + 1) % 24]], expect=(0, -1, 0))
st.part("rose_glass", ROSE_B).add(hub, (0, BACK_Y + 0.36, 0), mat=ROSE_G)

# the altar on a two-step dais, an ivory cloth over it, candles
dais = st.part("dais", STONE, sharp=40)
dais.box((8.0, 3.6, 0.2), (0, BACK_Y - 1.8, 0.1), bevel=0.03, segs=2)
dais.box((5.6, 2.4, 0.2), (0, BACK_Y - 1.2, 0.3), bevel=0.03, segs=2)
dais.box((2.8, 1.0, 0.85), (0, BACK_Y - 0.9, 0.4 + 0.425), bevel=0.02)
dais.box((3.0, 1.15, 0.12), (0, BACK_Y - 0.9, 1.31), bevel=0.03, segs=2)
dais.box((2.9, 1.08, 0.12), (0, BACK_Y - 0.9, 0.46), bevel=0.03)
cloth = st.part("altar_cloth", CLOTH, sharp=70)
g = S.Geo()
cols_n, rows_n = 16, 7
grid = []
for i in range(cols_n + 1):
    u = -1.55 + 3.1 * i / cols_n
    col = []
    for j in range(rows_n + 1):
        t = j / rows_n
        z = 1.375 - t * 0.78
        y = BACK_Y - 0.9 - 0.6 - 0.02 * math.sin(i * 1.3) - 0.035 * t * math.sin(i * 2.1 + 0.5)
        if j == 0:
            y = BACK_Y - 0.9 - 0.58
        col.append(g.vert((u, y, z)))
    grid.append(col)
for i in range(cols_n):
    for j in range(rows_n):
        g.face([grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]], expect=(0, -1, 0))
cloth.add(g, uv="box")
cloth.box((3.12, 1.2, 0.02), (0, BACK_Y - 0.9, 1.38), bevel=0.008)
brass = st.part("brass", BRASS, sharp=50)
for i in range(7):
    x = -1.2 + i * 0.4
    hh = 0.32 + 0.12 * (1.0 - abs(i - 3) / 3.0)
    brass.lathe([(0.07, 0), (0.06, 0.02), (0.018, 0.05), (0.022, 0.12), (0.014, 0.16), (0.04, 0.19), (0.035, 0.21), (0, 0.21)],
                (x, BACK_Y - 0.75, 1.39), segs=10, cap_bottom=False)
    candle((x, BACK_Y - 0.75, 1.6), hh, 0.03)
for row in ((BACK_Y - 3.45, 0.2, 7.4, 34), (BACK_Y - 2.35, 0.4, 5.2, 26)):
    for i in range(row[3]):
        x = -row[2] * 0.5 + (i + R.uniform(0.1, 0.9)) * row[2] / row[3]
        candle((x, row[0] + R.uniform(-0.12, 0.12), row[1]), R.uniform(0.05, 0.18), R.uniform(0.02, 0.032), R.random() < 0.9)
iron = st.part("ironwork", IRON, sharp=50)
for x in (-2.7, 2.7):
    iron.cyl(0.03, 1.7, (x, BACK_Y - 2.8, 0.2), segs=8)
    iron.lathe([(0.22, 0), (0.2, 0.03), (0.05, 0.08), (0.03, 0.12)], (x, BACK_Y - 2.8, 0.2), segs=12, cap_bottom=False, cap_top=False)
    iron.box((0.9, 0.04, 0.04), (x, BACK_Y - 2.8, 1.9), bevel=0.01)
    for k in range(5):
        iron.cyl(0.035, 0.03, (x - 0.4 + k * 0.2, BACK_Y - 2.8, 1.92 + (0.12 if k == 2 else 0.0)), segs=8)
        candle((x - 0.4 + k * 0.2, BACK_Y - 2.8, 1.95 + (0.12 if k == 2 else 0.0)), 0.22, 0.03)
        if k == 2:
            iron.cyl(0.01, 0.12, (x, BACK_Y - 2.8, 1.92), segs=5)

# ------------------------------------------------------------------ pews, some broken
z = 5.2
row = 0
while z < 26.4:
    for side in (-1, 1):
        kind = R.randint(0, 6)
        at = Vector((side * 3.8 + R.uniform(-0.2, 0.2), z + R.uniform(-0.15, 0.15), 0.0))
        yaw = R.uniform(-4.0, 4.0)
        if kind == 0:
            # the pew is gone: a few of its boards drift, roughly lined up
            drift = R.uniform(-25, 25)
            for k in range(3):
                pews.box((R.uniform(0.9, 1.7), 0.12, 0.035), at + Vector((R.uniform(-0.8, 0.8), -0.35 + k * 0.3 + R.uniform(-0.05, 0.05), WATER_Z - 0.012)),
                         (R.uniform(-3, 3), R.uniform(-3, 3), drift + R.uniform(-12, 12)), bevel=0.01)
        elif kind == 1:
            pew(at + Vector((0, -0.3, 0.2)), yaw + 6.0, (78, 0))
        elif kind == 2:
            pew(at + Vector((-side * 0.95, 0, -0.05)), yaw - 9.0, (0, side * 7.0), L=1.6)
            pew(at + Vector((side * 1.0, -0.2, -0.12)), yaw + 14.0, (-4, -side * 11.0), L=1.5)
        elif kind == 3:
            pew(at + Vector((0, 0, -0.1)), yaw, (0, side * 5.0))
        else:
            pew(at, yaw)
    z += 1.55
    row += 1

# ------------------------------------------------------------------ candle racks
for rx, ry, ryaw, lit in [(-6.0, 4.3, 8.0, 1), (6.1, 5.1, -10.0, 1), (-5.6, 13.0, 5.0, 1), (5.9, 11.8, -6.0, 1),
                          (-6.0, 21.8, 12.0, 0), (5.8, 20.8, -4.0, 0)]:
    M = S.xform((rx, ry, 0), (0, 0, ryaw))
    for lx in (-0.7, 0.7):
        iron.add(S.geo_cyl(0.025, 1.3, 8), m=M @ S.xform((lx, 0, 0)))
        iron.add(S.geo_box(0.08, 0.5, 0.03, bevel=0.008), m=M @ S.xform((lx, 0.05, 0.015)), uv="box")
    for t in range(3):
        y_ = 0.82 + t * 0.2
        tz = -0.2 + t * 0.2
        iron.add(S.geo_box(1.5, 0.13, 0.03, bevel=0.008), m=M @ S.xform((0, tz, y_)), uv="box")
        iron.add(S.geo_box(1.5, 0.01, 0.03, bevel=0), m=M @ S.xform((0, tz - 0.065, y_ + 0.02)), uv="box")
        for k in range(10):
            lx = -0.66 + k * 0.146 + R.uniform(-0.03, 0.03)
            p = M @ Vector((lx, tz + R.uniform(-0.03, 0.03), y_ + 0.015))
            candle(p, R.uniform(0.04, 0.2), R.uniform(0.018, 0.028), R.random() < 0.85)
# melted clusters on the pier plinths
for side in (-1, 1):
    for y in PIERS[1:6]:
        for k in range(R.randint(3, 7)):
            a = R.uniform(0, math.tau)
            candle((side * COL_X - side * 0.42 + math.cos(a) * 0.12, y - 0.42 + math.sin(a) * 0.12, 0.5), R.uniform(0.08, 0.35),
                   R.uniform(0.025, 0.045), R.random() < 0.8)

# ------------------------------------------------------------------ floating candles and funeral lilies
boards = st.part("floating_boards", WOOD, sharp=60)
spots = []
for i in range(60):
    if i < 12:
        x = R.choice((-1, 1)) * R.uniform(3.7, 7.0)
        p = Vector((x, R.uniform(-3.4, -1.6), WATER_Z))
    else:
        p = Vector((R.uniform(-6.5, 6.5), R.uniform(3.4, 24.8), WATER_Z))
    spots.append(p)
for p in spots:
    boards.cyl(0.06, 0.024, p + Vector((0, 0, -0.012)), (R.uniform(-3, 3), R.uniform(-3, 3), 0), segs=8, bevel=0.006)
    candle(p + Vector((0, 0, 0.012)), R.uniform(0.03, 0.06), 0.024, True, segs=5)
petal = st.part("lilies", st.surface("Fabric001__petal", tint=(0.66, 0.63, 0.56), two_sided=True), sharp=70)
for i in range(10):
    c = Vector((R.choice((-1, 1)) * R.uniform(3.7, 6.8), R.uniform(-3.6, -1.4), WATER_Z + 0.004))
    for k in range(6):
        a = math.tau * k / 6 + R.uniform(0, 0.3)
        g = S.Geo()
        pts = [(0, 0, 0), (0.02, -0.012, 0.006), (0.075, 0, 0.022), (0.02, 0.012, 0.006)]
        idx = [g.vert(q) for q in pts]
        g.face(idx, expect=(0, 0, 1))
        petal.add(g, c, (0, -15, math.degrees(a)), uv="box")

# ------------------------------------------------------------------ censers on long chains
for ci, (cx, cy, cz) in enumerate([(-3.3, PIERS[1], 3.8), (3.9, PIERS[2], 4.3), (-0.8, PIERS[3], 4.6), (2.4, PIERS[4], 5.0)]):
    top = Vector((cx, cy, 11.0))
    cs = st.part(f"censer_{ci + 1}", BRASS, pivot=top, sharp=55)
    link = S.geo_link(0.15, 0.07, 0.011, segs=6, rsegs=3)
    n = int((11.0 - cz - 0.55) / 0.13)
    for k in range(n):
        cs.add(link, (cx, cy, 11.0 - 0.08 - k * 0.13), (0, 0, 90 * (k % 2)), mat=IRON)
    rb = Vector((cx, cy, cz + 0.5))
    cs.ring(0.05, 0.01, rb, (0, 0, 0), segs=10, rsegs=4)
    for k in range(3):
        a = math.tau * k / 3
        cs.tube([rb, Vector((cx + math.cos(a) * 0.13, cy + math.sin(a) * 0.13, cz + 0.08))], 0.006, 4, mat=IRON)
    cs.lathe([(0, -0.24), (0.07, -0.24), (0.075, -0.22), (0.04, -0.2), (0.1, -0.15), (0.16, -0.08), (0.175, -0.02), (0.17, 0.0)],
             (cx, cy, cz), segs=16, cap_bottom=False, cap_top=False)
    cs.lathe([(0.172, -0.012), (0.172, 0.012)], (cx, cy, cz), segs=16, cap_bottom=False, cap_top=False, mat=EMBER)
    cs.lathe([(0.17, 0.0), (0.16, 0.06), (0.13, 0.12), (0.08, 0.16), (0.05, 0.18), (0.03, 0.22), (0.035, 0.25), (0, 0.27)],
             (cx, cy, cz + 0.012), segs=16, cap_bottom=False)
    st.animated(cs.name, "sway", 2.2, 0.45, axis=(1, 0, 0), pivot=top, note="pendulum from the tie beam; add 0.6x about Y; ember band flickers")
    st.fx.append({"kind": "smoke", "pos": [cx, cy, cz + 0.25], "attach": cs.name, "radius_m": 0.05, "rise_m_s": 0.35, "size_m": 0.5,
                  "color": [0.55, 0.6, 0.7, 0.28]})

# ------------------------------------------------------------------ banners, swaying
for bi, (side, by, pale) in enumerate([(-1, 8.3, True), (1, 12.8, False), (-1, 17.1, False), (1, 3.9, True), (1, 25.9, True), (-1, 30.3, False)]):
    piv = Vector((side * (COL_X - 0.45), by, 9.2))
    bn = st.part(f"banner_{bi + 1}", PALE if pale else DARK, pivot=piv, sharp=80)
    g = S.Geo()
    nc, nr = 5, 12
    cuts = [nr - R.randint(0, 3) for _ in range(nc + 1)]
    cols_ = []
    for i in range(nc + 1):
        u = -0.55 + 1.1 * i / nc
        col = []
        for j in range(nr + 1):
            t = j / nr
            col.append(g.vert((u, 0.05 * math.sin(i * 2.2 + t * 3.0) + 0.04 * t * math.sin(i * 1.3), -4.0 * t - (0.12 * math.sin(i * 2.7) if j == nr else 0))))
        cols_.append(col)
    for i in range(nc):
        m = min(cuts[i], cuts[i + 1])
        for j in range(m):
            g.face([cols_[i][j], cols_[i][j + 1], cols_[i + 1][j + 1], cols_[i + 1][j]], expect=(0, -1, 0))
    bn.add(g, piv, (0, 0, -65.0 * side), uv="box")
    bn.add(S.geo_cyl(0.022, 1.3, 8, centred=True), piv + Vector((0, 0, 0.03)), (0, 90, -65.0 * side), mat=IRON)
    st.animated(bn.name, "sway", 1.4, 0.5, axis=(1, 0, 0), pivot=piv, note="slow draught; better as cloth or a WPO wave")

# ------------------------------------------------------------------ ends of the floor
for side in (-1, 1):
    x = side * (COL_X + 0.55)
    rubble.add(S.geo_cyl(0.42, 1.6, 16, bevel=0.03, centred=True), (x + side * 0.15, 0.0, 0.42), (90, 0, side * 20.0))
    blocks_chunk((x + side * 0.1, 3.0, 0.3), (0.9, 0.7, 0.75), 3 + side)
    blocks_chunk((side * 8.3, -2.2, 0.08), (0.5, 0.45, 0.3), 9 + side)
    blocks_chunk((side * 9.2, 1.1, 0.2), (0.6, 0.5, 0.5), 13 + side)
pew((-COL_X - 0.3, 2.7, 0.55), 80.0, (0, 32), L=2.4)
pew((COL_X + 0.45, 2.8, 0.0), 95.0, (0, -6), L=2.4)
# fallen stones from the vault along the aisles and in the nave
for k in range(14):
    side = R.choice((-1, 1))
    blocks_chunk((side * R.uniform(5.8, 10.8), R.uniform(4.0, 30.0), 0.12), (R.uniform(0.3, 0.7), R.uniform(0.3, 0.6), R.uniform(0.25, 0.5)), 30 + k)

# ------------------------------------------------------------------ light
st.set_fog((0.06, 0.075, 0.1), 0.03, 0.08, base_z=0.5,
           note="volumetric fog: the moon spot through the rose should draw a visible shaft (Volumetric Scattering ~3)")
st.set_ambient((0.25, 0.3, 0.42), 0.3)
st.light("key_warm", "directional", (-6, -6, 10), (1.0, 0.8, 0.62), 3.0, direction=(0.370, 0.696, -0.616), shadows=True,
         source_radius=2.0, note="warm candle-coloured key from the upper left front")
st.light("rim_cool", "directional", (0, 20, 10), (0.62, 0.76, 1.0), 2.8, direction=(0.0, -0.914, -0.407), affects="not_floor",
         specular=1.5, note="cool moonlit rim from the rose end")
st.light("fill_camera", "directional", (0, -10, 3), (0.72, 0.8, 0.92), 1.3, direction=(-0.136, 0.968, -0.208), affects="fighters_only",
         specular=0.25, note="soft fill from the camera side, fighters only")
moon_from = Vector((0, BACK_Y + 3.0, ROSE_Z + 1.2))
moon_to = Vector((0, 14.8, 0))
st.light("moon_rose", "spot", tuple(moon_from), (0.6, 0.74, 1.0), 1400.0, radius=48.0, shadows=True, direction=tuple(moon_to - moon_from),
         cone=(12, 18), source_radius=0.3, volumetric=10.0,
         note="signature: moonlight from outside the rose window; its glass casts no shadow, so the tracery throws its pattern on the water")
for i, side in enumerate((-1, 1)):
    st.light(f"aisle_candles_{i + 1}", "point", (side * (AISLE_X - 1.0), 8.8, 2.0), (1.0, 0.62, 0.3), 22.0, radius=9.0, flicker=0.12,
             flicker_speed=6.0, source_radius=0.5, specular=0.1)
for i, (rx, ry) in enumerate([(-6.0, 4.3), (6.1, 5.1), (-5.6, 13.0), (5.9, 11.8)]):
    st.light(f"rack_{i + 1}", "point", (rx, ry - 0.5, 1.5), (1.0, 0.58, 0.26), 26.0, radius=6.0, flicker=0.15, flicker_speed=7.0,
             source_radius=0.4, specular=0.08, note="candle warmth")
st.light("altar", "point", (0, BACK_Y - 3.6, 1.8), (1.0, 0.62, 0.3), 30.0, radius=7.0, flicker=0.1, flicker_speed=5.0,
         source_radius=0.6, specular=0.2)
for i, side in enumerate((-1, 1)):
    st.light(f"pier_candles_{i + 1}", "point", (side * (COL_X - 0.6), 6.1, 0.9), (1.0, 0.6, 0.28), 10.0, radius=4.0, flicker=0.15,
             flicker_speed=7.0, source_radius=0.2, specular=0.1)
for i, side in enumerate((-1, 1)):
    st.light(f"aisle_moon_{i + 1}", "point", (side * (AISLE_X - 1.2), 20.0, 4.0), (0.45, 0.58, 1.0), 12.0, radius=10.0, source_radius=1.0,
             note="moonlight through the aisle windows")
st.fx += [
    {"kind": "motes", "note": "dust in the moon shaft", "box_center": [0, 21.8, 3.0], "box_extent": [3.0, 8.0, 3.0], "count": 200,
     "color": [0.75, 0.85, 1.0, 0.8], "size_m": 0.035},
    {"kind": "motes", "note": "warm specks near the fight", "box_center": [0, 6.8, 2.0], "box_extent": [8.0, 4.0, 1.8], "count": 60,
     "color": [1.0, 0.8, 0.55, 0.5], "size_m": 0.022},
    {"kind": "mist", "note": "low mist on the water", "center": [0, 20.8, 0.35], "size": [16, 16], "color": [0.42, 0.5, 0.62], "density": 0.25},
    {"kind": "drips", "note": "drip rings on the water, a few per second", "box_center": [0, 12, WATER_Z], "box_extent": [10, 14, 0]},
]
st.notes += [
    "The water sheet (WATER_flood) lies 3.5 cm over the flags across the strip: a surface, not an obstacle; fighters' feet show through it.",
    "Rose glass and lancets are emissive and must not cast shadows; the moon spot sits outside the east wall.",
    "Piers moved 0.8 m back from the original so the first pair clears the strip; their plinths' inner faces are at X +-7.2.",
]

st.finish()
st.facing_report()
if A["render"]:
    st.render_reviews(samples=24 if A["quick"] else 64)
if A["export"]:
    path, rows, tris = st.export()
    for r_ in rows:
        print("OBJ", r_[0], r_[1], r_[2])
    print("TRIS", tris, "OBJECTS", len(rows))
