"""Outfits for Nix, Mirela, Bram, Hollis, the Slopbat and Daxion (see fighter_outfits for Ashka
and Gorm, fighter_wardrobe for the garment library). Registered into fighter_outfits.BUILDERS."""
import math

import bpy
import numpy as np
from mathutils import Vector

import fighter_geo as geo
import fighter_outfits as fo
import fighter_wardrobe as wd
from fighter_outfits import smoothstep, src_coords, loft_boot, convex_hull, resample_polygon

FINGERS = ("thumb", "index", "middle", "ring", "pinky")


# ----------------------------------------------------------------------------- shared pieces

def hand_w(ctx, side, fingers=True):
    names = ["hand_" + side] + (["%s_%02d_%s" % (f, k, side) for f in FINGERS for k in (1, 2, 3)] if fingers else [])
    return ctx.w(*names)


def side_mask(ctx, side):
    return (ctx.V[:, 0] > 0) == (side == "l")


def top_with_sleeves(ctx, key, surface, z_bottom, sleeve_t, offset, flare=0.0, flare_from=0.6,
                     neck_top=None, vneck=None, smooth=10, gap=0.007, thickness=0.004, folds=0.0):
    """Torso + sleeves as one shell: from z_bottom up to the neck base (or a high collar at
    neck_top), sleeves cut across the forearm at sleeve_t, optional bell flare and V neck."""
    s, V = ctx.s, ctx.V
    z = V[:, 2]
    nb, nt = ctx.head("neck_01"), ctx.head("head")
    head_w = ctx.w("head")
    hands = hand_w(ctx, "l") + hand_w(ctx, "r")
    z_neck = neck_top if neck_top is not None else nb[2] + 0.012 * s
    sel = (z >= z_bottom - 0.03 * s) & (head_w < 0.45) & (hands < 0.25) & (z <= z_neck + 0.03 * s)
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        t, _, _ = geo.axis_t(V, el, wr)
        sel &= ~(side_mask(ctx, side) & (ctx.w("lowerarm_" + side) > 0.2) & (t > sleeve_t + 0.08))
    legs = ctx.w("thigh_l", "thigh_r", "calf_l", "calf_r")
    sel &= ~((legs > 0.3) & (z < z_bottom))
    fields = [wd.f_plane((0, 0, z_bottom), (0, 0, 1)), wd.f_plane((0, 0, z_neck), (0, 0, -1))]
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        fields.append(wd.f_limb(el, wr, sleeve_t, "proximal", side, radius=0.16 * s))
    if vneck is not None:
        z_vb, k = vneck
        y_cut = ctx.head("spine_03")[1] - 0.02 * s
        fields.append(lambda c, z_vb=z_vb, k=k, y_cut=y_cut: np.where(c[:, 1] < y_cut, z_vb + k * np.abs(c[:, 0]) - c[:, 2], 1.0))

    def shape(ob, nrm):
        if not flare and not folds:
            return
        src = src_coords(ctx, ob)
        c = geo.co(ob)
        for side in ("l", "r"):
            el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
            t, r, foot = geo.axis_t(src, el, wr)
            m = side_mask_pts(src, side) & (r < 0.16 * s)
            radial = src - foot
            radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
            amt = flare * s * smoothstep((t - flare_from) / max(sleeve_t - flare_from, 1e-3))
            if folds:
                ang = np.arctan2(radial[:, 1], radial[:, 0])
                amt = amt + folds * s * np.sin(5 * ang + 9 * t) * smoothstep(t / 0.3)
            c[m] += radial[m] * amt[m, None]
        geo.set_co(ob, c)

    return wd.field_shell(ctx, key, surface, sel, offset * s, wd.f_and(*fields), smooth=smooth, gap=gap,
                          thickness=thickness, extra=shape)


def side_mask_pts(c, side):
    return (c[:, 0] > 0) == (side == "l")


def leg_shell(ctx, key, surface, z_top, z_bottom, offset, folds=0.004, gather=None, smooth=8, gap=0.008,
              thickness=0.004, medial=0.4):
    """Trousers / leggings from z_top down to z_bottom (cut across the shins)."""
    s, V = ctx.s, ctx.V
    z = V[:, 2]
    feet = ctx.w("foot_l", "foot_r", "ball_l", "ball_r")
    sel = (z <= z_top + 0.03 * s) & (ctx.arm_w < 0.01) & (feet < 0.3) & (z >= z_bottom - 0.06 * s)
    x_hip = abs(ctx.head("thigh_l")[0])

    def bag(c, nrm):
        med = np.clip(np.abs(c[:, 0]) / (x_hip * 1.25), medial, 1.0)
        return offset * s * (0.35 + 0.65 * med)

    def shape(ob, nrm):
        src = src_coords(ctx, ob)
        c = geo.co(ob)
        for side in ("l", "r"):
            hip, an = ctx.head("thigh_" + side), ctx.head("foot_" + side)
            m = side_mask_pts(src, side)
            t, _, foot = geo.axis_t(src, hip, an)
            radial = src - foot
            radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
            ang = np.arctan2(radial[:, 1], radial[:, 0])
            amt = folds * s * np.sin(9 * ang + 3.0 * t) * smoothstep((t - 0.15) / 0.3)
            if gather:
                amt = amt - gather * s * smoothstep((t - 0.75) / 0.2)
            c[m] += radial[m] * amt[m, None]
        geo.set_co(ob, c)

    field = wd.f_and(wd.f_plane((0, 0, z_top), (0, 0, -1)), wd.f_plane((0, 0, z_bottom), (0, 0, 1)))
    return wd.field_shell(ctx, key, surface, sel, bag, field, smooth=smooth, gap=gap, thickness=thickness, extra=shape)


def gloves(ctx, key_fmt, surface, cuff_t, offset, fingers=True, thickness=0.0016, decimate=1.0):
    for side in ("l", "r"):
        s, V = ctx.s, ctx.V
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        tf, _, _ = geo.axis_t(V, el, wr)
        hw = hand_w(ctx, side, fingers)
        cuff = (ctx.w("lowerarm_" + side) >= 0.2) & (tf >= cuff_t - 0.08)
        if fingers:
            sel = (hw >= 0.4) | cuff
        else:   # wraps over palm, back of the hand and knuckles; fingers free (as Ashka's)
            k1 = ctx.w(*["%s_01_%s" % (f, side) for f in ("index", "middle", "ring", "pinky")])
            far = ctx.w(*["%s_%s_%s" % (f, n, side) for f in ("index", "middle", "ring", "pinky") for n in ("02", "03")]
                        + ["thumb_02_" + side, "thumb_03_" + side])
            sel = (((ctx.w("hand_" + side, "thumb_01_" + side) + k1) >= 0.5) | cuff) & (far < 0.04) & (k1 < 0.75)
        sel &= side_mask(ctx, side)
        wd.field_shell(ctx, key_fmt % side, surface, sel, offset * s,
                       wd.f_limb(el, wr, cuff_t, "distal", side, radius=0.2 * s), smooth=2, gap=0.6 * offset,
                       thickness=thickness, collider=geo.Collider([ctx.body]), decimate=decimate)


def boots(ctx, surface, sole_surface, z_top_frac, sole):
    for side in ("l", "r"):
        boot, so = loft_boot(ctx, side, z_top_frac * ctx.H, sole)
        ctx.add("boot_" + side, boot, surface)
        ctx.add("sole_" + side, so, sole_surface, collide=False)
        lw = ctx.w("calf_" + side, "foot_" + side, "ball_" + side)
        ctx.hide_faces(geo.face_mask(ctx.FV, (ctx.V[:, 2] <= z_top_frac * ctx.H - 0.012 * ctx.s) & (lw >= 0.5)
                                     & side_mask(ctx, side)), 1)


def band(ctx, key, surface, z0, z1, offset, gap=0.006, thickness=0.006):
    V = ctx.V
    sel = (ctx.arm_w < 0.01) & (V[:, 2] >= z0 - 0.02 * ctx.s) & (V[:, 2] <= z1 + 0.02 * ctx.s)
    f = wd.f_and(wd.f_plane((0, 0, z0), (0, 0, 1)), wd.f_plane((0, 0, z1), (0, 0, -1)))
    return wd.field_shell(ctx, key, surface, sel, offset * ctx.s, f, smooth=6, gap=gap, thickness=thickness, hide=False)


def palm_frame(ctx, side):
    """Palm centre, across-palm axis (pinky -> index), finger direction and palm normal (rest)."""
    ih, ph = ctx.head("index_01_" + side), ctx.head("pinky_01_" + side)
    hh = ctx.head("hand_" + side)
    across = (ih - ph) / np.linalg.norm(ih - ph)
    fwd = (ih + ph) / 2 - hh
    fwd /= np.linalg.norm(fwd)
    n = np.cross(across, fwd)
    n /= np.linalg.norm(n)
    th = ctx.head("thumb_01_" + side)
    if n @ (th - (ih + ph) / 2) < 0:
        n = -n
    centre = hh + ((ih + ph) / 2 - hh) * 0.55
    return centre, across, fwd, n


def ellipsoid_patch(name, centre, radii, keep, like):
    """Temporary collider: the faces of an ellipsoid whose centres pass keep((n,3) points)."""
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=64, v_segments=32, radius=1.0)
    for v in bm.verts:
        v.co = Vector((centre[0] + v.co.x * radii[0], centre[1] + v.co.y * radii[1], centre[2] + v.co.z * radii[2]))
    cen = np.array([tuple(f.calc_center_median()) for f in bm.faces])
    ok = keep(cen)
    bmesh.ops.delete(bm, geom=[f for f, k in zip(bm.faces, ok) if not k], context="FACES")
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    geo.link(ob, like)
    return ob


def hair_collider(ctx):
    objs = [ctx.body] + ([ctx.parts["hair"]] if "hair" in ctx.parts else [])
    return geo.Collider(objs)


# ============================================================================ Nix

def nix(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    hf = wd.head_frame(ctx)
    hc = hf["c"]
    # wrapped top to the hips, sleeves to the wrists
    top_with_sleeves(ctx, "top", "wrap", 0.49 * H, 0.96, 0.011, folds=0.0025, smooth=10)
    # loose trousers gathered into bandaged shins, split-toe footwraps
    leg_shell(ctx, "trousers", "trousers", 0.53 * H, 0.20 * H, 0.026, folds=0.005, gather=0.012)
    for side in ("l", "r"):
        kn, an = ctx.head("calf_" + side), ctx.head("foot_" + side)
        sel = side_mask(ctx, side) & (z >= 0.03 * H) & (z <= 0.25 * H) & (ctx.w("calf_" + side, "foot_" + side) > 0.4)
        f = wd.f_and(wd.f_plane((0, 0, 0.236 * H), (0, 0, -1)), wd.f_plane((0, 0, 0.034 * H), (0, 0, 1)))
        wd.field_shell(ctx, "shinwrap_" + side, "wrap_dark", sel, 0.012 * s, f, smooth=4, gap=0.008)
        lm = (ctx.w("calf_" + side) > 0.3) & side_mask(ctx, side)
        wd.ring_band(ctx, "shinband_" + side, "wrap", lm, kn, an, [0.45, 0.47, 0.49], [0.016, 0.019, 0.016])
        wd.ring_band(ctx, "shinband2_" + side, "wrap", lm, kn, an, [0.66, 0.68, 0.70], [0.016, 0.019, 0.016])
        fsel = side_mask(ctx, side) & (ctx.w("foot_" + side, "ball_" + side) > 0.4) & (z <= 0.06 * H)
        fsel &= ~((ctx.N[:, 2] < -0.55) & (z < 0.02 * s))
        wd.field_shell(ctx, "tabi_" + side, "feet", fsel, 0.005 * s, wd.f_plane((0, 0, 0.0015), (0, 0, 1)),
                       smooth=3, gap=0.004, collider=geo.Collider([ctx.body]))
    # dark hand wraps and bandaged forearms
    gloves(ctx, "handwrap_%s", "wrap_dark", 0.70, 0.003, fingers=False)
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        am = (ctx.w("lowerarm_" + side) > 0.3) & side_mask(ctx, side)
        for k, t in enumerate((0.25, 0.42, 0.59)):
            wd.ring_band(ctx, "armband%d_%s" % (k, side), "wrap_dark", am, el, wr, [t, t + 0.02, t + 0.04],
                         [0.014, 0.017, 0.014])
    # hood with capelet and neck, face opening
    nb = ctx.head("neck_01")
    sel = ((ctx.w("head") > 0.3) | (ctx.w("neck_01") > 0.2)
           | ((z > 0.73 * H) & (ctx.arm_w < 0.6) & (ctx.w("upperarm_l", "upperarm_r") < 0.6)))
    opening = wd.f_ellipse(0.0, hf["eye_z"] - 0.012 * s, 0.056 * s, 0.075 * s, front_y=hc[1] - 0.01 * s)

    def hood_shape(ob, nrm):
        src = src_coords(ctx, ob)
        c = geo.co(ob)
        back_top = smoothstep((src[:, 1] - hc[1]) / (0.09 * s)) * smoothstep((src[:, 2] - hf["eye_z"]) / (0.08 * s))
        c[:, 1] += 0.03 * s * back_top ** 2
        c[:, 2] += 0.012 * s * back_top ** 2
        cape = smoothstep((nb[2] - src[:, 2]) / (0.05 * s))
        c += nrm * (0.016 * s * cape)[:, None]
        geo.set_co(ob, c)

    # the ears: cut out of the hood shell, the holes closed with patches that are pushed onto a
    # smooth cap over the side of the skull, and hidden under the hood, so no ear shape shows
    ear = ((ctx.w("head") > 0.5) & (np.abs(V[:, 0] - hc[0]) > hf["half"][0] * 0.72)
           & (z > hf["eye_z"] - 0.075 * s) & (z < hf["eye_z"] + 0.05 * s) & (V[:, 1] > hc[1] - 0.045 * s))
    ear_f = geo.face_mask(ctx.FV, ear, "any")
    ctx.hide_faces(ear_f, 0)
    zone = np.zeros(len(V), bool)
    zone[np.unique(np.concatenate([f for f, m in zip(ctx.FV, ear_f) if m]))] = True
    hv = (ctx.w("head") > 0.5) & ~ear
    band_v = hv & (np.abs(z - hf["eye_z"]) < 0.06 * s)
    radii = np.array([np.abs(V[band_v, 0] - hc[0]).max(), np.abs(V[hv, 1] - hc[1]).max(),
                      np.abs(V[hv, 2] - hc[2]).max()])
    near = hv & (np.abs(V[:, 0] - hc[0]) > hf["half"][0] * 0.55) & (z > hf["eye_z"] - 0.1 * s) & (z < hf["eye_z"] + 0.08 * s)
    k = np.sqrt((((V[near] - hc) / radii) ** 2).sum(1))
    radii = radii * float(np.clip(np.quantile(k, 0.98), 1.0, 1.15))     # the cap clears the skull round it
    ez = V[ear]

    def in_window(c):
        return ((np.abs(c[:, 0] - hc[0]) > 0.5 * radii[0]) & (c[:, 2] > ez[:, 2].min() - 0.025 * s)
                & (c[:, 2] < ez[:, 2].max() + 0.025 * s) & (c[:, 1] > ez[:, 1].min() - 0.025 * s)
                & (c[:, 1] < ez[:, 1].max() + 0.025 * s))

    cap = ellipsoid_patch("nix_earcap_tmp", hc, radii, in_window, ctx.body)
    hood_coll = geo.Collider([ctx.body, cap] + ctx.colliders[1:], [~ear_f, None] + [None] * (len(ctx.colliders) - 1))

    def hood_extra(ob, nrm):
        hood_shape(ob, nrm)
        n = geo.fill_holes(ob, zone[geo.source_index(ob)], rings=2)
        fo.core.log("nix hood: closed %d ear holes" % n)

    wd.field_shell(ctx, "hood", "hood", sel & ~ear, 0.026 * s,
                   wd.f_and(opening, wd.f_plane((0, 0, 0.735 * H), (0, 0, 1))),
                   smooth=24, gap=0.014, thickness=0.004, extra=hood_extra, decimate=0.45, collider=hood_coll)
    bpy.data.objects.remove(cap)
    # cloth mask over nose and mouth
    msel = (ctx.w("head") > 0.8) & (V[:, 1] < hc[1] - 0.02 * s) & (z < hf["eye_z"]) & (np.abs(V[:, 0]) < 0.075 * s)
    mfield = lambda c: (hf["eye_z"] - 0.013 * s - 0.008 * s * np.exp(-(c[:, 0] / (0.018 * s)) ** 2)) - c[:, 2]
    wd.field_shell(ctx, "mask", "mask", msel, 0.004 * s, mfield, smooth=5, gap=0.003, thickness=0.002, decimate=0.4)
    # straps crossing the chest, front and back
    coll = ctx.collider()
    for k, ysgn in enumerate((-1, 1)):
        pts = []
        for f in np.linspace(0, 1, 14):
            x = 0.11 * s * (1 - f) - 0.12 * s * f
            zz = 0.81 * H * (1 - f) + 0.54 * H * f
            d = Vector((0, ysgn, 0))
            o = Vector((x, ctx.head("spine_03")[1], zz))
            loc, n, _, _ = coll.tree.ray_cast(o + d * 0.6, -d)
            if loc is not None:
                pts.append(np.array(loc + n * 0.004 * s))
        sv, sf = fo.ribbon(pts, 0.03 * s, coll)
        strap = geo.new_mesh_object("strap%d" % k, sv, sf, ctx.rig)
        geo.transfer_weights(ctx.body, strap)
        geo.solidify(strap, 0.004 * s, 1.0)
        ctx.add("crossstrap%d" % k, strap, "wrap_dark", collide=False)
    # slop-green sash, knot at the left hip, two tails
    band(ctx, "sash", "sash", 0.553 * H, 0.587 * H, 0.03, gap=0.01, thickness=0.006)
    coll = ctx.collider()
    kx = 0.11 * s
    kz = 0.57 * H
    loc, n, _, _ = coll.tree.ray_cast(Vector((kx + 0.4, ctx.head("pelvis")[1] - 0.05 * s, kz)), Vector((-1, 0, 0)))
    knot_c = np.array(loc + n * 0.012 * s) if loc is not None else np.array([kx + 0.05 * s, 0, kz])
    wd.lathe_part(ctx, "sash_knot", "sash", [(0, -0.02 * s), (0.026 * s, -0.012 * s), (0.03 * s, 0.0),
                                             (0.024 * s, 0.014 * s), (0, 0.02 * s)], knot_c, n, 10, rigid="pelvis")
    for k, (length, dx) in enumerate(((0.21, 0.0), (0.16, 0.035))):
        pts = [knot_c + np.array([dx * s, -0.01 * s, -0.01 * s - f * length * s]) + np.array(n) * 0.01 * s * f
               for f in np.linspace(0, 1, 9)]
        sv, sf = fo.ribbon(pts, 0.05 * s, coll)
        tail = geo.new_mesh_object("sashtail%d" % k, sv, sf, ctx.rig)
        geo.recalc_normals(tail)
        ct = geo.co(tail)
        fdown = np.clip((knot_c[2] - ct[:, 2]) / (length * s), 0, 1)
        geo.set_weights(tail, {"pelvis": 1 - 0.5 * fdown, "thigh_l": 0.5 * fdown})
        geo.solidify(tail, 0.003 * s, 1.0)
        ctx.add("sashtail%d" % k, tail, "sash", collide=False)
    # kusarigama: sickle in the right hand, chain hanging from the pommel
    centre, across, fwd, pn = palm_frame(ctx, "r")
    c0 = centre + pn * 0.022 * s
    axis = across                                  # handle across the palm, blade on the thumb side
    wd.lathe_part(ctx, "sickle_handle", "wood", [(0, -0.12 * s), (0.013 * s, -0.12 * s), (0.015 * s, -0.10 * s),
                                                  (0.014 * s, 0.10 * s), (0.016 * s, 0.11 * s), (0, 0.115 * s)],
                  c0, axis, 8, rigid="hand_r")
    tip = c0 + axis * 0.11 * s
    blade = []
    for f in np.linspace(0, 1, 9):
        ang = f * math.pi * 0.85
        p = tip + (-pn * math.sin(ang) * 0.11 + axis * (math.cos(ang) - 1) * -0.03) * s - fwd * 0.0
        blade.append(p)
    bv, bf = [], []
    for i, p in enumerate(blade):
        w = 0.022 * s * (1 - (i / (len(blade) - 1)) ** 1.5) + 0.002 * s
        inward = (tip - p)
        inward = inward / (np.linalg.norm(inward) + 1e-9)
        bv += [tuple(p - fwd * 0.002 * s), tuple(p + inward * w - fwd * 0.002 * s),
               tuple(p + fwd * 0.002 * s), tuple(p + inward * w + fwd * 0.002 * s)]
    for i in range(len(blade) - 1):
        a, b = 4 * i, 4 * (i + 1)
        bf += [(a, b, b + 1, a + 1), (a + 2, a + 3, b + 3, b + 2), (a, a + 2, b + 2, b), (a + 1, b + 1, b + 3, a + 3)]
    blade_ob = geo.new_mesh_object("sickle_blade", bv, bf, ctx.rig)
    geo.recalc_normals(blade_ob)
    geo.set_rigid(blade_ob, "hand_r")
    ctx.add("sickle_blade", blade_ob, "steel", collide=False)
    pommel = c0 - axis * 0.12 * s
    links_v, links_f = [], []
    down = np.array([0, 0, -1.0])
    for k in range(14):
        cen = pommel + down * (0.012 + k * 0.026) * s
        ax = np.array([1.0, 0, 0]) if k % 2 == 0 else np.array([0, 1.0, 0])
        F = wd.frame(ax)
        ring = [cen + F @ np.array([0.009 * s * math.cos(a), 0.014 * s * math.sin(a), 0]) for a in np.linspace(0, 2 * math.pi, 9)]
        v, f = geo.tube(ring, 0.0028 * s, 5)
        links_f += [tuple(i + len(links_v) for i in ff) for ff in f]
        links_v += v
    chain = geo.new_mesh_object("chain", links_v, links_f, ctx.rig)
    geo.set_rigid(chain, "hand_r")
    ctx.add("chain", chain, "steel", collide=False)
    wd.lathe_part(ctx, "chain_weight", "steel", [(0, 0), (0.016 * s, 0.004 * s), (0.016 * s, 0.03 * s), (0, 0.036 * s)],
                  pommel + down * (0.012 + 14 * 0.026) * s - down * 0.0 + np.array([0, 0, -0.03 * s]), (0, 0, 1), 8, rigid="hand_r")


# ============================================================================ Mirela

def gigot(ctx, ob, amount, folds=0.0, t_peak=0.3):
    """Leg-of-mutton sleeves on a finished top: the upper sleeve puffed out round the upper-arm
    axis (most a third of the way down, less on the side towards the body, gone by the elbow)
    and gathered into folds under the shoulder seam. Both layers of the solidified shell move
    together (they share their source vertex)."""
    s = ctx.s
    src = src_coords(ctx, ob)
    c = geo.co(ob)
    uw = ctx.w("upperarm_l", "upperarm_r")[geo.source_index(ob)]
    for side in ("l", "r"):
        a, b = ctx.head("upperarm_" + side), ctx.head("lowerarm_" + side)
        d = (b - a) / np.linalg.norm(b - a)
        e1 = np.cross(d, (0, 0, 1.0))
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(d, e1)
        t, r, foot = geo.axis_t(src, a, b)
        m = side_mask_pts(src, side) & (r < 0.13 * s) & (t > -0.3) & (t < 1.0)
        radial = src - foot
        radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
        inner = np.clip(-radial[:, 0] * (1 if side == "l" else -1), 0, 1)
        prof = smoothstep((t + 0.15) / 0.3) * (1 - smoothstep((t - t_peak) / (0.85 - t_peak)))
        amt = amount * s * prof * (1 - 0.75 * inner) * np.clip(uw * 1.5, 0, 1)
        if folds:
            ang = np.arctan2(radial @ e2, radial @ e1)
            amt = amt + folds * s * np.sin(9 * ang) * prof * (1 - smoothstep(t / 0.65)) * np.clip(uw * 1.5, 0, 1)
        c[m] += radial[m] * amt[m, None]
    geo.set_co(ob, c)


def mirela(ctx):
    """Ivory mourning gown that reads as cloth at fight distance: an ash-ivory gown with a high
    collar, puffed (leg-of-mutton) sleeves and black cuffs; a black overbust corset with seams
    and ivory piping over the bust and waist; black close lace from the corset to the throat;
    a long skirt of two wrap-over panels (closed at the front, each riding its own shin below
    the knee) tucked under the corset."""
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    hf = wd.head_frame(ctx)
    hc = hf["c"]
    nb, nt = ctx.head("neck_01"), ctx.head("head")
    chin = V[(ctx.w("head") > 0.9) & (V[:, 1] < hc[1] - 0.04 * s)]
    z_chin = float(chin[:, 2].min()) if len(chin) else 0.88 * H
    z_collar = min(0.888 * H, z_chin - 0.012 * s)
    gown = top_with_sleeves(ctx, "gown", "gown", 0.485 * H, 0.90, 0.012, flare=0.012, flare_from=0.7,
                            neck_top=z_collar, smooth=22, gap=0.010)
    gigot(ctx, gown, 0.036, folds=0.005)
    gloves(ctx, "glove_%s", "gloves", 0.60, 0.0022, fingers=True, decimate=0.55)
    leg_shell(ctx, "leggings", "underdress", 0.50 * H, 0.07 * H, 0.010, folds=0.002, gap=0.006, smooth=18, medial=0.6)
    boots(ctx, "boots", "boots", 0.11, 0.02)
    # black cuffs at the sleeve ends, choker at the collar top
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        am = (ctx.w("lowerarm_" + side) > 0.3) & side_mask(ctx, side)
        wd.ring_band(ctx, "cuff_" + side, "lace", am, el, wr, [0.80, 0.85, 0.895], [0.020, 0.023, 0.021])
    nk = (ctx.w("neck_01", "head") > 0.3)
    t_ch = (z_collar - nb[2]) / (nt[2] - nb[2])
    wd.ring_band(ctx, "choker", "lace", nk, nb, nt, [t_ch - 0.05, t_ch - 0.01, t_ch + 0.03], [0.036, 0.040, 0.036])
    # long skirt: two wrap-over panels, closed at the front and overlapping at the back; the top
    # sits at the waist, under the corset (no ledge)
    objs = [ctx.body, ctx.parts["leggings"], ctx.parts["gown"]]
    skirts = []
    for side, arc, gap in (("l", (-0.30, math.pi + 0.18), 0.005), ("r", (math.pi - 0.18, 2 * math.pi + 0.30), 0.011)):
        ob, _, _, _ = wd.wrap_skirt(ctx, "skirt_" + side, "gown", 0.585 * H, 0.085 * H, gap=gap, flare=0.17,
                                    folds=(0.013, 10), share=(0.75, 0.45, 0.2), thickness=0.004, row=0.035, objs=objs,
                                    arc=arc, side=side, calf_share=0.9, nu=28)
        skirts.append(ob)
    # overbust corset: black, with vertical seams, a front point, ivory piping along both edges
    z0, z1 = 0.535 * H, 0.745 * H
    yc = float(ctx.head("spine_02")[1])

    def bottom_edge(c):
        return z0 - 0.03 * s * np.clip(1 - np.abs(c[:, 0]) / (0.09 * s), 0, 1) * (c[:, 1] < yc)

    x_side = 0.07 * s

    def top_edge(c):     # straight over the bust, a little higher at the back, dipping under the arms
        return (z1 + 0.012 * s * (c[:, 1] > yc)
                - 0.035 * s * smoothstep((np.abs(c[:, 0]) - x_side) / (0.06 * s)))

    tors = (ctx.arm_w < 0.25) & (z >= z0 - 0.06 * s) & (z <= z1 + 0.04 * s)

    def seams(ob, nrm):
        src = src_coords(ctx, ob)
        ang = np.arctan2(src[:, 0], -(src[:, 1] - yc))
        groove = np.zeros(len(src))
        for k in range(10):
            a0 = -math.pi + k * 2 * math.pi / 10
            dd = np.abs(((ang - a0 + math.pi) % (2 * math.pi)) - math.pi)
            groove += np.exp(-(dd / 0.035) ** 2)
        geo.offset_along(ob, nrm, -0.0015 * s * np.clip(groove, 0, 1))

    corset = wd.field_shell(ctx, "corset", "corset", tors, 0.024 * s,
                            lambda c: np.minimum(c[:, 2] - bottom_edge(c), top_edge(c) - c[:, 2]),
                            smooth=20, gap=0.024, thickness=0.004, extra=seams, hide=False)

    def on_corset(key, field, lift, thick):
        """A strip of the corset's outer surface, lifted off it: piping that follows it exactly."""
        ob = geo.duplicate(corset, key)
        me = ob.data
        cen = np.array([tuple(p.center) for p in me.polygons])
        nr = np.array([tuple(p.normal) for p in me.polygons])
        rad = cen - (0.0, yc, 0.0)
        rad[:, 2] = 0
        rad /= np.linalg.norm(rad, axis=1)[:, None] + 1e-9
        geo.keep_faces(ob, (nr * rad).sum(1) > 0.2)
        geo.cut_field(ob, field)
        geo.offset_along(ob, geo.vnormals(ob), lift * s)
        if thick:
            geo.solidify(ob, thick * s)
        ctx.add(key, ob, "trim", collide=False)

    for key, edge in (("corset_trim_top", top_edge), ("corset_trim_bottom", bottom_edge)):
        on_corset(key, lambda c, e=edge: 0.011 * s - np.abs(c[:, 2] - e(c)), 0.0025, 0.003)
    seam_a = np.array([-math.pi + k * 2 * math.pi / 10 for k in range(10)])

    def seam_field(c):
        ang = np.arctan2(c[:, 0], -(c[:, 1] - yc))
        dd = np.abs(((ang[:, None] - seam_a[None] + math.pi) % (2 * math.pi)) - math.pi).min(1)
        dist = dd * np.hypot(c[:, 0], c[:, 1] - yc)
        return np.minimum(0.003 * s - dist, np.minimum(c[:, 2] - bottom_edge(c) - 0.009 * s, top_edge(c) - 0.009 * s - c[:, 2]))

    on_corset("corset_seams", seam_field, 0.0035, 0.0)
    # black close lace from the corset up to the collar, over the chest, shoulders and upper back
    x_sh = abs(ctx.head("upperarm_l")[0])
    ysel = ((z >= z1 - 0.02 * s) & (z <= z_collar + 0.02 * s) & (ctx.w("head") < 0.45) & (ctx.arm_w < 0.5)
            & (np.abs(V[:, 0]) < x_sh * 0.95))
    z_arm = float(ctx.head("upperarm_l")[2]) - 0.05 * s
    yfield = wd.f_and(lambda c: c[:, 2] - (top_edge(c) + 0.004 * s), wd.f_plane((0, 0, z_collar + 0.002 * s), (0, 0, -1)),
                      lambda c: x_sh * 0.78 - np.abs(c[:, 0]),
                      wd.f_or(wd.f_plane((0, 0, z_arm), (0, 0, 1)), lambda c: x_sh * 0.6 - np.abs(c[:, 0])))
    wd.field_shell(ctx, "yoke", "yoke_lace", ysel, 0.014 * s, yfield, smooth=22, gap=0.006, thickness=0.0,
                   hide=False, largest=True)
    # black lace veil over the hair and shoulders, open at the face
    objs = [ctx.body] + [ctx.parts[k] for k in ("hair", "gown") if k in ctx.parts]
    wd.wrap_skirt(ctx, "veil", "veil", hf["top"] + 0.004 * s, nb[2] - 0.035 * s, open_frac=0.36, gap=0.010, flare=0.015,
                  folds=(0.004, 6), y_c=hc[1] + 0.01 * s, objs=objs, weight_mode="transfer", thickness=0.0, nu=36,
                  row=0.022, snug_rows=1)


# ============================================================================ Bram

def bram(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    hf = wd.head_frame(ctx)
    hc = hf["c"]
    nb, nt = ctx.head("neck_01"), ctx.head("head")
    leg_shell(ctx, "trousers", "trousers", 0.55 * H, 0.12 * H, 0.016, folds=0.004)
    boots(ctx, "boots", "sole", 0.25, ctx.cfg.get("sole_m", 0.025))
    top_with_sleeves(ctx, "coat", "coat", 0.50 * H, 0.95, 0.014, smooth=10, folds=0.002)
    wd.wrap_skirt(ctx, "coat_tails", "coat", 0.565 * H, 0.29 * H, open_frac=0.12, gap=0.016, flare=0.05,
                  folds=(0.006, 7), slit_z=0.45 * H, share=(0.85, 0.5, 0.2), thickness=0.005)
    band(ctx, "belt", "belt", 0.548 * H, 0.578 * H, 0.03, gap=0.008, thickness=0.006)
    coll = ctx.collider()
    yb = ctx.ray_front(0.0, 0.563 * H, coll)
    wd.box_part(ctx, "buckle", "brass", (0, yb - 0.006 * s, 0.563 * H), (0.05 * s, 0.038 * s, 0.01 * s), axis=(0, -1, 0),
                hint=(1, 0, 0), rigid="pelvis")
    # double-breasted: two columns of brass buttons
    for col in (-1, 1):
        for k in range(4):
            zz = 0.735 * H - k * 0.042 * H
            x = col * (0.05 * s + 0.004 * s * k)
            y = ctx.ray_front(x, zz, coll)
            if y is None:
                continue
            wd.lathe_part(ctx, "button%d%d" % (col + 1, k), "brass", [(0, 0), (0.009 * s, 0), (0.008 * s, 0.003 * s), (0, 0.0045 * s)],
                          (x, y + 0.001 * s, zz), (0, -1, 0), 8, rigid="spine_02" if zz < 0.70 * H else "spine_03")
    # stand collar, epaulettes, dark cuffs
    nk = ctx.w("neck_01", "spine_03", "clavicle_l", "clavicle_r") > 0.3
    wd.ring_band(ctx, "collar", "coat_dark", nk & (z > nb[2] - 0.05 * s), nb, nt, [0.02, 0.2, 0.42], [0.016, 0.018, 0.016])
    for side in ("l", "r"):
        sh = ctx.head("upperarm_" + side)
        cl = ctx.head("clavicle_" + side)
        top = np.array([sh[0] * 0.92, sh[1], sh[2] + 0.045 * s])
        loc, n, _ = coll.nearest(top)
        wd.box_part(ctx, "epaulette_" + side, "coat_dark", np.array(loc) + np.array(n) * 0.006 * s,
                    (0.075 * s, 0.05 * s, 0.012 * s), axis=n, hint=(sh - cl), rigid="clavicle_" + side)
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        am = (ctx.w("lowerarm_" + side) > 0.3) & side_mask(ctx, side)
        wd.ring_band(ctx, "cuff_" + side, "coat_dark", am, el, wr, [0.80, 0.86, 0.94], [0.019, 0.021, 0.024])
    # peaked watch cap over the hair, rigid on the head
    cap(ctx, hf)
    # clean-shaven: the mutton chops read as a dark smudge in-engine and were removed
    # hardwood baton in the right hand, lantern at the left hip
    centre, across, fwd, pn = palm_frame(ctx, "r")
    c0 = centre + pn * 0.022 * s
    wd.lathe_part(ctx, "baton", "baton", [(0, -0.12 * s), (0.016 * s, -0.12 * s), (0.017 * s, -0.08 * s), (0.014 * s, -0.06 * s),
                                          (0.018 * s, 0.30 * s), (0.016 * s, 0.33 * s), (0, 0.335 * s)],
                  c0, -across, 10, rigid="hand_r")
    lx = 0.17 * s
    loc, n, _, _ = coll.tree.ray_cast(Vector((lx + 0.5, ctx.head("pelvis")[1], 0.53 * H)), Vector((-1, 0, 0)))
    lc = np.array(loc) + np.array([0.05 * s, 0, -0.06 * s]) if loc is not None else np.array([lx + 0.08 * s, 0, 0.47 * H])
    wd.lathe_part(ctx, "lantern_frame", "brass", [(0, 0), (0.035 * s, 0), (0.036 * s, 0.012 * s), (0.03 * s, 0.014 * s)], lc, (0, 0, 1), 10, rigid="pelvis")
    wd.lathe_part(ctx, "lantern_glass", "glass", [(0, 0.014 * s), (0.028 * s, 0.014 * s), (0.03 * s, 0.07 * s), (0, 0.072 * s)], lc, (0, 0, 1), 10, rigid="pelvis")
    wd.lathe_part(ctx, "lantern_cap", "iron", [(0, 0.072 * s), (0.034 * s, 0.072 * s), (0.02 * s, 0.09 * s), (0.006 * s, 0.1 * s), (0, 0.102 * s)],
                  lc, (0, 0, 1), 10, rigid="pelvis")
    wd.tube_part(ctx, "lantern_hook", "iron", [lc + np.array([0, 0, 0.1 * s]), lc + np.array([-0.02 * s, 0, 0.13 * s]),
                                                np.array([lc[0] - 0.04 * s, lc[1], 0.555 * H])], 0.003 * s, 5, rigid="pelvis")


def cap(ctx, hf):
    """Peaked watch cap: crown lofted over the head and hair sections, a dark band, a leather peak."""
    s = ctx.s
    pts = [geo.co(ctx.body)[ctx.w("head") > 0.5]]
    if "hair" in ctx.parts:
        pts.append(geo.co(ctx.parts["hair"]))
    P = np.concatenate(pts)
    z0 = hf["eye_z"] + 0.045 * s
    top = max(hf["top"], float(P[:, 2].max())) + 0.012 * s
    zs = [z0, z0 + 0.02 * s, z0 + 0.045 * s, top - 0.01 * s, top + 0.005 * s]
    grow = [0.008, 0.010, 0.014, 0.024, 0.026]
    rings = []
    c2 = None
    for zz, g in zip(zs, grow):
        sl = P[np.abs(P[:, 2] - min(zz, top - 0.02 * s)) < 0.012 * s][:, :2]
        if len(sl) < 4:
            sl = P[P[:, 2] > z0][:, :2]
        ring, c = geo.ring_by_angle(convex_hull(sl), 24)
        c2 = c if c2 is None else c2
        v = ring - c
        ring = c + v * (1 + g * s / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))
        rings.append(np.column_stack([ring, np.full(len(ring), zz)]))
    crown = fo.loft_rings(rings, "cap_crown", ctx.rig, ctx.body)
    # close the top
    me = crown.data
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    n = 24
    top_ring = [bm.verts[len(rings) * n - n + k] for k in range(n)]
    bm.faces.new(top_ring[::-1])
    bm.to_mesh(me)
    bm.free()
    geo.triangulate_ngons(crown)
    geo.recalc_normals(crown)
    geo.solidify(crown, 0.003 * s)
    geo.set_rigid(crown, "head")
    ctx.add("cap_crown", crown, "cap", collide=False)
    wd.ring_band(ctx, "cap_band", "cap_dark", ctx.w("head") > 0.5, np.array([0, 0, z0 - 0.002 * s]), np.array([0, 0, z0 + 1.0]),
                 [0.0, 0.02 * s, 0.035 * s], [0.012, 0.014, 0.012], rigid="head")
    # peak: a curved plate in front of the band
    front = rings[0][np.argmin(rings[0][:, 1])]
    pv, pf = [], []
    for i, u in enumerate(np.linspace(-1, 1, 9)):
        for j, d in enumerate((0.0, 0.03, 0.055)):
            x = u * 0.075 * s * (1 - 0.25 * d / 0.055)
            y = front[1] + 0.012 * s * u * u - d * s * 1.0
            zz = z0 + 0.004 * s - d * 0.35 * s
            pv.append((x, y, zz))
    for i in range(8):
        for j in range(2):
            a = i * 3 + j
            pf.append((a, a + 3, a + 4, a + 1))
    peak = geo.new_mesh_object("cap_peak", pv, pf, ctx.rig)
    geo.solidify(peak, 0.004 * s, 0.0)
    geo.set_rigid(peak, "head")
    ctx.add("cap_peak", peak, "peak", collide=False)


# ============================================================================ Hollis

def hollis(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    hf = wd.head_frame(ctx)
    hc = hf["c"]
    nb = ctx.head("neck_01")
    sole = ctx.cfg.get("sole_m", 0.035)
    boots(ctx, "boot", "lead", 0.25, sole)
    # the inflated canvas suit: everything from the boot tops to the neck except the hands
    hands = hand_w(ctx, "l") + hand_w(ctx, "r")
    sel = (ctx.w("head") < 0.4) & (hands < 0.3) & (z >= 0.20 * H) & (z <= nb[2] + 0.04 * s)
    fields = [wd.f_plane((0, 0, 0.22 * H), (0, 0, 1)), wd.f_plane((0, 0, nb[2] + 0.02 * s), (0, 0, -1))]
    for side in ("l", "r"):
        fields.append(wd.f_limb(ctx.head("lowerarm_" + side), ctx.head("hand_" + side), 0.93, "proximal", side, radius=0.16 * s))

    def puff(ob, nrm):
        src = src_coords(ctx, ob)
        c = geo.co(ob)
        tors = ctx.w("spine_01", "spine_02", "pelvis")[geo.source_index(ob)]
        c += nrm * (0.01 * s * tors)[:, None]
        geo.set_co(ob, c)

    wd.field_shell(ctx, "suit", "suit", sel, 0.026 * s, wd.f_and(*fields), smooth=14, gap=0.02, thickness=0.005, extra=puff)
    # rubber gloves and cuffs
    gloves(ctx, "glove_%s", "rubber", 0.86, 0.006, fingers=True, thickness=0.002, decimate=0.55)
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        am = (ctx.w("lowerarm_" + side, "hand_" + side) > 0.3) & side_mask(ctx, side)
        wd.ring_band(ctx, "cuff_" + side, "rubber", am, el, wr, [0.82, 0.88, 0.94], [0.034, 0.038, 0.034])
    band(ctx, "belt", "belt", 0.545 * H, 0.578 * H, 0.04, gap=0.008, thickness=0.006)
    # brass breastplate (corselet) over the shoulders, its edge dipping to a point front and back
    csel = (z > 0.66 * H) & (z <= nb[2] + 0.05 * s) & (ctx.w("head") < 0.4) & (ctx.w("upperarm_l", "upperarm_r") < 0.55) & (hands < 0.1)
    z_edge = 0.725 * H

    def cfield(c):
        dip = 0.05 * s * np.clip(1 - np.abs(c[:, 0]) / (0.16 * s), 0, 1)
        return np.minimum(c[:, 2] - (z_edge - dip), nb[2] + 0.04 * s - c[:, 2])

    wd.field_shell(ctx, "corselet", "brass", csel, 0.055 * s, cfield, smooth=24, gap=0.035, thickness=0.006, hide=False,
                   largest=True)
    # round brass helmet with three portholes, rigid on the head
    R = float(np.linalg.norm(hf["half"])) * 0.95 + 0.05 * s
    hcen = np.array([0.0, hc[1], hc[2] + 0.01 * s])
    prof = []
    for a in np.linspace(-0.62 * math.pi / 2, math.pi / 2, 12):
        prof.append((max(R * math.cos(a), 0.0005), R * math.sin(a)))
    prof.append((0.0, R))
    wd.lathe_part(ctx, "helmet", "brass", prof, hcen, (0, 0, 1), 24, rigid="head")
    bottom_z = hcen[2] + R * math.sin(-0.62 * math.pi / 2)
    wd.lathe_part(ctx, "helmet_collar", "brass_dark", [(R * 0.86, bottom_z - 0.03 * s), (R * 0.95, bottom_z - 0.028 * s),
                                                       (R * 0.95, bottom_z + 0.005 * s), (R * 0.82, bottom_z + 0.01 * s)],
                  hcen * [1, 1, 0], (0, 0, 1), 24, rigid="head")
    ports = [((0, -1, 0), 0.064 * s, 0.0), ((-1, 0, 0), 0.049 * s, 0.01 * s), ((1, 0, 0), 0.049 * s, 0.01 * s)]
    for k, (d, r, dz) in enumerate(ports):
        d = np.array(d, float)
        pc = hcen + d * R * 0.985 + np.array([0, 0, dz])
        wd.lathe_part(ctx, "port_frame%d" % k, "brass_dark", [(r * 1.0, -0.006 * s), (r * 1.32, -0.004 * s), (r * 1.34, 0.012 * s),
                                                              (r * 1.02, 0.014 * s), (r * 0.98, 0.0)], pc, d, 16, rigid="head")
        wd.lathe_part(ctx, "port_glass%d" % k, "glass", [(0, 0.004 * s), (r, 0.002 * s), (r * 0.99, -0.002 * s), (0, -0.002 * s)],
                      pc, d, 16, rigid="head")
        if k > 0:
            for bz in (-0.45, 0.0, 0.45):
                up = np.array([0, 0, 1.0])
                a = pc + d * 0.018 * s + up * bz * r - np.cross(d, up) * r * 1.1
                b = pc + d * 0.018 * s + up * bz * r + np.cross(d, up) * r * 1.1
                wd.tube_part(ctx, "guard%d%d" % (k, int(bz * 10 + 5)), "brass_dark", [a, b], 0.004 * s, 6, rigid="head")
    ctx.hide_faces(geo.face_mask(ctx.FV, (ctx.w("head") > 0.5) & (V[:, 2] > hf["eye_z"] + 0.02 * s)), 1)
    # harpoon gun in the right hand
    centre, across, fwd, pn = palm_frame(ctx, "r")
    c0 = centre + pn * 0.02 * s
    wd.lathe_part(ctx, "harpoon_grip", "iron", [(0, -0.06 * s), (0.016 * s, -0.06 * s), (0.016 * s, 0.06 * s), (0, 0.06 * s)],
                  c0, across, 8, rigid="hand_r")
    barrel_dir = -fwd if fwd[2] < 0 else fwd
    bstart = c0 + across * 0.06 * s
    wd.lathe_part(ctx, "harpoon_barrel", "brass_dark", [(0, -0.1 * s), (0.022 * s, -0.1 * s), (0.02 * s, 0.38 * s), (0, 0.38 * s)],
                  bstart, -pn, 10, rigid="hand_r")
    wd.lathe_part(ctx, "harpoon_tip", "iron", [(0, 0.38 * s), (0.014 * s, 0.38 * s), (0.02 * s, 0.4 * s), (0, 0.5 * s)],
                  bstart, -pn, 8, rigid="hand_r")


# ============================================================================ the Slopbat

def add_wing_bones(ctx):
    """Extra bones for the folded wings, under the clavicles; the 53 standard bones are untouched."""
    s, rig = ctx.s, ctx.rig
    sp = ctx.head("spine_03")
    sh_z = ctx.head("upperarm_l")[2]
    back = float(ctx.V[(np.abs(ctx.V[:, 2] - sp[2]) < 0.05 * s), 1].max())
    chains = {}
    with bpy.context.temp_override(active_object=rig, object=rig, selected_objects=[rig]):
        bpy.context.view_layer.objects.active = rig
        bpy.ops.object.mode_set(mode="EDIT")
        for side, sgn in (("l", 1), ("r", -1)):
            root = np.array([sgn * 0.075 * s, back + 0.03 * s, sh_z - 0.04 * s])
            wrist = root + np.array([sgn * 0.16 * s, 0.14 * s, 0.42 * s])
            knuck = wrist + np.array([sgn * 0.10 * s, 0.06 * s, -0.95 * s])
            tip = knuck + np.array([sgn * 0.03 * s, 0.02 * s, -0.42 * s])
            pts = [root, wrist, knuck, tip]
            parent = rig.data.edit_bones["clavicle_" + side]
            for k in range(3):
                eb = rig.data.edit_bones.new("wing_%s_%02d" % (side, k + 1))
                eb.head, eb.tail = Vector(pts[k]), Vector(pts[k + 1])
                eb.parent = parent
                eb.use_connect = k > 0
                eb.use_deform = True
                parent = eb
            chains[side] = pts
        bpy.ops.object.mode_set(mode="OBJECT")
    ctx.L = __import__("fighter_core").landmarks(rig)
    return chains


def wing(ctx, side, pts):
    """Folded bat wing: spars along the wing bones and a membrane from the finger spar back to
    the body line, scalloped at the trailing edge. Weights blend from the spar bones (outer
    edge) to the back (inner edge), so rotating wing_*_01 opens the wing."""
    s = ctx.s
    sgn = 1 if side == "l" else -1
    root, wrist, knuck, tip = [np.asarray(p, float) for p in pts]
    back_low = root + np.array([sgn * 0.04 * s, 0.05 * s, -0.75 * s])
    nv, nu = 22, 10
    verts, uvs, W = [], [], []
    for i in range(nv + 1):
        v = i / nv
        if v < 0.68:
            f = v / 0.68
            outer = wrist + (knuck - wrist) * f
        else:
            f = (v - 0.68) / 0.32
            outer = knuck + (tip - knuck) * f
        inner = root + (back_low - root) * min(1.0, v / 0.85)
        for j in range(nu + 1):
            u = j / nu
            p = outer + (inner - outer) * u
            sag = math.sin(math.pi * u) * (0.05 + 0.04 * v) * s
            p = p + np.array([sgn * 0.3, 1.0, 0]) / math.hypot(1, 0.3) * sag
            verts.append(tuple(p))
            uvs.append((u * 0.5, v * 1.4))
            ws = (1 - u) ** 1.4
            wb = 1 - ws
            w = {}
            if v < 0.68:
                w["wing_%s_02" % side] = ws * (0.3 + 0.7 * min(1, v / 0.2))
                w["wing_%s_01" % side] = ws * (0.7 * max(0, 1 - v / 0.2))
            else:
                ff = (v - 0.68) / 0.32
                w["wing_%s_02" % side] = ws * (1 - ff)
                w["wing_%s_03" % side] = ws * ff
            w["spine_03"] = wb * (1 - v)
            w["spine_02"] = wb * v * 0.6
            w["pelvis"] = wb * v * 0.4
            W.append(w)
    fcs = []
    for i in range(nv):
        for j in range(nu):
            a = i * (nu + 1) + j
            fcs.append((a, a + nu + 1, a + nu + 2, a + 1))
    ob = geo.new_mesh_object("wing_" + side, verts, fcs, None, ctx.body)
    me = ob.data
    uvl = me.uv_layers.new(name="UVMap")
    for li, loop in enumerate(me.loops):
        uvl.data[li].uv = uvs[loop.vertex_index]
    names = sorted({k for w in W for k in w})
    geo.set_weights(ob, {n: np.array([w.get(n, 0.0) for w in W]) for n in names})
    # scalloped trailing edge: cut between the finger spar and the body line
    c0 = np.array(verts)

    def scallop(c):
        # parameter along the wing from the wrist, measured on the outer spar direction
        d = tip - wrist
        t = (c - wrist) @ d / (d @ d)
        u = np.clip(np.linalg.norm(np.cross(c - wrist, d / np.linalg.norm(d)), axis=1) / (0.35 * s), 0, 1)
        edge = 1.0 - 0.14 * np.abs(np.sin(math.pi * u * 2.2)) - 0.05 * u
        return (edge - t) * np.linalg.norm(d)
    geo.cut_field(ob, scallop)
    geo.solidify(ob, 0.004 * s, 0.0)
    geo.bind(ob, ctx.rig)
    ctx.add("wing_" + side, ob, "membrane", collide=False)
    # spars and the thumb claw
    for k, (a, b, r0, r1) in enumerate(((root, wrist, 0.03, 0.022), (wrist, knuck, 0.02, 0.012), (knuck, tip, 0.012, 0.004))):
        L = float(np.linalg.norm(b - a))
        wd.lathe_part(ctx, "spar%d_%s" % (k, side), "spar", [(0, -0.01 * s), (r0 * s, 0), (r1 * s, L), (0, L + 0.006 * s)],
                      a, b - a, 8, rigid="wing_%s_%02d" % (side, k + 1))
    wd.lathe_part(ctx, "thumbclaw_" + side, "claw", [(0, 0), (0.02 * s, 0.0), (0.011 * s, 0.05 * s), (0, 0.11 * s)],
                  wrist, np.array([sgn * 0.2, -0.3, 0.9]), 8, rigid="wing_%s_01" % side)


def slopbat(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    z = V[:, 2]
    hf = wd.head_frame(ctx)
    hc = hf["c"]
    chains = add_wing_bones(ctx)
    # tattered membrane cloak from the shoulders, open at the front
    wd.wrap_skirt(ctx, "cloak", "membrane", 0.80 * H, 0.30 * H, open_frac=0.34, gap=0.02, flare=0.10, folds=(0.012, 9),
                  slit_z=0.55 * H, share=(0.7, 0.4, 0.2), thickness=0.004, y_c=float(ctx.head("spine_03")[1]),
                  tatter=(0.09, 11), hips_z=float(ctx.head("thigh_l")[2]), row=0.04, snug_rows=1)
    for side in ("l", "r"):
        wing(ctx, side, chains[side])
    # crown of horn: curved cones round the top of the head
    for k, a in enumerate(np.linspace(-150, 150, 7)):
        ar = math.radians(a)
        d = np.array([math.sin(ar), math.cos(ar), 0.0])      # around, 0 = back
        base = hc + d * hf["half"][0] * 0.82 + np.array([0, 0, hf["top"] - hc[2] - 0.035 * s])
        length = (0.16 - 0.05 * abs(math.cos(ar))) * s * (1.25 if abs(a) < 40 else 1.0)
        up = np.array([0, 0, 1.0]) + d * 0.45
        pts = [base + up / np.linalg.norm(up) * length * f + d * 0.05 * s * f * f for f in np.linspace(0, 1, 6)]
        prof_v, prof_f = [], []
        rings = []
        for i, p in enumerate(pts):
            r = 0.018 * s * (1 - i / (len(pts) - 1)) + 0.0015 * s
            F = wd.frame(pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])
            rings.append([p + F @ np.array([r * math.cos(t), r * math.sin(t), 0]) for t in np.linspace(0, 2 * math.pi, 9)[:-1]])
        horn = fo.loft_rings(rings, "horn%d" % k, ctx.rig, ctx.body)
        geo.set_rigid(horn, "head")
        ctx.add("horn%d" % k, horn, "horn", collide=False)
    # bat ears
    for side, sgn in (("l", 1), ("r", -1)):
        base = np.array([sgn * hf["half"][0] * 0.95, hc[1] + 0.01 * s, hf["eye_z"] + 0.03 * s])
        tipp = base + np.array([sgn * 0.07 * s, 0.03 * s, 0.17 * s])
        rings = []
        for f in np.linspace(0, 1, 7):
            p = base + (tipp - base) * f
            w = 0.045 * s * math.sin(math.pi * min(0.999, 0.15 + 0.85 * f)) * (1 - f * 0.9) + 0.002 * s
            F = wd.frame(tipp - base, (0, -1, 0))
            rings.append([p + F @ np.array([w * math.cos(t), 0.012 * s * math.sin(t), 0]) for t in np.linspace(0, 2 * math.pi, 9)[:-1]])
        ear = fo.loft_rings(rings, "ear_" + side, ctx.rig, ctx.body)
        geo.set_rigid(ear, "head")
        ctx.add("ear_" + side, ear, "membrane", collide=False)
    # claws on every finger and toe
    for side in ("l", "r"):
        for f in FINGERS:
            b = "%s_03_%s" % (f, side)
            h, t = ctx.L[b]
            d = (t - h) / np.linalg.norm(t - h)
            wd.lathe_part(ctx, "claw_%s_%s" % (f, side), "claw", [(0, -0.004 * s), (0.007 * s, 0), (0.004 * s, 0.03 * s), (0, 0.06 * s)],
                          t - d * 0.008 * s, d + np.array([0, 0, -0.25]), 6, rigid=b)
        bh, bt = ctx.L["ball_" + side]
        d = (bt - bh) / np.linalg.norm(bt - bh)
        across = np.cross(d, [0, 0, 1.0])
        for k in range(4):
            o = bt + across * (k - 1.5) * 0.018 * s + np.array([0, 0, 0.006 * s])
            wd.lathe_part(ctx, "toeclaw%d_%s" % (k, side), "claw", [(0, 0), (0.006 * s, 0), (0.003 * s, 0.02 * s), (0, 0.04 * s)],
                          o, d + np.array([0, 0, -0.35]), 6, rigid="ball_" + side)


# ============================================================================ Daxion

def daxion(ctx):
    H, s, V = ctx.H, ctx.s, ctx.V
    leg_shell(ctx, "trousers", "trousers", 0.55 * H, 0.13 * H, 0.022, folds=0.004, smooth=18, medial=0.6)
    boots(ctx, "boots", "boots", 0.21, ctx.cfg.get("sole_m", 0.015))
    top_with_sleeves(ctx, "robe", "robe", 0.50 * H, 0.72, 0.010, flare=0.055, flare_from=0.12,
                     vneck=(0.715 * H, 2.6), smooth=10, folds=0.002)
    # collar band along the crossed V opening, over the shoulders and round the neck
    coll = ctx.collider()
    nb = ctx.head("neck_01")
    for sgn in (-1, 1):
        pts = []
        for f in np.linspace(0, 1, 12):
            if f < 0.6:
                g = f / 0.6
                zz = 0.715 * H + g * (nb[2] + 0.005 * s - 0.715 * H)
                x = sgn * ((zz - 0.715 * H) / 2.6 + 0.006 * s)
                y = ctx.ray_front(x, zz, coll)
                if y is not None:
                    pts.append(np.array([x, y - 0.002 * s, zz]))
            else:
                a = (f - 0.6) / 0.4 * math.pi / 2
                d = Vector((sgn * math.cos(a), math.sin(a), 0))
                o = Vector((0, nb[1], nb[2] + 0.005 * s))
                loc, n, _, _ = coll.tree.ray_cast(o + d * 0.5, -d)
                if loc is not None:
                    pts.append(np.array(loc + n * 0.003 * s))
        sv, sf = fo.ribbon(pts, 0.028 * s, coll)
        cb = geo.new_mesh_object("collar%d" % (sgn + 1), sv, sf, ctx.rig)
        geo.transfer_weights(ctx.body, cb)
        geo.solidify(cb, 0.005 * s, 1.0)
        ctx.add("collar%d" % (sgn + 1), cb, "robe", collide=False)
    band(ctx, "sash", "sash", 0.57 * H, 0.60 * H, 0.03, gap=0.008, thickness=0.006)
    wd.wrap_skirt(ctx, "robe_skirt", "robe", 0.575 * H, 0.285 * H, open_frac=0.16, gap=0.014, flare=0.09, folds=(0.011, 9),
                  slit_z=0.45 * H, share=(0.85, 0.5, 0.2), thickness=0.004)
    # the reddish-gold ring on the right middle finger
    h, t = ctx.L["middle_01_r"]
    fm = (ctx.w("middle_01_r") > 0.4)
    wd.ring_band(ctx, "ring", "ring", fm, h, t, [0.38, 0.45, 0.52], [0.0015, 0.0028, 0.0015], n=14, rigid="middle_01_r")


fo.BUILDERS.update({"nix": nix, "mirela": mirela, "bram": bram, "hollis": hollis, "slopbat": slopbat, "daxion": daxion})
