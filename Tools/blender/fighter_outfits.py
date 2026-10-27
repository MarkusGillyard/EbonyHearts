"""Costumes authored as meshes on the MPFB body (see fighter_geo for the primitives).

Tight garments are shells duplicated from the body faces they cover, so they carry the body's
own skin weights and UVs; the body faces they fully cover are deleted (MakeHuman's delete-group
idea). Loose parts (shorts legs, apron) get radial flare / hanging-sheet construction and
hand-authored weights. Rigid props are weighted 100% to one bone."""
import math
import os

import bpy
import numpy as np
from mathutils import Vector

import fighter_core as core
import fighter_geo as geo

ARM_BONES = ["upperarm", "lowerarm", "hand"] + ["%s_%02d" % (f, k) for f in
                                               ("thumb", "index", "middle", "ring", "pinky") for k in (1, 2, 3)]


def smoothstep(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


class Ctx:
    def __init__(self, fid, cfg, rig, meshes):
        self.fid, self.cfg, self.rig = fid, cfg, rig
        self.body = meshes["body"]
        self.parts = dict(meshes)          # name -> object
        self.surface = {}                  # object name -> surface key
        for k in meshes:
            self.surface[meshes[k].name] = k  # body/eyes/brows/lashes/hair
        self.L = core.landmarks(rig)
        self.V = geo.co(self.body)
        self.N = geo.vnormals(self.body)
        self.W = geo.weights(self.body)
        self.FV = geo.faces(self.body)
        self.H = float(self.V[:, 2].max() - self.V[:, 2].min())
        self.s = self.H / 1.75             # size relative to an average adult
        self.hide = np.zeros(len(self.FV), bool)
        self.colliders = [self.body]
        nb = len(self.V)
        self.arm_w = np.zeros(nb)
        for side in ("l", "r"):
            for b in ARM_BONES:
                self.arm_w += self.W.get(b + "_" + side, np.zeros(nb))

    def w(self, *names):
        return geo.wsum(self.W, names)

    def head(self, b):
        return self.L[b][0]

    def tail(self, b):
        return self.L[b][1]

    def collider(self, extra=()):
        return geo.Collider(self.colliders + list(extra))

    def add(self, key, ob, surface, collide=True):
        ob.name = ob.data.name = "%s_%s" % (self.fid, key)
        self.parts[key] = ob
        self.surface[ob.name] = surface
        if collide:
            self.colliders.append(ob)
        return ob

    def hide_faces(self, fmask, erode=1, keep=None):
        m = geo.erode_faces(self.FV, fmask, erode) if erode else fmask.copy()
        if keep is not None:
            m &= ~keep
        self.hide |= m

    def ray_front(self, x, z, coll=None):
        """y of the first surface hit when looking from the front (-Y) at (x, z)."""
        coll = coll or self.collider()
        loc, n, i, d = coll.tree.ray_cast(Vector((x, -3.0, z)), Vector((0, 1, 0)))
        return None if loc is None else loc.y


def straighten(ob, axis_a, axis_b, t_target, vmask):
    """Move masked vertices along the bone axis so that their axis parameter is t_target."""
    c = geo.co(ob)
    a, b = np.asarray(axis_a), np.asarray(axis_b)
    t, _, _ = geo.axis_t(c, a, b)
    d = b - a
    c[vmask] += np.outer(t_target - t[vmask], d)
    geo.set_co(ob, c)


def split_off(ob, fmask_fn, name, rig):
    """Move the faces selected by fmask_fn(object) into a new object (same weights/UVs)."""
    new = geo.duplicate(ob, name)
    m = fmask_fn(new)
    geo.keep_faces(new, m)
    geo.delete_faces(ob, fmask_fn(ob))
    geo.bind(new, rig)
    return new


def src_coords(ctx, ob):
    """Rest position on the body of each vertex of a shell (via the src_vi attribute)."""
    return ctx.V[geo.source_index(ob)]


def limb_trim(ob, a, b, t, keep, region=None):
    """Cut perpendicular to the bone axis a->b at parameter t; keep 'proximal' (towards a) or
    'distal' (towards b)."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    n = d / np.linalg.norm(d)
    geo.trim(ob, a + d * t, n if keep == "proximal" else -n, region)


def on_side(side, zmax=None, zmin=None):
    def f(c):
        return ((c[0] > 0) == (side == "l")) and (zmax is None or c[2] < zmax) and (zmin is None or c[2] > zmin)
    return f


def z_cut(ob, z, keep):
    """Horizontal cut; keep 'below' or 'above' z."""
    geo.trim(ob, (0, 0, z), (0, 0, 1) if keep == "below" else (0, 0, -1))


# ============================================================================ Ashka

def ashka(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    torso_w = ctx.w("spine_01", "spine_02", "spine_03", "clavicle_l", "clavicle_r", "pelvis", "neck_01")
    x_sh = abs(ctx.head("upperarm_l")[0])

    # ---------------------------------------------------------------- shorts, hem bands, waistband
    z_top, z_hem = 0.566 * H, 0.392 * H
    legs = {}
    for side in ("l", "r"):
        hip, knee = ctx.head("thigh_" + side), ctx.head("calf_" + side)
        t, _, _ = geo.axis_t(V, hip, knee)
        legs[side] = (hip, knee, t, (hip[2] - z_hem) / (hip[2] - knee[2]))
    thigh_l, thigh_r = ctx.w("thigh_l"), ctx.w("thigh_r")
    left = thigh_l >= thigh_r
    t_v = np.where(left, legs["l"][2], legs["r"][2])
    t_hem_v = np.where(left, legs["l"][3], legs["r"][3])
    thighish = (thigh_l + thigh_r) > 0.15
    base = (ctx.arm_w < 0.01) & (z > 0.25 * H)
    sel = base & (z <= z_top + 0.03 * s) & (~thighish | (t_v <= t_hem_v + 0.08))
    fm = geo.face_mask(ctx.FV, sel)

    def flare(ob, nrm):
        src = src_coords(ctx, ob)
        idx = geo.source_index(ob)
        c = geo.co(ob)
        for side, w in (("l", thigh_l[idx]), ("r", thigh_r[idx])):
            hip, knee, _, t_hem = legs[side]
            t, _, foot = geo.axis_t(src, hip, knee)
            radial = src - foot
            radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
            lateral = radial @ np.array([1.0 if side == "l" else -1.0, 0, 0])
            amount = 0.05 * s * smoothstep((t - 0.10) / (t_hem - 0.10)) * (0.55 + 0.45 * lateral)
            ang = np.arctan2(radial[:, 1], radial[:, 0])
            folds = 0.005 * s * np.sin(7 * ang) * smoothstep((t - 0.2) / (t_hem - 0.2))
            c += radial * ((amount + folds) * np.clip((w - 0.3) / 0.5, 0, 1))[:, None]
        geo.set_co(ob, c)

    shorts = geo.shell(ctx.body, "shorts", fm, 0.011 * s, ctx.rig, smooth_iters=8,
                       gap=0.008 * s, collider=ctx.collider(), extra=flare)
    z_cut(shorts, z_top, "below")
    t_band = {}
    hems = geo.duplicate(shorts, "hems")
    geo.keep_faces(hems, np.array([p.center.z < z_hem + 0.12 * H for p in hems.data.polygons]))
    for side in ("l", "r"):
        hip, knee, _, t_hem = legs[side]
        reg = on_side(side, zmax=hip[2] - 0.03 * s)
        t_band[side] = t_hem - 0.065
        limb_trim(shorts, hip, knee, t_band[side], "proximal", reg)
        limb_trim(hems, hip, knee, t_hem, "proximal", reg)
        limb_trim(hems, hip, knee, t_band[side], "distal", reg)
    geo.offset_along(hems, geo.vnormals(hems), 0.002 * s)
    geo.solidify(shorts, 0.003 * s)
    geo.solidify(hems, 0.004 * s)
    ctx.add("shorts", shorts, "shorts")
    ctx.add("shorts_hems", hems, "trim")
    hide = base & (z <= z_top - 0.01 * s) & (~thighish | (t_v <= 0.12))
    ctx.hide_faces(geo.face_mask(ctx.FV, hide), 1)

    zb0, zb1 = 0.548 * H, 0.598 * H
    bf = geo.face_mask(ctx.FV, base & (z >= zb0 - 0.02 * s) & (z <= zb1 + 0.02 * s))
    band = geo.shell(ctx.body, "waistband", bf, 0.017 * s, ctx.rig, smooth_iters=6,
                     gap=0.012 * s, collider=ctx.collider())
    z_cut(band, zb1, "below")
    z_cut(band, zb0, "above")
    geo.solidify(band, 0.006 * s)
    ctx.add("waistband", band, "trim")
    ctx.hide_faces(geo.face_mask(ctx.FV, base & (z >= zb0 + 0.004) & (z <= zb1 - 0.004)), 1)

    # ---------------------------------------------------------------- vest
    # Cropped sleeveless vest: body panel from the bottom of the ribs to the armpits, a V neck in
    # front, a scooped back, and straps over the shoulders. Every edge is a plane cut, the face
    # selection is only a generous superset, so edges come out straight and clean.
    z_bot = 0.668 * H
    z_vb = 0.700 * H                       # bottom of the V neckline
    nb, nt = ctx.head("neck_01"), ctx.head("head")
    _, r_neck, _ = geo.axis_t(V, nb, nt)
    neckish = (ctx.w("neck_01") > 0.5) & (z > nb[2]) & (z < nt[2])
    R = float(np.median(r_neck[neckish])) if neckish.sum() > 10 else 0.055 * s
    neck_x = R + 0.016 * s                 # strap inner edge
    x_top = neck_x + 0.045 * s             # strap outer edge
    sh_j = ctx.head("upperarm_l")
    z_ap = sh_j[2] - 0.085 * s             # armpit line: top of the body panel
    y_mid = ctx.head("spine_03")[1]
    y_cut = y_mid - 0.02 * s
    front = V[:, 1] < y_cut
    kv = (z_ap - z_vb) / neck_x
    ax = np.abs(V[:, 0])
    sel = ((torso_w >= 0.25) & (z >= z_bot - 0.03 * s) & (ctx.arm_w < 0.6) & (ctx.w("head") < 0.01)
           & ~((r_neck < R - 0.004 * s) & (z > nb[2]))
           & ((ax < x_top + 0.03 * s) | (z < z_ap + 0.03 * s))
           & ~(front & (ax < neck_x - 0.02 * s) & (z > z_vb + 0.05 * s + ax * kv)))
    vf = geo.face_mask(ctx.FV, sel)
    vest = geo.shell(ctx.body, "vest", vf, 0.009 * s, ctx.rig, smooth_iters=16,
                     gap=0.0065 * s, collider=ctx.collider())
    yf, yb = float(V[:, 1].min()), float(V[:, 1].max())
    cols = (ax > neck_x) & (ax < x_top) & (ctx.w("neck_01") < 0.35) & (ctx.w("head") < 0.01)
    z_neck_top = float(z[cols].max()) + 0.004 * s if cols.any() else nb[2]   # top of the trapezius

    def vest_field(c):
        x, y, zz = np.abs(c[:, 0]), c[:, 1], c[:, 2]
        bottom = zz - (z_bot + 0.018 * s * np.clip((y - yf) / (yb - yf), 0, 1))
        strap = np.minimum(np.minimum(x - neck_x, x_top - x), z_neck_top - zz)
        panel = np.maximum(z_ap - zz, strap)
        not_v = np.maximum(np.maximum(z_vb + kv * x - zz, x - neck_x), y - y_cut)
        return np.minimum(np.minimum(bottom, panel), not_v)

    geo.cut_field(vest, vest_field)
    if os.environ.get("VEST_DEBUG"):
        print("VESTDBG", "R", round(R, 3), "neck_x", round(neck_x, 3), "x_top", round(x_top, 3), "z_ap", round(z_ap, 3),
              "kv", round(kv, 3), "faces", len(vest.data.polygons))
    geo.solidify(vest, 0.0035 * s)
    ctx.add("vest", vest, "vest")
    inner = (sel & (z >= z_bot + 0.008) & ~(front & (z > z_vb - 0.012 * s + ax * kv))
             & ((z < z_ap - 0.012 * s) | ((ax > neck_x + 0.01 * s) & (ax < x_top - 0.01 * s))))
    ctx.hide_faces(geo.face_mask(ctx.FV, inner), 1)
    z_lace_top = z_vb

    # gold lacing: four small crosses down the front below the V
    coll = ctx.collider()
    lv, lf = [], []
    for k in range(4):
        zz = z_lace_top - 0.010 * s - k * 0.0145 * s
        for sgn in (1, -1):
            ends = []
            for xx, dz in ((-0.011 * s, -0.006 * s * sgn), (0.011 * s, 0.006 * s * sgn)):
                y = ctx.ray_front(xx, zz + dz, coll)
                ends.append((xx, (y or 0) - 0.0022 * s, zz + dz))
            v, f = geo.tube(ends, 0.0016 * s, 5)
            lf += [tuple(i + len(lv) for i in ff) for ff in f]
            lv += v
    lace = geo.new_mesh_object("lace", lv, lf, ctx.rig)
    geo.set_rigid(lace, "spine_03")
    ctx.add("vest_lace", lace, "lace", collide=False)

    # ---------------------------------------------------------------- hand wraps
    for side in ("l", "r"):
        sfx = "_" + side
        el, wr = ctx.head("lowerarm" + sfx), ctx.head("hand" + sfx)
        tf, _, _ = geo.axis_t(V, el, wr)
        hand = ctx.w("hand" + sfx, "thumb_01" + sfx)
        k1 = ctx.w(*["%s_01%s" % (f, sfx) for f in ("index", "middle", "ring", "pinky")])
        far = ctx.w(*["%s_%s%s" % (f, n, sfx) for f in ("index", "middle", "ring", "pinky") for n in ("02", "03")]
                    + ["thumb_02" + sfx, "thumb_03" + sfx])
        fore = ctx.w("lowerarm" + sfx)
        sel = (((hand + k1) >= 0.5) | ((fore >= 0.2) & (tf >= 0.52))) & (far < 0.04) & (k1 < 0.75)
        sel &= (V[:, 0] > 0) == (side == "l")
        hf = geo.face_mask(ctx.FV, sel)
        wrap = geo.shell(ctx.body, "handwrap" + sfx, hf, 0.0026 * s, ctx.rig, smooth_iters=2,
                         gap=0.002 * s, collider=geo.Collider([ctx.body]))
        limb_trim(wrap, el, wr, 0.60, "distal", on_side(side))
        geo.solidify(wrap, 0.0016 * s)
        ctx.add("handwrap" + sfx, wrap, "wraps")
        ctx.hide_faces(geo.face_mask(ctx.FV, sel & ((tf >= 0.62) | (hand >= 0.5))), 1)

    # ---------------------------------------------------------------- shin and foot wraps
    for side in ("l", "r"):
        sfx = "_" + side
        kn, an = ctx.head("calf" + sfx), ctx.head("foot" + sfx)
        ts, _, _ = geo.axis_t(V, kn, an)
        legw = ctx.w("calf" + sfx, "foot" + sfx, "thigh" + sfx)
        sel = (((ctx.w("calf" + sfx) >= 0.2) & (ts >= 0.28)) | (ctx.w("foot" + sfx) >= 0.3))
        sel &= (ctx.w("ball" + sfx) < 0.4) & (legw >= 0.45)
        sel &= ~((ctx.N[:, 2] < -0.5) & (z < 0.035 * s))
        sel &= (V[:, 0] > 0) == (side == "l")
        sf = geo.face_mask(ctx.FV, sel)
        wrap = geo.shell(ctx.body, "shinwrap" + sfx, sf, 0.003 * s, ctx.rig, smooth_iters=2,
                         gap=0.0025 * s, collider=geo.Collider([ctx.body]))
        limb_trim(wrap, kn, an, 0.36, "distal", on_side(side))
        cw = geo.co(wrap)
        cw[:, 2] = np.maximum(cw[:, 2], 0.0015)
        geo.set_co(wrap, cw)
        geo.solidify(wrap, 0.0016 * s)
        ctx.add("shinwrap" + sfx, wrap, "wraps")
        ctx.hide_faces(geo.face_mask(ctx.FV, sel & ((ts >= 0.38) | (ctx.w("foot" + sfx) >= 0.3))), 1)

    # ---------------------------------------------------------------- prajioud arm band (left, rigid)
    sh, el = ctx.head("upperarm_l"), ctx.head("lowerarm_l")
    tu, _, _ = geo.axis_t(V, sh, el)
    vm = (ctx.w("upperarm_l") >= 0.25) & (V[:, 0] > 0)
    rings = []
    for t, extra in ((0.315, 0.004), (0.335, 0.0062), (0.375, 0.0062), (0.395, 0.004)):
        rings.append(section_ring(ctx, vm, sh + (el - sh) * t, el - sh, 20, extra * s))
    band = loft_rings(rings, "armband", ctx.rig, ctx.body, closed_ends=False)
    geo.solidify(band, 0.003 * s)
    geo.set_rigid(band, "upperarm_l")
    ctx.add("armband", band, "band", collide=False)
    ctx.hide_faces(geo.face_mask(ctx.FV, (tu >= 0.32) & (tu <= 0.39) & vm), 1)

    # ---------------------------------------------------------------- brass lamp charm on a cord
    coll = ctx.collider()
    zc = 0.742 * H
    yc = ctx.ray_front(0.0, zc, coll)
    cz = np.array([0.0, yc - 0.012 * s, zc - 0.018 * s])
    k = s
    brass = [(0.0, 0.0), (0.0085 * k, 0.0), (0.0095 * k, 0.0025 * k), (0.0065 * k, 0.0045 * k), (0.0055 * k, 0.006 * k)]
    glass = [(0.0055 * k, 0.006 * k), (0.0095 * k, 0.012 * k), (0.0098 * k, 0.018 * k), (0.0072 * k, 0.025 * k), (0.0055 * k, 0.027 * k)]
    cap = [(0.0055 * k, 0.027 * k), (0.0082 * k, 0.0285 * k), (0.0045 * k, 0.034 * k), (0.0012 * k, 0.037 * k)]
    lamp_v, lamp_f = [], []
    for prof in (brass, cap):
        v, f = geo.lathe(prof, 12, cz)
        lamp_f += [tuple(i + len(lamp_v) for i in ff) for ff in f]
        lamp_v += v
    ring_v, ring_f = geo.tube([(cz[0] - 0.004 * k * math.cos(a), cz[1], cz[2] + 0.040 * k + 0.004 * k * math.sin(a))
                               for a in np.linspace(0, 2 * math.pi, 9)], 0.0011 * k, 5)
    lamp_f += [tuple(i + len(lamp_v) for i in ff) for ff in ring_f]
    lamp_v += ring_v
    gv, gf = geo.lathe(glass, 12, cz)
    lamp = geo.new_mesh_object("lamp", lamp_v, lamp_f, ctx.rig)
    glob = geo.new_mesh_object("lamp_glass", gv, gf, ctx.rig)
    for ob in (lamp, glob):
        geo.recalc_normals(ob)
        geo.set_rigid(ob, "spine_03")
    ctx.add("charm_brass", lamp, "band", collide=False)
    ctx.add("charm_ember", glob, "ember", collide=False)
    top = cz + np.array([0, 0, 0.044 * k])
    sides = []
    for sgn in (-1, 1):
        pts = [top + np.array([sgn * 0.004 * k, 0, 0.002 * k])]
        for f in np.linspace(0.15, 1.0, 7):
            x = sgn * (0.012 * s + f * (neck_x + 0.004 * s - 0.012 * s))
            zz = top[2] + f * (nb[2] + 0.004 * s - top[2])
            y = ctx.ray_front(x, zz, coll)
            if y is not None:
                pts.append(np.array([x, y - 0.0025 * s, zz]))
        sides.append(pts)
    ring = []
    for a in np.linspace(0.0, math.pi, 11)[1:-1]:
        d = Vector((math.cos(a), math.sin(a), 0.0))
        o = Vector((nb[0], nb[1], nb[2] + 0.006 * s))
        loc, n, _, _ = coll.tree.ray_cast(o + d * 0.4, -d)
        if loc is not None:
            ring.append(np.array(loc + n * 0.0025 * s))
    cv, cf = geo.tube(sides[1] + ring + sides[0][::-1], 0.0012 * s, 5)
    cord = geo.new_mesh_object("cord", cv, cf, ctx.rig)
    geo.transfer_weights(ctx.body, cord)
    ctx.add("charm_cord", cord, "cord", collide=False)

    # ---------------------------------------------------------------- ponytail rides on top of the vest
    if "hair" in ctx.parts:
        hair = ctx.parts["hair"]
        ch = geo.co(hair)
        geo.push_out(hair, geo.Collider([ctx.body]), 0.016 * s, vmask=ch[:, 2] < nb[2] - 0.01 * s)


# ============================================================================ Gorm

def gorm(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    base = ctx.arm_w < 0.01

    # ---------------------------------------------------------------- canvas trousers, rolled cuffs
    z_top, z_cuff = 0.578 * H, 0.203 * H   # cuff 2.5% H below the boot top: the roll sits over it
    legs = {}
    for side in ("l", "r"):
        kn, an, hip = ctx.head("calf_" + side), ctx.head("foot_" + side), ctx.head("thigh_" + side)
        ts, _, _ = geo.axis_t(V, kn, an)
        legs[side] = (kn, an, ts, (kn[2] - z_cuff) / (kn[2] - an[2]), hip)
    calf_w = ctx.w("calf_l", "calf_r")
    foot_w = ctx.w("foot_l", "foot_r", "ball_l", "ball_r")
    ts_v = np.where(V[:, 0] > 0, legs["l"][2], legs["r"][2])
    tc_v = np.where(V[:, 0] > 0, legs["l"][3], legs["r"][3])
    sel = base & (z <= z_top + 0.03 * s) & (foot_w < 0.3) & ((calf_w < 0.2) | (ts_v <= tc_v + 0.08))
    tf = geo.face_mask(ctx.FV, sel)
    x_hip = abs(ctx.head("thigh_l")[0])

    def bag(c, nrm):
        medial = np.clip(np.abs(c[:, 0]) / (x_hip * 1.25), 0.25, 1.0)
        return 0.021 * s * (0.35 + 0.65 * medial)

    def folds(ob, nrm):
        src = src_coords(ctx, ob)
        c = geo.co(ob)
        for side in ("l", "r"):
            kn, an, _, t_cuff, hip = legs[side]
            m = (src[:, 0] > 0) == (side == "l")
            t, _, foot = geo.axis_t(src, hip, an)
            radial = src - foot
            radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
            ang = np.arctan2(radial[:, 1], radial[:, 0])
            amp = 0.006 * s * np.sin(9 * ang + 3.0 * t) * smoothstep((t - 0.15) / 0.3)
            tk, _, _ = geo.axis_t(src, kn, an)
            u = (tk - (t_cuff - 0.12)) / 0.12
            roll = np.where((u > 0) & (u <= 1.05), 0.024 * s * np.sin(np.pi * np.clip(u, 0, 1) * 0.9 + 0.15) ** 0.6, 0.0)
            c[m] += radial[m] * (amp[m] + roll[m])[:, None]
        geo.set_co(ob, c)

    trousers = geo.shell(ctx.body, "trousers", tf, bag, ctx.rig, smooth_iters=8, gap=0.010 * s,
                         collider=ctx.collider(), extra=folds)
    z_cut(trousers, z_top, "below")
    for side in ("l", "r"):
        kn, an, _, t_cuff, _ = legs[side]
        limb_trim(trousers, kn, an, t_cuff, "proximal", on_side(side, zmax=kn[2]))
    geo.solidify(trousers, 0.005 * s)
    ctx.add("trousers", trousers, "trousers")
    ctx.hide_faces(geo.face_mask(ctx.FV, base & (z <= z_top - 0.01 * s) & (foot_w < 0.3)
                                 & ((calf_w < 0.2) | (ts_v <= tc_v - 0.02))), 1)

    # ---------------------------------------------------------------- heavy boots + soles
    z_boot = 0.215 * H
    sole = ctx.cfg.get("sole_m", 0.03)
    for side in ("l", "r"):
        sfx = "_" + side
        boot, so = loft_boot(ctx, side, z_boot, sole)
        ctx.add("boot" + sfx, boot, "boots")
        ctx.add("sole" + sfx, so, "sole", collide=False)
        lw = ctx.w("calf" + sfx, "foot" + sfx, "ball" + sfx)
        hide = (z <= z_boot - 0.012 * s) & (lw >= 0.5) & ((V[:, 0] > 0) == (side == "l"))
        ctx.hide_faces(geo.face_mask(ctx.FV, hide), 1)

    # ---------------------------------------------------------------- apron (front only, to the knees)
    apron, grid, rows = build_apron(ctx)
    ctx.add("apron", apron, "apron")

    # ---------------------------------------------------------------- belt over apron and trousers
    zb0, zb1 = 0.553 * H, 0.580 * H
    blf = geo.face_mask(ctx.FV, base & (z >= zb0 - 0.02 * s) & (z <= zb1 + 0.02 * s))
    belt = geo.shell(ctx.body, "belt", blf, 0.03 * s, ctx.rig, smooth_iters=6, gap=0.006 * s,
                     collider=ctx.collider())
    z_cut(belt, zb1, "below")
    z_cut(belt, zb0, "above")
    acoll = geo.Collider([apron])
    cb = geo.co(belt)
    for i, p in enumerate(cb):
        if p[1] < ctx.head("pelvis")[1]:
            loc, _, _, _ = acoll.tree.ray_cast(Vector((p[0], -3.0, p[2])), Vector((0, 1, 0)))
            if loc is not None and loc.y - 0.006 * s < p[1]:
                cb[i, 1] = loc.y - 0.006 * s
    geo.set_co(belt, cb)
    geo.smooth(belt, 3, 0.4)
    geo.solidify(belt, 0.007 * s)
    ctx.add("belt", belt, "strap")
    zc = 0.5 * (zb0 + zb1)
    yb = ctx.ray_front(0.0, zc, ctx.collider())
    bx, by, bz = 0.026 * s, 0.006 * s, 0.019 * s
    cc = np.array([0.0, yb - by, zc])
    bv_ = [tuple(cc + np.array([sx * bx, sy * by, sz * bz])) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    bf_ = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    buckle = geo.new_mesh_object("buckle", bv_, bf_, ctx.rig)
    geo.recalc_normals(buckle)
    geo.set_rigid(buckle, "pelvis")
    ctx.add("buckle", buckle, "iron", collide=False)

    # ---------------------------------------------------------------- neck strap from the bib corners
    coll = ctx.collider()
    nb, nt = ctx.head("neck_01"), ctx.head("head")
    _, r_neck, _ = geo.axis_t(V, nb, nt)
    nk = (ctx.w("neck_01") > 0.5) & (z > nb[2]) & (z < nt[2])
    R = float(np.median(r_neck[nk])) if nk.sum() > 10 else 0.07 * s
    top_row = grid[0]
    z1 = nb[2] + 0.015 * s
    halves = []
    for sgn in (-1, 1):
        corner = top_row[0] if sgn < 0 else top_row[-1]
        x0, z0 = corner[0] - sgn * 0.014 * s, corner[2] - 0.006 * s
        x1 = sgn * (R + 0.024 * s)
        pts = []
        for f in np.linspace(0.0, 1.0, 10):
            x, zz = x0 + (x1 - x0) * f, z0 + (z1 - z0) * f
            y = ctx.ray_front(x, zz, coll)
            if y is not None:
                pts.append(np.array([x, y - 0.004 * s, zz]))
        halves.append(pts)
    ring = []
    for a in np.linspace(0.0, math.pi, 13)[1:-1]:
        d = Vector((math.cos(a), math.sin(a), 0.0))
        o = Vector((nb[0], nb[1], z1))
        loc, n, _, _ = coll.tree.ray_cast(o + d * 0.5, -d)
        if loc is not None:
            ring.append(np.array(loc + n * 0.004 * s))
    halves = [halves[0], halves[1]]
    sv, sf = ribbon(halves[1] + ring + halves[0][::-1], 0.026 * s, coll)
    strap = geo.new_mesh_object("neckstrap", sv, sf, ctx.rig)
    geo.transfer_weights(ctx.body, strap)
    geo.solidify(strap, 0.005 * s, 1.0)
    ctx.add("neckstrap", strap, "strap", collide=False)
    rv, rf = [], []
    for sgn in (-1, 1):
        corner = top_row[0] if sgn < 0 else top_row[-1]
        for dz in (0.0, -0.024 * s):
            cpos = np.array([corner[0] - sgn * 0.016 * s, corner[1] - 0.012 * s, corner[2] - 0.012 * s + dz])
            v, f = geo.lathe([(0.0, 0.0), (0.0068 * s, 0.0), (0.0052 * s, 0.003 * s), (0.0, 0.0048 * s)], 8, (0, 0, 0))
            v = [(cpos[0] + a, cpos[1] - c_, cpos[2] + b) for a, b, c_ in v]
            rf += [tuple(i + len(rv) for i in ff) for ff in f]
            rv += v
    rivets = geo.new_mesh_object("rivets", rv, rf, ctx.rig)
    geo.recalc_normals(rivets)
    geo.set_rigid(rivets, "spine_03")
    ctx.add("rivets", rivets, "iron", collide=False)

    # ---------------------------------------------------------------- iron vambraces (rigid)
    for side in ("l", "r"):
        sfx = "_" + side
        el, wr = ctx.head("lowerarm" + sfx), ctx.head("hand" + sfx)
        tfa, _, _ = geo.axis_t(V, el, wr)
        sel = (tfa >= 0.22) & (tfa <= 0.95) & (ctx.w("lowerarm" + sfx, "hand" + sfx, "upperarm" + sfx) >= 0.5)
        sel &= (V[:, 0] > 0) == (side == "l")
        vf = geo.face_mask(ctx.FV, sel)

        def ridges(ob, nrm, el=el, wr=wr):
            t, _, _ = geo.axis_t(src_coords(ctx, ob), el, wr)
            bump = np.zeros(len(t))
            for t0 in (0.30, 0.83):
                bump += 0.0055 * s * smoothstep((t - t0) / 0.012) * smoothstep((t0 + 0.055 - t) / 0.012)
            geo.offset_along(ob, nrm, bump)

        vb = geo.shell(ctx.body, "vambrace" + sfx, vf, 0.012 * s, ctx.rig, smooth_iters=5,
                       gap=0.009 * s, collider=geo.Collider([ctx.body]), extra=ridges)
        limb_trim(vb, el, wr, 0.28, "distal")
        limb_trim(vb, el, wr, 0.90, "proximal")
        geo.solidify(vb, 0.004 * s)
        geo.set_rigid(vb, "lowerarm" + sfx)
        ctx.add("vambrace" + sfx, vb, "iron", collide=False)
        ctx.hide_faces(geo.face_mask(ctx.FV, sel & (tfa >= 0.30) & (tfa <= 0.88)), 1)


def section_ring(ctx, vmask, point, normal, n, offset):
    """Convex cross-section of the body (masked verts' edges) by a plane, resampled to n points
    at even angles and pushed out by `offset`; returns (n, 3) points."""
    V = ctx.V
    point, normal = np.asarray(point, float), np.asarray(normal, float)
    normal = normal / np.linalg.norm(normal)
    a = np.cross(normal, [0, 0, 1.0])
    if np.linalg.norm(a) < 1e-3:
        a = np.cross(normal, [1.0, 0, 0])
    a /= np.linalg.norm(a)
    b = np.cross(normal, a)
    E = np.array([e.vertices[:] for e in ctx.body.data.edges])
    E = E[vmask[E[:, 0]] & vmask[E[:, 1]]]
    d0, d1 = (V[E[:, 0]] - point) @ normal, (V[E[:, 1]] - point) @ normal
    cr = d0 * d1 <= 0
    f = d0[cr] / np.where(np.abs(d0[cr] - d1[cr]) < 1e-12, 1e-12, d0[cr] - d1[cr])
    P = V[E[cr, 0]] + (V[E[cr, 1]] - V[E[cr, 0]]) * f[:, None]
    uv = np.stack([(P - point) @ a, (P - point) @ b], 1)
    ring, c = geo.ring_by_angle(convex_hull(uv), n)
    v = ring - c
    ring = c + v * (1 + offset / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))
    return point + np.outer(ring[:, 0], a) + np.outer(ring[:, 1], b)


def loft_rings(rings, name, rig, like, closed_ends=False):
    n = len(rings[0])
    verts = [tuple(p) for r in rings for p in r]
    fcs = []
    for i in range(len(rings) - 1):
        for k in range(n):
            a, b = i * n + k, i * n + (k + 1) % n
            fcs.append((a, b, b + n, a + n))
    ob = geo.new_mesh_object(name, verts, fcs, None, like)
    geo.recalc_normals(ob)
    uv = ob.data.uv_layers.new(name="UVMap")
    c = geo.co(ob)
    per = float(np.mean([np.linalg.norm(np.roll(r, -1, 0) - r, axis=1).sum() for r in rings]))
    for li, loop in enumerate(ob.data.loops):
        vi = loop.vertex_index
        uv.data[li].uv = ((vi % n) / n * per, (vi // n) * 0.01)
    geo.bind(ob, rig)
    return ob


def loft_boot(ctx, side, z_top, sole, n=28):
    """Heavy boot lofted through convex-hull slices of the foot and shin (no toes, a rounded
    toe box), weights from the body (nearest face interpolated), plus a sole slab under it."""
    s, V = ctx.s, ctx.V
    sfx = "_" + side
    lw = ctx.w("calf" + sfx, "foot" + sfx, "ball" + sfx)
    vm = (lw >= 0.3) & ((V[:, 0] > 0) == (side == "l")) & (V[:, 2] <= z_top + 0.03 * s)
    P = V[vm]
    E = np.array([e.vertices[:] for e in ctx.body.data.edges])
    E = E[vm[E[:, 0]] | vm[E[:, 1]]]
    za, zb_ = V[E[:, 0], 2], V[E[:, 1], 2]
    nz = 24
    zs = [0.004 + (z_top - 0.004) * (i / (nz - 1)) ** 1.35 for i in range(nz)]
    y_toe = P[:, 1].min()
    foot_len = P[:, 1].max() - y_toe
    rings, prev = [], None
    for zz in zs:
        # exact cross-section of the leg surface at this height (edge / plane crossings)
        cr = (za - zz) * (zb_ - zz) <= 0
        f = (zz - za[cr]) / np.where(np.abs(zb_[cr] - za[cr]) < 1e-12, 1e-12, zb_[cr] - za[cr])
        pts = (V[E[cr, 0]] + (V[E[cr, 1]] - V[E[cr, 0]]) * f[:, None])[:, :2]
        if len(pts) < 4:
            ring = prev
        else:
            hull = convex_hull(pts)
            ring, c = geo.ring_by_angle(hull, n)
            off = (0.013 * s - 0.005 * s * smoothstep((zz - 0.6 * z_top) / (0.4 * z_top))
                   + 0.007 * s * smoothstep(1.0 - (ring[:, 1] - y_toe) / (0.35 * foot_len)) * (zz < 0.09 * s))
            v = ring - c
            ring = c + v * (1 + off[:, None] / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))
        rings.append((ring, zz))
        if os.environ.get("BOOT_DEBUG"):
            rr = np.linalg.norm(ring - ring.mean(0), axis=1)
            print("BOOTDBG", side, round(zz, 3), len(pts), round(float(rr.min()), 3), round(float(rr.max()), 3))
        prev = ring
    verts = [(p[0], p[1], zz) for ring, zz in rings for p in ring]
    fcs = []
    for i in range(nz - 1):
        for k in range(n):
            a, b = i * n + k, i * n + (k + 1) % n
            fcs.append((a, b, b + n, a + n))
    fcs.append(tuple(range(n))[::-1])
    boot = geo.new_mesh_object("boot" + sfx, verts, fcs, None, ctx.body)
    geo.triangulate_ngons(boot)
    geo.recalc_normals(boot)
    uv = boot.data.uv_layers.new(name="UVMap")
    cb = geo.co(boot)
    per = float(np.mean([np.linalg.norm(np.roll(r, -1, 0) - r, axis=1).sum() for r, _ in rings]))
    for li, loop in enumerate(boot.data.loops):
        vi = loop.vertex_index
        k = vi % n
        uv.data[li].uv = (k / n * per, cb[vi, 2])
    geo.transfer_weights(ctx.body, boot)
    geo.solidify(boot, 0.005 * s)
    geo.bind(boot, ctx.rig)
    # sole slab from the lowest ring
    base = rings[0][0]
    c = base.mean(0)
    v = base - c
    wide = c + v * (1 + (0.005 * s) / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))
    vs = [(p[0], p[1], 0.010 * s) for p in wide] + [(p[0], p[1], -sole) for p in wide]
    fs = [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)]
    fs += [tuple(range(n))[::-1], tuple(range(n, 2 * n))]
    so = geo.new_mesh_object("sole" + sfx, vs, fs, ctx.rig)
    geo.triangulate_ngons(so)
    geo.recalc_normals(so)
    ft, bl = ctx.head("foot" + sfx), ctx.head("ball" + sfx)
    tt, _, _ = geo.axis_t(geo.co(so), ft, bl)
    wb = smoothstep((tt - 0.6) / 0.6)
    geo.set_weights(so, {"foot" + sfx: 1 - wb, "ball" + sfx: wb})
    return boot, so


def build_apron(ctx):
    """Front-only rubberised apron: a sheet that lies on the chest and belly (hanging straight
    down from the most forward point above, like stiff leather), widening to the knees, with a
    centre slit from the hem to mid-thigh so each half can follow its own thigh in kicks.
    Weights: bib from the body (nearest face interpolated); below the belt a blend from the body
    weights to 100% of the thigh on that side by mid-thigh."""
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    no_arms = geo.face_mask(ctx.FV, ctx.arm_w < 0.05, "all")
    coll = geo.Collider(ctx.colliders, [no_arms] + [None] * (len(ctx.colliders) - 1))
    z_bib, z_w = 0.765 * H, 0.575 * H
    z_hem = ctx.head("calf_l")[2] - 0.025 * s
    z_slit = 0.5 * (ctx.head("thigh_l")[2] + ctx.head("calf_l")[2]) + 0.04 * s
    xs_w = np.abs(V[(np.abs(z - z_w) < 0.02 * s) & (V[:, 1] < 0) & (ctx.arm_w < 0.01), 0])
    half_w = 0.80 * float(np.percentile(xs_w, 99))
    xs_h = np.abs(V[(np.abs(z - (z_hem + 0.15 * s)) < 0.03 * s) & (V[:, 1] < 0) & (ctx.arm_w < 0.01), 0])
    half_h = 0.92 * float(np.percentile(xs_h, 99))
    half_t = 0.13 * s
    nu, nv_bib, nv_sk = 16, 12, 18
    zs = list(np.linspace(z_bib, z_w, nv_bib + 1)) + list(np.linspace(z_w, z_hem, nv_sk + 1))[1:]
    rows = []
    for zz in zs:
        if zz >= z_w:
            hw = half_t + (half_w - half_t) * ((z_bib - zz) / (z_bib - z_w)) ** 0.8
        else:
            hw = half_w + (half_h - half_w) * (z_w - zz) / (z_w - z_hem)
        rows.append((zz, hw))
    us = np.linspace(-1, 1, nu + 1)
    grid = np.zeros((len(rows), nu + 1, 3))
    for i, (zz, hw) in enumerate(rows):
        for j, u in enumerate(us):
            y = ctx.ray_front(u * hw, zz, coll)
            grid[i, j] = (u * hw, np.nan if y is None else y, zz)
    for i in range(len(rows)):
        row = grid[i, :, 1]
        good = ~np.isnan(row)
        if not good.any():
            row[:] = grid[i - 1, :, 1]
        elif not good.all():
            row[~good] = np.interp(np.nonzero(~good)[0], np.nonzero(good)[0], row[good])
    yf = grid[:, :, 1].copy()
    env = yf.copy()
    iw = nv_bib                              # row index of the belt line
    for i in range(iw):                      # bib lies on the chest, bridging small hollows
        env[i] = yf[max(0, i - 2):min(iw, i + 3)].min(0)
    hang = env[iw].copy()
    z_slit = 0.5 * (ctx.head("thigh_l")[2] + ctx.head("calf_l")[2]) + 0.04 * s
    for i in range(iw + 1, len(rows)):       # below the belt it hangs from the most forward point,
        hang = np.minimum(hang, yf[i])       # then swings back towards the thighs: the halves are
        zz = rows[i][0]                      # rigid on the thighs in kicks, so a big gap would
        pull = 0.8 * smoothstep((z_slit + 0.1 * s - zz) / (z_slit + 0.1 * s - z_hem))  # float
        env[i] = hang + (yf[i] - hang) * pull
    for _ in range(3):
        env[:, 1:-1] = 0.5 * env[:, 1:-1] + 0.25 * (env[:, :-2] + env[:, 2:])
    gap = 0.017 * s
    for i, (zz, hw) in enumerate(rows):
        below = max(0.0, (z_w - zz) / (z_w - z_hem))
        for j, u in enumerate(us):
            y = env[i, j] - gap - 0.010 * s * below ** 1.5
            y += 0.022 * s * u ** 4 * (1 - 0.6 * below)
            y += 0.007 * s * math.sin(3.5 * math.pi * u) * below ** 1.3
            grid[i, j, 1] = y
    verts, fcs, vid = [], [], {}
    c_j = nu // 2

    def vindex(i, j, part):
        key = (i, j, part if (j == c_j and rows[i][0] < z_slit) else 0)
        if key not in vid:
            p = grid[i, j].copy()
            p[0] += key[2] * 0.004 * s
            vid[key] = len(verts)
            verts.append(tuple(p))
        return vid[key]

    for i in range(len(rows) - 1):
        for j in range(nu):
            part = -1 if j < c_j else 1
            fcs.append((vindex(i, j, part), vindex(i + 1, j, part), vindex(i + 1, j + 1, part), vindex(i, j + 1, part)))
    apron = geo.new_mesh_object("apron", verts, fcs, None, ctx.body)
    apron_uv(apron, rows)
    geo.transfer_weights(ctx.body, apron)
    ca = geo.co(apron)
    Wa = geo.weights(apron)
    z_full = z_slit + 0.06 * s
    sb = smoothstep((z_w - ca[:, 2]) / (z_w - z_full))
    centre = np.clip(ca[:, 0] / (0.25 * half_h) * 0.5 + 0.5, 0, 1)
    wl = np.where(ca[:, 2] >= z_slit, centre, (ca[:, 0] > 0).astype(float))
    newW = {b: w * (1 - sb) for b, w in Wa.items()}
    zero = np.zeros(len(ca))
    newW["thigh_l"] = newW.get("thigh_l", zero) + sb * wl
    newW["thigh_r"] = newW.get("thigh_r", zero) + sb * (1 - wl)
    geo.set_weights(apron, newW)
    geo.solidify(apron, 0.011 * s, -1.0)
    geo.bind(apron, ctx.rig)
    return apron, grid, rows


# ============================================================================ helpers

def apron_uv(ob, rows):
    """Planar UVs in metres (x across the apron, z down it)."""
    me = ob.data
    uv = me.uv_layers.new(name="UVMap")
    c = geo.co(ob)
    for li, loop in enumerate(me.loops):
        p = c[loop.vertex_index]
        uv.data[li].uv = (p[0] + 1.0, p[2] - rows[0][0] + 2.0)


def ribbon(path, width, coll):
    """Flat strap along a polyline lying on a surface (verts, quads)."""
    pts = [np.asarray(p, float) for p in path]
    verts, fcs = [], []
    for i, p in enumerate(pts):
        t = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        t /= np.linalg.norm(t) + 1e-12
        _, n, _ = coll.nearest(p)
        n = np.array(n)
        side = np.cross(t, n)
        side /= np.linalg.norm(side) + 1e-12
        verts.append(tuple(p - side * width / 2))
        verts.append(tuple(p + side * width / 2))
    for i in range(len(pts) - 1):
        fcs.append((2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2))
    return verts, fcs


def convex_hull(pts):
    pts = sorted(map(tuple, pts))
    if len(pts) < 3:
        return np.array(pts)

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return np.array(lower[:-1] + upper[:-1])


def inflate_polygon(poly, d):
    c = poly.mean(0)
    v = poly - c
    return c + v * (1 + d / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))


def resample_polygon(poly, n):
    seg = np.linalg.norm(np.roll(poly, -1, 0) - poly, axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)])
    ts = np.linspace(0, cum[-1], n, endpoint=False)
    out = []
    for t in ts:
        k = np.searchsorted(cum, t, side="right") - 1
        f = (t - cum[k]) / max(seg[k], 1e-9)
        out.append(poly[k] + (poly[(k + 1) % len(poly)] - poly[k]) * f)
    return np.array(out)


# ============================================================================ entry points

BUILDERS = {"ashka": ashka, "gorm": gorm}


def build(fid, cfg, rig, meshes):
    ctx = Ctx(fid, cfg, rig, meshes)
    skull_top = float(ctx.V[:, 2].max())        # before any head faces are hidden under hoods/helmets
    BUILDERS[cfg["outfit"]](ctx)
    geo.delete_faces(ctx.body, ctx.hide)
    for key, ob in ctx.parts.items():
        if ob.type != "MESH":
            continue
        geo.move_armature_last(ob)
        geo.clean_weights(ob, limit=8)
        for p in ob.data.polygons:
            p.use_smooth = True
        if "src_vi" in ob.data.attributes:
            ob.data.attributes.remove(ob.data.attributes["src_vi"])
    return {"ctx": ctx, "objects": ctx.parts, "surface": ctx.surface, "skull_top": skull_top}


def finalize(fid, cfg, rig, built):
    """Lift everything except the root bone by the sole thickness so boots stand on z=0."""
    sole = cfg.get("sole_m", 0.0)
    if sole <= 0:
        return
    with bpy.context.temp_override(active_object=rig, object=rig, selected_objects=[rig]):
        bpy.context.view_layer.objects.active = rig
        bpy.ops.object.mode_set(mode="EDIT")
        for eb in rig.data.edit_bones:
            if eb.name != "root":
                eb.head.z += sole
                eb.tail.z += sole
        bpy.ops.object.mode_set(mode="OBJECT")
    for ob in built["objects"].values():
        c = geo.co(ob)
        c[:, 2] += sole
        geo.set_co(ob, c)


def export(fid, cfg, rig, built, path):
    objs = [o for o in built["objects"].values() if o.type == "MESH"]
    stats = {}
    for ob in objs:
        me = ob.data
        nt = sum(len(p.vertices) - 2 for p in me.polygons)
        gidx = {vg.index for vg in ob.vertex_groups}
        infl = [sum(1 for g in v.groups if g.group in gidx and g.weight > 0) for v in me.vertices]
        stats[ob.name] = {"vertices": len(me.vertices), "faces": len(me.polygons), "triangles": nt,
                          "max_influences": max(infl) if infl else 0,
                          "unweighted_vertices": sum(1 for i in infl if i == 0),
                          "material": me.materials[0].name if me.materials else None,
                          "surface": built["surface"].get(ob.name),
                          "bones": sorted({vg.name for vg in ob.vertex_groups})[:60]}
    zs = np.concatenate([geo.co(o)[:, 2] for o in objs])
    head_top = built["skull_top"] + cfg.get("sole_m", 0.0)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    rig.scale = (100.0, 100.0, 100.0)
    bpy.ops.object.select_all(action="DESELECT")
    for ob in [rig] + objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=False, object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
        axis_forward="Y", axis_up="Z", bake_space_transform=False,
        use_mesh_modifiers=True, mesh_smooth_type="FACE", use_tspace=True, use_triangles=False,
        use_custom_props=False, add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
        use_armature_deform_only=True, armature_nodetype="NULL", bake_anim=False,
        path_mode="STRIP", embed_textures=False, colors_type="NONE")
    tris = sum(v["triangles"] for v in stats.values())
    return {"parts": stats, "triangles_total": tris, "top_of_everything_cm": round((float(zs.max()) - float(zs.min())) * 100, 2),
            "standing_height_cm": round((head_top - float(zs.min())) * 100, 2),
            "lowest_point_cm": round(float(zs.min()) * 100, 3),
            "bones": len(rig.data.bones), "root_bone": [b.name for b in rig.data.bones if b.parent is None]}
