"""Shared building blocks for the Mortal Slopbat stage sets (Blender 5.2, headless; see run.sh).

Coordinates: Blender metres, Z up. The fighting plane is XZ at Y = 0; fighters walk along X
(-7..+7) on a floor at Z = 0; the camera stands at -Y looking toward +Y; scenery runs toward +Y.

A stage script builds geometry into Parts (one mesh object each, usually one material), adds
lights, glows, water, props and animated pieces, then calls Stage.finish(), Stage.render_reviews()
and Stage.export(). Every Part gets world-scale UVs (1 UV unit per metre), smooth shading with
sharp edges above its crease angle, and a weighted-normal modifier so bevelled edges catch light.

Units in the JSON (and in the light calls here) are Unreal's: candela for point and spot lights,
lux for directional lights, cd/m2 (nits) for emissive glows, all for a manual exposure of
EV100 = Stage.ev100. Blender values are derived from them: W = cd * 4pi / 683, sun = lux / 683,
emission = nits / 683, and the review renders use a film exposure of log2(683 / 1.2) - EV100
stops, so what the renders show is what those Unreal values should give.
"""
import bpy
import bmesh
import json
import math
import os
import random
import sys
import time
from mathutils import Euler, Matrix, Vector, noise

HERE = os.path.dirname(os.path.abspath(__file__))
UE_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SURF = os.path.join(UE_ROOT, "SourceArt", "Surfaces")
OUT_ROOT = os.path.join(UE_ROOT, "SourceArt", "Stages")
PH_MODELS = os.path.join(UE_ROOT, "SourceArt", "ThirdParty", "PolyHaven", "models")
PH_HDRI = os.path.join(UE_ROOT, "SourceArt", "ThirdParty", "PolyHaven", "hdris")

# Real-world width of one texture repeat, metres (Poly Haven / ambientCG metadata where given;
# the rest are judged from the images). Unreal M_Surface "Tiling" = 1 / size with these UVs.
SURFACE_SIZE = {
    "MetalPlates013": 1.6, "Wood051": 0.8, "Rope003": 0.35, "Rock035": 2.0, "Ivory001B": 0.6,
    "Ground051": 2.0, "Ground037": 2.1, "factory_brick": 1.5, "metal_plate": 0.5,
    "rusty_metal_04": 2.0, "monastery_stone_floor": 1.8, "medieval_blocks_03": 2.0,
    "mossy_stone_wall": 2.0, "weathered_planks": 2.0, "moss_wood": 1.0, "hessian_380": 0.274,
    "brown_mud_03": 1.3, "dark_rock": 2.42, "Leather021": 0.5, "Leather026": 0.5,
    "Fabric040": 0.5, "Fabric001": 0.5, "Fabric066": 0.4, "Metal042A": 1.0, "Metal055A": 1.0,
}
LUMENS_PER_WATT = 683.0
STRIP = dict(x=7.5, y0=-1.0, y1=1.0)          # walkable strip kept flat and empty


def log(*a):
    print("[stage]", *a, flush=True)


def V(*a):
    return Vector(a[0]) if len(a) == 1 else Vector(a)


def xform(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    """4x4 from a location, an XYZ Euler rotation in degrees and a scale."""
    if not isinstance(scale, (tuple, list, Vector)):
        scale = (scale, scale, scale)
    r = Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4()
    s = Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))
    return Matrix.Translation(Vector(loc)) @ r @ s


def look_rot(direction, up=(0, 0, 1)):
    """Rotation matrix whose local +Z points along `direction`."""
    d = Vector(direction).normalized()
    return d.to_track_quat("Z", "Y" if abs(d.dot(Vector(up))) > 0.99 else "Y").to_matrix()


# ------------------------------------------------------------------------------ mesh building
class Geo:
    """A local-space piece under construction: vertices, faces and per-loop UVs."""

    def __init__(self):
        self.v = []
        self.f = []
        self.uv = []          # per face: list of (u, v) or None (box-projected later)
        self.mi = []          # per face: material slot name or None (the Part's default)

    def vert(self, co):
        self.v.append(Vector(co))
        return len(self.v) - 1

    def face(self, idx, uvs=None, expect=None, mat=None):
        """Adds a face; flips it if its normal points away from `expect`."""
        if len(set(idx)) < 3:
            return
        if expect is not None:
            n = self.normal(idx)
            if n.dot(Vector(expect)) < 0:
                idx = list(reversed(idx))
                if uvs:
                    uvs = list(reversed(uvs))
        self.f.append(list(idx))
        self.uv.append(uvs)
        self.mi.append(mat)

    def normal(self, idx):
        n = Vector((0, 0, 0))
        pts = [self.v[i] for i in idx]
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            n.x += (a.y - b.y) * (a.z + b.z)
            n.y += (a.z - b.z) * (a.x + b.x)
            n.z += (a.x - b.x) * (a.y + b.y)
        return n.normalized() if n.length > 1e-12 else n

    def extend(self, other, m=None, mat=None):
        base = len(self.v)
        if m is None:
            self.v.extend(Vector(p) for p in other.v)
        else:
            self.v.extend(m @ Vector(p) for p in other.v)
        for f, uv, mi in zip(other.f, other.uv, other.mi):
            self.f.append([base + i for i in f])
            self.uv.append(uv)
            self.mi.append(mi if mi is not None else mat)
        return self

    @staticmethod
    def from_bm(bm, uv_layer=True):
        g = Geo()
        bm.verts.index_update()
        g.v = [v.co.copy() for v in bm.verts]
        uvl = bm.loops.layers.uv.active if uv_layer else None
        for f in bm.faces:
            g.f.append([v.index for v in f.verts])
            g.uv.append([tuple(l[uvl].uv) for l in f.loops] if uvl else None)
            g.mi.append(None)
        return g

    def to_bm(self):
        bm = bmesh.new()
        vs = [bm.verts.new(p) for p in self.v]
        uvl = bm.loops.layers.uv.new("UVMap")
        for f, uv in zip(self.f, self.uv):
            try:
                bf = bm.faces.new([vs[i] for i in f])
            except ValueError:
                continue
            if uv:
                for l, t in zip(bf.loops, uv):
                    l[uvl].uv = t
        return bm


def _bevel_geo(g, offset, segs=1, min_angle=0.6, uv_mode="box"):
    """Bevels the edges of `g` whose faces meet at more than `min_angle` radians."""
    bm = g.to_bm()
    bm.normal_update()
    edges = [e for e in bm.edges if len(e.link_faces) == 2 and
             e.link_faces[0].normal.angle(e.link_faces[1].normal, 0.0) > min_angle]
    edges += [e for e in bm.edges if len(e.link_faces) == 1]
    if edges:
        bmesh.ops.bevel(bm, geom=edges, offset=offset, offset_type="OFFSET", segments=segs,
                        profile=0.5, affect="EDGES", clamp_overlap=True)
    out = Geo.from_bm(bm, uv_layer=False)
    bm.free()
    return out


def geo_box(sx, sy, sz, bevel=0.0, segs=1, drop=()):
    """Box centred on the origin; `drop` lists faces to leave out: '-z', '+y', ..."""
    g = Geo()
    hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
    p = [g.vert((x, y, z)) for z in (-hz, hz) for y in (-hy, hy) for x in (-hx, hx)]
    sides = {"-z": ([0, 2, 3, 1], (0, 0, -1)), "+z": ([4, 5, 7, 6], (0, 0, 1)),
             "-y": ([0, 1, 5, 4], (0, -1, 0)), "+y": ([2, 6, 7, 3], (0, 1, 0)),
             "-x": ([0, 4, 6, 2], (-1, 0, 0)), "+x": ([1, 3, 7, 5], (1, 0, 0))}
    for k, (idx, n) in sides.items():
        if k not in drop or bevel > 0:
            g.face([p[i] for i in idx], expect=n)
    if bevel > 0:
        g = _bevel_geo(g, min(bevel, 0.45 * min(sx, sy, sz)), segs)
        if drop:
            keep = []
            for i, f in enumerate(g.f):
                n = g.normal(f)
                if any(n.dot(Vector(sides[k][1])) > 0.999 for k in drop):
                    continue
                keep.append(i)
            g.f = [g.f[i] for i in keep]
            g.uv = [g.uv[i] for i in keep]
            g.mi = [g.mi[i] for i in keep]
    return g


def geo_lathe(profile, segs=16, cap_bottom=True, cap_top=True, arc=360.0, u_radius=None, phase=0.0):
    """Surface of revolution about Z from `profile` [(r, z), ...] (bottom to top). Points with
    r == 0 close to a pole. UVs: u around (metres at u_radius), v along the profile (metres)."""
    g = Geo()
    full = arc >= 359.999
    n = segs if full else segs + 1
    angs = [math.radians(phase) + math.radians(arc) * j / segs for j in range(n)]
    rmax = u_radius or max(r for r, z in profile)
    rings = []
    for r, z in profile:
        if r < 1e-6:
            rings.append([g.vert((0, 0, z))])
        else:
            rings.append([g.vert((r * math.cos(a), r * math.sin(a), z)) for a in angs])
    vs = [0.0]
    for i in range(1, len(profile)):
        (r0, z0), (r1, z1) = profile[i - 1], profile[i]
        vs.append(vs[-1] + math.hypot(r1 - r0, z1 - z0))
    cu = 2 * math.pi * rmax * (arc / 360.0)
    us = [cu * j / segs for j in range(segs + 1)]
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for j in range(segs):
            j1 = (j + 1) % n if full else j + 1
            if len(a) == 1 and len(b) == 1:
                continue
            if len(a) == 1:
                # pole first: (pole, next, this) faces along tangent x profile direction
                g.face([a[0], b[j1], b[j]], [((us[j] + us[j + 1]) * 0.5, vs[i]), (us[j + 1], vs[i + 1]), (us[j], vs[i + 1])])
            elif len(b) == 1:
                g.face([a[j], a[j1], b[0]], [(us[j], vs[i]), (us[j + 1], vs[i]), ((us[j] + us[j + 1]) * 0.5, vs[i + 1])])
            else:
                g.face([a[j], a[j1], b[j1], b[j]],
                       [(us[j], vs[i]), (us[j + 1], vs[i]), (us[j + 1], vs[i + 1]), (us[j], vs[i + 1])])
    if full:
        r0, z0 = profile[0]
        if cap_bottom and r0 > 1e-6:
            ring = rings[0]
            g.face(list(reversed(ring)), [(p.x, p.y) for p in (g.v[i] for i in reversed(ring))], expect=(0, 0, -1))
        r1, z1 = profile[-1]
        if cap_top and r1 > 1e-6:
            ring = rings[-1]
            g.face(list(ring), [(g.v[i].x, g.v[i].y) for i in ring], expect=(0, 0, 1))
    return g


def geo_cyl(r, h, segs=16, r2=None, bevel=0.0, caps=(True, True), centred=False):
    """Cylinder (or cone frustum) along Z from 0 to h; a chamfer of `bevel` on capped rims."""
    r2 = r if r2 is None else r2
    b = min(bevel, 0.4 * min(r, r2 if r2 > 0 else r, h)) if bevel > 0 else 0.0
    prof = []
    if caps[0]:
        prof.append((0.0, 0.0))
        if b:
            prof += [(r - b, 0.0), (r, b)]
        else:
            prof.append((r, 0.0))
    else:
        prof.append((r, 0.0))
    if caps[1]:
        if b:
            prof += [(r2, h - b), (r2 - b, h)]
        else:
            prof.append((r2, h))
        prof.append((0.0, h))
    else:
        prof.append((r2, h))
    g = geo_lathe(prof, segs, cap_bottom=False, cap_top=False)
    # Caps from pole fans get planar UVs (x, y) instead of the lathe's polar ones.
    for i, f in enumerate(g.f):
        n = g.normal(f)
        if abs(n.z) > 0.95:
            g.uv[i] = [(g.v[k].x, g.v[k].y) for k in f]
    if centred:
        for p in g.v:
            p.z -= h * 0.5
    return g


def geo_tube(pts, radius, segs=8, caps=True, closed=False, twist=0.0):
    """A tube swept along the polyline `pts` (parallel-transport frames). `radius` may be a list
    (one per point). UVs: u around (metres), v along (metres)."""
    pts = [Vector(p) for p in pts]
    if closed and (pts[0] - pts[-1]).length < 1e-6:
        pts = pts[:-1]
    n = len(pts)
    rad = list(radius) if isinstance(radius, (list, tuple)) else [radius] * n
    tans = []
    for i in range(n):
        if closed:
            t = pts[(i + 1) % n] - pts[i - 1]
        else:
            t = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        tans.append(t.normalized())
    t0 = tans[0]
    ref = Vector((0, 0, 1)) if abs(t0.z) < 0.9 else Vector((1, 0, 0))
    nrm = (ref - t0 * ref.dot(t0)).normalized()
    frames = []
    for i in range(n):
        if i > 0:
            q = tans[i - 1].rotation_difference(tans[i])
            nrm = q @ nrm
            nrm = (nrm - tans[i] * nrm.dot(tans[i])).normalized()
        frames.append((nrm.copy(), tans[i].cross(nrm).normalized()))
    g = Geo()
    rings = []
    for i in range(n):
        nn, bb = frames[i]
        ring = []
        for j in range(segs):
            a = 2 * math.pi * j / segs + twist * i
            ring.append(g.vert(pts[i] + rad[i] * (math.cos(a) * nn + math.sin(a) * bb)))
        rings.append(ring)
    vs = [0.0]
    for i in range(1, n):
        vs.append(vs[-1] + (pts[i] - pts[i - 1]).length)
    if closed:
        vs.append(vs[-1] + (pts[0] - pts[-1]).length)
    cu = 2 * math.pi * (sum(rad) / n)
    us = [cu * j / segs for j in range(segs + 1)]
    last = n if closed else n - 1
    for i in range(last):
        a, b = rings[i], rings[(i + 1) % n]
        for j in range(segs):
            j1 = (j + 1) % segs
            mid = (g.v[a[j]] + g.v[b[j1]]) * 0.5
            axis_mid = (pts[i] + pts[(i + 1) % n]) * 0.5
            g.face([a[j], a[j1], b[j1], b[j]],
                   [(us[j], vs[i]), (us[j + 1], vs[i]), (us[j + 1], vs[i + 1]), (us[j], vs[i + 1])],
                   expect=mid - axis_mid)
    if caps and not closed:
        g.face(list(rings[0]), [(g.v[k] - pts[0]).to_tuple()[:2] for k in rings[0]], expect=-tans[0])
        g.face(list(rings[-1]), [(g.v[k] - pts[-1]).to_tuple()[:2] for k in rings[-1]], expect=tans[-1])
    return g


def geo_ring(R, r, segs=16, rsegs=6, scale=(1, 1, 1)):
    """A torus about Z (major radius R, minor r), optionally squashed."""
    pts = [Vector((R * math.cos(a) * scale[0], R * math.sin(a) * scale[1], 0))
           for a in (2 * math.pi * k / segs for k in range(segs))]
    return geo_tube(pts, r, rsegs, closed=True)


def geo_link(length, width, wire, segs=10, rsegs=5):
    """A chain link: a stadium loop in the XZ plane, long along Z, centred on the origin."""
    pts = []
    straight = max(0.0, length - width) * 0.5
    rr = width * 0.5 - wire
    for k in range(segs):
        a = 2 * math.pi * k / segs
        x = rr * math.cos(a)
        z = rr * math.sin(a) + (straight if math.sin(a) >= 0 else -straight)
        pts.append(Vector((x, 0, z)))
    return geo_tube(pts, wire, rsegs, closed=True)


def geo_rock(seed, subdiv=2, rough=0.35, flat=-0.35, freq=1.3):
    """A lumpy faceted boulder of radius ~1 (scale it with the transform)."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    off = Vector((seed * 7.13, seed * 3.71, seed * 1.93))
    for v in bm.verts:
        d = v.co.normalized()
        k = noise.noise(d * freq + off) * rough + noise.noise(d * freq * 2.7 + off * 1.7) * rough * 0.4
        v.co = d * (1.0 + k)
        if flat is not None and v.co.z < flat:
            v.co.z = flat + (v.co.z - flat) * 0.25
    g = Geo.from_bm(bm, uv_layer=False)
    bm.free()
    return g


def geo_prism(poly, depth, bevel=0.0, segs=1, axis="y"):
    """Polygon `poly` [(a, b), ...] extruded by `depth`, centred. axis 'y': poly in XZ (a=x,
    b=z), extruded along Y; axis 'z': poly in XY, extruded along Z; axis 'x': poly in YZ."""
    g = Geo()
    h = depth * 0.5

    def P(a, b, c):
        if axis == "y":
            return (a, c, b)
        if axis == "z":
            return (a, b, c)
        return (c, a, b)
    front = [g.vert(P(a, b, -h)) for a, b in poly]
    back = [g.vert(P(a, b, h)) for a, b in poly]
    nax = {"y": (0, 1, 0), "z": (0, 0, 1), "x": (1, 0, 0)}[axis]
    g.face(front, expect=[-c for c in nax])
    g.face(list(reversed(back)), expect=nax)
    m = len(poly)
    ccw = _poly_area(poly) > 0
    for i in range(m):
        j = (i + 1) % m
        idx = [front[i], front[j], back[j], back[i]]
        # outward = perpendicular to the edge in the outline's own plane, away from the interior,
        # mapped to 3D like the points (each axis mapping has its own handedness)
        ea, eb = poly[j][0] - poly[i][0], poly[j][1] - poly[i][1]
        oa, ob = (eb, -ea) if ccw else (-eb, ea)
        g.face(idx, expect=P(oa, ob, 0.0))
    if bevel > 0:
        g = _bevel_geo(g, bevel, segs)
    return g


def _poly_area(poly):
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    return a * 0.5


def geo_band(outer, inner, depth, closed=False, axis="y"):
    """The strip between two matching polylines (in XZ for axis 'y'), extruded by `depth`:
    arch rings, tracery, mouldings, spandrels."""
    g = Geo()
    h = depth * 0.5

    def P(p, c):
        a, b = p
        return (a, c, b) if axis == "y" else (a, b, c)
    nax = Vector((0, 1, 0)) if axis == "y" else Vector((0, 0, 1))
    of = [g.vert(P(p, -h)) for p in outer]
    ob = [g.vert(P(p, h)) for p in outer]
    inf = [g.vert(P(p, -h)) for p in inner]
    ib = [g.vert(P(p, h)) for p in inner]
    m = len(outer)
    last = m if closed else m - 1
    for i in range(last):
        j = (i + 1) % m
        g.face([of[i], of[j], inf[j], inf[i]], expect=-nax)
        g.face([ob[i], ob[j], ib[j], ib[i]], expect=nax)
        mo = (g.v[of[i]] + g.v[of[j]]) * 0.5
        mi = (g.v[inf[i]] + g.v[inf[j]]) * 0.5
        g.face([of[i], of[j], ob[j], ob[i]], expect=mo - mi)
        g.face([inf[i], inf[j], ib[j], ib[i]], expect=mi - mo)
    if not closed:
        for i, d in ((0, 1), (m - 1, -1)):
            k = i + d
            t = (g.v[of[i]] + g.v[inf[i]]) * 0.5 - (g.v[of[k]] + g.v[inf[k]]) * 0.5
            g.face([of[i], inf[i], ib[i], ob[i]], expect=t)
    return g


def arch_points(x0, x1, z, kind="pointed", n=10):
    """Intrados points from the left spring (x0, z) over the crown to (x1, z)."""
    span = x1 - x0
    pts = []
    if kind == "round":
        r = span * 0.5
        cx = x0 + r
        for k in range(n + 1):
            a = math.pi - math.pi * k / n
            pts.append((cx + r * math.cos(a), z + r * math.sin(a)))
        return pts
    if kind == "segmental":
        rise = span * 0.18
        r = (span * span / 4 + rise * rise) / (2 * rise)
        cx, cz = x0 + span * 0.5, z + rise - r
        a0 = math.atan2(z - cz, x0 - cx)
        a1 = math.atan2(z - cz, x1 - cx)
        for k in range(n + 1):
            a = a0 + (a1 - a0) * k / n
            pts.append((cx + r * math.cos(a), cz + r * math.sin(a)))
        return pts
    # pointed (equilateral): each half is an arc centred on the opposite spring
    half = n // 2
    for k in range(half + 1):
        a = math.pi - (math.pi / 3) * k / half
        pts.append((x1 + span * math.cos(a), z + span * math.sin(a)))
    for k in range(half - 1, -1, -1):
        a = (math.pi / 3) * k / half
        pts.append((x0 + span * math.cos(a), z + span * math.sin(a)))
    return pts


def geo_blob(seed, r=1.0, segs=16, wobble=0.35, h=0.0):
    """A flat irregular disc (puddle, pool, spill) in XY at z = 0, its rim raised by `h`."""
    g = Geo()
    c = g.vert((0, 0, h * 0.5))
    off = Vector((seed * 3.1, seed * 1.7, 0.0))
    ring = []
    for k in range(segs):
        a = 2 * math.pi * k / segs
        d = Vector((math.cos(a), math.sin(a), 0))
        k_ = 1.0 + wobble * noise.noise(d * 1.4 + off) + wobble * 0.5 * noise.noise(d * 3.3 + off)
        ring.append(g.vert((d.x * r * k_, d.y * r * k_, 0.0)))
    for k in range(segs):
        g.face([c, ring[k], ring[(k + 1) % segs]], expect=(0, 0, 1))
    return g


def catenary(a, b, sag, n=10):
    """Points of a rope hanging from a to b, dipping `sag` metres at the middle (parabola)."""
    a, b = Vector(a), Vector(b)
    return [a.lerp(b, t) - Vector((0, 0, sag * 4 * t * (1 - t))) for t in (k / n for k in range(n + 1))]


# ------------------------------------------------------------------------------ parts
class Part:
    """One exported mesh object. Geometry is added in world space (metres)."""

    def __init__(self, stage, name, mat, sharp=50.0, uv_rot=False, weighted=True, shadows=True,
                 floor=False, pivot=None, smooth=True, surface_z=0.0005):
        self.stage = stage
        self.name = name
        self.mat = mat
        self.sharp = sharp
        self.uv_rot = uv_rot
        self.weighted = weighted
        self.shadows = shadows
        self.floor = floor
        self.pivot = Vector(pivot) if pivot is not None else None
        self.smooth = smooth
        self.surface_z = surface_z
        self.geo = Geo()
        self.obj = None

    # every builder takes a world transform (loc, rot degrees, scale) or a ready matrix `m`
    def add(self, g, loc=(0, 0, 0), rot=(0, 0, 0), scale=1.0, m=None, mat=None, uv=None):
        M = m if m is not None else xform(loc, rot, scale)
        base = len(self.geo.v)
        self.geo.v.extend(M @ Vector(p) for p in g.v)
        n3 = M.to_3x3()
        for f, fuv, fm in zip(g.f, g.uv, g.mi):
            idx = [base + i for i in f]
            if uv == "box" or fuv is None:
                fuv = self._box_uv(idx)
            elif uv == "scaled":
                s = (n3 @ Vector((1, 0, 0))).length
                fuv = [(a * s, b * s) for a, b in fuv]
            self.geo.f.append(idx)
            self.geo.uv.append(fuv)
            self.geo.mi.append(mat if mat is not None else fm)
        return self

    def _box_uv(self, idx):
        pts = [self.geo.v[i] for i in idx]
        n = self.geo.normal(idx)
        ax = max(range(3), key=lambda k: abs(n[k]))
        out = []
        for p in pts:
            if ax == 2:
                u, v = (p.x, p.y) if n.z > 0 else (p.x, -p.y)
                if self.uv_rot:
                    u, v = v, -u
            elif ax == 1:
                u, v = (p.x if n.y < 0 else -p.x), p.z
            else:
                u, v = (p.y if n.x > 0 else -p.y), p.z
            out.append((u, v))
        return out

    def box(self, size, loc, rot=(0, 0, 0), bevel=0.02, segs=1, drop=(), mat=None):
        return self.add(geo_box(*size, bevel=bevel, segs=segs, drop=drop), loc, rot, mat=mat, uv="box")

    def cyl(self, r, h, loc, rot=(0, 0, 0), segs=16, r2=None, bevel=0.0, caps=(True, True), mat=None, uv=None):
        return self.add(geo_cyl(r, h, segs, r2, bevel, caps), loc, rot, mat=mat, uv=uv)

    def lathe(self, profile, loc, rot=(0, 0, 0), segs=16, mat=None, scale=1.0, **kw):
        return self.add(geo_lathe(profile, segs, **kw), loc, rot, scale, mat=mat,
                        uv="scaled" if scale != 1.0 else None)

    def tube(self, pts, r, segs=8, caps=True, closed=False, mat=None):
        return self.add(geo_tube(pts, r, segs, caps, closed), mat=mat)

    def ring(self, R, r, loc, rot=(0, 0, 0), segs=16, rsegs=6, mat=None):
        return self.add(geo_ring(R, r, segs, rsegs), loc, rot, mat=mat)

    def rock(self, loc, size, seed, rot=None, subdiv=2, rough=0.35, flat=-0.35, mat=None):
        rot = rot if rot is not None else (0, 0, (seed * 47.0) % 360)
        return self.add(geo_rock(seed, subdiv, rough, flat), loc, rot, size, mat=mat, uv="box")

    def prism(self, poly, depth, loc=(0, 0, 0), rot=(0, 0, 0), bevel=0.0, axis="y", mat=None):
        return self.add(geo_prism(poly, depth, bevel, axis=axis), loc, rot, mat=mat, uv="box")

    def band(self, outer, inner, depth, loc=(0, 0, 0), rot=(0, 0, 0), closed=False, mat=None, axis="y"):
        return self.add(geo_band(outer, inner, depth, closed, axis), loc, rot, mat=mat, uv="box")

    def rope(self, a, b, sag, r=0.016, n=10, segs=6, mat=None):
        return self.tube(catenary(a, b, sag, n), r, segs, caps=False, mat=mat)

    def tris(self):
        return sum(len(f) - 2 for f in self.geo.f)

    # ---------------------------------------------------------------- object
    def build(self):
        g = self.geo
        if not g.f:
            return None
        me = bpy.data.meshes.new(self.name)
        piv = self.pivot if self.pivot is not None else Vector((0, 0, 0))
        me.from_pydata([p - piv for p in g.v], [], g.f)
        uvl = me.uv_layers.new(name="UVMap")
        flat = []
        for fuv in g.uv:
            flat.extend(fuv)
        if len(flat) == len(me.loops):
            uvl.data.foreach_set("uv", [c for t in flat for c in t])
        # material slots: default first, then any per-face overrides
        names = [self.mat] + sorted({m for m in g.mi if m and m != self.mat})
        for n in names:
            me.materials.append(self.stage.material(n))
        idx = {n: i for i, n in enumerate(names)}
        me.polygons.foreach_set("material_index", [idx.get(m or self.mat, 0) for m in g.mi])
        # n-gons (caps, concave outlines) become triangles here, so the FBX holds no n-gons
        bm = bmesh.new()
        bm.from_mesh(me)
        big = [f for f in bm.faces if len(f.verts) > 4]
        if big:
            bmesh.ops.triangulate(bm, faces=big, quad_method="BEAUTY", ngon_method="EAR_CLIP")
        bm.to_mesh(me)
        bm.free()
        me.validate(clean_customdata=False)
        if self.smooth:
            me.shade_smooth()
            me.set_sharp_from_angle(angle=math.radians(self.sharp))
        ob = bpy.data.objects.new(self.name, me)
        ob.location = piv
        self.stage.coll.objects.link(ob)
        if self.weighted and self.smooth:
            wn = ob.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
            wn.keep_sharp = True
            wn.mode = "FACE_AREA"
            wn.weight = 50
        ob.visible_shadow = self.shadows
        self.obj = ob
        return ob


# ------------------------------------------------------------------------------ the stage
class Stage:
    def __init__(self, sid, title, seed=1, ev100=1.0):
        self.id = sid
        self.title = title
        self.rng = random.Random(seed)
        self.ev100 = ev100
        self.out = os.path.join(OUT_ROOT, sid)
        os.makedirs(os.path.join(self.out, "review"), exist_ok=True)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.scene = bpy.context.scene
        self.scene.name = sid
        self.coll = bpy.data.collections.new(sid)
        self.scene.collection.children.link(self.coll)
        self.aux = bpy.data.collections.new("review_only")
        self.scene.collection.children.link(self.aux)
        self.parts = {}
        self.props = []
        self.lights = []
        self.blights = []
        self.anims = []
        self.fx = []
        self.mats = {}
        self.surfaces = {}
        self.glows = {}
        self.waters = {}
        self.fog = None
        self.ambient = None
        self.sky = "none"
        self.sky_strength = 0.0
        self.sky_rot = 0.0
        self.music = "music_" + sid
        self.footstep = "stone"
        self.notes = []
        self._ph_cache = {}

    # ---------------------------------------------------------------- materials
    def surface(self, name, set_id=None, tint=(1, 1, 1), rough=1.0, metal=0.0, normal=1.0,
                tiling=None, two_sided=False, emissive=None):
        """A material named after its surface set (or `<Set>__<variant>` for a second tint)."""
        set_id = set_id or name.split("__")[0]
        size = SURFACE_SIZE[set_id]
        tiling = tiling if tiling is not None else round(1.0 / size, 4)
        self.surfaces[name] = {"set": set_id, "tiling": tiling, "tint": [round(c, 3) for c in tint],
                               "roughness": rough, "metallic": metal, "normal_strength": normal}
        if two_sided:
            self.surfaces[name]["two_sided"] = True
        if emissive:
            self.surfaces[name]["emissive_color"] = list(emissive[0])
            self.surfaces[name]["emissive"] = emissive[1]
        return name

    def glow(self, name, color, nits, note=None, pattern=0.0, scale=2.0):
        """`pattern` (0..1) breaks the preview emission up with noise, as the Unreal material's
        animated pattern would; the JSON carries the mean colour and strength."""
        assert name.startswith("GLOW_")
        self.glows[name] = {"material": name, "color": [round(c, 4) for c in color], "strength": nits}
        self._glow_pattern = getattr(self, "_glow_pattern", {})
        self._glow_pattern[name] = (pattern, scale)
        if pattern:
            # For the Unreal material: noise amount (0..1) at `scale` per metre, or "ribs" every `scale` m.
            self.glows[name]["pattern"] = pattern
            self.glows[name]["pattern_scale"] = scale
        if note:
            self.glows[name]["note"] = note
        return name

    def water(self, name, color, roughness, opacity, note=None, emissive=None):
        """`emissive` = (colour, nits): a faint glow the Unreal water/tar material should add."""
        assert name.startswith("WATER_")
        self.waters[name] = {"material": name, "color": [round(c, 4) for c in color],
                             "roughness": roughness, "opacity": opacity}
        if emissive:
            self.waters[name]["emissive_color"] = list(emissive[0])
            self.waters[name]["emissive"] = emissive[1]
        if note:
            self.waters[name]["note"] = note
        return name

    def material(self, name):
        if name in self.mats:
            return self.mats[name]
        m = bpy.data.materials.new(name)
        if name in self.surfaces:
            self._surface_nodes(m, self.surfaces[name])
        elif name in self.glows:
            self._glow_nodes(m, self.glows[name])
        elif name in self.waters:
            self._water_nodes(m, self.waters[name])
        elif name == "ORIGIN_MARKER":
            m.diffuse_color = (1, 0, 1, 1)
        else:
            raise KeyError(f"material {name} was never declared")
        self.mats[name] = m
        return m

    def _img(self, path, colour):
        im = bpy.data.images.load(path, check_existing=True)
        im.colorspace_settings.name = "sRGB" if colour else "Non-Color"
        return im

    def _surface_nodes(self, m, s):
        nt = m.node_tree
        nt.nodes.clear()
        N = nt.nodes.new
        L = nt.links.new
        out = N("ShaderNodeOutputMaterial")
        bsdf = N("ShaderNodeBsdfPrincipled")
        L(bsdf.outputs[0], out.inputs[0])
        tc = N("ShaderNodeTexCoord")
        mp = N("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (s["tiling"], s["tiling"], 1)
        L(tc.outputs["UV"], mp.inputs[0])
        d = os.path.join(SURF, s["set"])
        def tex(suffix, colour):
            t = N("ShaderNodeTexImage")
            t.image = self._img(os.path.join(d, f"{s['set']}_{suffix}.png"), colour)
            L(mp.outputs[0], t.inputs[0])
            return t
        bc = tex("basecolor", True)
        tint = N("ShaderNodeMix")
        tint.data_type = "RGBA"
        tint.blend_type = "MULTIPLY"
        tint.inputs["Factor"].default_value = 1.0
        L(bc.outputs[0], tint.inputs[6])
        tint.inputs[7].default_value = (*s["tint"], 1)
        L(tint.outputs[2], bsdf.inputs["Base Color"])
        orm = tex("orm", False)
        sep = N("ShaderNodeSeparateColor")
        L(orm.outputs[0], sep.inputs[0])
        rk = N("ShaderNodeMath"); rk.operation = "MULTIPLY"; rk.inputs[1].default_value = s["roughness"]
        L(sep.outputs[1], rk.inputs[0]); L(rk.outputs[0], bsdf.inputs["Roughness"])
        mk = N("ShaderNodeMath"); mk.operation = "MULTIPLY"; mk.inputs[1].default_value = s["metallic"]
        L(sep.outputs[2], mk.inputs[0]); L(mk.outputs[0], bsdf.inputs["Metallic"])
        nrm_path = os.path.join(d, f"{s['set']}_normal.png")
        if os.path.exists(nrm_path):
            nt_ = tex("normal", False)
            sp = N("ShaderNodeSeparateColor")
            L(nt_.outputs[0], sp.inputs[0])
            inv = N("ShaderNodeMath"); inv.operation = "SUBTRACT"; inv.inputs[0].default_value = 1.0
            L(sp.outputs[1], inv.inputs[1])                     # DirectX normal -> OpenGL (flip G)
            cb = N("ShaderNodeCombineColor")
            L(sp.outputs[0], cb.inputs[0]); L(inv.outputs[0], cb.inputs[1]); L(sp.outputs[2], cb.inputs[2])
            nm = N("ShaderNodeNormalMap"); nm.inputs["Strength"].default_value = s["normal_strength"]
            L(cb.outputs[0], nm.inputs["Color"]); L(nm.outputs[0], bsdf.inputs["Normal"])
        if s.get("emissive"):
            bsdf.inputs["Emission Color"].default_value = (*s["emissive_color"], 1)
            bsdf.inputs["Emission Strength"].default_value = s["emissive"] / LUMENS_PER_WATT
        if s.get("two_sided"):
            m.use_backface_culling = False

    def _glow_nodes(self, m, gl):
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        em = nt.nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (*gl["color"], 1)
        em.inputs["Strength"].default_value = gl["strength"] / LUMENS_PER_WATT
        nt.links.new(em.outputs[0], out.inputs[0])
        pat, sc = self._glow_pattern.get(m.name, (0.0, 2.0))
        if isinstance(pat, str) and pat == "ribs":
            # horizontal paper ribs (world Z bands `sc` metres apart), as the lantern material would draw
            geo = nt.nodes.new("ShaderNodeNewGeometry")
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(geo.outputs["Position"], sep.inputs[0])
            k = nt.nodes.new("ShaderNodeMath"); k.operation = "MULTIPLY"; k.inputs[1].default_value = math.tau / sc
            nt.links.new(sep.outputs[2], k.inputs[0])
            sn = nt.nodes.new("ShaderNodeMath"); sn.operation = "SINE"
            nt.links.new(k.outputs[0], sn.inputs[0])
            mr = nt.nodes.new("ShaderNodeMapRange")
            mr.inputs["From Min"].default_value = -1.0
            mr.inputs["To Min"].default_value = 0.45 * gl["strength"] / LUMENS_PER_WATT
            mr.inputs["To Max"].default_value = 1.15 * gl["strength"] / LUMENS_PER_WATT
            nt.links.new(sn.outputs[0], mr.inputs["Value"])
            nt.links.new(mr.outputs[0], em.inputs["Strength"])
        elif pat > 0:
            tc = nt.nodes.new("ShaderNodeTexCoord")
            nz = nt.nodes.new("ShaderNodeTexNoise")
            nz.inputs["Scale"].default_value = sc
            nz.inputs["Detail"].default_value = 4.0
            nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
            ramp = nt.nodes.new("ShaderNodeMapRange")
            ramp.inputs["From Min"].default_value = 0.35
            ramp.inputs["From Max"].default_value = 0.65
            ramp.inputs["To Min"].default_value = 1.0 - pat
            ramp.inputs["To Max"].default_value = 1.0 + pat
            nt.links.new(nz.outputs[0], ramp.inputs["Value"])
            mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"
            mul.inputs[1].default_value = gl["strength"] / LUMENS_PER_WATT
            nt.links.new(ramp.outputs[0], mul.inputs[0])
            nt.links.new(mul.outputs[0], em.inputs["Strength"])

    def _water_nodes(self, m, w):
        nt = m.node_tree
        nt.nodes.clear()
        N = nt.nodes.new
        out = N("ShaderNodeOutputMaterial")
        bsdf = N("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Base Color"].default_value = (*w["color"], 1)
        bsdf.inputs["Roughness"].default_value = w["roughness"]
        bsdf.inputs["Alpha"].default_value = w["opacity"]
        bsdf.inputs["IOR"].default_value = 1.33
        # a faint ripple so reflections break up in the review renders
        tc = N("ShaderNodeTexCoord")
        nz = N("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 3.0
        nz.inputs["Detail"].default_value = 3.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        bump = N("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.08
        nt.links.new(nz.outputs[0], bump.inputs["Height"])
        nt.links.new(bump.outputs[0], bsdf.inputs["Normal"])
        nt.links.new(bsdf.outputs[0], out.inputs[0])
        if w.get("emissive"):
            bsdf.inputs["Emission Color"].default_value = (*w["emissive_color"], 1)
            bsdf.inputs["Emission Strength"].default_value = w["emissive"] / LUMENS_PER_WATT
        if w["opacity"] < 1.0:
            # dithered (not blended) so Eevee still raytraces the reflections; near-opaque
            # puddles render opaque here, the JSON keeps their opacity for Unreal
            if hasattr(m, "surface_render_method"):
                m.surface_render_method = "DITHERED"
            if w["opacity"] >= 0.8:
                bsdf.inputs["Alpha"].default_value = 1.0
            m.use_backface_culling = False

    # ---------------------------------------------------------------- geometry
    def part(self, key, mat, **kw):
        name = f"{self.id}_{key}"
        if name not in self.parts:
            self.parts[name] = Part(self, name, mat, **kw)
        return self.parts[name]

    def prop(self, model, loc, rot_z=0.0, scale=1.0, tilt=(0, 0)):
        """Places a Poly Haven model (glTF, imported as data) as its own object."""
        n = sum(1 for p in self.props if p["model"] == model) + 1
        name = f"{self.id}_prop_{model}_{n}"
        src = self._ph_cache.get(model)
        if src is None:
            src = self._import_ph(model)
            self._ph_cache[model] = src
        me = src.data.copy()
        me.name = name
        ob = bpy.data.objects.new(name, me)
        ob.location = loc
        ob.rotation_euler = (math.radians(tilt[0]), math.radians(tilt[1]), math.radians(rot_z))
        ob.scale = (scale, scale, scale)
        self.coll.objects.link(ob)
        me.transform(Matrix.Diagonal((scale, scale, scale, 1)) @ ob.rotation_euler.to_matrix().to_4x4())
        ob.rotation_euler = (0, 0, 0)
        ob.scale = (1, 1, 1)
        me.calc_loop_triangles()
        self.props.append({"model": model, "object": name, "pos": list(loc), "rot_z": rot_z,
                           "scale": scale, "tris": len(me.loop_triangles)})
        return ob

    def _import_ph(self, model):
        d = os.path.join(PH_MODELS, model)
        gltf = None
        for dd, _, fs in os.walk(d):
            for f in fs:
                if f.endswith(".gltf"):
                    gltf = os.path.join(dd, f)
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=gltf)
        new = [o for o in bpy.data.objects if o not in before]
        meshes = [o for o in new if o.type == "MESH"]
        for o in meshes:
            o.data.transform(o.matrix_world)
            o.matrix_world = Matrix.Identity(4)
        bm = bmesh.new()
        mats = []
        for o in meshes:
            tmp = o.data.copy()
            remap = []
            for slot in o.data.materials:
                if slot not in mats:
                    mats.append(slot)
                remap.append(mats.index(slot))
            for p in tmp.polygons:
                p.material_index = remap[p.material_index] if remap else 0
            bm.from_mesh(tmp)
            bpy.data.meshes.remove(tmp)
        me = bpy.data.meshes.new("PH_" + model)
        bm.to_mesh(me)
        bm.free()
        for m in mats:
            suffix = m.name[len(model):].strip("_.") if m.name.startswith(model) else m.name
            m.name = f"PH_{model}_{suffix or 'main'}"
            me.materials.append(m)
        for o in new:
            bpy.data.objects.remove(o)
        holder = bpy.data.objects.new("PH_src_" + model, me)
        return holder

    def animated(self, obj, kind, amp, speed, axis=None, note=None, pivot=None):
        e = {"object": obj, "kind": kind, "amp": amp, "speed": speed}
        if axis is not None:
            e["axis"] = [round(c, 4) for c in axis]
            e["axis_ue"] = [round(axis[0], 4), round(-axis[1], 4), round(axis[2], 4)]
        if pivot is not None:
            e["pivot"] = [round(c, 4) for c in pivot]
            e["pivot_ue_cm"] = ue_cm(pivot)
        if note:
            e["note"] = note
        self.anims.append(e)

    def preview_volume(self, loc, radius, height, color, density, noise_scale=1.2, emission=None):
        """Review-only smoke or steam (not exported; Unreal gets an fx entry instead)."""
        g = geo_cyl(radius, height, 16, r2=radius * 1.4)
        me = bpy.data.meshes.new("pv")
        me.from_pydata([p for p in g.v], [], g.f)
        ob = bpy.data.objects.new("preview_volume", me)
        ob.location = loc
        self.aux.objects.link(ob)
        m = bpy.data.materials.new("pv")
        nt = m.node_tree
        nt.nodes.clear()
        N = nt.nodes.new
        out = N("ShaderNodeOutputMaterial")
        vol = N("ShaderNodeVolumePrincipled")
        vol.inputs["Color"].default_value = (*color, 1)
        tc = N("ShaderNodeTexCoord")
        nz = N("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = noise_scale
        nz.inputs["Detail"].default_value = 3.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        sep = N("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        fade = N("ShaderNodeMapRange")
        fade.inputs["From Min"].default_value = 0.0
        fade.inputs["From Max"].default_value = height
        fade.inputs["To Min"].default_value = 1.0
        fade.inputs["To Max"].default_value = 0.0
        nt.links.new(sep.outputs[2], fade.inputs["Value"])
        cut = N("ShaderNodeMapRange")
        cut.inputs["From Min"].default_value = 0.45
        cut.inputs["From Max"].default_value = 0.75
        nt.links.new(nz.outputs[0], cut.inputs["Value"])
        mul = N("ShaderNodeMath"); mul.operation = "MULTIPLY"
        nt.links.new(cut.outputs[0], mul.inputs[0]); nt.links.new(fade.outputs[0], mul.inputs[1])
        mul2 = N("ShaderNodeMath"); mul2.operation = "MULTIPLY"; mul2.inputs[1].default_value = density
        nt.links.new(mul.outputs[0], mul2.inputs[0])
        nt.links.new(mul2.outputs[0], vol.inputs["Density"])
        if emission:
            vol.inputs["Emission Color"].default_value = (*emission[0], 1)
            em = N("ShaderNodeMath"); em.operation = "MULTIPLY"; em.inputs[1].default_value = emission[1] / LUMENS_PER_WATT
            nt.links.new(mul.outputs[0], em.inputs[0])
            nt.links.new(em.outputs[0], vol.inputs["Emission Strength"])
        nt.links.new(vol.outputs[0], out.inputs["Volume"])
        me.materials.append(m)
        return ob

    # ---------------------------------------------------------------- lights
    def light(self, name, kind, pos, color, value, radius=10.0, shadows=False, flicker=0.0,
              flicker_speed=0.0, direction=None, cone=None, source_radius=0.05, affects=None,
              specular=1.0, note=None, render_scale=1.0, volumetric=1.0):
        """kind point|spot: value in candela; directional: value in lux. `cone` = (inner, outer)
        half-angles in degrees for spots. `affects`: None (everything), 'fighters_only' or
        'not_floor' (lighting channels in Unreal; light linking in the review renders)."""
        e = {"name": name, "type": kind, "pos": [round(c, 3) for c in pos], "color": [round(c, 4) for c in color]}
        if kind == "directional":
            e["lux"] = value
        else:
            e["intensity_candela"] = value
            e["radius_m"] = radius
        if direction is not None:
            d = Vector(direction).normalized()
            e["dir"] = [round(c, 4) for c in d]
            e["dir_ue"] = [round(d.x, 4), round(-d.y, 4), round(d.z, 4)]
        if cone:
            e["cone_inner_deg"], e["cone_outer_deg"] = cone
        e["shadows"] = shadows
        e["flicker"] = flicker
        e["flicker_speed"] = flicker_speed
        e["pos_ue_cm"] = ue_cm(pos)
        if kind == "directional":
            e["source_angle_deg"] = source_radius
        else:
            e["source_radius_m"] = source_radius
        if specular != 1.0:
            e["specular_scale"] = specular
        if volumetric != 1.0:
            e["volumetric_scattering"] = volumetric
        if affects:
            e["affects"] = affects
        if note:
            e["note"] = note
        self.lights.append(e)
        # Blender light for the review renders
        btype = {"point": "POINT", "spot": "SPOT", "directional": "SUN"}[kind]
        ld = bpy.data.lights.new(name, btype)
        ld.color = color
        if kind == "directional":
            ld.energy = value / LUMENS_PER_WATT * render_scale
            ld.angle = math.radians(max(0.5, source_radius))
        else:
            ld.energy = value * 4 * math.pi / LUMENS_PER_WATT * render_scale
            ld.shadow_soft_size = source_radius
            ld.use_custom_distance = True
            ld.cutoff_distance = radius
            if kind == "spot":
                ld.spot_size = math.radians(cone[1] * 2)
                ld.spot_blend = max(0.0, min(1.0, 1.0 - cone[0] / cone[1]))
        ld.use_shadow = shadows
        ld.specular_factor = specular
        ld.volume_factor = volumetric
        ob = bpy.data.objects.new(name, ld)
        ob.location = pos
        if direction is not None:
            ob.rotation_euler = Vector(direction).to_track_quat("-Z", "Y").to_euler()
        self.aux.objects.link(ob)
        self.blights.append((ob, affects))
        return ob

    # ---------------------------------------------------------------- finish
    def finish(self):
        for p in self.parts.values():
            p.build()
        m = Part(self, "ORIGIN_MARKER_1m", "ORIGIN_MARKER", smooth=False)
        m.add(geo_box(1, 1, 1))
        ob = m.build()
        ob.hide_render = True
        self.marker = ob
        self.check()

    def facing_report(self, cams=None, nx=160, ny=90):
        """Casts rays from the review cameras; for each object counts first hits that land on the
        back of a face (what single-sided materials in Unreal would cull). Two-sided surfaces
        (cloth, reeds) are reported separately and expected to have back hits."""
        cams = cams or [((0, -6.5, 1.6), (0, 0, 1.4)), ((0, -8.4, 2.3), (0, 0, 1.95)), ((-3.5, -6.5, 1.6), (-3.5, 0, 1.4)),
                        ((3.5, -6.5, 1.6), (3.5, 0, 1.4)), ((0, -2.0, 16.0), (0, 7.0, 0.0)), ((-9, -3.0, 9.0), (-6, 8.0, 0.0)),
                        ((9, -3.0, 9.0), (6, 8.0, 0.0))]
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        stats = {}
        th = math.tan(math.radians(15.0))
        for pos, look in cams:
            pos = Vector(pos)
            fwd = (Vector(look) - pos).normalized()
            right = fwd.cross(Vector((0, 0, 1)) if abs(fwd.z) < 0.99 else Vector((0, 1, 0))).normalized()
            up = right.cross(fwd)
            for j in range(ny):
                for i in range(nx):
                    u = (2 * (i + 0.5) / nx - 1) * th * 16 / 9
                    v = (1 - 2 * (j + 0.5) / ny) * th
                    d = (fwd + right * u + up * v).normalized()
                    hit, loc, nrm, idx, ob, mat = self.scene.ray_cast(dg, pos, d)
                    if not hit or ob.name.startswith(("standin", "preview_volume", "fog_volume")):
                        continue
                    st_ = stats.setdefault(ob.name, [0, 0])
                    st_[0] += 1
                    if nrm.dot(d) > 0:
                        st_[1] += 1
        two_sided = {n for n, s_ in self.surfaces.items() if s_.get("two_sided")}
        bad = []
        for name, (n, back) in sorted(stats.items()):
            ob = bpy.data.objects.get(name)
            mats = {sl.material.name for sl in ob.material_slots if sl.material} if ob else set()
            ts = bool(mats & two_sided)
            if back > max(3, 0.03 * n) and not ts:
                bad.append((name, n, back))
            log(f"facing {name}: hits {n}, back {back}{' (two-sided)' if ts else ''}")
        self.facing_problems = bad
        log("facing:", "clean" if not bad else f"{len(bad)} objects show back faces: {bad}")
        return bad

    def objects(self):
        obs = [p.obj for p in self.parts.values() if p.obj] + \
              [bpy.data.objects[p["object"]] for p in self.props] + [self.marker]
        return obs

    def stats(self):
        dg = bpy.context.evaluated_depsgraph_get()
        rows = []
        for ob in self.objects():
            ev = ob.evaluated_get(dg)
            me = ev.to_mesh()
            me.calc_loop_triangles()
            rows.append((ob.name, len(me.loop_triangles), [s.material.name for s in ob.material_slots if s.material]))
            ev.to_mesh_clear()
        return rows

    def check(self):
        """The walkable strip stays flat and empty; the foreground stays low and to the sides."""
        bad = []
        bpy.context.view_layer.update()
        for p in self.parts.values():
            for v in p.geo.v:
                in_strip = abs(v.x) < STRIP["x"] and STRIP["y0"] < v.y < STRIP["y1"]
                if in_strip and (v.z > p.surface_z if p.floor else v.z > -0.02) and v.z < 8.0:
                    bad.append((p.name, "strip", tuple(round(c, 2) for c in v)))
                    break
                if v.y < -1.0 and abs(v.x) < 3.5 and v.z > 0.05 and v.y > -12:
                    bad.append((p.name, "foreground-centre", tuple(round(c, 2) for c in v)))
                    break
                if v.y < -1.0 and v.z > 0.4 and v.y > -12:
                    bad.append((p.name, "foreground-high", tuple(round(c, 2) for c in v)))
                    break
        for pr in self.props:
            ob = bpy.data.objects[pr["object"]]
            for v in ob.data.vertices:
                w = ob.matrix_world @ v.co
                if abs(w.x) < STRIP["x"] and STRIP["y0"] < w.y < STRIP["y1"] and w.z > 0.0005:
                    bad.append((ob.name, "strip", tuple(round(c, 2) for c in w)))
                    break
                if w.y < -1.0 and (abs(w.x) < 3.5 or w.z > 0.4):
                    bad.append((ob.name, "foreground", tuple(round(c, 2) for c in w)))
                    break
        self.violations = bad
        for b in bad:
            log("CHECK", *b)
        log("check:", "clean" if not bad else f"{len(bad)} problems")

    # ---------------------------------------------------------------- environment for renders
    def set_fog(self, color, density_per_m, height_falloff=0.0, base_z=0.0, top=40.0, note=None):
        """Exponential height fog. density_per_m is the extinction at base_z (Blender world
        volume / physical). Unreal's ExponentialHeightFog FogDensity = density_per_m * 10 (it
        divides by 1000 per uu); FogHeightFalloff = height_falloff * 10 likewise."""
        self.fog = {"color": [round(c, 4) for c in color], "density": round(density_per_m * 10, 4),
                    "height_falloff": round(height_falloff * 10, 4), "density_per_m": density_per_m,
                    "height_falloff_per_m": height_falloff, "fog_height_m": base_z,
                    "fog_height_ue_cm": round(base_z * 100, 1)}
        if note:
            self.fog["note"] = note
        self._fog_render = (color, density_per_m, height_falloff, base_z, top)

    def set_ambient(self, color, nits, sky="none", sky_strength=0.0, sky_rot=0.0):
        self.ambient = {"color": [round(c, 4) for c in color], "intensity": nits,
                        "units": "cd/m2 sky radiance at EV100 %.1f (Sky Light intensity: tune)" % self.ev100}
        self.sky = sky
        self.sky_strength = sky_strength
        self.sky_rot = sky_rot

    def _world(self):
        w = bpy.data.worlds.new("world")
        self.scene.world = w
        nt = w.node_tree
        nt.nodes.clear()
        N = nt.nodes.new
        out = N("ShaderNodeOutputWorld")
        bg = N("ShaderNodeBackground")
        col, nits = self.ambient["color"], self.ambient["intensity"]
        if self.sky != "none":
            tc = N("ShaderNodeTexCoord")
            mp = N("ShaderNodeMapping")
            mp.inputs["Rotation"].default_value = (0, 0, math.radians(self.sky_rot))
            env = N("ShaderNodeTexEnvironment")
            env.image = bpy.data.images.load(os.path.join(PH_HDRI, self.sky, f"{self.sky}_2k.hdr"))
            nt.links.new(tc.outputs["Generated"], mp.inputs[0])
            nt.links.new(mp.outputs[0], env.inputs[0])
            mix = N("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = 1.0
            nt.links.new(env.outputs[0], mix.inputs[6])
            mix.inputs[7].default_value = (*col, 1)
            nt.links.new(mix.outputs[2], bg.inputs[0])
            bg.inputs[1].default_value = self.sky_strength / LUMENS_PER_WATT
        else:
            bg.inputs[0].default_value = (*col, 1)
            bg.inputs[1].default_value = nits / LUMENS_PER_WATT
        nt.links.new(bg.outputs[0], out.inputs["Surface"])

    def _fog_volume(self):
        if not self.fog:
            return
        color, dens, fall, z0, top = self._fog_render
        me = bpy.data.meshes.new("fog")
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new("fog_volume", me)
        ob.scale = (160, 160, top + 30)
        ob.location = (0, 60, (top + 30) * 0.5 - 15)
        self.aux.objects.link(ob)
        m = bpy.data.materials.new("fog")
        nt = m.node_tree
        nt.nodes.clear()
        N = nt.nodes.new
        out = N("ShaderNodeOutputMaterial")
        vol = N("ShaderNodeVolumePrincipled")
        vol.inputs["Color"].default_value = (*color, 1)
        vol.inputs["Anisotropy"].default_value = 0.35
        geo = N("ShaderNodeNewGeometry")
        sep = N("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Position"], sep.inputs[0])
        sub = N("ShaderNodeMath"); sub.operation = "SUBTRACT"; sub.inputs[1].default_value = z0
        nt.links.new(sep.outputs[2], sub.inputs[0])
        mul = N("ShaderNodeMath"); mul.operation = "MULTIPLY"; mul.inputs[1].default_value = -fall
        nt.links.new(sub.outputs[0], mul.inputs[0])
        ex = N("ShaderNodeMath"); ex.operation = "EXPONENT"
        nt.links.new(mul.outputs[0], ex.inputs[0])
        mn = N("ShaderNodeMath"); mn.operation = "MINIMUM"; mn.inputs[1].default_value = 4.0
        nt.links.new(ex.outputs[0], mn.inputs[0])
        dk = N("ShaderNodeMath"); dk.operation = "MULTIPLY"; dk.inputs[1].default_value = dens
        nt.links.new(mn.outputs[0], dk.inputs[0])
        nt.links.new(dk.outputs[0], vol.inputs["Density"])
        nt.links.new(vol.outputs[0], out.inputs["Volume"])
        me.materials.append(m)

    def _capsules(self, xs):
        caps = []
        m = bpy.data.materials.new("standin_grey")
        b = m.node_tree.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = (0.42, 0.42, 0.42, 1)
        b.inputs["Roughness"].default_value = 0.55
        for i, x in enumerate(xs):
            g = geo_lathe([(0, 0)] + [(0.22 * math.sin(math.pi / 2 * k / 6), 0.22 - 0.22 * math.cos(math.pi / 2 * k / 6)) for k in range(1, 7)] +
                          [(0.22 * math.cos(math.pi / 2 * k / 6), 1.58 + 0.22 * math.sin(math.pi / 2 * k / 6)) for k in range(0, 6)] + [(0, 1.8)], 24,
                          cap_bottom=False, cap_top=False)
            me = bpy.data.meshes.new(f"standin_{i}")
            me.from_pydata([p for p in g.v], [], g.f)
            me.shade_smooth()
            me.materials.append(m)
            ob = bpy.data.objects.new(f"standin_{i}", me)
            ob.location = (x, 0, 0)
            self.aux.objects.link(ob)
            caps.append(ob)
        return caps

    def _compositor(self, bloom=0.6):
        sc = self.scene
        try:
            ng = bpy.data.node_groups.new("comp", "CompositorNodeTree")
            sc.compositing_node_group = ng
            rl = ng.nodes.new("CompositorNodeRLayers")
            gl = ng.nodes.new("CompositorNodeGlare")
            gl.inputs["Type"].default_value = "Bloom"
            gl.inputs["Threshold"].default_value = 1.0
            gl.inputs["Strength"].default_value = bloom
            gl.inputs["Size"].default_value = 0.6
            ng.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
            go = ng.nodes.new("NodeGroupOutput")
            ng.links.new(rl.outputs["Image"], gl.inputs["Image"])
            ng.links.new(gl.outputs["Image"], go.inputs[0])
        except Exception as e:  # compositor API differs: renders stay unbloomed
            log("compositor skipped:", e)

    def render_reviews(self, shots=None, samples=48, bloom=0.5, standins=True):
        sc = self.scene
        sc.render.engine = "BLENDER_EEVEE"
        sc.render.resolution_x, sc.render.resolution_y = 1600, 900
        sc.render.resolution_percentage = 100
        sc.render.image_settings.file_format = "PNG"
        sc.view_settings.view_transform = "AgX"
        sc.view_settings.look = "AgX - Medium High Contrast"
        sc.view_settings.exposure = math.log2(LUMENS_PER_WATT / 1.2) - self.ev100
        ee = sc.eevee
        for k, v in (("taa_render_samples", samples), ("use_raytracing", True), ("use_shadows", True),
                     ("shadow_ray_count", 2), ("shadow_step_count", 8), ("volumetric_start", 0.5),
                     ("volumetric_end", 90.0), ("volumetric_tile_size", "4"), ("volumetric_samples", 64),
                     ("use_volumetric_shadows", True), ("light_threshold", 0.0001),
                     ("clamp_surface_indirect", 10.0), ("fast_gi_method", "GLOBAL_ILLUMINATION"),
                     ("use_fast_gi", True)):
            try:
                setattr(ee, k, v)
            except Exception as e:
                log("eevee setting", k, "skipped:", e)
        try:
            ee.ray_tracing_options.resolution_scale = "2"
        except Exception as e:
            log("raytrace resolution skipped:", e)
        self._world()
        self._fog_volume()
        self._compositor(bloom)
        cam_data = bpy.data.cameras.new("cam")
        cam_data.sensor_fit = "VERTICAL"
        cam_data.sensor_height = 24.0
        cam_data.lens = 12.0 / math.tan(math.radians(15.0))
        cam_data.clip_start = 0.1
        cam_data.clip_end = 400
        cam = bpy.data.objects.new("review_cam", cam_data)
        self.aux.objects.link(cam)
        sc.camera = cam
        shots = shots or [("gameplay", (0, -6.5, 1.6), (0, 0, 1.4), (-1.5, 1.5)),
                          ("wide", (0, -8.4, 2.3), (0, 0, 1.95), (-1.5, 1.5)),
                          ("left", (-3.5, -6.5, 1.6), (-3.5, 0, 1.4), (-5.0, -2.0)),
                          ("right", (3.5, -6.5, 1.6), (3.5, 0, 1.4), (2.0, 5.0))]
        # the fill reaches only the stand-ins (Unreal: a lighting channel the fighters share)
        caps = self._capsules((-1.5, 1.5)) if standins else []
        recv = bpy.data.collections.new("fill_receivers")
        for c in caps:
            recv.objects.link(c)
        nofloor = bpy.data.collections.new("not_floor")
        for p in self.parts.values():
            if p.obj and not p.floor:
                nofloor.objects.link(p.obj)
        for c in caps:
            nofloor.objects.link(c)
        for ob, aff in self.blights:
            if aff == "fighters_only":
                ob.light_linking.receiver_collection = recv
            elif aff == "not_floor":
                ob.light_linking.receiver_collection = nofloor
        paths = []
        for name, pos, look, xs in shots:
            for c, x in zip(caps, xs):
                c.location.x = x
            cam.location = pos
            cam.rotation_euler = (Vector(look) - Vector(pos)).to_track_quat("-Z", "Y").to_euler()
            p = os.path.join(self.out, "review", f"{self.id}_{name}.png")
            sc.render.filepath = p
            t = time.time()
            bpy.ops.render.render(write_still=True)
            log(f"render {name}: {time.time() - t:.1f}s -> {p}")
            paths.append(p)
        self._sheet(paths)

    def _sheet(self, paths):
        import numpy as np
        tiles = []
        for p in paths:
            im = bpy.data.images.load(p)
            w, h = im.size
            a = np.empty(w * h * 4, dtype=np.float32)
            im.pixels.foreach_get(a)
            a = a.reshape(h, w, 4)
            a = a[: h // 2 * 2, : w // 2 * 2].reshape(h // 2, 2, w // 2, 2, 4).mean(axis=(1, 3))
            tiles.append(a)
            bpy.data.images.remove(im)
        while len(tiles) < 4:
            tiles.append(np.zeros_like(tiles[0]))
        top = np.concatenate([tiles[0], tiles[1]], axis=1)
        bot = np.concatenate([tiles[2], tiles[3]], axis=1)
        sheet = np.concatenate([bot, top], axis=0)       # image rows run bottom-up
        h, w = sheet.shape[:2]
        out = bpy.data.images.new("sheet", w, h, alpha=True)
        out.pixels.foreach_set(np.ascontiguousarray(sheet, dtype=np.float32).ravel())
        out.filepath_raw = os.path.join(self.out, "review", f"{self.id}_sheet.png")
        out.file_format = "PNG"
        out.save()

    # ---------------------------------------------------------------- export
    def export(self):
        rows = self.stats()
        tris = sum(r[1] for r in rows if r[0] != "ORIGIN_MARKER_1m")
        obs = self.objects()
        for c in list(self.aux.objects):
            bpy.data.objects.remove(c)
        sc = self.scene
        sc.unit_settings.system = "METRIC"
        sc.unit_settings.scale_length = 0.01
        # metres -> centimetres: scale every object and its offset from the world origin by 100
        for ob in obs:
            ob.location = ob.location * 100.0
            ob.scale = (100.0, 100.0, 100.0)
        bpy.ops.object.select_all(action="DESELECT")
        for ob in obs:
            ob.select_set(True)
        bpy.context.view_layer.objects.active = obs[0]
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        path = os.path.join(self.out, f"{self.id}.fbx")
        bpy.ops.export_scene.fbx(
            filepath=path, use_selection=True, object_types={"MESH"},
            apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
            axis_forward="Y", axis_up="Z", bake_space_transform=False,
            use_mesh_modifiers=True, mesh_smooth_type="FACE", use_tspace=True, use_triangles=False,
            use_custom_props=False, add_leaf_bones=False, bake_anim=False,
            path_mode="STRIP", embed_textures=False, colors_type="NONE")
        log("exported", path)
        self.write_json(rows, tris)
        return path, rows, tris

    def write_json(self, rows, tris):
        placements = []
        for ob in self.objects():
            placements.append({"object": ob.name, "pivot_ue_cm": [round(ob.location.x, 2), round(-ob.location.y, 2), round(ob.location.z, 2)]})
        data = {
            "stage": self.id, "title": self.title,
            "units": {"positions": "Blender metres, Z up, fighting plane XZ at Y=0, camera at -Y looking +Y",
                      "unreal": "UE = (100*x, -100*y, 100*z) cm; *_ue_cm fields are already converted",
                      "lights": "candela (point/spot), lux (directional), glow strength in cd/m2; for manual exposure EV100 = %.1f" % self.ev100,
                      "exposure_ev100": self.ev100},
            "lights": self.lights,
            "glows": list(self.glows.values()),
            "water": list(self.waters.values()),
            "animated": self.anims,
            "fog": self.fog,
            "ambient": self.ambient,
            "music": self.music,
            "footstep": self.footstep,
            "sky": self.sky,
            "surfaces": self.surfaces,
            "props": self.props,
            "fx": self.fx,
            "objects": [{"name": r[0], "triangles": r[1], "materials": r[2]} for r in rows],
            "placements": placements,
            "triangles_total": tris,
            "strip_check": "clean" if not self.violations else self.violations,
            "notes": self.notes,
        }
        with open(os.path.join(self.out, f"{self.id}.json"), "w") as f:
            json.dump(data, f, indent=1)
        log("json written; triangles", tris, "objects", len(rows))


def ue_cm(p):
    return [round(p[0] * 100, 1), round(-p[1] * 100, 1), round(p[2] * 100, 1)]


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return {"render": "--no-render" not in a, "quick": "--quick" in a, "export": "--no-export" not in a,
            "shots": next((x.split("=", 1)[1].split(",") for x in a if x.startswith("--shots=")), None)}
