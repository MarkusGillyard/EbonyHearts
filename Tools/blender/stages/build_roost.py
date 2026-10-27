"""The Slopbat's Roost, beneath the Sump: a broad ledge of rock and tar over a lake of glowing slop.
The ribs of something colossal rise from the lake on the left, spines of bone-like vertebrae on
the right beside the mast of a drowned ship; stalactites hang overhead and tar falls pour from the
dark into the lake. The drowned hoard glints at the ledge's ends and on islands. Green lightning
flickers far back in the cavern (a distant light, never a full-screen flash).

Composition from scripts/view/arenas/roost.gd (Godot (x, y, z) -> Blender (x, -z, y)); the end
spires stand with their inner faces on X +-7.2.
Run: Tools/blender/stages/run.sh build_roost.py [-- --no-render | --quick]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402
from mathutils import Vector  # noqa: E402

A = S.args()
st = S.Stage("roost", "The Slopbat's Roost", seed=23, ev100=1.0)
R = st.rng
st.music = "music_roost"
st.footstep = "rock"

LAKE_Z = -2.2
LEDGE_BACK = 2.9
LEDGE_FRONT = -4.3

FLOOR = st.surface("Rock035", tint=(0.42, 0.48, 0.44), rough=0.55)
ROCK = st.surface("dark_rock", tint=(0.36, 0.4, 0.36), rough=0.8)
ROCK_FAR = st.surface("dark_rock__far", tint=(0.2, 0.24, 0.2), rough=0.9)
MUD = st.surface("brown_mud_03", tint=(0.3, 0.32, 0.26), rough=0.45)
BONE = st.surface("Ivory001B", tint=(0.62, 0.58, 0.47), rough=1.2)
GOLD = st.surface("Metal042A", tint=(1.0, 0.78, 0.42), rough=0.7, metal=1.0)
WOOD = st.surface("Wood051", tint=(0.32, 0.26, 0.21), rough=1.2)
IRON = st.surface("Metal055A", tint=(0.25, 0.24, 0.22), rough=1.0, metal=1.0)
LEATHER = st.surface("Leather021", tint=(0.6, 0.5, 0.42), rough=1.0)
LAKE = st.glow("GLOW_slop_lake", (0.32, 0.9, 0.08), 0.65, "the slop lake: slow churning noise with dark crusted patches, bubbles, pulses with the lake lights", pattern=0.85, scale=0.3)
CRACK = st.glow("GLOW_crack", (0.42, 0.95, 0.12), 2.2, "slop glowing through cracks in the ledge (good candidates for decals)")
BOLT = st.glow("GLOW_lightning", (0.7, 1.0, 0.55), 40.0, "lightning bolts far back: visible only during flashes (see fx)")
TAR = st.water("WATER_tar", (0.008, 0.011, 0.007), 0.08, 1.0, "tar pools on the ledge: black, glossy, a faint green sheen, slow bubbles")
TARFALL = st.water("WATER_tarfall", (0.01, 0.014, 0.01), 0.2, 1.0, "tar falls: scrolling downward flow, a faint green glow on the edges, two-sided",
                   emissive=((0.4, 0.9, 0.15), 0.07))

floor = st.part("ledge", FLOOR, floor=True, sharp=35)
rocks = st.part("rocks", ROCK, sharp=62)
far = st.part("cavern", ROCK_FAR, sharp=62)
bones = st.part("bones", BONE, sharp=55)
gold = st.part("hoard", GOLD, sharp=60)
coins = st.part("coins", GOLD, sharp=60, shadows=False)


def boulder(c, r, seed, part=rocks, squash=0.65):
    rr = S.random.Random(seed)
    for k in range(3):
        o = Vector((rr.uniform(-0.5, 0.5) * r, rr.uniform(-0.3, 0.3) * r, rr.uniform(-0.1, 0.3) * r))
        rad = r * rr.uniform(0.6, 1.0)
        part.rock(Vector(c) + o, (rad * rr.uniform(1.0, 1.5), rad, rad * squash), seed * 7 + k, (rr.uniform(-15, 15), rr.uniform(-15, 15), rr.uniform(0, 360)),
                  subdiv=1, rough=0.32)


# ------------------------------------------------------------------ the ledge
floor.box((32, LEDGE_BACK - LEDGE_FRONT, 1.0), (0, (LEDGE_BACK + LEDGE_FRONT) * 0.5, -0.5), bevel=0)
# the ledge breaks away to the lake behind: a broken slope of rock
for k in range(22):
    x = -15.5 + k * 1.48 + R.uniform(-0.3, 0.3)
    rocks.rock((x, LEDGE_BACK + 0.6, -1.1), (R.uniform(1.0, 1.4), 0.9, 1.3), k + 100, (R.uniform(-20, -5), 0, R.uniform(0, 360)), subdiv=1, rough=0.3)
rocks.rock((0, LEDGE_FRONT - 0.6, -1.7), (17.0, 1.2, 1.3), 777, (0, 0, 0), subdiv=2, rough=0.12)
# a ragged lip of boulders along the back edge, with gaps; small behind the centre
x = -15.5
k = 0
while x < 15.5:
    r = R.uniform(0.25, 0.65)
    if abs(x) < 2.2 and r > 0.4:
        r *= 0.6
    boulder((x, LEDGE_BACK + R.uniform(-0.05, 0.3), r * 0.15), r, 300 + k)
    x += r * 1.6 + R.uniform(0.0, 0.8)
    k += 1
# tar pools on the ledge, behind the strip
tar = st.part("tar_pools", TAR, floor=True, smooth=False, surface_z=0.002)
mud = st.part("tar_mud", MUD, sharp=40)
for i, (px, py, pr) in enumerate([(-5.2, 2.3, 1.4), (3.8, 2.4, 1.1), (-11.0, 1.95, 1.0), (11.5, 2.2, 0.9)]):
    tar.add(S.geo_blob(i + 3, pr, 20, 0.3), (px, py, 0.004), (0, 0, R.uniform(-10, 10)), (1.0, 0.45, 1.0))
    mud.add(S.geo_blob(i + 9, pr * 1.18, 20, 0.3, h=-0.0), (px, py, 0.002), (0, 0, R.uniform(-10, 10)), (1.0, 0.5, 1.0))
    for b in range(3):
        a = R.uniform(0, math.tau)
        tar.lathe([(0.07, 0), (0.06, 0.03), (0, 0.045)], (px + math.cos(a) * pr * 0.5, py + math.sin(a) * pr * 0.2, 0.004), segs=8, cap_bottom=False)
# glowing cracks wandering from the back edge toward the camera (1 mm proud of the floor)
cr = st.part("cracks", CRACK, floor=True, shadows=False, smooth=False, surface_z=0.0012)
def crack(p, heading, w, steps, fork):
    for k in range(steps):
        step = R.uniform(0.12, 0.3)
        heading = max(-1.3, min(1.3, heading + R.uniform(-0.9, 0.9)))
        q = p + Vector((math.sin(heading), -math.cos(heading), 0)) * step
        mid = (p + q) * 0.5
        cr.box((w, step + w * 0.5, 0.001), (mid.x, mid.y, 0.0005), (0, 0, math.degrees(heading)), bevel=0, drop=("-z",))
        p = q
        w *= 0.88
        if fork and k == steps // 2:
            crack(p, heading + (0.9 if R.random() < 0.5 else -0.9), w * 0.8, steps // 2, False)
for i in range(12):
    crack(Vector((R.uniform(-13.0, 13.0), LEDGE_BACK - 0.25, 0)), R.uniform(-0.7, 0.7), 0.035, R.randint(6, 14), True)
# bones and stones scattered at the front, low and to the sides
for i in range(12):
    bp = Vector((R.choice((-1, 1)) * R.uniform(3.8, 11.0), R.uniform(-3.8, -1.6), 0.04))
    L = R.uniform(0.3, 0.6)
    a = R.uniform(0, 180)
    d = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0))
    bones.tube([bp - d * L * 0.5, bp + d * L * 0.5], [0.035, 0.03], 6)
    for e in (-1, 1):
        bones.rock(bp + d * L * 0.5 * e, (0.05, 0.045, 0.04), i * 3 + e, subdiv=1, rough=0.15, flat=None)
    rocks.rock(bp + Vector((R.uniform(-0.5, 0.5), 0.3, 0.02)), (R.uniform(0.1, 0.2), R.uniform(0.08, 0.15), 0.08), i + 500, subdiv=1, rough=0.3)
skull = Vector((-6.4, -2.9, 0.1))
bones.rock(skull, (0.13, 0.15, 0.11), 61, (0, 0, 30), subdiv=2, rough=0.08, flat=-0.5)
bones.rock(skull + Vector((0.02, -0.11, -0.05)), (0.07, 0.07, 0.05), 62, (0, 0, 30), subdiv=1, rough=0.1, flat=None)

# ------------------------------------------------------------------ the lake and its islands
st.part("lake", LAKE, floor=True, smooth=False, shadows=False).box((260, 150, 0.001), (0, 72, LAKE_Z), bevel=0, drop=("-z", "-x", "+x", "-y", "+y"))
for (sx, sy, r, d) in [(-6.5, 13.0, 2.6, 1.6), (8.0, 20.0, 3.4, 2.2), (-16.0, 24.0, 3.0, 2.0), (18.0, 12.0, 2.2, 1.5)]:
    rocks.rock((sx, sy, LAKE_Z - r * 0.15), (r, d, r * 0.42), int(sx * 10 + sy), subdiv=2, rough=0.22)

# ------------------------------------------------------------------ the ribcage of something colossal
xc = -6.0
for i in range(6):
    y = 10.5 + i * 2.3
    w = 3.4 - i * 0.15
    h = 6.4 - i * 0.45
    for side in (-1, 1):
        pts, rad = [], []
        for k in range(13):
            t = k / 12
            a = t * math.pi * 0.5
            pts.append(Vector((xc + side * (w * math.cos(a) * (1.0 - t * 0.15) + 0.35), y + math.sin(a) * 0.4, LAKE_Z - 0.4 + h * math.sin(a) * (1.0 + t * 0.1))))
            rad.append(0.27 * (1.0 - t * 0.72))
        bones.tube(pts, rad, 8)
for i in range(14):
    bones.rock((xc, 9.6 + i * 1.0, LAKE_Z + 0.1), (0.45, 0.32, 0.34), i + 700, subdiv=1, rough=0.12, flat=None)

# ------------------------------------------------------------------ spines of vertebrae
for si, (sx, sy, hgt, lean_deg) in enumerate([(6.0, 10.5, 7.0, 4.0), (10.5, 15.0, 9.0, -6.0), (3.0, 24.0, 8.0, 3.0),
                                              (-13.0, 15.5, 8.5, -5.0), (14.5, 26.0, 10.0, 6.0), (-2.0, 34.0, 9.0, -3.0)]):
    base = Vector((sx, sy, LAKE_Z - 0.3))
    n = int(hgt / 0.58)
    lean = Vector((math.sin(math.radians(lean_deg)), 0, 0))
    yaw = R.uniform(0, 180)
    for k in range(n):
        t = k / n
        r = 0.62 * (1.0 - t * 0.66)
        c = base + Vector((0, 0, k * 0.58)) + lean * k * 0.58 * t
        M = S.xform(c, (0, 0, yaw))
        # centrum: a waisted drum with flared rims; a thin disc between it and the next
        bones.add(S.geo_lathe([(0, 0.0), (r * 0.95, 0.0), (r, 0.05), (r * 0.82, 0.2), (r, 0.36), (r * 0.95, 0.41), (0, 0.41)], 12), m=M)
        bones.add(S.geo_cyl(r * 0.85, 0.17, 12), m=M @ S.xform((0, 0, 0.41)))
        # neural arch behind the centrum, a spinous process curving down and back, and two
        # transverse processes like short wings
        arch = [M @ Vector((math.cos(a) * r * 0.55, -r * 0.95 - math.sin(a) * r * 0.5, 0.2)) for a in [math.pi * j / 6 for j in range(7)]]
        bones.tube(arch, 0.07 * (1 - t * 0.4), 6)
        bones.tube([M @ Vector((0, -r * 1.4, 0.22)), M @ Vector((0, -r * 1.95, 0.05)), M @ Vector((0, -r * 2.25, -0.2))],
                   [0.1 * (1 - t * 0.5), 0.07 * (1 - t * 0.5), 0.025], 6)
        for sd in (-1, 1):
            bones.tube([M @ Vector((sd * r * 0.7, -r * 0.55, 0.22)), M @ Vector((sd * r * 1.4, -r * 0.65, 0.3)), M @ Vector((sd * r * 1.8, -r * 0.6, 0.2))],
                       [0.09 * (1 - t * 0.5), 0.065 * (1 - t * 0.5), 0.03], 6)
    rocks.rock(base + Vector((0, 0, 0.2)), (1.8, 1.4, 0.7), si + 800, subdiv=2, rough=0.25)

# ------------------------------------------------------------------ the drowned ship's mast and spar
ship = st.part("ship", WOOD, sharp=50)
mb = Vector((12.0, 31.0, LAKE_Z - 1.0))
mt = mb + Vector((math.sin(math.radians(14)) * 12.0, 0, math.cos(math.radians(14)) * 12.0))
ship.tube([mb, mt], [0.22, 0.13], 10)
ship.tube([mt - (mt - mb) * 0.3 + Vector((-2.4, -0.2, 0.9)), mt - (mt - mb) * 0.3 + Vector((2.4, 0.2, -0.9))], [0.1, 0.1], 8)
ship.box((6.0, 2.0, 1.6), (10.5, 31.5, LAKE_Z + 0.2), (-8, 14, 20), bevel=0.06)
for k in range(5):
    ship.box((0.16, 2.2, 1.9), (8.2 + k * 1.1, 31.5, LAKE_Z + 0.45 + k * 0.4), (-8, 14, 20), bevel=0.02)
st.part("rigging", IRON, sharp=70).tube(S.catenary(mt, mb + Vector((5.5, -1.0, 0.8)), 0.6, 10), 0.02, 4)

# ------------------------------------------------------------------ ceiling, far wall, tar falls
for i in range(38):
    p = Vector((R.uniform(-26, 26), R.uniform(5.0, 40.0), 13.0))
    drop = R.uniform(3.0, 8.5) * (0.8 if p.y < 12.0 else 1.0)
    r = drop * R.uniform(0.1, 0.16)
    prof = [(0.0, -drop)] + [(r * (1 - t) ** 1.2 * (1 + 0.15 * math.sin(t * 17 + i)), -drop * t) for t in (0.85, 0.7, 0.5, 0.3, 0.1)] + [(r * 1.2, 0.4)]
    (rocks if p.y < 14 else far).lathe(prof, p, (R.uniform(-4, 4), R.uniform(-4, 4), R.uniform(0, 360)), segs=8, cap_bottom=False)
far.box((90, 60, 1.0), (0, 30.0, 13.8), bevel=0)
far.box((220, 2.0, 50), (0, 68.0, 20.0), bevel=0)
for i in range(11):
    x = -60.0 + i * 12.0 + R.uniform(-3, 3)
    far.lathe([(R.uniform(2.5, 5.0), 0), (R.uniform(2.0, 3.5), 18), (R.uniform(1.5, 3.0), 40)], (x, 64.0 + R.uniform(-4, 3), LAKE_Z - 1), segs=10, cap_bottom=False)
for sd in (-1, 1):
    far.box((10, 62, 40), (sd * 39.0, 37.0, 15.0), (0, 0, -sd * 8.0), bevel=0)
    for k in range(6):
        far.rock((sd * R.uniform(26, 32), 10 + k * 9, R.uniform(0, 10)), (R.uniform(3, 6), R.uniform(3, 5), R.uniform(4, 7)), 900 + k + sd * 10, subdiv=1, rough=0.3)
falls = st.part("tar_falls", TARFALL, shadows=False, sharp=80)
for fi, (fx_, fy_, fw) in enumerate([(-30.0, 60.0, 3.5), (-11.0, 61.0, 2.6), (9.0, 60.5, 3.0), (27.0, 61.0, 2.4), (-19.0, 27.0, 1.5), (18.0, 36.0, 1.8)]):
    g = S.Geo()
    nc, nr = 6, 14
    top_z = LAKE_Z + 17.8
    grid = []
    for i in range(nc + 1):
        u = i / nc - 0.5
        col = []
        for j in range(nr + 1):
            t = j / nr
            ww = fw * (0.5 + 0.5 * (1.0 - t) ** 0.7) * (1.0 + 0.18 * math.sin(j * 1.7 + i * 0.9 + fi))
            meander = 0.35 * fw * math.sin(t * 3.2 + fi * 1.7)
            col.append(g.vert((fx_ + meander + u * ww, fy_ - 0.6 * math.sin(t * math.pi * 0.5) - 0.25 * abs(u) + 0.05 * math.sin(i * 2 + j), top_z - 18.0 * t)))
        grid.append(col)
    for i in range(nc):
        for j in range(nr):
            g.face([grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]], expect=(0, -1, 0))
    falls.add(g, uv="box")
    far.rock((fx_, fy_ - 0.4, top_z + 0.4), (fw * 0.9, 1.4, 1.0), 950 + fi, subdiv=1, rough=0.35)
    st.fx.append({"kind": "splash_mist", "pos": [fx_, fy_ - 0.8, LAKE_Z + 0.4], "radius_m": fw * 0.4, "rise_m_s": 0.7, "size_m": fw * 1.2,
                  "color": [0.35, 0.6, 0.25, 0.4]})

# ------------------------------------------------------------------ the ends: rock spires and boulders
for side in (-1, 1):
    x = side * 8.2                                   # r 1.0 at the base: inner face at X +-7.2
    rocks.lathe([(1.05, -0.2), (1.0, 0.6), (0.85, 2.5), (0.7, 4.5), (0.5, 6.5), (0.36, 8.0), (0.22, 9.2), (0, 9.6)], (x, 1.75, 0),
                (0, side * 3.0, R.uniform(0, 360)), segs=12, cap_bottom=False)
    rocks.lathe([(0.6, -0.2), (0.55, 1.5), (0.4, 3.0), (0.2, 4.6), (0, 5.0)], (x + side * 1.3, 0.5, 0), (-6, side * 6.0, 0), segs=10, cap_bottom=False)
    boulder((side * 9.4, 2.6, 0.25), 1.1, 40 + side)
    boulder((side * 8.9, -0.45, 0.05), 0.6, 50 + side)
    boulder((side * 9.6, -2.4, 0.0), 0.33, 60 + side)
    for k in range(3):
        rocks.rock((x + side * R.uniform(0.25, 0.8), 1.95 + R.uniform(-0.3, 0.5), 1.2 + k * 2.2), (0.5, 0.45, 0.6), 70 + k + side * 9, subdiv=1, rough=0.35)

# ------------------------------------------------------------------ the drowned hoard
def coin(p, n, tilt=0.0):
    coins.add(S.geo_cyl(0.035, 0.007, 7), p, (tilt * R.uniform(-1, 1), tilt * R.uniform(-1, 1), R.uniform(0, 60)))
for (hx, hy, hz, rx, rz, ry) in [(-9.4, 0.4, 0.0, 1.5, 0.65, 1.1), (9.6, 0.35, 0.0, 1.7, 0.75, 1.2), (-11.4, -1.6, 0.0, 0.9, 0.33, 0.7),
                                 (10.9, -1.9, 0.0, 0.7, 0.3, 0.6), (-6.5, 13.0, LAKE_Z + 0.55, 1.6, 0.7, 1.0), (8.0, 20.0, LAKE_Z + 0.9, 2.2, 0.9, 1.4),
                                 (-6.9, 2.5, 0.0, 0.75, 0.32, 0.42), (6.4, 2.55, 0.0, 0.85, 0.36, 0.45)]:
    c = Vector((hx, hy, hz))
    gold.rock(c, (rx, ry, rz), int(hx * 7 + hy), (0, 0, 0), subdiv=2, rough=0.08, flat=0.0)
    count = int(rx * ry * 30)
    for i in range(count):
        th = R.uniform(0, math.tau)
        ph = math.sqrt(R.random()) * math.pi * 0.42
        nrm = Vector((math.sin(ph) * math.cos(th), math.sin(ph) * math.sin(th), math.cos(ph)))
        p = c + Vector((nrm.x * rx, nrm.y * ry, nrm.z * rz)) * 1.0
        up = Vector((nrm.x / rx, nrm.y / ry, nrm.z / rz)).normalized()
        rot = up.to_track_quat("Z", "Y").to_euler()
        coins.add(S.geo_cyl(0.035, 0.007, 7), p, tuple(math.degrees(a) + R.uniform(-15, 15) for a in rot))
    for i in range(int(rx * 14)):
        a = R.uniform(0, math.tau)
        d = R.uniform(1.0, 1.6)
        p = c + Vector((math.cos(a) * rx * d, math.sin(a) * ry * d, 0.0))
        if hz == 0.0 and (abs(p.x) < 7.7 or p.y < -3.9):
            continue
        coin(p, i, 6.0)
    st.fx.append({"kind": "glints", "center": [hx, hy, hz + rz * 0.6], "extent": [rx, ry, rz * 0.5], "count": int(rx * ry * 6),
                  "color": [1.0, 0.85, 0.5], "note": "twinkling sparkles on the gold"})
# treasures: goblets, a crown, swords, a shield, a chest spilling over
for gx, gz, gy in [(-9.0, 0.55, -0.2), (9.2, 0.62, -0.35), (-10.2, 0.35, 0.5), (10.6, 0.4, -0.5)]:
    gold.lathe([(0.05, 0), (0.05, 0.012), (0.012, 0.03), (0.01, 0.13), (0.03, 0.15), (0.07, 0.2), (0.078, 0.27), (0.07, 0.27), (0.05, 0.2), (0, 0.18)],
               (gx, gy, gz), (R.uniform(-15, 15), R.uniform(-15, 15), 0), segs=10, cap_bottom=True)
crown_c = Vector((9.9, 0.4, 0.74))
gold.ring(0.115, 0.018, crown_c, (23, 0, 0), segs=16, rsegs=5)
for k in range(6):
    a = math.tau * k / 6
    gold.cyl(0.016, 0.1, crown_c + Vector((math.cos(a) * 0.115, math.sin(a) * 0.105, math.sin(a) * 0.045)), (23, 0, 0), segs=5, r2=0.003)
for sx_, sz_, sy_, ang in [(-8.7, 0.6, 0.8, 25.0), (9.0, 0.7, 0.9, -30.0), (-9.8, 0.5, -0.4, -12.0)]:
    M = S.xform((sx_, sy_, sz_), (0, ang, 0))
    gold.add(S.geo_prism([(-0.025, 0), (0.025, 0), (0.02, 0.85), (0, 0.92), (-0.02, 0.85)], 0.008, bevel=0.003), m=M @ S.xform((0, 0, 0.05)))
    gold.add(S.geo_box(0.26, 0.04, 0.035, bevel=0.01), m=M @ S.xform((0, 0, 0.04)), uv="box")
    st.part("grips", LEATHER, sharp=50).add(S.geo_cyl(0.018, 0.17, 8), m=M @ S.xform((0, 0, -0.14)))
    gold.add(S.geo_rock(int(sx_ * 3), 1, 0.05, None), m=M @ S.xform((0, 0, -0.16), (0, 0, 0), 0.032))
gold.add(S.geo_cyl(0.36, 0.04, 20, bevel=0.012), (10.3, 0.9, 0.55), (70, 20, 0))
gold.add(S.geo_cyl(0.08, 0.04, 12, bevel=0.01), (10.3 + 0.0, 0.9 - 0.04, 0.55 + 0.0), (70, 20, 0))
chest = st.part("chest", WOOD, sharp=45)
cm = S.xform((-8.3, -0.6, 0.0), (0, 0, 18))
chest.add(S.geo_box(0.8, 0.5, 0.45, bevel=0.02), m=cm @ S.xform((0, 0, 0.225)), uv="box")
chest.add(S.geo_box(0.82, 0.5, 0.08, bevel=0.02), m=cm @ S.xform((0, 0.2, 0.62), (-55, 0, 0)), uv="box")
for bx_ in (-0.3, 0.3):
    st.part("chest_iron", IRON, sharp=45).add(S.geo_box(0.05, 0.52, 0.47, bevel=0.008), m=cm @ S.xform((bx_, 0, 0.225)), uv="box")
gold.add(S.geo_rock(99, 2, 0.1, 0.0), m=cm @ S.xform((0, 0, 0.42), (0, 0, 0), (0.36, 0.22, 0.13)))
st.prop("wooden_crate_02", (-12.0, 2.1, -0.12), rot_z=-35, tilt=(4, -9))
st.prop("wine_barrel_01", (12.2, 1.6, 0.37), rot_z=40, tilt=(0, 88))

# ------------------------------------------------------------------ lightning bolts (shown only during flashes)
bolt = st.part("lightning", BOLT, shadows=False, smooth=False, pivot=(0, 38.0, LAKE_Z + 16.0))
for bi, bx_ in enumerate((-12.0, 3.0, 15.0)):
    pts = [Vector((bx_, 38.0, LAKE_Z + 16.0))]
    p = pts[0].copy()
    for k in range(14):
        p = p + Vector((R.uniform(-0.9, 0.9), 0, -18.0 / 14))
        pts.append(p.copy())
    def strip(ps, w):
        g = S.Geo()
        for k in range(len(ps) - 1):
            a, b = ps[k], ps[k + 1]
            side = (b - a).cross(Vector((0, 1, 0))).normalized() * w * 0.5
            idx = [g.vert(a - side), g.vert(a + side), g.vert(b + side), g.vert(b - side)]
            g.face(idx, expect=(0, -1, 0))
        bolt.add(g, uv="box")
    strip(pts, 0.18)
    for f in range(2):
        at = 3 + f * 5
        q = pts[at].copy()
        fork = [q.copy()]
        dr = 1 if f == 0 else -1
        for k in range(5):
            q = q + Vector((dr * R.uniform(0.3, 1.0), 0, -1.0))
            fork.append(q.copy())
        strip(fork, 0.09)

# ------------------------------------------------------------------ light
st.set_fog((0.035, 0.075, 0.04), 0.034, 0.2, base_z=LAKE_Z,
           note="dense green height fog over the lake; volumetric fog so the lake lights and the storm glow scatter")
st.set_ambient((0.25, 0.42, 0.25), 0.25)
st.light("key_warm", "directional", (-6, -6, 10), (1.0, 0.88, 0.72), 2.4, direction=(0.383, 0.663, -0.643), shadows=True, source_radius=2.0,
         note="warm key from the upper left front (torchlight from the cave mouth)")
st.light("rim_green", "directional", (0, 20, 10), (0.5, 0.95, 0.4), 3.2, direction=(-0.081, -0.924, -0.375), affects="not_floor", specular=1.5,
         note="green storm rim from the back of the cavern")
st.light("fill_camera", "directional", (0, -10, 3), (0.75, 0.85, 0.8), 1.4, direction=(-0.136, 0.968, -0.208), affects="fighters_only", specular=0.25)
for i, lx in enumerate((-8.0, 0.0, 8.0)):
    st.light(f"lake_glow_{i + 1}", "point", (lx, LEDGE_BACK + 1.6, LAKE_Z + 1.0), (0.45, 1.0, 0.2), 30.0, radius=8.5, shadows=False,
             flicker=0.18, flicker_speed=1.1, source_radius=1.0, specular=0.25, note="the slop lake glowing up onto the ledge's lip")
st.light("lake_deep", "point", (0.0, 22.0, LAKE_Z + 2.5), (0.45, 1.0, 0.2), 120.0, radius=28.0, flicker=0.2, flicker_speed=0.7, source_radius=3.0,
         specular=0.2)
for i, sd in enumerate((-1, 1)):
    st.light(f"hoard_{i + 1}", "point", (sd * 9.3, -0.9, 1.6), (1.0, 0.72, 0.35), 14.0, radius=4.0, flicker=0.06, flicker_speed=2.0, source_radius=0.3,
             specular=0.8, note="warm light catching the gold")
st.light("storm", "point", (0.0, 30.0, 8.0), (0.6, 1.0, 0.45), 900.0, radius=30.0, shadows=True, flicker=1.0, flicker_speed=0.0, source_radius=2.0,
         render_scale=0.25, note="signature: green lightning. Off between flashes; every 5-11 s at a random X in -16..16: 7x for 0.07 s, 1.5x to "
         "0.12 s, 5x to 0.2 s, then fading to 0 by 0.55 s (values relative to intensity_candela/7). Lights only the far cavern "
         "(exclude the ledge and lake). With flashes off: a slow 1.2 s swell to 2/7.")
st.fx += [
    {"kind": "lightning", "object": "roost_lightning", "note": "three bolts at X -12, 3, 15 (Y 38): show the nearest one during the first and third pulse of each flash; hidden otherwise"},
    {"kind": "motes", "note": "spores over the slop", "box_center": [0, 9.0, LAKE_Z + 1.6], "box_extent": [16, 6, 1.6], "count": 120,
     "color": [0.55, 1.0, 0.25, 0.9], "size_m": 0.045, "rise_m_s": 0.25},
    {"kind": "mist", "center": [0, 10.0, LAKE_Z + 0.5], "size": [70, 16], "color": [0.22, 0.42, 0.12], "density": 0.3},
    {"kind": "mist", "center": [0, 32.0, LAKE_Z + 1.0], "size": [120, 30], "color": [0.18, 0.36, 0.1], "density": 0.45},
]
st.notes += [
    "End spires at X +-8.2 (radius 1.0 at the floor) put their inner faces on X +-7.2.",
    "Glowing cracks (roost_cracks) are 1 mm proud of the ledge; they could equally be decals.",
    "roost_lightning is pivoted at its top and should be hidden except during flashes.",
]

st.finish()
st.facing_report()
if A["render"]:
    st.preview_volume((0, 20, LAKE_Z - 0.2), 40.0, 2.0, (0.4, 0.7, 0.3), 0.12, 0.3, emission=((0.4, 1.0, 0.15), 0.08))
    st.part("lightning", BOLT).obj.hide_render = True
    st.render_reviews(samples=24 if A["quick"] else 64)
if A["export"]:
    path, rows, tris = st.export()
    for r_ in rows:
        print("OBJ", r_[0], r_[1], r_[2])
    print("TRIS", tris, "OBJECTS", len(rows))
