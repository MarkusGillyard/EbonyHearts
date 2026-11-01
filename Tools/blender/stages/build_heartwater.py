"""The Heartwater, where the Ebony Heart Fragment waits: a shallow sheet of black water without a shore,
under a sky that is all stars. Broken pillars of black stone stand far out in it. A band of galaxy
crosses the sky behind the strip, and the water holds its light.

The name is provisional.
Run: Tools/blender/stages/run.sh build_heartwater.py [-- --no-render | --quick]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402
from mathutils import Vector  # noqa: E402

A = S.args()
st = S.Stage("heartwater", "The Heartwater", seed=61, ev100=0.6)
R = st.rng
st.music = "music_roost"
st.footstep = "rock"

STONE = st.surface("dark_rock", tint=(0.1, 0.09, 0.14), rough=0.5)
SEABED = st.surface("Rock035", tint=(0.06, 0.05, 0.09), rough=0.5)
WATER = st.water("WATER_heartwater", (0.006, 0.005, 0.012), 0.03, 1.0, "the Heartwater: black, still, a mirror for the stars; "
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

# broken pillars far out, thinning with distance
for i, (x, y, h, r) in enumerate([(-15, 12, 7.0, 1.3), (-9.5, 26, 11.0, 1.8), (-22, 34, 9.0, 1.6), (13, 16, 8.5, 1.5), (19.5, 30, 12.5, 2.0),
                                  (6, 44, 10.0, 1.9), (-3, 60, 14.0, 2.4), (30, 58, 13.0, 2.2), (-34, 62, 15.0, 2.5)]):
    tilt = (R.uniform(-5, 5), R.uniform(-6, 6), R.uniform(0, 360))
    stone.cyl(r, h, (x, y, h * 0.5 - 0.3), rot=tilt, segs=7, r2=r * R.uniform(0.55, 0.8))
    stone.rock((x + R.uniform(-1, 1), y + 1.0, 0.1), (r * 1.6, r * 1.3, 0.9), 400 + i, (0, 0, R.uniform(0, 360)), subdiv=1, rough=0.3)
    stone.rock((x, y, h - 0.2), (r * 0.8, r * 0.8, 0.8), 500 + i, (R.uniform(-20, 20), R.uniform(-20, 20), R.uniform(0, 360)), subdiv=1, rough=0.4)

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
st.set_ambient((0.3, 0.26, 0.5), 0.3)
st.light("key_star", "directional", (-6, -6, 10), (0.72, 0.72, 1.0), 2.2, direction=(0.383, 0.663, -0.643), shadows=True,
         note="cold starlight from the upper left front")
st.light("rim_galaxy", "directional", (0, 20, 10), (0.6, 0.3, 1.0), 3.4, direction=(-0.081, -0.924, -0.375), affects="not_floor",
         note="violet rim from the galaxy band")
st.light("fill_camera", "directional", (0, -10, 3), (0.75, 0.75, 0.95), 1.3, direction=(-0.136, 0.968, -0.208), affects="fighters_only",
         note="camera side fill so the fighters read")
for i, lx in enumerate((-7.0, 7.0)):
    st.light(f"sheen_{i + 1}", "point", (lx, 6.0, 1.6), (0.5, 0.25, 1.0), 26.0, radius=11.0, flicker=0.08, flicker_speed=0.6, source_radius=1.0,
             specular=0.3, note="violet light lying on the water")
st.fx += [
    {"kind": "motes", "note": "sparks of Ebony Energy rising from the water", "box_center": [0, 4.0, 1.4], "box_extent": [15, 5, 1.4], "count": 60,
     "color": [0.55, 0.3, 1.0, 0.9], "size_m": 0.03, "rise_m_s": 0.18},
    {"kind": "mist", "center": [0, 14.0, 0.4], "size": [90, 14], "color": [0.1, 0.06, 0.2], "density": 0.25},
]
st.notes += ["No shore on purpose: the strip's ends are held by the game's walls, as on every stage."]

st.finish()
st.facing_report()
if A["render"]:
    st.render_reviews(samples=16 if A["quick"] else 32)
if A["export"]:
    path, rows, tris = st.export()
    for r_ in rows:
        print("OBJ", r_[0], r_[1], r_[2])
    print("TRIS", tris, "OBJECTS", len(rows))
