"""The Rendering Yards of Tallowmarket (Gorm Tallow's home): an iron plate floor between great
rendering vats of glowing slop, a catwalk with chains overhead, a brick furnace house with fire
mouths behind, smokestacks against a cloud-lit night, drizzle.

Composition from the Godot original (scripts/view/arenas/yards.gd), mapped Godot (x, y, z) ->
Blender (x, -z, y). Run: Tools/blender/stages/run.sh build_rendering_yards.py [-- --no-render]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402
from mathutils import Vector  # noqa: E402

A = S.args()
st = S.Stage("rendering_yards", "The Rendering Yards", seed=5, ev100=1.0)
R = st.rng
st.music = "music_yards"
st.footstep = "metal"

# ------------------------------------------------------------------ materials
PLATE = st.surface("MetalPlates013", tint=(0.6, 0.6, 0.6), rough=0.85, metal=0.9)
VAT = st.surface("MetalPlates013__vat", tint=(0.4, 0.38, 0.36), rough=0.95, metal=0.85)
RUST = st.surface("rusty_metal_04", tint=(0.6, 0.52, 0.46), rough=1.0, metal=1.0)
IRON = st.surface("Metal055A", tint=(0.3, 0.29, 0.28), rough=1.0, metal=1.0)
TREAD = st.surface("metal_plate", tint=(0.55, 0.55, 0.55), rough=1.0, metal=1.0)
BRICK = st.surface("factory_brick", tint=(0.48, 0.38, 0.34), rough=1.0)
SOOT = st.surface("factory_brick__soot", tint=(0.26, 0.21, 0.2), rough=1.0)
SETTS = st.surface("medieval_blocks_03", tint=(0.34, 0.33, 0.34), rough=0.7)
SLOP = st.glow("GLOW_slop", (0.36, 1.0, 0.06), 1.9, "vat slop, spills and the channel: animate (slow scrolling noise, bubbles)", pattern=0.6, scale=3.0)
FIRE = st.glow("GLOW_furnace", (1.0, 0.3, 0.05), 3.4, "furnace mouths and fireboxes: flicker 0.25", pattern=0.5, scale=4.0)
WIN = st.glow("GLOW_window", (1.0, 0.48, 0.16), 0.9, "furnace-house windows")
FARWIN = st.glow("GLOW_window_far", (1.0, 0.45, 0.15), 0.6, "distant windows")

VATS = [(0.0, 6.6, 2.0, 2.55), (-5.5, 7.6, 1.6, 2.3), (5.6, 7.4, 1.65, 2.45), (-10.8, 7.0, 1.9, 2.7), (10.9, 7.2, 1.85, 2.6)]
VAT_BASE = 0.75
YARD_Z = -0.05
WALL_Y = 12.1        # front face of the furnace house
MOUTHS = [-13.5, -8.2, -2.7, 2.9, 8.3, 13.6]
CAT_Z = 4.1          # catwalk deck top
CAT_Y0, CAT_Y1 = 4.35, 5.75
MAN_Z = 3.25         # the manifold pipe along the furnace-house wall


def rivets(part, pts, r=0.022, h=0.012, axis=(0, -1, 0)):
    """Dome rivet heads at `pts`, facing along `axis`."""
    g = S.geo_lathe([(r, 0), (r * 0.8, h * 0.7), (r * 0.4, h), (0, h * 1.05)], 6, cap_bottom=False)
    rot = Vector(axis).to_track_quat("Z", "Y").to_euler()
    deg = tuple(math.degrees(a) for a in rot)
    for p in pts:
        part.add(g, p, deg, uv="box")


# ------------------------------------------------------------------ the iron floor
floor = st.part("floor", PLATE, floor=True, sharp=40)
bed = st.part("floor_bed", IRON, floor=True)
PW, PD = 1.875, 1.5
grates = {(-2, -1), (2, 1), (-5, 1), (5, -1)}
ys = [-4.5 + PD * k for k in range(5)]            # rows from Y -4.5 to 3.0
for j, y0 in enumerate(ys):
    off = (PW * 0.5) if j % 2 else 0.0
    for i0 in range(20):
        x0 = -15.0 - off + i0 * PW
        if x0 >= 15.0:
            break
        x1 = min(x0 + PW, 15.0)
        x0c = max(x0, -15.0)
        cx, cy = (x0c + x1) * 0.5, y0 + PD * 0.5
        cell = (round(cx / PW), j - 2)
        if cell in grates:
            # a drain grate in place of the plate: frame and bars flush with the floor, a faint
            # green glow from the slop drain beneath
            gw, gd = x1 - x0c - 0.012, PD - 0.012
            for sx in (-1, 1):
                bed.box((0.08, gd, 0.06), (cx + sx * (gw * 0.5 - 0.04), cy, -0.03), bevel=0.008)
            for sy in (-1, 1):
                bed.box((gw, 0.08, 0.06), (cx, cy + sy * (gd * 0.5 - 0.04), -0.03), bevel=0.008)
            n = int(gw / 0.11)
            for k in range(n):
                bed.box((0.035, gd - 0.16, 0.05), (cx - gw * 0.5 + 0.12 + k * (gw - 0.24) / (n - 1), cy, -0.025), bevel=0.006)
            bed.box((gw, gd, 0.04), (cx, cy, -0.32), bevel=0)
            st.part("drain_glow", SLOP, floor=True).box((gw - 0.2, gd - 0.2, 0.01), (cx, cy, -0.28), bevel=0)
            continue
        floor.box((x1 - x0c - 0.012, PD - 0.012, 0.06), (cx, cy, -0.03), bevel=0.014, drop=("-z",))
bed.box((30.4, 7.6, 0.1), (0, -0.75, -0.1), bevel=0)

# kerb of granite blocks, then the slop channel and the stone-set yard behind
kerb = st.part("kerb", SETTS, sharp=40)
x = -15.0
while x < 15.0:
    L = min(R.uniform(1.0, 1.5), 15.0 - x)
    kerb.box((L - 0.02, 0.42, 0.24), (x + L * 0.5, 3.22, 0.07), bevel=0.03, segs=2)
    x += L
chan = st.part("channel", IRON)
chan.box((30.4, 0.12, 0.5), (0, 3.48, -0.25), bevel=0.01)
chan.box((30.4, 0.14, 0.35), (0, 4.08, -0.12), bevel=0.02)
chan.box((30.4, 0.7, 0.05), (0, 3.78, -0.4), bevel=0)
slop = st.part("slop", SLOP, floor=True)
slop.box((30.4, 0.5, 0.02), (0, 3.78, -0.17), bevel=0)
# bubbles breaking the channel surface
for k in range(16):
    bx = R.uniform(-14.5, 14.5)
    slop.lathe([(0.06 + R.random() * 0.05, 0), (0.05, 0.03), (0.0, 0.045)], (bx, 3.78 + R.uniform(-0.15, 0.15), -0.165), segs=8, cap_bottom=False)
# iron bridging plates over the channel at the catwalk columns
for x in (-14.2, -8.1, -3.0, 3.0, 8.1, 14.2):
    chan.box((0.9, 0.72, 0.04), (x, 3.78, -0.02), bevel=0.01)

yard = st.part("yard", SETTS, floor=True, sharp=40)
yard.box((48, 9.0, 0.3), (0, 8.65, YARD_Z - 0.15), bevel=0.02)

# ------------------------------------------------------------------ the vats
vat = st.part("vats", VAT, sharp=35)
band = st.part("vat_bands", RUST, sharp=40)
plinth = st.part("vat_plinths", BRICK, sharp=40)
iron = st.part("ironwork", IRON, sharp=45)
fire = st.part("fire", FIRE)
spill = st.part("vat_slop", SLOP, sharp=60)
for vi, (x, y, r, h) in enumerate(VATS):
    top = VAT_BASE + h
    pr = r + 0.3
    # brick plinth with an iron coping ring
    plinth.lathe([(0, YARD_Z), (pr, YARD_Z), (pr, 0.6), (pr - 0.02, 0.62), (0, 0.62)], (x, y, 0), segs=36)
    band.lathe([(pr - 0.01, 0.6), (pr + 0.04, 0.61), (pr + 0.05, 0.64), (pr + 0.05, 0.7), (pr + 0.03, 0.72),
                (r + 0.02, 0.74), (r, VAT_BASE)], (x, y, 0), segs=36)
    # the tank: plates over a slight belly, rolled lip, inner wall down to the slop
    vat.lathe([(r - 0.04, VAT_BASE), (r, VAT_BASE + 0.05), (r + 0.015, VAT_BASE + h * 0.5), (r, top - 0.14),
               (r + 0.01, top - 0.05), (r - 0.04, top - 0.02), (r - 0.06, top - 0.38)], (x, y, 0), segs=40,
              cap_bottom=False, cap_top=False)
    band.ring(r + 0.02, 0.06, (x, y, top - 0.04), segs=48, rsegs=6)
    for zb in (VAT_BASE + 0.2, VAT_BASE + h * 0.5, top - 0.42):
        rb = r + 0.012 if abs(zb - (VAT_BASE + h * 0.5)) < 0.01 else r
        band.lathe([(rb, zb - 0.07), (rb + 0.03, zb - 0.055), (rb + 0.035, zb - 0.035), (rb + 0.035, zb + 0.035),
                    (rb + 0.03, zb + 0.055), (rb, zb + 0.07)], (x, y, 0), segs=40, cap_bottom=False, cap_top=False)
        if vi < 3:
            # rivets round the front half of the nearer vats' bands
            n = int(math.pi * r / 0.24)
            pts, axes = [], []
            for k in range(n):
                a = math.radians(-180 + 180 * (k + 0.5) / n)
                d = Vector((math.cos(a), math.sin(a), 0))
                rivets(band, [Vector((x, y, zb)) + d * (rb + 0.034)], axis=d)
    # slop surface, gently domed, with a few bubbles
    spill.lathe([(r - 0.06, top - 0.36), (r * 0.6, top - 0.345), (0, top - 0.33)], (x, y, 0), segs=24, cap_bottom=False)
    for k in range(3):
        a = R.uniform(0, math.tau)
        d = R.uniform(0.2, r * 0.7)
        br = R.uniform(0.08, 0.22)
        spill.lathe([(br, 0), (br * 0.85, br * 0.5), (br * 0.45, br * 0.85), (0, br * 0.95)],
                    (x + math.cos(a) * d, y + math.sin(a) * d, top - 0.36), segs=10, cap_bottom=False)
    # the spill: slop bulging over the lip and running down the front in rivulets that end in
    # drips; a few reach the coping and pool on the yard
    a0 = math.radians(-90 + 14.5 + R.uniform(-6, 6))
    span = (0.42 + r * 0.08) / r
    arc = [a0 - span * 0.5 + span * k / 9 for k in range(10)]
    spill.tube([(x + math.cos(a) * (r + 0.075), y + math.sin(a) * (r + 0.075), top - 0.035) for a in arc],
               [0.03 + 0.05 * math.sin(math.pi * k / 9) for k in range(10)], 8, caps=True)
    bands_z = (VAT_BASE + 0.2, VAT_BASE + h * 0.5, top - 0.42)
    nriv = 4 if r > 1.7 else 3
    for k in range(nriv):
        a = a0 + span * (-0.38 + 0.76 * (k + R.uniform(0.2, 0.8)) / nriv)
        z_end = VAT_BASE + 0.05 if k in (1, nriv - 1) else R.uniform(VAT_BASE + 0.4, top - 0.9)
        pts, rad = [], []
        zz = top + 0.0
        n_ = 0
        while zz > z_end:
            near = min(abs(zz - b) for b in bands_z)
            rr = r + (0.05 if near < 0.09 else 0.022)
            aa = a + 0.025 * math.sin(zz * 3.1 + k) / r
            pts.append((x + math.cos(aa) * rr, y + math.sin(aa) * rr, zz))
            t = (top - zz) / (top - z_end + 1e-6)
            rad.append(0.034 * (1.0 - 0.45 * t) + 0.01 * math.sin(n_ * 1.7 + k))
            zz -= 0.12
            n_ += 1
        spill.tube(pts, rad, 6, caps=True)
        ex, ey, ez = pts[-1]
        dr = rad[-1] * 1.6
        spill.lathe([(0, -dr * 1.4), (dr * 0.7, -dr * 1.1), (dr, -dr * 0.5), (dr * 0.6, 0), (0.0, dr * 0.6)],
                    (ex, ey, ez), segs=8, cap_bottom=False, cap_top=False)
        if z_end < VAT_BASE + 0.1:
            cx_, cy_ = x + math.cos(a) * (r + 0.15), y + math.sin(a) * (r + 0.15)
            spill.lathe([(0.16, 0), (0.1, 0.012), (0, 0.016)], (cx_, cy_, 0.735), segs=10, cap_bottom=False)
    # a puddle on the yard in front of the plinth, fed over the coping
    spill.tube([(x + math.cos(a0) * (pr + 0.02), y + math.sin(a0) * (pr + 0.02), 0.73),
                (x + math.cos(a0) * (pr + 0.06), y + math.sin(a0) * (pr + 0.06), 0.62),
                (x + math.cos(a0) * (pr + 0.07), y + math.sin(a0) * (pr + 0.07), 0.2),
                (x + math.cos(a0) * (pr + 0.12), y + math.sin(a0) * (pr + 0.12), YARD_Z + 0.02)], [0.03, 0.028, 0.024, 0.03], 6)
    pcx, pcy = x + math.cos(a0) * (pr + 0.45), y + math.sin(a0) * (pr + 0.45)
    g = S.geo_lathe([(0.55, 0), (0.3, 0.01), (0, 0.014)], 14, cap_bottom=False)
    spill.add(g, (pcx, pcy, YARD_Z), (0, 0, R.uniform(0, 90)), (1.0, 0.6, 1.0))
    # firebox in the plinth front: iron frame, glowing mouth, bars
    fy = y - pr
    iron.box((1.15, 0.14, 0.58), (x, fy - 0.02, 0.3), bevel=0.025)
    iron.box((1.3, 0.3, 0.06), (x, fy - 0.08, 0.62), bevel=0.015)
    fire.box((0.74, 0.04, 0.3), (x, fy - 0.075, 0.3), bevel=0)
    for k in range(6):
        iron.cyl(0.018, 0.34, (x - 0.3 + k * 0.12, fy - 0.11, 0.13), segs=6)
    iron.box((0.84, 0.05, 0.05), (x, fy - 0.11, 0.47), bevel=0.01)
    iron.box((0.84, 0.05, 0.05), (x, fy - 0.11, 0.13), bevel=0.01)
    # a feed pipe from the back of the vat to the furnace house, flanged, on a trestle
    px = x - r * 0.4
    pz = top - 0.6
    py0 = y + math.sqrt(max(0.0, r * r - (r * 0.4) ** 2))
    pipe = st.part("pipes", RUST, sharp=50)
    pipe.tube([(px, py0 - 0.1, pz), (px, WALL_Y - 1.2, pz), (px, WALL_Y - 0.75, MAN_Z - 0.02), (px, WALL_Y - 0.45, MAN_Z)], 0.13, 14)
    yy = py0 + 0.4
    while yy < WALL_Y - 1.4:
        pipe.cyl(0.18, 0.08, (px, yy, pz), (90, 0, 0), segs=14, bevel=0.015)
        yy += 1.6
    ty = (py0 + WALL_Y) * 0.5
    iron.box((0.12, 0.12, pz - 0.15 - YARD_Z), (px, ty, (pz - 0.15 + YARD_Z) * 0.5), bevel=0.015)
    iron.box((0.5, 0.18, 0.06), (px, ty, pz - 0.17), bevel=0.01)

# the ladder up the right-hand vat, and a valve wheel on the centre vat
for rx in (4.35, 4.75):
    iron.cyl(0.025, 3.0, (rx, 5.78, 0.62), segs=8)
for k in range(10):
    iron.cyl(0.018, 0.4, (4.35, 5.78, 0.95 + k * 0.3), (0, 90, 0), segs=6)
for zz in (1.4, 3.2):
    iron.box((0.06, 0.25, 0.04), (4.35, 5.9, zz), bevel=0.01)
    iron.box((0.06, 0.25, 0.04), (4.75, 5.9, zz), bevel=0.01)
vx, vy = -1.25, 6.6 - math.sqrt(4.0 - 1.25 ** 2) - 0.05
pipe = st.part("pipes", RUST)
pipe.tube([(vx, vy + 0.1, 1.35), (vx, vy - 0.32, 1.35)], 0.07, 10)
iron.ring(0.2, 0.018, (vx, vy - 0.36, 1.35), (90, 0, 0), segs=20, rsegs=6)
for k in range(4):
    iron.add(S.geo_cyl(0.012, 0.4, 5, centred=True), (vx, vy - 0.36, 1.35), (0, k * 45, 0))
iron.cyl(0.05, 0.06, (vx, vy - 0.33, 1.35), (90, 0, 0), segs=10)

# ------------------------------------------------------------------ the catwalk
deck = st.part("catwalk_deck", TREAD)
x = -15.0
while x < 15.0:
    deck.box((2.98, CAT_Y1 - CAT_Y0, 0.04), (x + 1.5, (CAT_Y0 + CAT_Y1) * 0.5, CAT_Z - 0.02), bevel=0.008)
    x += 3.0
for yy in (CAT_Y0, CAT_Y1):
    iron.box((30.2, 0.07, 0.24), (0, yy, CAT_Z - 0.13), bevel=0.012)
for k in range(11):
    iron.box((0.07, CAT_Y1 - CAT_Y0, 0.12), (-15 + k * 3.0, (CAT_Y0 + CAT_Y1) * 0.5, CAT_Z - 0.1), bevel=0.01)
rail = st.part("rails", RUST, sharp=50)
for yy in (CAT_Y0 + 0.04, CAT_Y1 - 0.04):
    for k in range(21):
        xx = -15 + k * 1.5
        rail.box((0.05, 0.05, 1.05), (xx, yy, CAT_Z + 0.52), bevel=0.01)
    rail.tube([(-15.2, yy, CAT_Z + 1.06), (15.2, yy, CAT_Z + 1.06)], 0.026, 8)
    rail.tube([(-15.2, yy, CAT_Z + 0.56), (15.2, yy, CAT_Z + 0.56)], 0.018, 6)
    rail.box((30.2, 0.012, 0.1), (0, yy, CAT_Z + 0.05), bevel=0.003)
# H-section columns between the vats, with knee braces
COLS = (-14.2, -8.1, 8.1, 14.2)
# between the vats the catwalk hangs from a roof girder on pairs of rods instead of columns, so
# nothing heavy stands right behind the fighters
for hx in (-3.0, 3.0):
    for hy in (CAT_Y0 + 0.05, CAT_Y1 - 0.05):
        iron.cyl(0.022, 9.2 - CAT_Z, (hx, hy, CAT_Z - 0.1), segs=8)
        iron.box((0.12, 0.12, 0.1), (hx, hy, CAT_Z - 0.2), bevel=0.02)
iron.box((32, 0.35, 0.5), (0, (CAT_Y0 + CAT_Y1) * 0.5, 9.35), bevel=0.03)
for cx in COLS:
    cy = 4.62
    hgt = CAT_Z - 0.25 - YARD_Z
    for sy in (-1, 1):
        iron.box((0.3, 0.025, hgt), (cx, cy + sy * 0.14, YARD_Z + hgt * 0.5), bevel=0.006)
    iron.box((0.022, 0.26, hgt), (cx, cy, YARD_Z + hgt * 0.5), bevel=0)
    iron.box((0.45, 0.45, 0.04), (cx, cy, YARD_Z + 0.02), bevel=0.01)
    rivets(iron, [(cx + sx * 0.1, cy - 0.155, YARD_Z + 0.4 + k * 0.35) for sx in (-1, 1) for k in range(10)], r=0.016, h=0.009)
    for sx in (-1, 1):
        a = math.atan2(0.9, 0.9)
        iron.box((0.06, 0.05, 1.27), (cx + sx * 0.45, cy - 0.12, CAT_Z - 0.7), (0, sx * 45, 0), bevel=0.008)

# chains on the front edge of the catwalk, each its own swinging object pivoted at the top
CHAINS = [(-6.6, 1.95), (-2.3, 2.6), (2.7, 1.75), (6.4, 2.85)]
for ci, (cx, ln) in enumerate(CHAINS):
    top = Vector((cx, CAT_Y0 - 0.02, CAT_Z - 0.25))
    ch = st.part(f"chain_{ci + 1}", IRON, pivot=top, sharp=60, shadows=True)
    link = S.geo_link(0.1, 0.055, 0.009, segs=8, rsegs=4)
    n = int(ln / 0.075)
    for k in range(n):
        ch.add(link, (cx, top.y, top.z - 0.05 - k * 0.075), (0, 0, 90 * (k % 2)))
    hook = [Vector((0, 0, 0.06)), Vector((0, 0, -0.12)), Vector((0.02, 0, -0.2)), Vector((0.09, 0, -0.24)),
            Vector((0.15, 0, -0.2)), Vector((0.16, 0, -0.12)), Vector((0.13, 0, -0.08))]
    base = top + Vector((0, 0, -0.05 - n * 0.075))
    ch.tube([base + p for p in hook], 0.018, 8)
    ch.cyl(0.035, 0.07, base + Vector((0, 0, 0.02)), segs=8)
    st.animated(ch.name, "sway", 3.0, 0.8, axis=(1, 0, 0), pivot=top,
                note="swing about the top link, amp in degrees; second axis Y at 0.6x")
# a trolley block on the catwalk from which the chains drop
for cx, ln in CHAINS:
    iron.box((0.22, 0.16, 0.1), (cx, CAT_Y0 - 0.02, CAT_Z - 0.3), bevel=0.02)

# ------------------------------------------------------------------ the furnace house
wall = st.part("furnace_house", BRICK, sharp=40)
soot = st.part("brick_far", SOOT, sharp=40)
WD = 1.0
yc = WALL_Y + WD * 0.5
X0, X1 = -24.0, 24.0
wall.box((X1 - X0, WD + 0.16, 0.45), (0, yc - 0.08, YARD_Z + 0.225), bevel=0.03)         # plinth course
wall.box((X1 - X0, WD, 1.52), (0, yc, 2.08 + 0.76), bevel=0.0)                          # between arches and windows
wall.box((X1 - X0, WD, 1.15), (0, yc, 5.45 + 0.575), bevel=0.0)                          # above the windows
wall.box((X1 - X0, WD + 0.24, 0.22), (0, yc - 0.12, 6.0), bevel=0.02)                     # corbel course
wall.box((X1 - X0, WD + 0.12, 0.2), (0, yc - 0.06, 6.2), bevel=0.02)
MW, MZ0, MZ1 = 1.9, 0.4, 1.7
WW, WZ0, WZ1 = 1.1, 3.6, 4.85
edges = [X0]
for mx in MOUTHS:
    edges += [mx - MW / 2, mx + MW / 2]
edges.append(X1)
for k in range(0, len(edges), 2):
    a, b = edges[k], edges[k + 1]
    wall.box((b - a, WD, 2.08 - 0.4), (0.5 * (a + b), yc, 0.4 + (2.08 - 0.4) * 0.5), bevel=0)
wedges = [X0]
for mx in MOUTHS:
    wedges += [mx - WW / 2, mx + WW / 2]
wedges.append(X1)
for k in range(0, len(wedges), 2):
    a, b = wedges[k], wedges[k + 1]
    wall.box((b - a, WD, 5.45 - 3.6), (0.5 * (a + b), yc, 3.6 + (5.45 - 3.6) * 0.5), bevel=0)
for mx in MOUTHS:
    # segmental arch over the mouth, spandrel to the course above, brick voussoir ring
    arch = S.arch_points(mx - MW / 2, mx + MW / 2, MZ1, "segmental", 12)
    topl = [(p[0], 2.08) for p in arch]
    wall.band(topl, arch, WD, (0, yc, 0))
    ring_out = S.arch_points(mx - MW / 2 - 0.16, mx + MW / 2 + 0.16, MZ1 - 0.05, "segmental", 12)
    iron.band(ring_out, arch, 0.12, (0, WALL_Y - 0.05, 0))
    for sx in (-1, 1):
        iron.box((0.16, 0.12, MZ1 - MZ0 - 0.05), (mx + sx * (MW / 2 + 0.08), WALL_Y - 0.05, MZ0 + (MZ1 - MZ0) * 0.5 - 0.02), bevel=0.015)
    iron.box((MW + 0.5, 0.3, 0.12), (mx, WALL_Y - 0.12, MZ0 - 0.04), bevel=0.02)
    # firebox: dark reveal, glowing coals and fire wall, firebars
    iron.box((MW, 0.9, 0.05), (mx, WALL_Y + 0.45, MZ0 + 0.02), bevel=0)
    fire.box((MW, 0.05, 1.7), (mx, WALL_Y + 0.75, 1.1), bevel=0)
    fire.box((MW - 0.1, 0.5, 0.1), (mx, WALL_Y + 0.45, MZ0 + 0.08), bevel=0.03)
    for k in range(7):
        iron.cyl(0.03, MZ1 - MZ0 + 0.1, (mx - 0.84 + k * 0.28, WALL_Y + 0.08, MZ0 - 0.02), segs=8, bevel=0.008)
    iron.box((MW, 0.07, 0.07), (mx, WALL_Y + 0.08, MZ0 + 0.55), bevel=0.012)
    # sooty iron hood over the mouth
    iron.prism([(-1.35, 0), (1.35, 0), (1.1, 0.75), (-1.1, 0.75)], 0.45, (mx, WALL_Y - 0.17, 2.1), (-14, 0, 0), bevel=0.02)
    # round-headed window above, iron glazing bars, sill
    warch = S.arch_points(mx - WW / 2, mx + WW / 2, WZ1, "round", 10)
    wall.band([(p[0], 5.45) for p in warch], warch, WD, (0, yc, 0))
    wring = S.arch_points(mx - WW / 2 - 0.12, mx + WW / 2 + 0.12, WZ1, "round", 10)
    wall.band(wring, warch, 0.1, (0, WALL_Y - 0.03, 0))
    st.part("windows", WIN).box((WW, 0.04, 5.45 - WZ0), (mx, WALL_Y + 0.5, (WZ0 + 5.45) * 0.5), bevel=0)
    wall.box((WW + 0.3, 0.24, 0.1), (mx, WALL_Y - 0.06, WZ0 - 0.05), bevel=0.02)
    iron.box((0.05, 0.05, 1.75), (mx, WALL_Y + 0.42, WZ0 + 0.85), bevel=0.01)
    for zz in (WZ0 + 0.45, WZ0 + 0.95):
        iron.box((WW, 0.05, 0.05), (mx, WALL_Y + 0.42, zz), bevel=0.01)
# the manifold: a long flanged pipe on wall brackets, fed by the vats, with valve wheels
pipe = st.part("pipes", RUST)
pipe.tube([(-23.5, WALL_Y - 0.45, MAN_Z), (23.5, WALL_Y - 0.45, MAN_Z)], 0.17, 16)
for k in range(17):
    fxm = -22.5 + k * 2.8
    pipe.cyl(0.23, 0.09, (fxm, WALL_Y - 0.45, MAN_Z), (0, 90, 0), segs=16, bevel=0.02)
for k in range(12):
    bx = -22.0 + k * 4.0
    iron.box((0.1, 0.5, 0.08), (bx, WALL_Y - 0.25, MAN_Z - 0.21), bevel=0.012)
    iron.box((0.08, 0.06, 0.4), (bx, WALL_Y - 0.03, MAN_Z - 0.05), bevel=0.01)
for vx_ in (-4.1, 6.9, -12.2):
    pipe.tube([(vx_, WALL_Y - 0.45, MAN_Z + 0.1), (vx_, WALL_Y - 0.45, MAN_Z + 0.42)], 0.06, 10)
    iron.ring(0.17, 0.016, (vx_, WALL_Y - 0.45, MAN_Z + 0.46), (0, 0, 0), segs=18, rsegs=5)
    for k in range(3):
        iron.add(S.geo_cyl(0.01, 0.34, 5, centred=True), (vx_, WALL_Y - 0.45, MAN_Z + 0.46), (90, 0, k * 60))

# pilasters between the bays
for px in (-22.5, -16.3, -10.85, -5.45, 0.1, 5.6, 11.0, 16.4, 22.5):
    wall.box((0.75, 0.3, 6.0), (px, WALL_Y - 0.14, YARD_Z + 3.0), bevel=0.03, segs=2)
    wall.box((0.95, 0.42, 0.3), (px, WALL_Y - 0.18, 0.2), bevel=0.03)
    wall.prism([(-0.375, 0), (0.375, 0), (0.375, 0.1), (0, 0.32), (-0.375, 0.1)], 0.3, (px, WALL_Y - 0.14, 6.0 + YARD_Z), bevel=0.02)
# iron gutter and a pitched roof of rusted sheet behind the parapet, with smoke vents
iron.box((X1 - X0, 0.4, 0.22), (0, WALL_Y + 0.0, 6.42), bevel=0.03)
roof = st.part("roofs", RUST, sharp=40)
for k in range(4):
    x0 = X0 + k * 12.0
    roof.prism([(WALL_Y + 0.6, 6.3), (WALL_Y + 6.0, 9.2), (WALL_Y + 6.2, 9.2), (WALL_Y + 11.0, 6.3)], 11.9, (x0 + 6.0, 0, 0), axis="x", bevel=0.03)
    soot.box((0.8, 0.8, 1.4), (x0 + 4.0, WALL_Y + 5.4, 9.4), bevel=0.04)
    iron.box((1.1, 1.1, 0.1), (x0 + 4.0, WALL_Y + 5.4, 10.15), bevel=0.02)

# smokestacks
for sx, sy, sr in [(-15.0, 20.0, 1.0), (-6.5, 24.0, 1.2), (4.5, 21.0, 0.9), (13.0, 23.0, 1.1), (21.0, 26.0, 1.0)]:
    hgt = 16.0
    soot.box((sr * 2.6, sr * 2.6, 3.0), (sx, sy, 1.5), bevel=0.05, segs=1)
    soot.box((sr * 2.8, sr * 2.8, 0.25), (sx, sy, 3.1), bevel=0.04)
    soot.lathe([(sr * 1.1, 3.2), (sr * 1.02, 4.0), (sr * 0.8, hgt - 1.2), (sr * 0.92, hgt - 0.9), (sr * 0.92, hgt - 0.4),
                (sr * 0.8, hgt - 0.2), (sr * 0.62, hgt), (sr * 0.55, hgt - 0.6), (0, hgt - 0.6)], (sx, sy, 0), segs=24,
               cap_bottom=False)
    for t in (0.35, 0.6, 0.82):
        zz = 4.0 + (hgt - 5.2) * t
        rr = sr * (1.02 - 0.22 * t)
        band.lathe([(rr, zz - 0.08), (rr + 0.04, zz - 0.06), (rr + 0.04, zz + 0.06), (rr, zz + 0.08)], (sx, sy, 0), segs=24,
                   cap_bottom=False, cap_top=False)
    st.fx.append({"kind": "smoke", "pos": [sx, sy, hgt + 0.3], "radius_m": sr * 0.6, "rise_m_s": 1.0, "color": [0.22, 0.18, 0.16, 0.6]})

# the far mill: a long dark range with a few lit windows, closing the view at Y 30
soot.box((70, 2.0, 9.0), (0, 31.0, 4.45), bevel=0.05)
soot.prism([(-35, 9.0), (35, 9.0), (35, 9.4), (-35, 9.4)], 2.4, (0, 31.0, 0), bevel=0.03)
for k in range(28):
    fx_ = -33 + k * 2.4 + R.uniform(-0.3, 0.3)
    for zz in (2.0, 5.2):
        if R.random() < 0.38:
            st.part("windows_far", FARWIN).box((0.9, 0.05, 1.3), (fx_, 29.98, zz), bevel=0)
        else:
            iron.box((0.9, 0.05, 1.3), (fx_, 29.98, zz), bevel=0)

# ------------------------------------------------------------------ the ends of the floor
for side in (-1, 1):
    lx = side * 7.55                       # inner face of the leg at X = +-7.2
    ly = 1.75
    hgt = 8.0
    for sy in (-1, 1):
        iron.box((0.7, 0.05, hgt), (lx, ly + sy * 0.3, hgt * 0.5), bevel=0.012)
    iron.box((0.05, 0.55, hgt), (lx, ly, hgt * 0.5), bevel=0)
    iron.box((1.0, 0.9, 0.06), (lx, ly, 0.15), bevel=0.015)
    kerb.box((1.2, 1.1, 0.16), (lx, ly, 0.04), bevel=0.03)
    rivets(iron, [(lx + sx * 0.25, ly - 0.326, 0.35 + k * 0.28) for sx in (-1, 1) for k in range(16)], r=0.018, h=0.01)
    # second leg further out and a lattice of flat bars between them: a gantry tower
    ox = side * 9.9
    for sy in (-1, 1):
        iron.box((0.5, 0.04, hgt), (ox, ly + sy * 0.22, hgt * 0.5), bevel=0.01)
    iron.box((0.04, 0.4, hgt), (ox, ly, hgt * 0.5), bevel=0)
    span = abs(ox - lx)
    for k in range(5):
        z0 = 0.6 + k * 1.5
        ang = math.degrees(math.atan2(1.5, span - 0.6))
        iron.box((math.hypot(span - 0.6, 1.5), 0.08, 0.05), ((lx + ox) * 0.5, ly - 0.3, z0 + 0.75), (0, -side * ang * (1 if k % 2 else -1), 0), bevel=0.008)
        iron.box((span, 0.08, 0.06), ((lx + ox) * 0.5, ly - 0.3, z0), bevel=0.01)
    # steel drums and tallow barrels stacked beyond the edge of the floor
    drum = st.part("drums", RUST, sharp=50)
    prof = [(0, 0), (0.28, 0), (0.3, 0.02), (0.3, 0.28), (0.312, 0.29), (0.312, 0.31), (0.3, 0.32), (0.3, 0.56),
            (0.312, 0.57), (0.312, 0.59), (0.3, 0.6), (0.3, 0.86), (0.285, 0.88), (0.27, 0.87), (0, 0.86)]
    for dx, dy, dz, rot in [(8.05, 0.45, 0, 0), (8.15, -0.3, 0, 30), (8.1, -0.64, 0, 60), (8.2, 0.05, 0.88, 15)]:
        drum.lathe(prof, (side * dx, dy, dz), (0, 0, rot), segs=18)
    drum.lathe(prof, (side * 9.3, -0.42, 0.31), (90, 0, side * 70), segs=18)
    st.prop("wine_barrel_01", (side * 9.0, 0.55, 0.0), rot_z=side * 25)
    # a short pipe run along the front, low and to the side
    pipe = st.part("pipes", RUST)
    pipe.tube([(side * 15.5, -3.0, 0.09), (side * 4.6, -3.0, 0.09), (side * 4.2, -3.0, -0.2)], 0.09, 10)
    for xx in (5.5, 8.5, 11.5, 14.5):
        iron.box((0.08, 0.3, 0.06), (side * xx, -3.0, 0.03), bevel=0.01)
        pipe.cyl(0.12, 0.06, (side * xx + side * 0.6, -3.0, 0.09), (0, 90, 0), segs=10, bevel=0.01)
    # a coil of chain on the floor near the drums
    link = S.geo_link(0.1, 0.055, 0.009, segs=8, rsegs=4)
    for k in range(26):
        a = k * 0.42
        rr = 0.16 + k * 0.006
        st.part("floor_chain", IRON, sharp=60).add(link, (side * 6.0 + math.cos(a) * rr * side, -2.4 + math.sin(a) * rr, 0.012 + (k // 15) * 0.02),
                                              (90, 0, math.degrees(a) + 90 * (k % 2)))

# hanging lamps under the catwalk (Poly Haven), each with a warm downlight
for lx in (-3.9, 3.9):
    st.prop("hanging_industrial_lamp", (lx, 5.05, CAT_Z - 0.26), rot_z=0)

# wet floor: puddles off the strip that pick up the furnace and slop glows
PUD = st.water("WATER_puddle", (0.012, 0.014, 0.012), 0.03, 0.5, "rain puddles on the plates: near-black, glossy, ripples from the drizzle")
pud = st.part("puddles", PUD, floor=True, smooth=False)
for k, (px_, py_, pr_) in enumerate([(-5.6, 1.9, 0.75), (-1.0, 1.6, 0.5), (2.1, 2.2, 0.85), (6.3, 1.75, 0.6), (-9.8, 2.1, 0.9),
                                     (10.5, 2.3, 0.7), (-4.8, -2.4, 0.55), (5.2, -3.6, 0.7), (-11.5, -2.9, 0.8), (12.0, -1.8, 0.6),
                                     (0.6, 2.55, 0.4), (-13.4, 1.4, 0.5)]):
    pud.add(S.geo_blob(k + 1, pr_, 18, 0.4), (px_, py_, 0.003), (0, 0, R.uniform(0, 360)), (1.0, R.uniform(0.45, 0.7), 1.0))
for k, (px_, py_, pr_) in enumerate([(-2.6, 4.6, 0.7), (2.8, 5.1, 0.6), (-8.3, 5.4, 0.8), (8.3, 5.0, 0.7), (0.4, 9.6, 1.2)]):
    pud.add(S.geo_blob(k + 20, pr_, 16, 0.4), (px_, py_, YARD_Z + 0.003), (0, 0, R.uniform(0, 360)), (1.0, 0.6, 1.0))

# ------------------------------------------------------------------ light
st.set_fog((0.08, 0.09, 0.105), 0.022, 0.05, base_z=0.0,
           note="volumetric fog on: furnace, vat and lamp lights should scatter (Volumetric Scattering ~1-2)")
st.set_ambient((0.36, 0.4, 0.5), 0.35, sky="kloppenheim_07", sky_strength=0.55, sky_rot=200)
st.light("key_warm", "directional", (-6, -6, 10), (1.0, 0.86, 0.72), 3.2, direction=(0.426, 0.609, -0.669), shadows=True,
         source_radius=2.0, note="warm key from the upper left front")
st.light("rim_cool", "directional", (6, 12, 10), (0.5, 0.7, 1.0), 2.6, direction=(-0.209, -0.781, -0.588), affects="not_floor",
         specular=1.5, note="cool rim from behind; exclude floors (lighting channel) so it leaves no glare band")
st.light("fill_camera", "directional", (0, -10, 3), (0.78, 0.84, 0.92), 1.4, direction=(-0.136, 0.968, -0.208), affects="fighters_only",
         specular=0.25, note="soft fill from the camera side, fighters only")
for vi, (x, y, r, h) in enumerate(VATS):
    top = VAT_BASE + h
    st.light(f"vat_glow_{vi + 1}", "point", (x, y - r - 0.5, top + 0.25), (0.5, 1.0, 0.18), 34.0 if vi == 0 else 26.0,
             radius=3.4 + r + 1.0, shadows=(vi == 0), flicker=0.12, flicker_speed=1.7, source_radius=0.4,
             note="signature: green slop glow spilling over the lip")
    st.light(f"firebox_{vi + 1}", "point", (x, y - r - 0.75, 0.35), (1.0, 0.45, 0.12), 5.0, radius=3.0, flicker=0.3,
             flicker_speed=8.0, source_radius=0.2)
for i, (fx_, cd) in enumerate([(-8.2, 46.0), (8.3, 46.0), (-13.5, 30.0), (13.6, 30.0), (0.1, 26.0)]):
    st.light(f"furnace_{i + 1}", "point", (fx_, WALL_Y - 1.4, 1.4), (1.0, 0.5, 0.18), cd, radius=8.0, flicker=0.28,
             flicker_speed=7.0, source_radius=0.6, note="signature: furnace mouths")
for i, lx in enumerate((-3.9, 3.9)):
    st.light(f"lamp_{i + 1}", "spot", (lx, 5.05, CAT_Z - 1.45), (1.0, 0.74, 0.44), 60.0, radius=9.0,
             direction=(0, -0.25, -1), cone=(30, 55), source_radius=0.12, flicker=0.03, flicker_speed=12.0)

st.fx += [
    {"kind": "rain", "note": "drizzle over the whole stage", "box_center": [0, 2, 11], "box_extent": [13, 6, 0.2],
     "velocity": [1.4, 0, -12.5], "streak_m": 0.42, "count": 1200, "color": [0.62, 0.68, 0.72, 0.28]},
    {"kind": "embers", "box_center": [0, 11.4, 2.2], "box_extent": [15, 0.5, 1.8], "count": 70, "rise_m_s": 0.35,
     "color": [1.0, 0.55, 0.2, 1.0]},
] + [{"kind": "steam", "pos": [x, y, VAT_BASE + h + 0.3], "radius_m": r * 0.5, "rise_m_s": 0.6, "size_m": 1.6,
      "color": [0.5, 0.56, 0.48, 0.35]} for (x, y, r, h) in VATS]
st.notes += [
    "The centre vat (X 0, Y 6.6, r 2.0, lip at Z 3.3) is the one the stage finisher uses.",
    "Drain grates in the floor are flush (tops at Z 0) with a faint slop glow under them.",
    "Gantry legs at X +-7.55 put their inner faces on X +-7.2: the visual edge of the floor.",
]

st.finish()
st.facing_report()
if A["render"]:
    # review-only steam over the vats, lit green from below (Unreal: the fx entries)
    for (x, y, r, h) in VATS:
        st.preview_volume((x, y, VAT_BASE + h - 0.2), r * 0.85, 3.2, (0.6, 0.7, 0.6), 0.9, 0.9,
                          emission=((0.4, 1.0, 0.15), 0.25))
    st.render_reviews(samples=24 if A["quick"] else 64)
if A["export"]:
    path, rows, tris = st.export()
    for r_ in rows:
        print("OBJ", r_[0], r_[1], r_[2])
    print("TRIS", tris, "OBJECTS", len(rows))
