"""The Fragmented Palace: the sole entrance, where Hollis Fathom keeps his watch. A shallow sheet of black water,
under a sky of stars, and the palace rising broken out of it behind the strip: great shards of black stone
hanging apart, one tall door between them, and a low step before it where he meditates.

How it looks is provisional.
Run: Tools/blender/stages/run.sh build_palace_gate.py [-- --no-render | --quick]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402
from mathutils import Vector  # noqa: E402

A = S.args()
st = S.Stage("palace_gate", "The Fragmented Palace", seed=67, ev100=0.6)
R = st.rng
st.music = "music_roost"
st.footstep = "rock"

STONE = st.surface("dark_rock", tint=(0.1, 0.09, 0.14), rough=0.5)
SEABED = st.surface("Rock035", tint=(0.06, 0.05, 0.09), rough=0.5)
WATER = st.water("WATER_palace", (0.006, 0.005, 0.012), 0.03, 1.0, "black and still before the door, a mirror for the stars; "
                 "slow ripples, a faint violet sheen", emissive=((0.3, 0.1, 0.8), 0.02), ripple=0.008, ripple_scale=0.03)
STARS = st.glow("GLOW_stars", (0.85, 0.85, 1.0), 26.0, "stars on the far dome")
BAND = st.glow("GLOW_galaxy", (0.42, 0.14, 0.95), 2.6, "the galaxy band across the sky", pattern=0.6, scale=9.0)
BAND2 = st.glow("GLOW_galaxy_core", (0.85, 0.5, 0.95), 5.0, "its bright core", pattern=0.5, scale=6.0)

floor = st.part("seabed", SEABED, floor=True, sharp=35)
water = st.part("water", WATER, floor=True, smooth=False, surface_z=0.035)
stone = st.part("pillars", STONE, sharp=55)
stars = st.part("stars", STARS, shadows=False, smooth=False)
band = st.part("galaxy", BAND, shadows=False, smooth=True)
core = st.part("galaxy_core", BAND2, shadows=False, smooth=True)

# the floor and the water over it run out to the horizon on every side
floor.box((400, 300, 1.0), (0, 110, -0.5), bevel=0)
water.box((400, 300, 0.02), (0, 110, 0.025), bevel=0)

# the door: two towers of black stone, the lintel floating apart from them, a line of violet light down its middle
for sd in (-1, 1):
    stone.box((3.2, 3.0, 16.0), (sd * 4.4, 15.0, 7.8), rot=(0, sd * -2.0, 0), bevel=0.12)
    stone.box((2.0, 2.4, 7.0), (sd * 7.6, 15.2, 3.3), rot=(0, sd * 3.0, sd * 8.0), bevel=0.1)
stone.box((11.0, 2.6, 2.0), (0.2, 15.0, 18.0), rot=(0, 2.5, 0), bevel=0.1)
seam = st.part("door_light", st.glow("GLOW_door", (0.5, 0.18, 1.0), 16.0, "the line of light between the doors"), shadows=False, smooth=False)
seam.box((0.09, 0.09, 15.5), (0.0, 13.9, 7.8), bevel=0)
stone.box((5.8, 0.6, 15.6), (0.0, 14.3, 7.8), bevel=0.04)
# the step before the door, where he sits
stone.box((6.4, 2.2, 0.32), (0.0, 7.2, 0.14), bevel=0.05)
stone.box((4.6, 1.5, 0.3), (0.0, 7.6, 0.42), bevel=0.05)
# the palace: great shards hanging apart overhead and behind, more of them with distance
for i in range(46):
    x = R.uniform(-46, 46)
    y = R.uniform(18, 70)
    z = R.uniform(3, 34) + abs(x) * 0.15
    w = R.uniform(1.6, 6.0)
    stone.box((w, R.uniform(1.6, 4.5), w * R.uniform(1.2, 3.6)), (x, y, z), rot=(R.uniform(-22, 22), R.uniform(-22, 22), R.uniform(0, 90)), bevel=0.06)

# the stars: small far diamonds on a dome before and above the strip
DOME = 150.0
for i in range(1400):
    az = R.uniform(math.radians(8), math.radians(172))
    el = R.uniform(math.radians(1), math.radians(62))
    d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el) + 0.1, math.sin(el))) * DOME
    s = R.choice((0.12, 0.16, 0.2, 0.28, 0.42))
    stars.box((s, s, s), (d.x, d.y + 40, d.z), rot=(45, 45, 0), bevel=0)

# the galaxy: long soft lobes laid along a slanted arc
for i in range(64):
    t = i / 63.0
    x = -150 + 300 * t
    z = 16 + 36 * math.sin(t * math.pi * 0.9 + 0.2) + R.uniform(-2.5, 2.5)
    y = 165 + R.uniform(-8, 8)
    w = (10 + 22 * math.sin(t * math.pi)) * R.uniform(0.7, 1.2)
    band.rock((x, y, z), (w, 3.0, w * 0.34), 900 + i, (0, R.uniform(-16, -4), 0), subdiv=2, rough=0.3)
    if 0.25 < t < 0.75:
        core.rock((x, y - 4, z), (w * 0.42, 2.0, w * 0.12), 1000 + i, (0, R.uniform(-16, -4), 0), subdiv=2, rough=0.3)

# ------------------------------------------------------------------ light
st.set_fog((0.03, 0.02, 0.06), 0.006, 0.05, base_z=0.0, note="thin violet haze over the water, the pillars fading into it")
st.set_ambient((0.42, 0.38, 0.62), 0.7)
st.light("key_star", "directional", (-6, -6, 10), (0.78, 0.78, 1.0), 3.6, direction=(0.383, 0.663, -0.643), shadows=True,
         note="cold starlight from the upper left front")
st.light("rim_galaxy", "directional", (0, 20, 10), (0.66, 0.36, 1.0), 6.0, direction=(-0.081, -0.924, -0.375), affects="not_floor",
         note="violet rim from the galaxy band")
st.light("fill_camera", "directional", (0, -10, 3), (0.82, 0.8, 1.0), 2.6, direction=(-0.136, 0.968, -0.208), affects="fighters_only",
         note="camera side fill so the fighters read")
for i, lx in enumerate((-7.0, 7.0)):
    st.light(f"sheen_{i + 1}", "point", (lx, 6.0, 1.6), (0.5, 0.25, 1.0), 26.0, radius=11.0, flicker=0.08, flicker_speed=0.6, source_radius=1.0,
             specular=0.3, note="violet light lying on the water")
st.fx += [
    {"kind": "motes", "note": "sparks of Ebony Energy before the door", "box_center": [0, 4.0, 1.4], "box_extent": [15, 5, 1.4], "count": 60,
     "color": [0.55, 0.3, 1.0, 0.9], "size_m": 0.03, "rise_m_s": 0.18},
    {"kind": "mist", "center": [0, 14.0, 0.4], "size": [90, 14], "color": [0.1, 0.06, 0.2], "density": 0.25},
]
st.notes += ["The door stands on the strip's centre line, 14 m back."]

st.finish()
st.facing_report()
if A["render"]:
    st.render_reviews(samples=16 if A["quick"] else 32)
if A["export"]:
    path, rows, tris = st.export()
    for r_ in rows:
        print("OBJ", r_[0], r_[1], r_[2])
    print("TRIS", tris, "OBJECTS", len(rows))
