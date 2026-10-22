"""What each fighter wears, built over the shared human body (see docs/DESIGN.md for the art
direction). Each outfit returns the whole outer surface: garments where clothed and skin where
bare. Garments are the body's own shapes grown outward and smoothed, cut open at hems, with folds
added as small displacements. Rigid pieces (helmets, jewellery, bracers) come from extras().
"""
import numpy as np

import mesher
from sdf import (Displace, Intersect, Leaf, Offset, Shell, Subtract, Union, box, curve_tube, cut, ellipsoid, fbm, frame_from,
                 half_space, retag, round_cone, sphere, torus)


def plane_keep(point, normal):
    """Keeps the side of the plane that `normal` points away from."""
    return half_space(point, normal)


def layer(node, thick, mat, bones):
    """A garment layer: any shape grown outward by `thick`, as one primitive of material `mat`."""
    return retag(Offset(node, thick), mat, bones)


def strap(surface, thick, path, width, mat, bones, k=0.0):
    """A strap `thick` proud of `surface`, following `path` with half-width `width`."""
    tube = curve_tube(path, [width] * len(path), mat, bones, k=width * 0.5)
    return Intersect(layer(surface, thick, mat, bones), tube, k)


def y_slab(lo, hi):
    """The region between two heights (metres)."""
    def fn(p):
        return np.maximum(p[:, 1] - hi, lo - p[:, 1]).astype(np.float32)
    return cut(fn)


def axis_slab(a, c, t0, t1):
    """The region between two planes across the segment a-c, at fractions t0 and t1 along it."""
    a = np.asarray(a, np.float32)
    ax = np.asarray(c, np.float32) - a
    L = float(np.linalg.norm(ax))
    ax = ax / L

    def fn(p):
        t = (p - a) @ ax
        return np.maximum(t - t1 * L, t0 * L - t).astype(np.float32)
    return cut(fn)


def on_surface(node, pts, off=0.0):
    """Points moved onto a shape's surface (then `off` out along its normal), with the normals."""
    p = np.asarray(pts, np.float32).copy()
    for _ in range(40):
        d, _ = mesher.evaluate(node, p)
        g = mesher.gradient(node, p)
        g /= np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9)
        p = p - g * np.clip(d, -0.02, 0.02)[:, None]
    n = mesher.normals(node, p)
    return p + n * off, n


def smooth_path(points, per=6):
    """Catmull-Rom points through a polyline, `per` per segment."""
    pts = [np.asarray(p, np.float32) for p in points]
    out = []
    for i in range(len(pts) - 1):
        p0, p1, p2, p3 = pts[max(i - 1, 0)], pts[i], pts[i + 1], pts[min(i + 2, len(pts) - 1)]
        for t in np.linspace(0, 1, per, endpoint=False):
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(pts[-1])
    return np.array(out, np.float32)


def cast(node, pts, centre, reach=3.0):
    """Each point pushed onto the outermost surface of `node` along the line from `centre`
    through it (sphere tracing inward from `reach` times as far out)."""
    pts = np.asarray(pts, np.float32)
    c = np.broadcast_to(np.asarray(centre, np.float32), pts.shape)
    d = pts - c
    d /= np.maximum(np.linalg.norm(d, axis=1, keepdims=True), 1e-9)
    t = np.linalg.norm(pts - c, axis=1) * reach
    for _ in range(160):
        dist, _ = mesher.evaluate(node, c + d * t[:, None])
        t = np.maximum(t - np.clip(dist, 0.0, 0.02) * 0.9, 0.0)
    return c + d * t[:, None]


def surface_strap(surface, thick, points, centre, width, mat, bones, per=6):
    """A strap laid over the outside of `surface` along rough `points`, cast toward `centre`."""
    path = cast(surface, smooth_path(points, per), centre)
    return strap(surface, thick, path, width, mat, bones, k=thick * 0.3)


def wrap_bands(surface, a, c, t0, t1, turns, width, thick, mat, bones, phase=0.0, per_turn=18, hand=1.0):
    """Bandage wound round the segment a-c from t0 to t1: a helix laid on `surface`."""
    a, c = np.asarray(a, np.float32), np.asarray(c, np.float32)
    ax = (c - a) / np.linalg.norm(c - a)
    p1 = np.cross(ax, [0.0, 0.0, 1.0])
    if np.linalg.norm(p1) < 1e-3:
        p1 = np.cross(ax, [1.0, 0.0, 0.0])
    p1 /= np.linalg.norm(p1)
    p2 = np.cross(ax, p1)
    n = max(int(turns * per_turn), 4)
    t = np.linspace(t0, t1, n)
    th = phase + hand * np.linspace(0, turns * 2 * np.pi, n)
    centres = a[None, :] + (c - a)[None, :] * t[:, None]
    out = centres + (np.cos(th)[:, None] * p1 + np.sin(th)[:, None] * p2) * 0.25
    path = cast(surface, out, centres, reach=1.0)
    return strap(surface, thick, path, width, mat, bones, k=thick * 0.3)


def elliptic_cylinder(c, a, b, y0, y1, edge, material, bones, rot=None, tag=""):
    """A rounded elliptic cylinder: semi-axes a (x) and b (z), from y0 to y1 in its frame
    (columns of `rot`) about c, edges rounded by `edge`."""
    c = np.asarray(c, np.float32)
    R = np.eye(3, dtype=np.float32) if rot is None else np.asarray(rot, np.float32)
    m = min(a, b)

    def fn(p):
        q = (p - c) @ R
        d2 = (np.sqrt((q[:, 0] / a) ** 2 + (q[:, 2] / b) ** 2) - 1.0) * m + edge
        dy = np.maximum(q[:, 1] - y1, y0 - q[:, 1]) + edge
        out = np.sqrt(np.maximum(d2, 0) ** 2 + np.maximum(dy, 0) ** 2)
        return (out + np.minimum(np.maximum(d2, dy), 0.0) - edge).astype(np.float32)

    ext = max(a, b, abs(y0), abs(y1)) * 1.5
    return Leaf(fn, c - ext, c + ext, material, bones, tag)


def limb_ring(b, ja, jb, t, radius, minor, mat, bones, squash=1.0):
    """A ring round the bone ja-jb at fraction t (cuffs, rolled hems)."""
    a, c = b.rig.pos(ja), b.rig.pos(jb)
    axis = (c - a) / np.linalg.norm(c - a)
    return torus(a + (c - a) * t, radius, minor, mat, bones, rot=frame_from(axis, [0, 0, 1]), squash=squash)


def _folds(a, b, centres, radius, amp, seed, along_scale=55.0, around_scale=9.0):
    """Irregular creases around the limb a-b, gathered near `centres` (elbows, knees, ankles):
    ridged noise stretched so the folds run around the limb rather than along it."""
    a = np.asarray(a, np.float32)
    axis = np.asarray(b, np.float32) - a
    axis /= np.linalg.norm(axis)
    p1 = np.cross(axis, [0.0, 0.0, 1.0]).astype(np.float32)
    if np.linalg.norm(p1) < 1e-3:
        p1 = np.array([1.0, 0.0, 0.0], np.float32)
    p1 /= np.linalg.norm(p1)
    p2 = np.cross(axis, p1)
    cs = np.asarray(centres, np.float32)

    def fn(p):
        w = np.zeros(len(p), np.float32)
        for c in cs:
            w += np.exp(-np.sum((p - c) ** 2, axis=1) / (radius * radius))
        w = np.clip(w, 0.0, 1.0)
        rel = p - a
        q = np.stack([rel @ axis * along_scale, rel @ p1 * around_scale, rel @ p2 * around_scale], axis=1)
        ridged = 1.0 - 2.0 * np.abs(fbm(q / 10.0, 10.0, 2, seed))
        return (amp * ridged * w).astype(np.float32)

    return fn


def waist_band(b, y_frac, half_height, extra, mat, bones):
    """A band (sash, waistband) that follows the torso at height `y_frac`."""
    shape = b.torso(mat, inflate=extra, smooth=1.8, detail=False)
    y0 = b.y(y_frac)

    def slab(p):
        return (np.abs(p[:, 1] - y0) - half_height).astype(np.float32)

    return Intersect(shape, cut(slab), half_height * 0.35)


class Outfit:
    """Unclothed reference body; fighters override pieces."""

    def __init__(self, rig):
        self.rig = rig

    def body(self, b):
        limbs = []
        for s in ("l", "r"):
            limbs.append(b.arm(s))
            limbs.append(b.leg(s))
        core = Union([b.torso(), b.neck()] + limbs, b.m(0.03))
        hands = [b.hand(s)[0] for s in ("l", "r")]
        feet = [b.foot(s) for s in ("l", "r")]
        return Union([core, b.head()] + hands + feet, b.m(0.012))

    def extras(self, b):
        """Separate rigid pieces (jewellery, rings, helmets): list of (name, node, bone) with an
        optional fourth item of meshing options {"voxel", "triangles"}."""
        return []

    def face_hair(self, b, head, tree, rng):
        """Short strand cards on the face (beards, sideburns), or None."""
        return None

    # --- shared garment pieces -----------------------------------------------------------------

    def skin_core(self, b, arms_to_wrist=True):
        limbs = []
        for s in ("l", "r"):
            limbs.append(b.arm(s, to_wrist=arms_to_wrist))
        return Union([b.torso(), b.neck()] + limbs, b.m(0.03))

    def sleeve(self, b, s, mat, end_frac, end_r, inflate, flare_from=0.0, thick=0.004, folds=0.0015, seed=0):
        """A sleeve from the shoulder to `end_frac` of the forearm, flaring from `flare_from` of
        the forearm to radius `end_r` at its open end."""
        r = b.rig
        sh, el, wr = r.pos("upperarm_" + s), r.pos("forearm_" + s), r.pos("hand_" + s)
        lw = b.lw * (0.86 if b.female else 1.0)
        start = el + (wr - el) * flare_from
        end = el + (wr - el) * end_frac
        bones_f = ["forearm_" + s, "upperarm_" + s]
        # The arm grown outward, so the sleeve always contains it.
        upper = [b.arm(s, mat, inflate=inflate, detail=True, to_wrist=False)]
        if flare_from > 0.0:
            upper.append(round_cone(el, start, b.m(0.04) * lw + inflate, b.m(0.037) * lw + inflate, mat, bones_f))
        upper = Union(upper, b.m(0.02))
        axis = (wr - el) / np.linalg.norm(wr - el)
        flare = round_cone(start, end + axis * b.m(0.01), b.m(0.037) * lw + inflate, end_r, mat, bones_f)
        hollow = Intersect(Shell(flare, thick * 0.5), plane_keep(end, axis), b.m(0.002))
        piece = Union([upper, hollow], b.m(0.012))
        if folds > 0:
            piece = Displace(piece, _folds(sh, el, [el, el + (sh - el) * 0.25], b.m(0.07), folds, seed), folds)
        return piece

    def trousers(self, b, s, mat, inflate, bottom=0.1, folds=0.002, seed=3):
        """A trouser leg from the hip down to `bottom` (height fraction)."""
        r = b.rig
        hp, kn, an = r.pos("thigh_" + s), r.pos("shin_" + s), r.pos("foot_" + s)
        lw = b.lw
        tb, sb = ["thigh_" + s, "hips"], ["shin_" + s, "thigh_" + s]
        low = an + (kn - an) * 0.15
        leg = Union([round_cone(hp, kn, b.m(0.085) * lw + inflate, b.m(0.055) * lw + inflate, mat, tb),
                     round_cone(kn, low, b.m(0.055) * lw + inflate, b.m(0.045) * lw + inflate, mat, sb)], b.m(0.03))
        cut = Intersect(leg, half_space([0, b.y(bottom), 0], [0, -1, 0]), b.m(0.004))
        if folds > 0:
            cut = Displace(cut, _folds(hp, kn, [kn, low, hp + (kn - hp) * 0.15], b.m(0.08), folds, seed), folds)
        return cut

    def boot(self, b, s, mat, top=0.2, inflate=0.006, shaft=0.004):
        """A boot: the foot grown outward on a raised sole, with a shaft up the shin to `top`."""
        r = b.rig
        kn, an = r.pos("shin_" + s), r.pos("foot_" + s)
        foot = b.foot(s, mat, inflate=inflate)
        shin_r = b.m(0.034) * b.lw + inflate + shaft
        top_pt = an + (kn - an) * ((b.y(top) - an[1]) / (kn[1] - an[1]))
        leg = round_cone(an + np.array([0, -b.m(0.02), 0], np.float32), top_pt, shin_r + b.m(0.006), b.m(0.042) * b.lw + inflate + shaft, mat, ["shin_" + s, "foot_" + s])
        shaft_n = Intersect(leg, half_space(top_pt, [0, 1, 0]), b.m(0.003))
        return Union([foot, shaft_n], b.m(0.012))

    def heavy_boot(self, b, s, mat, sole_mat, top=0.2, inflate=0.012, shaft=0.006, sole=0.028, toe=1.1):
        """A work boot: the boot grown over a thick sole that juts past the upper, with a broad toe."""
        upper = self.boot(b, s, mat, top=top, inflate=inflate, shaft=shaft)
        if toe != 1.0:
            upper = Union([upper, b.foot(s, mat, inflate=inflate, toe_cap=toe)], b.m(0.01))
        welt = b.foot(s, sole_mat, inflate=inflate + b.m(0.006), toe_cap=toe)
        sole_n = Intersect(welt, half_space([0, b.m(sole), 0], [0, 1, 0]), b.m(0.003))
        return Union([upper, sole_n], b.m(0.003))

    def glove(self, b, s, mat, cuff=0.25):
        """Thin leather over the hand, with a cuff up the wrist."""
        r = b.rig
        hand, _ = b.hand(s, mat, inflate=b.m(0.0012))
        el, wr = r.pos("forearm_" + s), r.pos("hand_" + s)
        start = wr + (el - wr) * cuff
        c = round_cone(start, wr, b.m(0.031) * b.lw, b.m(0.029) * b.lw, mat, ["forearm_" + s, "hand_" + s])
        return Union([hand, c], b.m(0.006))


class Daxion(Outfit):
    """A simple ice-white robe, white boots, no ornament; the reddish ring on the right middle
    finger is the Five Skies Mortal Staff at rest."""

    def body(self, b):
        skin = Union([b.torso(), b.neck(), b.head()] + [b.arm(s) for s in ("l", "r")] + [b.hand(s)[0] for s in ("l", "r")], b.m(0.012))
        robe = b.torso("robe", inflate=b.m(0.01), smooth=1.25, detail=True)
        # Crossed collar: the robe opens in a V from the neck to the sternum.
        v_bottom = b.y(0.715)

        def v_open(p):
            # Kept (negative) everywhere except above a V line at the front.
            x, y, z = p[:, 0], p[:, 1], p[:, 2]
            line = v_bottom + np.abs(x + b.m(0.012)) * 2.6
            front = np.clip(-z / b.m(0.03), 0.0, 1.0)
            return ((y - line) * 0.36 * front - (1 - front) * 0.02).astype(np.float32)

        robe = Intersect(robe, cut(v_open), b.m(0.003))
        # The collar band along the opening, over the shoulders and round the back of the neck.
        def cpt(x, y, z):
            return np.array([b.m(x), b.y(y), b.m(z)], np.float32)

        band_l = [cpt(0.03, 0.716, -0.09), cpt(-0.02, 0.745, -0.1), cpt(-0.055, 0.785, -0.088), cpt(-0.075, 0.82, -0.04), cpt(-0.06, 0.84, 0.02), cpt(-0.025, 0.845, 0.055), cpt(0.0, 0.846, 0.06)]
        band_r = [np.array([-p[0], p[1], p[2]], np.float32) for p in band_l]
        band_r[0] = cpt(0.03, 0.72, -0.086)
        collar = Union([curve_tube(band_l, [b.m(0.012)] * len(band_l), "robe", ["chest", "neck"], k=b.m(0.004)),
                        curve_tube(band_r, [b.m(0.011)] * len(band_r), "robe", ["chest", "neck"], k=b.m(0.004))], b.m(0.004))
        sash = waist_band(b, 0.585, b.m(0.03), b.m(0.024), "sash", ["spine", "hips"])
        sleeves = [self.sleeve(b, s, "robe", 0.72, b.m(0.098), b.m(0.009), flare_from=0.12, thick=b.m(0.005), seed=11 if s == "l" else 12) for s in ("l", "r")]
        legs = [self.trousers(b, s, "trousers", b.m(0.022), bottom=0.13, folds=0.003, seed=21 if s == "l" else 22) for s in ("l", "r")]
        boots = [self.boot(b, s, "boots", top=0.21, inflate=b.m(0.006)) for s in ("l", "r")]
        clothed = Union([robe, collar, sash] + sleeves + legs, b.m(0.008))
        return Union([skin, clothed] + boots, b.m(0.004))

    def extras(self, b):
        from human import HandLayout
        lay = HandLayout(b, "r", 0.0)
        name, pts, radii, d = lay.chains[1]
        a, c = lay.to_bind(pts[0]), lay.to_bind(pts[1])
        mid = a + (c - a) * 0.45
        axis = (c - a) / np.linalg.norm(c - a)
        rot = frame_from(axis, self.rig.basis["hand_r"][:, 2])
        ring = torus(mid, radii[0] * 1.02 + b.m(0.0012), b.m(0.0019), "ring", ["middle1_r"], rot=rot, squash=1.6)
        return [("ring", ring, "middle1_r")]


class Ashka(Outfit):
    """Muay Thai fighter and lamplighter: a sleeveless leather vest over bare arms and midriff,
    wide fight shorts with a gold waistband, hand wraps to mid-forearm, shin and foot wraps, and a
    prajioud band on the left arm."""

    def body(self, b):
        limbs = [b.arm(s) for s in ("l", "r")] + [b.leg(s) for s in ("l", "r")]
        skin = Union([b.torso(), b.neck(), b.head()] + limbs, b.m(0.012))
        vest = b.torso("vest", inflate=b.m(0.008), smooth=1.2, detail=True)

        def chest_slab(p):
            y = p[:, 1]
            # Below the collarbones down to the bottom of the ribs, a little higher at the back.
            top = b.y(0.805) - np.clip(-p[:, 2], 0, None) * 0.25
            bottom = b.y(0.672) + np.clip(p[:, 2], 0, None) * 0.3
            return np.maximum(y - top, bottom - y).astype(np.float32)

        vest = Intersect(vest, cut(chest_slab), b.m(0.004))
        lace = [curve_tube([b.torso_pt(0.0, 0.69 + 0.02 * k, -0.118), b.torso_pt(0.012, 0.70 + 0.02 * k, -0.119)], [b.m(0.003)] * 2, "trim", ["chest"]) for k in range(5)]
        shorts = [self.trousers(b, s, "shorts", b.m(0.03), bottom=0.39, folds=0.004, seed=71 if s == "l" else 72) for s in ("l", "r")]
        hem = [self.leg_band(b, s, 0.395, "trim", b.m(0.033)) for s in ("l", "r")]
        belt = waist_band(b, 0.575, b.m(0.028), b.m(0.03), "trim", ["spine", "hips"])
        wraps = [self.glove(b, s, "wraps", cuff=0.42) for s in ("l", "r")]
        shins = [self.trousers(b, s, "wraps", b.m(0.006), bottom=0.05, folds=0.0, seed=73) for s in ("l", "r")]
        shins = [Intersect(sh, half_space([0, b.y(0.26), 0], [0, 1, 0]), b.m(0.004)) for sh in shins]
        feet = [b.foot(s, "wraps", inflate=b.m(0.003)) for s in ("l", "r")]
        band = self.arm_band(b, "l", 0.35, "band", b.m(0.012))
        clothed = Union([vest, belt] + shorts + hem, b.m(0.006))
        return Union([skin, clothed] + lace + wraps + shins + feet + [band], b.m(0.004))

    def leg_band(self, b, s, y_frac, mat, extra):
        r = b.rig
        hp, kn = r.pos("thigh_" + s), r.pos("shin_" + s)
        t = (hp[1] - b.y(y_frac)) / (hp[1] - kn[1])
        c = hp + (kn - hp) * t
        axis = (kn - hp) / np.linalg.norm(kn - hp)
        rad = b.m(0.085) * b.lw * (1 - t) + b.m(0.055) * b.lw * t + extra
        return torus(c, rad, b.m(0.007), mat, ["thigh_" + s], rot=frame_from(axis, [0, 0, 1]))

    def arm_band(self, b, s, t, mat, extra):
        r = b.rig
        sh, el = r.pos("upperarm_" + s), r.pos("forearm_" + s)
        c = sh + (el - sh) * t
        axis = (el - sh) / np.linalg.norm(el - sh)
        rad = b.m(0.044) * b.lw * 0.86 + extra
        return torus(c, rad, b.m(0.008), mat, ["upperarm_" + s], rot=frame_from(axis, [0, 0, 1]))

    def extras(self, b):
        # The lamplighter's ember charm, a small brass lamp on a cord at the breastbone.
        c = b.torso_pt(0.0, 0.745, -0.128)
        charm = Union([ellipsoid(c, [b.m(0.012), b.m(0.016), b.m(0.008)], "band", ["chest"]),
                       sphere(c + np.array([0, -b.m(0.006), -b.m(0.006)], np.float32), b.m(0.006), "ember", ["chest"])], b.m(0.002))
        return [("charm", charm, "chest")]


class Gorm(Outfit):
    """The Renderer: a giant with a bare barrel chest and arms under a thick rubberised leather
    apron, rolled canvas trousers, heavy boots and iron vambraces. The apron's bib is part of the
    body; its lower panel is a cloth sheet hanging from the belt (look.cloth)."""

    def bulk(self, b, mat="skin", inflate=0.0):
        """The gut and barrel chest over the base torso."""
        i = inflate
        return [ellipsoid(b.torso_pt(0, 0.605, -0.05), [b.m(0.165) + i, b.m(0.125) + i, b.m(0.125) + i], mat, ["spine", "hips"]),
                ellipsoid(b.torso_pt(0, 0.725, 0.01), [b.m(0.17) + i, b.m(0.12) + i, b.m(0.125) + i], mat, ["chest", "spine"])]

    def bull_neck(self, b, mat="skin", inflate=0.0):
        i = inflate
        r = b.rig
        parts = [round_cone(b.torso_pt(0, 0.815, 0.015), b.torso_pt(0, 0.88, 0.024), b.m(0.084) + i, b.m(0.062) + i, mat, ["neck", "chest", "head"])]
        for s in ("l", "r"):
            sh = r.pos("upperarm_" + s)
            top = np.array([sh[0] * 0.72, sh[1] + b.m(0.03), sh[2] + b.m(0.016)], np.float32)
            parts.append(round_cone(b.torso_pt(r.side(s) * 0.03, 0.84, 0.03), top, b.m(0.075) + i, b.m(0.05) + i, mat, ["chest", "clav_" + s, "neck"]))
        return parts

    def body(self, b):
        r = b.rig
        arms = [b.arm(s) for s in ("l", "r")]
        core = Union([b.torso()] + self.bulk(b) + self.bull_neck(b) + arms, b.m(0.035))
        skin = Union([core, b.head()] + [b.hand(s)[0] for s in ("l", "r")], b.m(0.012))
        # The bib: torso and gut grown out, cut to a panel that narrows up the chest.
        apron_body = Union([b.torso("apron", inflate=b.m(0.016), smooth=1.4, detail=False)] + self.bulk(b, "apron", b.m(0.016)), b.m(0.05))
        y0, y1 = b.y(0.575), b.y(0.765)

        def bib(p):
            x, y, z = p[:, 0], p[:, 1], p[:, 2]
            t = np.clip((y - y0) / (y1 - y0), 0.0, 1.0)
            half = b.m(0.215) + (b.m(0.112) - b.m(0.215)) * t ** 0.8
            return np.maximum.reduce([np.abs(x) - half, y - y1, b.y(0.545) - y, z + b.m(0.012)]).astype(np.float32)

        apron = Intersect(apron_body, cut(bib), b.m(0.006))
        # Neck strap from the bib's top corners round the back of the neck, riveted at the bib.
        surf = Union([b.torso(detail=True)] + self.bulk(b) + self.bull_neck(b), b.m(0.035))
        straps, rivets = [], []
        for sd in (-1, 1):
            rough = [b.torso_pt(sd * x, y, z) for x, y, z in ((0.1, 0.755, -0.1), (0.098, 0.79, -0.08), (0.092, 0.83, -0.04), (0.08, 0.85, 0.0),
                                                               (0.05, 0.855, 0.04), (0.0, 0.857, 0.05))]
            straps.append(surface_strap(surf, b.m(0.012), rough, b.torso_pt(0, 0.79, 0.0), b.m(0.019), "strap", ["chest", "neck"]))
            for dy in (0.752, 0.737):
                c, n = on_surface(apron, [b.torso_pt(sd * 0.1, dy, -0.2)], b.m(0.001))
                rivets.append(sphere(c[0], b.m(0.0065), "iron", ["chest"]))
        belt_body = Union([b.torso("strap", inflate=b.m(0.026), smooth=1.4, detail=False)] + self.bulk(b, "strap", b.m(0.026)), b.m(0.05))
        belt = Intersect(belt_body, y_slab(b.y(0.553), b.y(0.578)), b.m(0.004))
        buckle_c = cast(belt, [b.torso_pt(0, 0.5655, -0.1)], b.torso_pt(0, 0.5655, 0.0))
        buckle = box(buckle_c[0], [b.m(0.04), b.m(0.03), b.m(0.008)], b.m(0.004), "iron", ["hips", "spine"])
        # Canvas trousers: seat and legs, rolled up over the boots.
        seat = Intersect(b.torso("trousers", inflate=b.m(0.02), smooth=1.3), y_slab(-1.0, b.y(0.57)), b.m(0.004))
        legs = [self.trousers(b, s, "trousers", b.m(0.022), bottom=0.2, folds=0.005, seed=31 if s == "l" else 32) for s in ("l", "r")]
        rolls = []
        for s in ("l", "r"):
            kn, an = r.pos("shin_" + s), r.pos("foot_" + s)
            t = (kn[1] - b.y(0.205)) / (kn[1] - an[1])
            rad = b.m(0.0512) * b.lw + b.m(0.024)
            rolls.append(limb_ring(b, "shin_" + s, "foot_" + s, t, rad, b.m(0.017), "trousers", ["shin_" + s], squash=1.25))
        boots = [self.heavy_boot(b, s, "boots", "sole", top=0.215, inflate=b.m(0.014), shaft=b.m(0.008), sole=0.03, toe=1.15) for s in ("l", "r")]
        clothed = Union([seat] + legs, b.m(0.03))
        return Union([skin, apron, belt, buckle, clothed] + straps + rivets + rolls + boots, b.m(0.003))

    def vambrace(self, b, s):
        r = b.rig
        el, wr = r.pos("forearm_" + s), r.pos("hand_" + s)
        fa, ff, fl = r.limb_frame("forearm_" + s, "hand_" + s, s)
        arm = b.arm(s, "iron")
        shell = Intersect(Offset(arm, b.m(0.012)), axis_slab(el, wr, 0.28, 0.9), b.m(0.005))
        bands, rivets = [], []
        for t in (0.3, 0.83):
            band = Intersect(Offset(arm, b.m(0.0175)), axis_slab(el, wr, t, t + 0.055), b.m(0.003))
            bands.append(band)
            c = el + (wr - el) * (t + 0.0275)
            dirs = [np.cos(a) * ff + np.sin(a) * fl for a in np.linspace(0, 2 * np.pi, 7)[:-1]]
            pts, _ = on_surface(band, [c + d * b.m(0.08) for d in dirs])
            rivets += [sphere(p, b.m(0.005), "iron", ["forearm_" + s]) for p in pts]
        # A raised spine down the back of the forearm.
        a, c = el + (wr - el) * 0.36, el + (wr - el) * 0.8
        pts, _ = on_surface(shell, [a - ff * b.m(0.08), c - ff * b.m(0.08)])
        ridge = round_cone(pts[0], pts[1], b.m(0.006), b.m(0.006), "iron", ["forearm_" + s])
        return Union([shell] + bands + rivets + [ridge], b.m(0.002))

    def extras(self, b):
        return [("vambrace_" + s, self.vambrace(b, s), "forearm_" + s, {"triangles": 7000, "voxel": 0.0012}) for s in ("l", "r")]


class Nix(Outfit):
    """Hand of the Drowned: charcoal and black wrapped garments, a deep hood with a capelet, a cloth
    mask over nose and mouth, a slop-green sash knotted at the left hip (its tails are cloth
    sheets), bandaged forearms and shins, and split-toe footwraps."""

    def hood(self, b):
        h = b.head_model()
        P, M = h.P, h.M
        hb = ["head"]
        shell = Union([ellipsoid(P(0, 0.118, 0.006), [M(0.1), M(0.122), M(0.13)], "hood", hb),
                       # A pointed fall at the back of the hood.
                       round_cone(P(0, 0.15, 0.06), P(0, 0.1, 0.165), M(0.07), M(0.012), "hood", hb),
                       round_cone(P(0, 0.03, 0.01), P(0, -0.05, 0.03), M(0.092), M(0.105), "hood", ["head", "neck"])], M(0.03))

        def cavity(p):
            x, y, z = h.to_ref(p)
            ring = np.sqrt((x / 0.058) ** 2 + ((y - 0.078) / 0.082) ** 2) - 1.0
            return (np.maximum(ring * 0.05, z + 0.03) * h.hs).astype(np.float32)

        opening = cut(cavity)
        return Subtract(shell, opening, M(0.006))

    def mask(self, b):
        h = b.head_model()
        P, M = h.P, h.M
        face = Union([Offset(h.node("mask"), M(0.0035)),
                      round_cone(P(0, 0.097, -0.104), P(0, 0.07, -0.13), M(0.008), M(0.012), "mask", ["head"]),
                      round_cone(P(0, 0.064, -0.121), P(0, 0.01, -0.096), M(0.013), M(0.016), "mask", ["head"])], M(0.016))

        def edge(p):
            x, y, z = h.to_ref(p)
            top = 0.087 + 0.009 * np.exp(-(x / 0.018) ** 2)
            return (np.maximum(y - top, z + 0.02) * h.hs).astype(np.float32)

        return Intersect(retag(face, "mask", ["head"]), cut(edge), M(0.002))

    def body(self, b):
        r = b.rig
        skin = Union([b.head(), b.neck()], b.m(0.012))
        # The wrapped top to the hips, and the capelet that falls from the hood over the shoulders.
        top = Intersect(b.torso("wrap", inflate=b.m(0.011), smooth=1.3), y_slab(b.y(0.49), 9.0), b.m(0.004))
        top = Displace(top, _folds(b.torso_pt(0, 0.8, 0), b.torso_pt(0, 0.5, 0), [b.torso_pt(0, 0.6, -0.08), b.torso_pt(0, 0.6, 0.08)], b.m(0.1), 0.0025, 41, 30.0, 6.0), 0.0025)
        cape = Intersect(b.torso("hood", inflate=b.m(0.03), smooth=1.6, detail=False), y_slab(b.y(0.755), 9.0), b.m(0.006))
        neck = b.neck("hood", inflate=b.m(0.022), top=0.88)
        hood = Union([self.hood(b), neck, cape], b.m(0.03))
        sleeves = [b.arm(s, "wrap", inflate=b.m(0.009)) for s in ("l", "r")]
        sleeves = [Displace(sl, _folds(r.pos("upperarm_" + s), r.pos("forearm_" + s), [r.pos("forearm_" + s)], b.m(0.07), 0.0022, 43 + i), 0.0022)
                   for i, (sl, s) in enumerate(zip(sleeves, ("l", "r")))]
        hands = [self.glove(b, s, "wrap_dark", cuff=0.3) for s in ("l", "r")]
        arm_wraps = [wrap_bands(b.arm(s, "wrap", inflate=b.m(0.009)), r.pos("forearm_" + s), r.pos("hand_" + s), 0.12, 0.93, 4.2, b.m(0.011),
                                b.m(0.004), "wrap_dark", ["forearm_" + s], phase=1.0, hand=r.side(s)) for s in ("l", "r")]
        # A band crossing the chest from the right shoulder to the left hip.
        chest = Union([b.torso("wrap", inflate=b.m(0.011), smooth=1.3)], 0.0)
        cross = surface_strap(chest, b.m(0.006), [b.torso_pt(x, y, z) for x, y, z in ((0.1, 0.81, -0.05), (0.06, 0.75, -0.12), (0.0, 0.68, -0.14), (-0.07, 0.6, -0.12), (-0.12, 0.54, -0.06))],
                              b.torso_pt(0, 0.68, 0.0), b.m(0.022), "wrap_dark", ["chest", "spine", "hips"])
        cross_b = surface_strap(chest, b.m(0.006), [b.torso_pt(x, y, z) for x, y, z in ((0.1, 0.81, 0.05), (0.06, 0.75, 0.11), (0.0, 0.68, 0.12), (-0.07, 0.6, 0.11), (-0.12, 0.54, 0.06))],
                                b.torso_pt(0, 0.68, 0.0), b.m(0.022), "wrap_dark", ["chest", "spine", "hips"])
        sash = waist_band(b, 0.57, b.m(0.032), b.m(0.024), "sash", ["spine", "hips"])
        knot_c = cast(sash, [b.torso_pt(-0.11, 0.57, -0.09)], b.torso_pt(0, 0.57, 0.0))[0]
        knot = Union([ellipsoid(knot_c, [b.m(0.03), b.m(0.026), b.m(0.02)], "sash", ["hips", "spine"]),
                      ellipsoid(knot_c + np.array([b.m(0.03), b.m(0.01), -b.m(0.004)], np.float32), [b.m(0.03), b.m(0.018), b.m(0.012)], "sash", ["hips", "spine"],
                                rot=frame_from([0.4, 1.0, 0.0], [0, 0, -1])),
                      ellipsoid(knot_c + np.array([-b.m(0.02), -b.m(0.006), b.m(0.006)], np.float32), [b.m(0.025), b.m(0.016), b.m(0.012)], "sash", ["hips", "spine"],
                                rot=frame_from([-0.5, 1.0, 0.0], [0, 0, -1]))], b.m(0.008))
        # Loose trousers gathered into bandaged shins.
        seat = Intersect(b.torso("trousers", inflate=b.m(0.012), smooth=1.3), y_slab(-1.0, b.y(0.53)), b.m(0.004))
        legs = [self.trousers(b, s, "trousers", b.m(0.026), bottom=0.2, folds=0.005, seed=45 if s == "l" else 46) for s in ("l", "r")]
        shins, shin_wraps = [], []
        for s in ("l", "r"):
            leg = b.leg(s, "wrap_dark", inflate=b.m(0.012))
            shin = Intersect(leg, y_slab(b.y(0.035), b.y(0.235)), b.m(0.004))
            shins.append(shin)
            shin_wraps.append(wrap_bands(leg, r.pos("shin_" + s), r.pos("foot_" + s), 0.22, 0.97, 4.5, b.m(0.012), b.m(0.004), "wrap",
                                         ["shin_" + s], phase=0.5, hand=-r.side(s)))
        feet = [self.tabi(b, s) for s in ("l", "r")]
        clothed = Union([top, seat] + legs + sleeves, b.m(0.02))
        return Union([skin, self.mask(b), hood, clothed, sash, knot, cross, cross_b] + hands + arm_wraps + shins + shin_wraps + feet, b.m(0.003))

    def tabi(self, b, s):
        """A split-toe footwrap: the foot grown a little, with a groove between the big toe and the rest."""
        r = b.rig
        an = r.pos("foot_" + s)
        foot = b.foot(s, "feet", inflate=b.m(0.005))
        medial = -r.side(s)
        groove = box([an[0] + medial * b.m(0.012), b.m(0.035), an[2] - b.m(0.22)], [b.m(0.0035), b.m(0.05), b.m(0.06)], b.m(0.002), "feet", ["foot_" + s])
        return Subtract(foot, groove, b.m(0.004))


class Mirela(Outfit):
    """The Keening Widow: an ivory mourning gown with a high collar, a long-line bodice to the
    hips and long sleeves, a black ribbon at the waist and a black choker, black gloves, and
    ivory leggings and black boots under the skirt. The skirt, the black lace veil and the long
    hair are cloth sheets (look.cloth)."""

    def body(self, b):
        r = b.rig
        skin = Union([b.head(), b.neck()], b.m(0.012))
        bodice = Intersect(b.torso("gown", inflate=b.m(0.008), smooth=1.2), y_slab(b.y(0.485), 9.0), b.m(0.004))
        bodice = Displace(bodice, _folds(b.torso_pt(0, 0.7, 0), b.torso_pt(0, 0.5, 0), [b.torso_pt(0, 0.6, -0.08)], b.m(0.08), 0.0015, 51, 40.0, 8.0), 0.0015)
        collar = b.neck("gown", inflate=b.m(0.006), top=0.872)
        choker = torus(b.torso_pt(0, 0.874, 0.016), b.m(0.058), b.m(0.0055), "lace", ["neck", "head"], rot=frame_from([0, 1, 0.12], [0, 0, 1]))
        ribbon = waist_band(b, 0.6, b.m(0.014), b.m(0.014), "lace", ["spine", "hips"])
        bow_c = cast(ribbon, [b.torso_pt(0.0, 0.6, -0.08)], b.torso_pt(0, 0.6, 0.0))[0]
        bow = Union([ellipsoid(bow_c + np.array([sd * b.m(0.018), 0, -b.m(0.004)], np.float32), [b.m(0.018), b.m(0.011), b.m(0.007)], "lace", ["spine", "hips"],
                               rot=frame_from([sd * 0.3, 1.0, 0.0], [0, 0, -1])) for sd in (-1, 1)] + [sphere(bow_c, b.m(0.008), "lace", ["spine", "hips"])], b.m(0.004))
        sleeves = [self.sleeve(b, s, "gown", 0.9, b.m(0.052), b.m(0.007), flare_from=0.55, thick=b.m(0.004), folds=0.002, seed=53 if s == "l" else 54) for s in ("l", "r")]
        gloves = [self.glove(b, s, "gloves", cuff=0.42) for s in ("l", "r")]
        legs = [self.trousers(b, s, "underdress", b.m(0.012), bottom=0.07, folds=0.002, seed=55 if s == "l" else 56) for s in ("l", "r")]
        seat = Intersect(b.torso("underdress", inflate=b.m(0.006)), y_slab(-1.0, b.y(0.5)), b.m(0.004))
        boots = [self.boot(b, s, "boots", top=0.11, inflate=b.m(0.004), shaft=b.m(0.003)) for s in ("l", "r")]
        clothed = Union([bodice, collar, seat] + sleeves + legs, b.m(0.012))
        return Union([skin, clothed, choker, ribbon, bow] + gloves + boots, b.m(0.003))


class Bram(Outfit):
    """Captain of the Lantern Watch: a long navy watch coat (double-breasted with brass buttons, a
    stand collar, epaulettes and cuffs, belted in black leather; its skirts are a cloth sheet open
    at the front), dark trousers, polished boots, a peaked watch cap (extras) and mutton chops."""

    @staticmethod
    def chops(x, y, z):
        """The mutton chops and moustache, negative inside (head reference units)."""
        ax = np.abs(x)
        front = np.interp(y, [-0.01, 0.02, 0.05, 0.075, 0.1, 0.135], [-0.06, -0.1, -0.1, -0.066, -0.042, -0.02])
        side = np.maximum.reduce([0.029 - ax, -0.006 - y, y - 0.135, front - z, z - 0.004])
        mous = np.maximum.reduce([ax - 0.034, 0.0445 - y, y - 0.061, z + 0.082])
        return np.minimum(side, mous)

    def body(self, b):
        r = b.rig
        h = b.head_model()
        skin = Union([b.head(), b.neck()] + [b.arm(s) for s in ("l", "r")], b.m(0.012))
        hands = [b.hand(s)[0] for s in ("l", "r")]

        def chop_region(p):
            x, y, z = h.to_ref(p)
            return (self.chops(x, y, z) * h.hs).astype(np.float32)

        beard = Intersect(retag(Offset(h.node("beard"), h.M(0.0035)), "beard", ["head"]), cut(chop_region), h.M(0.002))
        coat_shape = b.torso("coat", inflate=b.m(0.017), smooth=1.25)
        coat = Intersect(coat_shape, y_slab(b.y(0.535), 9.0), b.m(0.004))
        coat = Displace(coat, _folds(b.torso_pt(0, 0.8, 0), b.torso_pt(0, 0.55, 0), [b.torso_pt(sd * 0.12, 0.6, 0.0) for sd in (-1, 1)], b.m(0.08), 0.003, 61, 30.0, 6.0), 0.003)
        # The overlapping front panel of the double-breasted coat.
        def panel(p):
            x, y, z = p[:, 0], p[:, 1], p[:, 2]
            return np.maximum.reduce([x - b.m(0.07), -x - b.m(0.12), z + b.m(0.04), b.y(0.56) - y, y - b.y(0.818)]).astype(np.float32)

        front = Intersect(layer(coat_shape, b.m(0.0045), "coat", ["chest", "spine"]), cut(panel), b.m(0.002))
        buttons = []
        for x in (-0.048, 0.05):
            for y in (0.625, 0.675, 0.725, 0.775):
                c = cast(coat_shape, [b.torso_pt(x, y, -0.05)], b.torso_pt(x * 0.5, y, 0.0))[0]
                buttons.append(sphere(c, b.m(0.0105), "brass", ["chest", "spine"]))
        collar = Intersect(b.neck("coat", inflate=b.m(0.028), top=0.88), y_slab(b.y(0.812), b.y(0.866)), b.m(0.006))
        collar_rim = Intersect(b.neck("coat_dark", inflate=b.m(0.031), top=0.88), y_slab(b.y(0.857), b.y(0.869)), b.m(0.004))
        belt = Intersect(b.torso("belt", inflate=b.m(0.028), smooth=1.3, detail=False), y_slab(b.y(0.556), b.y(0.586)), b.m(0.004))
        bc = cast(belt, [b.torso_pt(0.0, 0.571, -0.1)], b.torso_pt(0, 0.571, 0.0))[0]
        buckle = Subtract(box(bc, [b.m(0.034), b.m(0.026), b.m(0.007)], b.m(0.003), "brass", ["hips", "spine"]),
                          box(bc - np.array([0, 0, b.m(0.006)], np.float32), [b.m(0.022), b.m(0.014), b.m(0.006)], b.m(0.002), "brass", ["hips", "spine"]), b.m(0.001))
        epaulettes = []
        for s in ("l", "r"):
            sh = r.pos("upperarm_" + s)
            sd = r.side(s)
            pts = [b.torso_pt(sd * 0.075, 0.835, 0.005), np.array([sh[0] * 0.9, sh[1] + b.m(0.05), sh[2]], np.float32)]
            epaulettes.append(surface_strap(coat_shape, b.m(0.006), pts, b.torso_pt(sd * 0.06, 0.76, 0.0), b.m(0.02), "coat_dark", ["chest", "clav_" + s], per=8))
            c = cast(coat_shape, [np.array([sh[0] * 0.86, sh[1] + b.m(0.06), sh[2]], np.float32)], b.torso_pt(sd * 0.06, 0.76, 0.0))[0]
            epaulettes.append(sphere(c, b.m(0.008), "brass", ["chest", "clav_" + s]))
        sleeves, cuffs = [], []
        for s in ("l", "r"):
            el, wr = r.pos("forearm_" + s), r.pos("hand_" + s)
            arm = b.arm(s, "coat", inflate=b.m(0.014))
            sl = Intersect(arm, axis_slab(el, wr, -9.0, 0.88), b.m(0.004))
            sleeves.append(Displace(sl, _folds(r.pos("upperarm_" + s), el, [el, el + (wr - el) * 0.4], b.m(0.07), 0.0028, 63 if s == "l" else 64), 0.0028))
            cuff = Intersect(Offset(b.arm(s, "coat_dark"), b.m(0.021)), axis_slab(el, wr, 0.72, 0.88), b.m(0.004))
            cuffs.append(cuff)
            fa, ff, fl = r.limb_frame("forearm_" + s, "hand_" + s, s)
            c = cast(cuff, [el + (wr - el) * 0.8 + fl * b.m(0.06)], el + (wr - el) * 0.8)[0]
            cuffs.append(sphere(c, b.m(0.007), "brass", ["forearm_" + s]))
        seat = Intersect(b.torso("trousers", inflate=b.m(0.01), smooth=1.3), y_slab(-1.0, b.y(0.545)), b.m(0.004))
        legs = [self.trousers(b, s, "trousers", b.m(0.017), bottom=0.17, folds=0.004, seed=65 if s == "l" else 66) for s in ("l", "r")]
        boots = [self.heavy_boot(b, s, "boots", "sole", top=0.22, inflate=b.m(0.01), shaft=b.m(0.006), sole=0.022, toe=1.04) for s in ("l", "r")]
        clothed = Union([coat, collar] + sleeves + [seat] + legs, b.m(0.012))
        return Union([skin, beard, clothed, front, collar_rim, belt, buckle] + hands + buttons + epaulettes + cuffs + boots, b.m(0.003))

    def cap(self, b):
        h = b.head_model()
        P, M = h.P, h.M
        hb = ["head"]
        tilt = frame_from([0.0, 1.0, 0.1], [0, 0, 1])
        band = elliptic_cylinder(P(0, 0.0, 0.004), M(0.089), M(0.111), M(0.146), M(0.205), M(0.004), "cap", hb, rot=tilt)
        crown = elliptic_cylinder(P(0, 0.0, 0.0), M(0.095), M(0.12), M(0.2), M(0.248), M(0.012), "cap", hb, rot=frame_from([0.0, 1.0, 0.16], [0, 0, 1]))
        piping = elliptic_cylinder(P(0, 0.0, 0.0), M(0.0965), M(0.1215), M(0.2), M(0.207), M(0.003), "cap_dark", hb, rot=frame_from([0.0, 1.0, 0.13], [0, 0, 1]))
        peak = Intersect(ellipsoid(P(0, 0.155, -0.112), [M(0.084), M(0.0045), M(0.062)], "peak", hb, rot=frame_from([0.0, 1.0, -0.42], [0, 0, -1])),
                         half_space(P(0, 0, -0.088), [0, 0, 1]), M(0.002))
        strap_ = Intersect(elliptic_cylinder(P(0, 0.0, 0.004), M(0.0915), M(0.1135), M(0.149), M(0.161), M(0.002), "peak", hb, rot=tilt),
                           half_space(P(0, 0, -0.03), [0, 0, 1]), M(0.002))
        studs = [sphere(P(sd * 0.084, 0.156, -0.05), M(0.0055), "brass", hb) for sd in (-1, 1)]
        badge = Union([ellipsoid(P(0, 0.182, -0.115), [M(0.015), M(0.019), M(0.005)], "brass", hb, rot=frame_from([0, 1, 0.1], [0, 0, -1])),
                       sphere(P(0, 0.183, -0.119), M(0.006), "brass", hb)], M(0.002))
        return Union([band, crown, piping, peak, strap_, badge] + studs, M(0.002))

    def extras(self, b):
        return [("cap", self.cap(b), "head", {"triangles": 9000, "voxel": 0.0012})]

    def face_hair(self, b, head, tree, rng):
        import hair

        def flow(x, y, z):
            if y > 0.044 and abs(x) < 0.034:
                return head.P(np.sign(x) * 0.6, -1.0, 0.0) - head.P(0, 0, 0)
            return head.P(np.sign(x) * 0.15, -1.0, 0.25) - head.P(0, 0, 0)

        return hair.face_strands(head, tree, self.chops, 900, 0.016, 0.004, 0.0035, rng, flow)


class Hollis(Outfit):
    """The Diving Bell: an inflated olive canvas suit with rubber cuffs and thick rubber gloves, a
    lead weight on the chest and lead-soled boots with brass toe caps; the brass corselet and the
    round helmet with three portholes are rigid extras (their glass and inner glow are a prop)."""

    SUIT = 0.032

    def body(self, b):
        r = b.rig
        skin = Union([b.head(), b.neck()], b.m(0.012))
        i = b.m(self.SUIT)
        suit = b.torso("suit", inflate=i, smooth=1.6, detail=False)
        suit = Displace(suit, _folds(b.torso_pt(0, 0.75, 0), b.torso_pt(0, 0.5, 0), [b.torso_pt(sd * 0.1, 0.6, -0.05) for sd in (-1, 1)] + [b.torso_pt(0, 0.56, 0.1)],
                                     b.m(0.1), 0.006, 71, 26.0, 5.0), 0.006)
        neck = b.neck("suit", inflate=i * 0.8, top=0.86)
        arms, cuffs, gloves = [], [], []
        for k, s in enumerate(("l", "r")):
            sh, el, wr = r.pos("upperarm_" + s), r.pos("forearm_" + s), r.pos("hand_" + s)
            arm = Intersect(b.arm(s, "suit", inflate=b.m(0.028), detail=False), axis_slab(el, wr, -9.0, 0.84), b.m(0.006))
            arm = Displace(arm, _folds(sh, el, [el, sh + (el - sh) * 0.3, el + (wr - el) * 0.5], b.m(0.08), 0.006, 73 + k, 40.0, 7.0), 0.006)
            arms.append(arm)
            cuffs.append(Intersect(Offset(b.arm(s, "rubber", detail=False), b.m(0.024)), axis_slab(el, wr, 0.78, 0.92), b.m(0.006)))
            hand, _ = b.hand(s, "rubber", inflate=b.m(0.0055))
            gloves.append(Union([hand, round_cone(el + (wr - el) * 0.8, wr, b.m(0.04) * b.lw, b.m(0.034) * b.lw, "rubber", ["forearm_" + s, "hand_" + s])], b.m(0.008)))
        legs = []
        for k, s in enumerate(("l", "r")):
            legs.append(self.trousers(b, s, "suit", b.m(0.034), bottom=0.13, folds=0.0065, seed=75 + k))
        weight = Intersect(layer(b.torso(detail=False), b.m(0.075), "lead", ["chest", "spine"]),
                           cut(lambda p: np.maximum.reduce([np.abs(p[:, 0]) - b.m(0.1), b.y(0.655) - p[:, 1], p[:, 1] - b.y(0.735), p[:, 2] + b.m(0.06)]).astype(np.float32)), b.m(0.01))
        weight_rim = Intersect(layer(b.torso(detail=False), b.m(0.079), "brass_dark", ["chest", "spine"]),
                               cut(lambda p: np.maximum.reduce([np.abs(p[:, 0]) - b.m(0.106), b.y(0.651) - p[:, 1], p[:, 1] - b.y(0.66), p[:, 2] + b.m(0.06)]).astype(np.float32)), b.m(0.003))
        belt = Intersect(b.torso("belt", inflate=i + b.m(0.012), smooth=1.6, detail=False), y_slab(b.y(0.555), b.y(0.585)), b.m(0.004))
        boots = [self.diving_boot(b, s) for s in ("l", "r")]
        clothed = Union([suit, neck] + arms + legs, b.m(0.03))
        return Union([skin, clothed, weight, weight_rim, belt] + cuffs + gloves + boots, b.m(0.004))

    def diving_boot(self, b, s):
        r = b.rig
        an = r.pos("foot_" + s)
        upper = self.heavy_boot(b, s, "boot", "lead", top=0.18, inflate=b.m(0.022), shaft=b.m(0.014), sole=0.05, toe=1.22)
        cap = Intersect(b.foot(s, "brass", inflate=b.m(0.027), toe_cap=1.22), half_space([0, 0, an[2] - b.m(0.11)], [0, 0, 1]), b.m(0.006))
        cap = Intersect(cap, half_space([0, b.m(0.045), 0], [0, -1, 0]), b.m(0.003))
        strap_ = Intersect(b.foot(s, "belt", inflate=b.m(0.026), toe_cap=1.22),
                           cut(lambda p: (np.abs(p[:, 2] - (an[2] - b.m(0.06))) - b.m(0.014)).astype(np.float32)), b.m(0.003))
        return Union([upper, cap, strap_], b.m(0.004))

    def corselet(self, b):
        r = b.rig
        shx = abs(r.pos("upperarm_l")[0])
        base = b.torso(detail=False)
        plate = layer(base, b.m(0.062), "brass", ["chest"])

        def region(p):
            x, y, z = p[:, 0], p[:, 1], p[:, 2]
            # Lower edge dips to a rounded point front and back.
            edge = b.y(0.745) + b.m(0.06) * (np.abs(x) / shx) ** 2
            return np.maximum(edge - y, np.abs(x) - shx * 0.88).astype(np.float32)

        body = Intersect(plate, cut(region), b.m(0.012))
        ring = elliptic_cylinder(b.torso_pt(0, 0, 0.015), b.m(0.105), b.m(0.1), b.y(0.82), b.y(0.872), b.m(0.006), "brass", ["chest"])
        flange = elliptic_cylinder(b.torso_pt(0, 0, 0.015), b.m(0.118), b.m(0.113), b.y(0.86), b.y(0.874), b.m(0.004), "brass_dark", ["chest"])
        hole = elliptic_cylinder(b.torso_pt(0, 0, 0.015), b.m(0.088), b.m(0.083), b.y(0.8), b.y(0.9), 0.0, "brass", ["chest"])
        studs = []
        rim_pts = []
        for a in np.linspace(0, 2 * np.pi, 15)[:-1]:
            x = np.sin(a) * shx * 0.8
            y = b.y(0.745) + b.m(0.06) * (np.abs(x) / shx) ** 2 + b.m(0.02)
            rim_pts.append([x, y, -np.cos(a) * b.m(0.3)])
        rim_pts = np.array(rim_pts, np.float32)
        centres = np.stack([np.zeros(len(rim_pts)), rim_pts[:, 1], np.zeros(len(rim_pts))], axis=1).astype(np.float32)
        on = cast(body, rim_pts, centres, reach=1.0)
        studs += [sphere(p, b.m(0.011), "brass_dark", ["chest"]) for p in on]
        for a in np.linspace(0, 2 * np.pi, 9)[:-1] + 0.2:
            p = b.torso_pt(np.sin(a) * 0.118, 0.867, 0.015 - np.cos(a) * 0.113)
            studs.append(ellipsoid(p, [b.m(0.012), b.m(0.009), b.m(0.012)], "brass_dark", ["chest"]))
        shell = Union([body, ring, flange] + studs, b.m(0.004))
        return Subtract(shell, hole, b.m(0.004))

    def helmet(self, b):
        h = b.head_model()
        P, M = h.P, h.M
        hb = ["head"]
        c = P(0, 0.105, -0.004)
        R = M(0.172)
        dome = Shell(sphere(c, R - M(0.005), "brass", hb), M(0.005))
        dome = Intersect(dome, half_space(P(0, -0.035, 0), [0, -1, 0]), M(0.004))
        ports = [(P(0, 0.1, -0.172), np.array([0, 0, -1.0], np.float32), M(0.062)),
                 (P(-0.172, 0.11, 0.0), np.array([-1.0, 0, 0], np.float32), M(0.047)),
                 (P(0.172, 0.11, 0.0), np.array([1.0, 0, 0], np.float32), M(0.047))]
        holes, frames = [], []
        for pc, n, rad in ports:
            holes.append(round_cone(pc - n * M(0.06), pc + n * M(0.08), rad, rad, "brass", hb))
            rot = frame_from(n, [0, 1, 0])
            frames.append(torus(pc + n * M(0.004), rad + M(0.009), M(0.012), "brass", hb, rot=rot))
            frames.append(torus(pc + n * M(0.016), rad + M(0.004), M(0.006), "brass_dark", hb, rot=rot))
            # Bolts round each frame.
            up = np.array([0, 1.0, 0], np.float32)
            side = np.cross(n, up)
            for a in np.linspace(0, 2 * np.pi, 7)[:-1]:
                q = pc + n * M(0.008) + (np.cos(a) * up + np.sin(a) * side) * (rad + M(0.02))
                frames.append(sphere(q, M(0.0065), "brass_dark", hb))
        # Guard bars across the side ports.
        for sd in (-1, 1):
            for dz in (-0.022, 0.0, 0.022):
                frames.append(round_cone(P(sd * 0.181, 0.11 + 0.052, dz), P(sd * 0.181, 0.11 - 0.052, dz), M(0.0045), M(0.0045), "brass_dark", hb))
        neck = torus(P(0, -0.03, 0.0), M(0.122), M(0.014), "brass", hb, rot=frame_from([0, 1, 0], [0, 0, 1]))
        neck2 = elliptic_cylinder(P(0, 0.0, 0.0), M(0.128), M(0.128), M(-0.045), M(-0.02), M(0.005), "brass_dark", hb)
        neck_hole = elliptic_cylinder(P(0, 0.0, 0.0), M(0.104), M(0.104), M(-0.1), M(0.0), 0.0, "brass", hb)
        # Air inlet gooseneck at the back and the exhaust valve on the right.
        goose = curve_tube([P(0.03, 0.15, 0.15), P(0.04, 0.16, 0.205), P(0.045, 0.12, 0.235), P(0.05, 0.06, 0.24)], [M(0.016), M(0.016), M(0.015), M(0.015)], "brass", hb, k=M(0.006))
        valve = Union([round_cone(P(0.115, 0.03, -0.11), P(0.135, 0.02, -0.135), M(0.017), M(0.02), "brass", hb),
                       sphere(P(0.14, 0.018, -0.142), M(0.012), "brass_dark", hb)], M(0.004))
        crest = Intersect(Shell(sphere(c, R + M(0.003), "brass_dark", hb), M(0.003)),
                          cut(lambda p: (np.abs((p - c)[:, 0]) - M(0.012)).astype(np.float32)), M(0.002))
        crest = Intersect(crest, half_space(P(0, 0.18, 0), [0, -1, 0]), M(0.002))
        shell = Union([dome, neck, neck2, goose, valve, crest] + frames, M(0.003))
        return Subtract(shell, Union(holes + [neck_hole], 0.0), M(0.003))

    def extras(self, b):
        return [("corselet", self.corselet(b), "chest", {"triangles": 16000, "voxel": 0.0016}),
                ("helmet", self.helmet(b), "head", {"triangles": 22000, "voxel": 0.0014})]


class Slopbat(Outfit):
    """The sovereign of the Sump: a gaunt body of glossy black-green tar with faint green veins,
    long clawed fingers and toes, bat ears and a crown of horn. Its wings are props (props.gd,
    kind "wing") and its tattered membrane cloak is a cloth sheet."""

    def body(self, b):
        base = super().body(b)
        r = b.rig
        veins = []
        for k, s in enumerate(("l", "r")):
            sh, el, wr = r.pos("upperarm_" + s), r.pos("forearm_" + s), r.pos("hand_" + s)
            hp, kn, an = r.pos("thigh_" + s), r.pos("shin_" + s), r.pos("foot_" + s)
            side = r.side(s)
            for a, c, bones in ((sh, el, ["upperarm_" + s]), (el, wr, ["forearm_" + s]), (hp, kn, ["thigh_" + s]), (kn, an, ["shin_" + s])):
                mid = (a + c) * 0.5
                wiggle = [a + (c - a) * t + np.array([side * b.m(0.06), 0.0, -b.m(0.05) + b.m(0.02) * np.sin(t * 9.0 + k)], np.float32) for t in (0.08, 0.35, 0.62, 0.92)]
                veins.append(surface_strap(base, b.m(0.0016), wiggle, mid, b.m(0.0042), "vein", bones, per=5))
        chest = [b.torso_pt(sd * x, y, -0.14) for sd in (-1, 1) for x, y in ((0.02, 0.62), (0.06, 0.68), (0.1, 0.74), (0.14, 0.79))]
        for sd in (0, 1):
            pts = chest[sd * 4:sd * 4 + 4]
            veins.append(surface_strap(base, b.m(0.0016), pts, b.torso_pt(0, 0.7, 0.0), b.m(0.004), "vein", ["chest", "spine"], per=5))
        return Union([base] + veins, b.m(0.002))

    def extras(self, b):
        from human import HandLayout
        h = b.head_model()
        P, M = h.P, h.M
        hb = ["head"]
        parts = []
        for sd in (-1, 1):
            # Tall bat ears swept back, and three horns each side curving back over the skull.
            parts.append(round_cone(P(sd * 0.07, 0.14, 0.02), P(sd * 0.13, 0.3, 0.07), M(0.03), M(0.004), "skin", hb, squash=(1.0, 0.35)))
            for j, (x, y, z, l) in enumerate(((0.035, 0.205, -0.02, 0.12), (0.06, 0.19, 0.02, 0.15), (0.075, 0.165, 0.06, 0.12))):
                base = P(sd * x, y, z)
                tip = P(sd * (x + 0.02), y + l * 0.6, z + l * 0.9)
                parts.append(round_cone(base, tip, M(0.012), M(0.002), "horn", hb))
        out = [("crown", Union(parts, M(0.004)), "head")]
        for s in ("l", "r"):
            lay = HandLayout(b, s, 0.0)
            for name, pts, radii, d in lay.chains:
                tip = lay.to_bind(pts[3])
                prev = lay.to_bind(pts[2])
                ax = (tip - prev) / max(float(np.linalg.norm(tip - prev)), 1e-6)
                claw = round_cone(tip - ax * b.m(0.004), tip + ax * b.m(0.05), radii[3] * 0.9, b.m(0.0015), "claw", ["%s3_%s" % (name, s)])
                out.append(("claw_%s_%s" % (name, s), claw, "%s3_%s" % (name, s)))
            an = b.rig.pos("foot_" + s)
            toes = Union([round_cone(np.array([an[0] + d * b.m(0.022), b.m(0.02), an[2] - b.m(0.15)], np.float32),
                                     np.array([an[0] + d * b.m(0.03), b.m(0.006), an[2] - b.m(0.22)], np.float32), b.m(0.012), b.m(0.002), "claw", ["foot_" + s])
                          for d in (-1.0, 0.0, 1.0)], b.m(0.004))
            out.append(("toes_" + s, toes, "foot_" + s))
        return out


def for_fighter(rig):
    return {"daxion": Daxion, "ashka": Ashka, "gorm": Gorm, "nix": Nix, "mirela": Mirela, "bram": Bram, "hollis": Hollis, "slopbat": Slopbat}.get(rig.id, Outfit)(rig)
