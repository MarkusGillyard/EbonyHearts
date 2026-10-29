"""Outfits for Wren Sallow and the Grandmaster (see fighter_outfits for Ashka and
Gorm, fighter_outfits2 for the rest of the roster). Registered into fighter_outfits.BUILDERS.

Both wear a crossed front: Wren a heavy-weave judo-style jacket, the Grandmaster a jiaoling robe over a
bone-grey inner robe. The crossing is built once here (Cross): the wearer's left panel lies over
the right, so seen from the front the collar makes the traditional 'y'."""
import math

import numpy as np
from mathutils import Vector

import fighter_core as core
import fighter_geo as geo
import fighter_outfits as fo
import fighter_wardrobe as wd
from fighter_outfits import smoothstep, src_coords, convex_hull
from fighter_outfits2 import hand_w, side_mask, side_mask_pts, leg_shell, palm_frame


# ----------------------------------------------------------------------------- small helpers

def set_attr(ob, name, vals):
    a = ob.data.attributes.get(name) or ob.data.attributes.new(name, "FLOAT", "POINT")
    a.data.foreach_set("value", np.ascontiguousarray(vals, dtype=np.float32))


def neck_radius(ctx):
    V = ctx.V
    nb, nt = ctx.head("neck_01"), ctx.head("head")
    _, r, _ = geo.axis_t(V, nb, nt)
    nk = (ctx.w("neck_01") > 0.5) & (V[:, 2] > nb[2]) & (V[:, 2] < nt[2])
    return float(np.median(r[nk])) if nk.sum() > 10 else 0.055 * ctx.s


def hit_from(coll, origin, direction):
    loc, n, _, _ = coll.tree.ray_cast(Vector(origin), Vector(direction))
    return (None, None) if loc is None else (np.array(loc), np.array(n))


def ribbon_part(ctx, key, surface, pts, width, thick, coll, emb_width=None, transfer=True):
    """Flat band along a path lying on a surface, solidified outwards. With emb_width the band
    carries emb_u (along, in band widths) / emb_v (across, 0..1) for fighter_tex.embroider."""
    sv, sf = fo.ribbon(pts, width, coll)
    ob = geo.new_mesh_object(key, sv, sf, ctx.rig)
    if emb_width:
        P = np.asarray(pts, float)
        along = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]) / emb_width
        set_attr(ob, "emb_u", np.repeat(along, 2))
        set_attr(ob, "emb_v", np.tile([0.0, 1.0], len(P)))
    if transfer:
        geo.transfer_weights(ctx.body, ob)
    if thick:
        geo.solidify(ob, thick, 1.0)
    ctx.add(key, ob, surface, collide=False)
    return ob


# ----------------------------------------------------------------------------- the crossing

class Cross:
    """Crossed front opening. over=+1: the wearer's left panel lies on top, its edge running from
    the left side of the neck (x_neck, z_neck) down across the chest to the right of the waist
    (-x_waist, z_waist); the under panel's edge runs from the right side of the neck to the
    crossing point (x_cross on the outer edge), where it goes under the top panel. Coordinates:
    +x is the wearer's left, the fighter faces -y."""

    def __init__(self, x_neck, z_neck, x_waist, z_waist, x_cross, over=1):
        self.xn, self.zn, self.xw, self.zw, self.over = x_neck, z_neck, x_waist, z_waist, over
        # the outer edge is x(z) = over * (xn - (xn + xw) * (zn - z) / (zn - zw)); it crosses x_cross at zc
        self.xc = x_cross
        fc = (x_neck - over * x_cross) / (x_neck + x_waist)
        self.zc = z_neck - fc * (z_neck - z_waist)

    def x_outer(self, z):
        return self.over * (self.xn - (self.xn + self.xw) * (self.zn - z) / (self.zn - self.zw))

    def x_under(self, z):
        f = (self.zn - z) / (self.zn - self.zc)
        return -self.over * self.xn + (self.xc + self.over * self.xn) * f

    def field(self, y_cut):
        """>= 0 outside the V opening (only in front of y_cut and above the crossing)."""
        def f(c):
            x, z = c[:, 0], c[:, 2]
            o = self.over
            inside = np.minimum((x - self.x_under(z)) * o, (self.x_outer(z) - x) * o)
            v = np.maximum(-inside, self.zc - z)
            return np.maximum(v, c[:, 1] - y_cut)
        return f

    def collar_path(self, ctx, coll, width, z_end, z_under_end, ring_z, ring_lift=0.0, lift_cross=0.0, n_edge=16):
        """Band centre line: up the under edge from below the crossing, round the back of the neck,
        down the outer edge to z_end. Each straight part is moved half a band width onto its own
        panel; near the crossing the outer part is lifted over the under part by lift_cross."""
        o, s = self.over, ctx.s
        nb = ctx.head("neck_01")

        def edge_pts(xfun, z0, z1, side, n):
            out = []
            for z in np.linspace(z0, z1, n):
                dz = 1e-3
                tx, tz = xfun(z + dz) - xfun(z - dz), 2 * dz
                L = math.hypot(tx, tz)
                px, pz = tz / L, -tx / L                  # perpendicular in the front plane
                if px * side < 0:
                    px, pz = -px, -pz
                x, zz = xfun(z) + px * width / 2, z + pz * width / 2
                loc, n = hit_from(coll, (x, -3.0, zz), (0, 1, 0))
                if loc is not None:
                    out.append(loc + n * 0.0015 * s)
            return out

        under = edge_pts(self.x_under, z_under_end, self.zn, -o, n_edge // 2 + 2)
        outer = edge_pts(self.x_outer, self.zn, z_end, o, n_edge)
        # round the back of the neck, between the two top points
        a0 = math.atan2(under[-1][0] - nb[0], -(under[-1][1] - nb[1]))
        a1 = math.atan2(outer[0][0] - nb[0], -(outer[0][1] - nb[1]))
        if o > 0:            # under starts on the right (negative angle), outer on the left
            angs = np.linspace(a0, a1 - 2 * math.pi, 15)[1:-1] if a1 > a0 else np.linspace(a0, a1, 15)[1:-1]
        else:
            angs = np.linspace(a0, a1 + 2 * math.pi, 15)[1:-1] if a1 < a0 else np.linspace(a0, a1, 15)[1:-1]
        ring = []
        for k, a in enumerate(angs):
            back = 0.5 - 0.5 * math.cos(math.pi * (k + 1) / (len(angs) + 1))
            zz = ring_z + ring_lift * math.sin(math.pi * (k + 1) / (len(angs) + 1))
            d = np.array([math.sin(a), -math.cos(a), 0.0])
            loc, n = hit_from(coll, np.array([nb[0], nb[1], zz]) + d * 0.5, -d)
            if loc is not None:
                ring.append(loc + n * 0.0015 * s)
        if lift_cross:
            for i, p in enumerate(outer):
                w = math.exp(-((p[2] - self.zc) / (0.8 * width)) ** 2)
                _, n, _ = coll.nearest(p)
                outer[i] = p + np.array(n) * lift_cross * w
        return under + ring + outer


def hang_front(ctx, ob, z_sash, z_top, width=0.19):
    """Loose cloth on the front of the torso: each column hangs from the most forward point above it
    (no hollow under the chest), drawn back in gradually from the chest to the sash; then a stiff
    cloth envelope bridges the pectorals. It only ever moves cloth
    forward, away from the body."""
    s = ctx.s
    idx = geo.source_index(ob)
    src = src_coords(ctx, ob)
    c = geo.co(ob)
    yc = ctx.head("spine_03")[1]
    on_arm = np.zeros(len(src), bool)          # the sleeves, by distance from the arm axis (the
    for side in ("l", "r"):                    # pectorals carry upper-arm weight, so not by weight)
        sh, el = ctx.head("upperarm_" + side), ctx.head("lowerarm_" + side)
        tu, ru, _ = geo.axis_t(src, sh, el)
        on_arm |= side_mask_pts(src, side) & (ru < 0.08 * s) & (tu > -0.05)
    F = np.nonzero((src[:, 1] < yc - 0.02 * s) & ~on_arm & (np.abs(src[:, 0]) < width * s)
                   & (c[:, 2] > z_sash) & (c[:, 2] < z_top))[0]
    if len(F) < 10:
        return
    P = c[F]
    y = P[:, 1].copy()
    dx = np.abs(P[:, None, 0] - P[None, :, 0]) < 0.012 * s
    above = P[None, :, 2] >= P[:, None, 2]
    hang = np.where(dx & above, P[None, :, 1], np.inf).min(1)
    z_chest = 0.70 * ctx.H                                                 # drawn in gradually to the sash
    back = np.clip((z_chest - P[:, 2]) / (z_chest - z_sash), 0, 1) ** 0.7
    edge = smoothstep((width * s - np.abs(P[:, 0])) / (0.05 * s)) * smoothstep((z_top - P[:, 2]) / (0.04 * s))
    y1 = np.minimum(y, hang + (y - hang) * back)
    # stiff cloth: it cannot bend tighter than radius R, so it bridges from one pectoral to the
    # other and spreads each peak into a broad, gentle dome (a parabolic lower envelope in x, z)
    R = 0.3 * s
    d2 = (P[:, None, 0] - P[None, :, 0]) ** 2 + (P[:, None, 2] - P[None, :, 2]) ** 2
    y2 = (y1[None, :] + d2 / (2 * R)).min(1)
    c[F, 1] = y + (y2 - y) * edge
    core.log("hang_front %s: %d vertices, moved forward up to %.1f mm" % (ob.name, len(F), 1000 * float(np.max(y - c[F, 1]))))
    geo.set_co(ob, c)


def chest_smooth(ctx, ob, iters, amount=1.0):
    """Laplacian smoothing of a shell over the chest and belly only (MakeHuman's pectoral and nipple
    detail shows through a cloth layer otherwise); the arms and the open edges stay put."""
    idx = geo.source_index(ob)
    src = src_coords(ctx, ob)
    s = ctx.s
    z0, z1 = 0.58 * ctx.H, ctx.head("neck_01")[2] - 0.035 * s
    band_z = smoothstep((src[:, 2] - z0) / (0.04 * s)) * smoothstep((z1 - src[:, 2]) / (0.04 * s))
    f = 0.5 * amount * band_z * np.clip(1 - ctx.arm_w[idx] * 8, 0, 1) * (ctx.w("head")[idx] < 0.05)
    bv, _ = geo.boundary_verts(ob)
    m = (f > 0.01) & ~bv
    c = geo.co(ob)
    nb = geo.neighbours(ob)
    for _ in range(iters):
        avg = np.array([c[n].mean(0) if len(n) else c[i] for i, n in enumerate(nb)])
        c[m] += (avg[m] - c[m]) * f[m, None]
    geo.set_co(ob, c)


def flatten_chest(ctx, z0, z1, iters=40):
    """Smooth away the pectoral and nipple relief of the body between z0 and z1 at the front,
    before any shell is made from it. Those body faces end up under the robes and are deleted, so
    only the garments see the change (offset shells keep the body's shape otherwise)."""
    s, V = ctx.s, ctx.V
    m = (V[:, 2] > z0) & (V[:, 2] < z1) & (V[:, 1] < ctx.head("spine_03")[1]) & (np.abs(V[:, 0]) < 0.135 * s)
    w = (smoothstep((V[:, 2] - z0) / (0.03 * s)) * smoothstep((z1 - V[:, 2]) / (0.012 * s)) * m
         * smoothstep((0.135 * s - np.abs(V[:, 0])) / (0.025 * s)))     # (the pectorals carry arm weight)
    nb = geo.neighbours(ctx.body)
    c = V.copy()
    for _ in range(iters):
        avg = np.array([c[n].mean(0) if len(n) else c[i] for i, n in enumerate(nb)])
        c += (avg - c) * (0.5 * w)[:, None]
    core.log("flatten_chest: %d vertices, z %.3f..%.3f, largest move %.1f mm" % (
        int((w > 0.3).sum()), z0, z1, 1000 * float(np.linalg.norm(c - V, axis=1).max())))
    geo.set_co(ctx.body, c)
    ctx.V = c
    ctx.N = geo.vnormals(ctx.body)


def crossed_top(ctx, key, surface, z_bottom, sleeve_t, offset, cross=None, neck_top=None, sleeves=True,
                flare=0.0, under=0.0, taper=True, folds=0.0, smooth=10, gap=0.007, thickness=0.004,
                collider=None, chest=0, body_ease=0.0, loose_front=False):
    """Torso (+ sleeves) shell from z_bottom to the neck with the crossed V cut out. Sleeves are
    cut across the forearm at sleeve_t; flare widens them from a third of the way down the upper
    arm (tapering back to the cuff when taper), `under` adds hang on the forearm's underside."""
    s, V = ctx.s, ctx.V
    z = V[:, 2]
    nb = ctx.head("neck_01")
    hands = hand_w(ctx, "l") + hand_w(ctx, "r")
    z_neck = neck_top if neck_top is not None else nb[2] + 0.012 * s
    sel = (z >= z_bottom - 0.03 * s) & (ctx.w("head") < 0.45) & (hands < 0.25) & (z <= z_neck + 0.03 * s)
    fields = [wd.f_plane((0, 0, z_bottom), (0, 0, 1)), wd.f_plane((0, 0, z_neck), (0, 0, -1))]
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        t, _, _ = geo.axis_t(V, el, wr)
        if sleeves:
            sel &= ~(side_mask(ctx, side) & (ctx.w("lowerarm_" + side) > 0.2) & (t > sleeve_t + 0.08))
            fields.append(wd.f_limb(el, wr, sleeve_t, "proximal", side, radius=0.16 * s))
        else:
            sh = ctx.head("upperarm_" + side)
            tu, ru, _ = geo.axis_t(V, sh, el)
            sel &= ~(side_mask(ctx, side) & (ctx.w("upperarm_" + side, "lowerarm_" + side) > 0.5) & (ru < 0.07 * s) & (tu > 0.1))
            fields.append(wd.f_limb(sh, el, 0.08, "proximal", side, radius=0.075 * s))
    legs = ctx.w("thigh_l", "thigh_r", "calf_l", "calf_r")
    sel &= ~((legs > 0.3) & (z < z_bottom))
    if cross is not None:
        fields.append(cross.field(ctx.head("spine_03")[1] - 0.02 * s))

    def shape(ob, nrm):
        if body_ease:          # extra room over the torso, none at the neck and on the arms
            idx = geo.source_index(ob)
            src = src_coords(ctx, ob)
            tw = np.clip(1 - ctx.arm_w[idx] * 3, 0, 1) * smoothstep((nb[2] - 0.03 * s - src[:, 2]) / (0.08 * s))
            geo.offset_along(ob, nrm, body_ease * s * tw)
        if chest:
            chest_smooth(ctx, ob, chest)
        if loose_front:     # (below the crossing: the neckline stays close, the collar lies on it)
            hang_front(ctx, ob, z_bottom + 0.02 * s, (cross.zc - 0.01 * s) if cross is not None else nb[2] - 0.05 * s)
        if not (flare or under or folds):
            return
        src = src_coords(ctx, ob)
        aw = ctx.arm_w[geo.source_index(ob)]
        c = geo.co(ob)
        for side in ("l", "r"):
            sh, el, wr = ctx.head("upperarm_" + side), ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
            tu, ru, fu = geo.axis_t(src, sh, el)
            tf, rf, ff = geo.axis_t(src, el, wr)
            fore = tu > 1.0
            T = np.where(fore, 1.0 + tf, tu)
            foot = np.where(fore[:, None], ff, fu)
            radial = src - foot
            radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
            # only round the arm itself: the pectorals carry upper-arm weight but are far off its axis
            m = side_mask_pts(src, side) & (aw > 0.2) & (np.where(fore, rf, ru) < 0.075 * s)
            end = 1.0 + sleeve_t
            prof = smoothstep((T - 0.35) / 0.45)
            if taper:
                prof = prof * (1 - smoothstep((T - (end - 0.32)) / 0.3))
            hang = smoothstep((T - 0.75) / 0.4) * (prof if taper else 1.0) * np.clip(-radial[:, 2], 0, 1) ** 1.3
            amt = s * (flare * prof + under * hang)
            if folds:
                F = wd.frame(wr - el)
                ang = np.arctan2(radial @ F[:, 1], radial @ F[:, 0])
                amt = amt + folds * s * np.sin(6 * ang + 7 * T) * smoothstep((T - 0.3) / 0.3)
            c[m] += radial[m] * amt[m, None]
        geo.set_co(ob, c)

    ob = wd.field_shell(ctx, key, surface, sel, offset * s, wd.f_and(*fields), smooth=smooth, gap=gap,
                        thickness=thickness, extra=shape, collider=collider)
    garment_uv(ob)
    return ob


def garment_uv(ob):
    """Fresh UVs for a solidified garment shell. The body's UVs it inherits are pinched at the
    nipples and stretched over the shoulders (fine for skin, not for a woven fabric). The outer
    layer is smart-projected and the inner layer (Solidify's offset copy: vertex i + n, face k + n)
    copies its UVs, so the atlas packer merges the two and the slot keeps its texel density."""
    import bmesh
    import bpy
    me = ob.data
    n_out = int(ob.get("outer_faces", len(me.polygons)))
    fv = [p.vertices[:] for p in me.polygons]
    nv = max(max(f) for f in fv[:n_out]) + 1
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    me.uv_layers.new(name="UVMap")
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(me)
    bm.faces.ensure_lookup_table()
    for f in bm.faces:
        f.select_set(f.index < n_out)
    bmesh.update_edit_mesh(me)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.01, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    uvl = me.uv_layers.active.data
    uv = np.empty(len(uvl) * 2)
    uvl.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    per_vert = np.zeros((nv, 2))
    for p in me.polygons[:n_out]:
        for li in p.loop_indices:
            per_vert[me.loops[li].vertex_index] = uv[li]
    for k, p in enumerate(me.polygons):
        if k < n_out:
            continue
        src = {}
        if k < 2 * n_out:
            q = me.polygons[k - n_out]
            src = {me.loops[li].vertex_index: uv[li] for li in q.loop_indices}
        for li in p.loop_indices:
            v = me.loops[li].vertex_index
            vo = v - nv if v >= nv else v
            uv[li] = src.get(vo, per_vert[vo])
    uvl.foreach_set("uv", uv.ravel())
    ob.select_set(False)


def girdle(ctx, key, surface, z0, z1, offset, over, gap=0.008, thickness=0.006):
    """fighter_outfits2.band that also clears hanging panels (wrap_skirt parts are not colliders)."""
    V = ctx.V
    sel = (ctx.arm_w < 0.01) & (V[:, 2] >= z0 - 0.02 * ctx.s) & (V[:, 2] <= z1 + 0.02 * ctx.s)
    f = wd.f_and(wd.f_plane((0, 0, z0), (0, 0, 1)), wd.f_plane((0, 0, z1), (0, 0, -1)))
    coll = geo.Collider(ctx.colliders + list(over), outer_only=True)
    return wd.field_shell(ctx, key, surface, sel, offset * ctx.s, f, smooth=6, gap=gap, thickness=thickness, hide=False,
                          collider=coll)


def ring_emb(ob, a, b, t0, t1, width):
    """emb_u / emb_v for a band round a limb axis a->b between parameters t0..t1."""
    c = geo.co(ob)
    t, r, foot = geo.axis_t(c, a, b)
    F = wd.frame(np.asarray(b) - np.asarray(a))
    rad = c - foot
    ang = np.arctan2(rad @ F[:, 1], rad @ F[:, 0])
    R = float(np.median(r))
    set_attr(ob, "emb_u", (ang + math.pi) * R / width)
    set_attr(ob, "emb_v", np.clip((t - t0) / (t1 - t0), -0.5, 1.5))
    ob["emb_period"] = 2 * math.pi * R / width


def panel_uv(ob, y_c, arc):
    """Rewrite a wrap_skirt panel's UVs (arc length, height) with the angle unwrapped from a little
    before the arc start: vertices exactly on the start otherwise wrap a full turn at random and
    stretch slivers of texture across the panel."""
    me = ob.data
    c = geo.co(ob)
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    p = c[lv]
    phi = np.mod(np.arctan2(p[:, 0], -(p[:, 1] - y_c)) - arc[0] + 0.3, 2 * math.pi) + arc[0] - 0.3
    rm = float(np.mean(np.hypot(c[:, 0], c[:, 1] - y_c)))
    me.uv_layers.active.data.foreach_set("uv", np.stack([phi * rm, p[:, 2]], 1).ravel())


def skirt_emb(ob, y_c, z_hem, width, arc, edge_phi=None, edge_dir=1):
    """emb coordinates on a hanging panel: the hem band (and the band up its free front edge).
    The angle is unwrapped from the panel's arc start, so the back panel has no seam."""
    c = geo.co(ob)
    phi = np.mod(np.arctan2(c[:, 0], -(c[:, 1] - y_c)) - arc[0] + 0.3, 2 * math.pi) + arc[0] - 0.3
    r = np.hypot(c[:, 0], c[:, 1] - y_c)
    d_hem = c[:, 2] - z_hem
    u_hem = phi * r / width
    if edge_phi is None:
        u, v = u_hem, d_hem / width
    else:
        dphi = np.mod((phi - edge_phi) * edge_dir + math.pi, 2 * math.pi) - math.pi
        d_edge = dphi * r
        use_edge = d_edge < d_hem
        u = np.where(use_edge, -c[:, 2] / width, u_hem)
        v = np.where(use_edge, d_edge, d_hem) / width
    set_attr(ob, "emb_u", u)
    set_attr(ob, "emb_v", v)


def slim_boot(ctx, side, z_top, sole, wall=0.0065, toe=0.004, n=28, thick=0.0035):
    """fighter_outfits.loft_boot with a close-fitting wall: a shoe or low boot lofted through convex
    sections of the foot and ankle, plus its sole slab."""
    s, V = ctx.s, ctx.V
    sfx = "_" + side
    lw = ctx.w("calf" + sfx, "foot" + sfx, "ball" + sfx)
    vm = (lw >= 0.3) & side_mask(ctx, side) & (V[:, 2] <= z_top + 0.03 * s)
    P = V[vm]
    E = np.array([e.vertices[:] for e in ctx.body.data.edges])
    E = E[vm[E[:, 0]] | vm[E[:, 1]]]
    za, zb_ = V[E[:, 0], 2], V[E[:, 1], 2]
    nz = 20
    zs = [0.004 + (z_top - 0.004) * (i / (nz - 1)) ** 1.3 for i in range(nz)]
    y_toe = P[:, 1].min()
    foot_len = P[:, 1].max() - y_toe
    rings, prev = [], None
    for zz in zs:
        cr = (za - zz) * (zb_ - zz) <= 0
        f = (zz - za[cr]) / np.where(np.abs(zb_[cr] - za[cr]) < 1e-12, 1e-12, zb_[cr] - za[cr])
        pts = (V[E[cr, 0]] + (V[E[cr, 1]] - V[E[cr, 0]]) * f[:, None])[:, :2]
        if len(pts) < 4:
            ring = prev
        else:
            ring, c = geo.ring_by_angle(convex_hull(pts), n)
            off = wall * s + toe * s * smoothstep(1.0 - (ring[:, 1] - y_toe) / (0.3 * foot_len)) * (zz < 0.07 * s)
            v = ring - c
            ring = c + v * (1 + off[:, None] / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))
        rings.append((ring, zz))
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
        uv.data[li].uv = ((vi % n) / n * per, cb[vi, 2])
    geo.transfer_weights(ctx.body, boot)
    geo.solidify(boot, thick * s)
    geo.bind(boot, ctx.rig)
    base = rings[0][0]
    c = base.mean(0)
    v = base - c
    wide = c + v * (1 + (0.004 * s) / (np.linalg.norm(v, axis=1)[:, None] + 1e-9))
    vs = [(p[0], p[1], 0.008 * s) for p in wide] + [(p[0], p[1], -sole) for p in wide]
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


def shoes(ctx, key, surface, sole_surface, z_top, sole, **kw):
    for side in ("l", "r"):
        boot, so = slim_boot(ctx, side, z_top, sole, **kw)
        ctx.add("%s_%s" % (key, side), boot, surface)
        ctx.add("sole_" + side, so, sole_surface, collide=False)
        lw = ctx.w("calf_" + side, "foot_" + side, "ball_" + side)
        ctx.hide_faces(geo.face_mask(ctx.FV, (ctx.V[:, 2] <= z_top - 0.012 * ctx.s) & (lw >= 0.5) & side_mask(ctx, side)), 1)


def laces(ctx, key, surface, boot_key, z_lo, z_hi, n_cross=5, half=0.011):
    """Crossed laces up the front of a shoe: points on the instep and the front of the ankle
    (rays from in front and above), eyelet pairs across the foot, X crossings between them."""
    s = ctx.s
    for side in ("l", "r"):
        coll = geo.Collider([ctx.parts["%s_%s" % (boot_key, side)]], outer_only=True)
        ft, bl = ctx.head("foot_" + side), ctx.head("ball_" + side)
        fwd = bl - ft
        fwd[2] = 0
        fwd /= np.linalg.norm(fwd)
        across = np.cross(fwd, (0, 0, 1.0))
        rows = []
        for k in range(n_cross + 1):
            f = k / n_cross
            zz = z_hi + (z_lo - z_hi) * f
            base = ft + fwd * (0.01 + 0.075 * f) * s
            pr = []
            for sgn in (-1, 1):
                o = np.array([base[0], base[1], zz]) + across * sgn * half * s + fwd * 0.3 + np.array([0, 0, 0.12])
                d = -(fwd * 0.3 + np.array([0, 0, 0.12]))
                loc, n = hit_from(coll, o, d / np.linalg.norm(d))
                pr.append(None if loc is None else loc + n * 0.0018 * s)
            if pr[0] is not None and pr[1] is not None:
                rows.append(pr)
        lv, lf = [], []
        for i in range(len(rows) - 1):
            for a, b in ((rows[i][0], rows[i + 1][1]), (rows[i][1], rows[i + 1][0])):
                v, f = geo.tube([a, (a + b) / 2, b], 0.0017 * s, 5)
                lf += [tuple(j + len(lv) for j in ff) for ff in f]
                lv += v
        for a, b in rows[-1:] + rows[:1]:                         # straight bars top and bottom
            v, f = geo.tube([a, b], 0.0017 * s, 5)
            lf += [tuple(j + len(lv) for j in ff) for ff in f]
            lv += v
        ob = geo.new_mesh_object("%s_%s" % (key, side), lv, lf, ctx.rig)
        geo.transfer_weights(ctx.body, ob)
        ctx.add("%s_%s" % (key, side), ob, surface, collide=False)


def hanging_tails(ctx, key, surface, start, ends, width, thick, coll, side_bones, hang_from=None, gap=0.012,
                  n=12, fringe=None, emb=False, share=0.5, flare=0.0):
    """Cloth tails from `start` down to each end point, lying on the surface (rays from the front
    or back) while it is there and hanging straight below the most outward point above.
    Weights: the body's at the top, blending to pelvis plus half of that side's thigh."""
    s = ctx.s
    obs = []
    for k, (end, bone) in enumerate(zip(ends, side_bones)):
        start_k = np.asarray(start[k] if np.ndim(start) == 2 else start, float)
        end = np.asarray(end, float)
        ydir = 1.0 if hang_from == "back" else -1.0
        pts, extreme = [], None
        for f in np.linspace(0, 1, n):
            p = start_k + (end - start_k) * f
            loc, nn = hit_from(coll, (p[0], p[1] + ydir * 3.0, p[2]), (0, -ydir, 0))
            y = p[1] if loc is None else loc[1] + ydir * gap * s
            extreme = y if extreme is None else (max(extreme, y) if ydir > 0 else min(extreme, y))
            pts.append(np.array([p[0], extreme, p[2]]))
        sv, sf = fo.ribbon(pts, width * s, coll)
        if flare:
            sv = np.array(sv)
            for i in range(len(pts)):
                f = 1 + flare * (i / (len(pts) - 1)) ** 1.5
                mid = (sv[2 * i] + sv[2 * i + 1]) / 2
                sv[2 * i], sv[2 * i + 1] = mid + (sv[2 * i] - mid) * f, mid + (sv[2 * i + 1] - mid) * f
        ob = geo.new_mesh_object("%s%d" % (key, k), sv, sf, ctx.rig)
        if emb:
            Pp = np.asarray(pts, float)
            along = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(Pp, axis=0), axis=1))]) / (width * s)
            set_attr(ob, "emb_u", np.repeat(along, 2))
            set_attr(ob, "emb_v", np.tile([0.0, 1.0], len(Pp)))
        if fringe:
            depth, teeth = fringe
            zend = end[2]

            def ragged(c, zend=zend):
                u = (c[:, 0] - end[0]) / (width * s) * teeth
                saw = np.abs((u % 1.0) - 0.5) * 2
                return c[:, 2] - (zend + depth * s * saw)
            geo.cut_field(ob, ragged)
        c = geo.co(ob)
        geo.transfer_weights(ctx.body, ob)
        W = geo.weights(ob)
        fdown = np.clip((start_k[2] - c[:, 2]) / max(start_k[2] - end[2], 1e-6), 0, 1)
        sb = smoothstep(fdown / 0.35)
        zero = np.zeros(len(c))
        newW = {b: w * (1 - sb) for b, w in W.items() if not b.startswith(("thigh", "calf"))}
        newW["pelvis"] = newW.get("pelvis", zero) + sb * (1 - share * fdown)
        newW[bone] = W.get(bone, zero) * (1 - sb) + sb * share * fdown
        geo.set_weights(ob, newW)
        geo.solidify(ob, thick * s, 1.0)
        ctx.add("%s%d" % (key, k), ob, surface, collide=False)
        obs.append(ob)
    return obs


# ============================================================================ tight braided hair

def hairline_z(ctx, hf):
    """Height of a pulled-back hairline round the head (angle from the front, both sides)."""
    s = ctx.s
    brows = ctx.parts.get("brows")
    zb = float(geo.co(brows)[:, 2].mean()) if brows is not None else hf["eye_z"] + 0.03 * s
    ez, hc = hf["eye_z"], hf["c"]
    deg = [0, 25, 45, 60, 75, 90, 120, 150, 180]           # (the ears are cut out separately)
    zl = [zb + 0.060 * s, zb + 0.056 * s, zb + 0.038 * s, ez + 0.010 * s, ez - 0.018 * s, ez - 0.03 * s,
          ez - 0.06 * s, ez - 0.08 * s, ez - 0.088 * s]

    def f(c):
        a = np.degrees(np.abs(np.arctan2(c[:, 0] - hc[0], -(c[:, 1] - hc[1]))))
        return np.interp(a, deg, zl)
    return f


def braided_hair(ctx, hf, width=0.026, hang=0.19, cap_off=0.0035):
    """Tight hair: a cap of pulled-back strands over the scalp (alpha-feathered hairline, the ears
    clear) and two Dutch braids that run from the front hairline over the head, part at the nape and
    hang to the shoulder blades. Each braid is a chain of overlapping ellipsoid lobes, alternating
    sides and angled like a three-strand plait; all lobes share one UV rectangle (fighter_tex draws
    the strands). The scalp part is rigid on the head, the hanging part blends to the neck and
    upper back."""
    s, V = ctx.s, ctx.V
    hc, ez, half = hf["c"], hf["eye_z"], hf["half"]
    nb = ctx.head("neck_01")
    hl = hairline_z(ctx, hf)
    ear_c = np.array([half[0], hc[1] + 0.012 * s, ez - 0.016 * s])

    def ear_field(c):                      # an oval just round the root of each ear
        dy = (c[:, 1] - ear_c[1]) / (0.024 * s)
        dz = (c[:, 2] - ear_c[2]) / (0.035 * s)
        side = np.abs(c[:, 0] - hc[0]) > 0.6 * half[0]
        return np.where(side, (np.hypot(dy, dz) - 1.0) * 0.024 * s, 1.0)

    def cap_field(c):
        return np.minimum(c[:, 2] - hl(c), ear_field(c))

    head = ctx.w("head")
    ear_v = ((np.abs(V[:, 0] - hc[0]) > 0.6 * half[0])
             & (np.hypot((V[:, 1] - ear_c[1]) / (0.024 * s), (V[:, 2] - ear_c[2]) / (0.035 * s)) < 1.15))
    sel = (head > 0.5) & (cap_field(V) > -0.025 * s) & ~ear_v
    body_only = geo.Collider([ctx.body])
    cap = wd.field_shell(ctx, "hair_cap", "hair_cap", sel, cap_off * s, cap_field, smooth=6, gap=cap_off * 0.8,
                         thickness=0.0, collider=body_only, hide_margin=0.008)
    me = cap.data
    c = geo.co(cap)
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    p = c[lv] - hc
    # both halves share one mirrored UV rectangle: u = angle round the front-back axis (0 on top, 1
    # underneath), v = angle from the front; strands run along v, front to back
    u = np.arctan2(np.abs(p[:, 0]), p[:, 2]) / math.pi
    v = np.arccos(np.clip(-p[:, 1] / (np.linalg.norm(p, axis=1) + 1e-9), -1, 1)) / math.pi
    u0, u1, v0, v1 = 0.0, float(u.max()), float(v.min()), float(v.max())
    uv = np.stack([(u - u0) / (u1 - u0), (v - v0) / (v1 - v0)], 1)
    me.uv_layers.active.data.foreach_set("uv", np.clip(uv, 0, 1).ravel())
    R = float(np.mean(np.linalg.norm(c - hc, axis=1)))
    cap["uv_span_m"] = [(u1 - u0) * math.pi * R, (v1 - v0) * math.pi * R]
    # ---- two braids
    lay = geo.Collider([ctx.body] + [ctx.parts[k] for k in ("jacket", "lapel", "rashguard", "hair_cap") if k in ctx.parts],
                       outer_only=True)
    z_nape = float(hl(np.array([[hc[0], hc[1] + 1.0, 0.0]]))[0])
    verts, faces_, uvs, tparam = [], [], [], []
    for sgn in (-1, 1):
        P, N = [], []
        for thd in np.linspace(5, 260, 200):
            t = math.radians(thd)
            xo = sgn * (0.026 + 0.016 * smoothstep((thd - 140) / 90)) * s
            d = np.array([0.0, -math.cos(t), math.sin(t)])
            loc, n = hit_from(body_only, np.array([hc[0] + xo, hc[1], hc[2]]) + d * 0.5, -d)
            if loc is None:
                continue
            if thd < 90 and loc[2] < hl(loc[None])[0] + 0.004 * s:
                continue
            if thd > 150 and loc[2] < z_nape:
                break
            P.append(loc + n * (cap_off * s + 0.20 * width * s))
            N.append(n)
        n_scalp = len(P)
        top = P[-1].copy()
        ext = top[1]
        for f in np.linspace(0, 1, 14)[1:]:
            q = top + np.array([sgn * 0.035 * s * f, 0.0, -hang * s * f])
            loc, n = hit_from(lay, (q[0], q[1] + 3.0, q[2]), (0, -1, 0))
            y = q[1] if loc is None else loc[1] + (0.45 * width + 0.006) * s
            ext = max(ext, y)
            P.append(np.array([q[0], ext, q[2]]))
            N.append(np.array([0.0, 1.0, 0.0]))
        P, N = np.array(P), np.array(N)
        seg = np.linalg.norm(np.diff(P, axis=0), axis=1)
        cum = np.concatenate([[0.0], np.cumsum(seg)])
        t_nape = cum[n_scalp - 1]
        total = cum[-1]
        sp = 0.40 * width * s
        for i in range(int(total / sp)):
            t = (i + 0.5) * sp
            k = min(np.searchsorted(cum, t, side="right") - 1, len(P) - 2)
            f = (t - cum[k]) / max(seg[k], 1e-9)
            pc = P[k] + (P[k + 1] - P[k]) * f
            T = P[k + 1] - P[k]
            T /= np.linalg.norm(T) + 1e-12
            Nn = N[k] + (N[k + 1] - N[k]) * f
            B = np.cross(T, Nn)
            B /= np.linalg.norm(B) + 1e-12
            Nn = np.cross(B, T)
            w = width * s * (1 - 0.35 * smoothstep((t - t_nape) / max(total - t_nape, 1e-6)))
            side = 1 if i % 2 else -1
            ang = math.radians(34)
            ax = T * math.cos(ang) - B * side * math.sin(ang)
            e1 = np.cross(Nn, ax)
            e1 /= np.linalg.norm(e1) + 1e-12
            cen = pc + B * side * 0.15 * w
            a_l, a_w, a_t = 0.66 * w, 0.40 * w, 0.30 * w
            nr, ns = 5, 7
            base_i = len(verts)
            for j in range(nr + 1):
                ph = math.pi * j / nr
                for kk in range(ns + 1):
                    ps = 2 * math.pi * kk / ns
                    q = cen + ax * math.cos(ph) * a_l + (e1 * math.cos(ps) * a_w + Nn * math.sin(ps) * a_t) * math.sin(ph)
                    verts.append(tuple(q))
                    uvs.append((kk / ns, 1 - j / nr))
                    tparam.append(t - t_nape)
            for j in range(nr):
                for kk in range(ns):
                    a = base_i + j * (ns + 1) + kk
                    faces_.append((a, a + 1, a + ns + 2, a + ns + 1))
    ob = geo.new_mesh_object("braids", verts, faces_, None, ctx.body)
    me = ob.data
    uvl = me.uv_layers.new(name="UVMap")
    lvb = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lvb)
    uvl.data.foreach_set("uv", np.asarray(uvs)[lvb].ravel())
    geo.recalc_normals(ob)
    ob["uv_span_m"] = [math.pi * 0.7 * width * s, 1.32 * width * s]
    geo.transfer_weights(ctx.body, ob)
    W = geo.weights(ob)
    tp = np.asarray(tparam)
    b = smoothstep(tp / (0.07 * s))
    zero = np.zeros(len(tp))
    newW = {k: w * b for k, w in W.items()}
    newW["head"] = newW.get("head", zero) + (1 - b)
    geo.set_weights(ob, newW)
    geo.bind(ob, ctx.rig)
    ctx.add("braids", ob, "braids", collide=False)


# ============================================================================ Wren

def mma_gloves(ctx, surface, strap_surface):
    """Open-finger grappling gloves: palm, back of the hand and the first finger segments, a
    padded knuckle bar over the back of the hand, and a wrist strap."""
    s, V = ctx.s, ctx.V
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        tf, _, _ = geo.axis_t(V, el, wr)
        k1 = ctx.w(*["%s_01_%s" % (f, side) for f in ("index", "middle", "ring", "pinky")])
        far = ctx.w(*["%s_%s_%s" % (f, n, side) for f in ("index", "middle", "ring", "pinky") for n in ("02", "03")]
                    + ["thumb_02_" + side, "thumb_03_" + side])
        cuff = (ctx.w("lowerarm_" + side) >= 0.2) & (tf >= 0.80)
        sel = (((ctx.w("hand_" + side, "thumb_01_" + side) + k1) >= 0.5) | cuff) & (far < 0.06) & (k1 < 0.8)
        sel &= side_mask(ctx, side)
        centre, across, fwd, pn = palm_frame(ctx, side)
        kn = np.array([ctx.head("%s_01_%s" % (f, side)) for f in ("index", "middle", "ring", "pinky")])

        def pad(ob, nrm, pn=pn, kn=kn, fwd=fwd):
            src = src_coords(ctx, ob)
            dorsal = np.clip(-(nrm @ pn), 0, 1)
            # distance to the knuckle line (index..pinky heads), along the hand
            a, b = kn[0], kn[-1]
            t, r, foot = geo.axis_t(src, a, b)
            near = np.exp(-(((src - foot) @ fwd + 0.004 * s) / (0.026 * s)) ** 2) * smoothstep((t + 0.25) / 0.2) * smoothstep((1.25 - t) / 0.2)
            geo.offset_along(ob, nrm, s * (0.0035 + 0.016 * near * dorsal ** 1.2))

        kc = kn.mean(0)
        th2, th3 = ctx.head("thumb_02_" + side), ctx.head("thumb_03_" + side)
        tdir = (th3 - th2) / np.linalg.norm(th3 - th2)

        def cut(c, kc=kc, fwd=fwd, th2=th2, tdir=tdir):
            f = 0.017 * s - (c - kc) @ fwd                       # across the first finger segments
            t, r, _ = geo.axis_t(c, th2, th2 + tdir)
            thumb = np.where(r < 0.03 * s, -(c - th2) @ tdir + 0.004 * s, 1.0)
            return np.minimum(np.minimum(f, thumb), wd.f_limb(el, wr, 0.84, "distal", side, radius=0.2 * s)(c))

        sel = (((ctx.w("hand_" + side, "thumb_01_" + side, "thumb_02_" + side) + k1) >= 0.4) | cuff) & side_mask(ctx, side)
        sel &= ctx.w(*["%s_03_%s" % (f, side) for f in ("index", "middle", "ring", "pinky", "thumb")]) < 0.05
        wd.field_shell(ctx, "glove_" + side, surface, sel, 0.0028 * s, cut, smooth=3, gap=0.002,
                       thickness=0.0022, collider=geo.Collider([ctx.body]), extra=pad)
        am = (ctx.w("lowerarm_" + side, "hand_" + side) > 0.3) & side_mask(ctx, side)
        wd.ring_band(ctx, "glovestrap_" + side, strap_surface, am, el, wr, [0.86, 0.92, 0.985], [0.0075, 0.009, 0.0075])


def knee_panels(ctx, ob, rx=0.055, rz=0.072, pad=0.0035):
    """Reinforced knees on the spats: an oval panel over each kneecap, painted into the spats'
    texture (fighter_tex embroider 'panel': darker, smoother, a stitched seam) through emb_u/emb_v,
    and padded outwards a little (both layers move out from the leg axis)."""
    s = ctx.s
    c = geo.co(ob)
    ev = np.full(len(c), 3.0)
    eu = np.zeros(len(c))
    for side in ("l", "r"):
        kn, an, hip = ctx.head("calf_" + side), ctx.head("foot_" + side), ctx.head("thigh_" + side)
        m = side_mask_pts(c, side) & (c[:, 1] < kn[1])
        dx = (c[:, 0] - kn[0]) / (rx * s)
        dz = (c[:, 2] - (kn[2] - 0.01 * s)) / (rz * s)
        r = np.hypot(dx, dz)
        ev = np.where(m, r, ev)
        eu = np.where(m, np.arctan2(dz, dx) * rz * s / (0.02 * s), eu)
        t, _, foot = geo.axis_t(c, hip, an)
        radial = c - foot
        radial /= np.linalg.norm(radial, axis=1)[:, None] + 1e-9
        c[m] += radial[m] * (pad * s * smoothstep((1.0 - r[m]) / 0.3))[:, None]
    geo.set_co(ob, c)
    set_attr(ob, "emb_u", eu)
    set_attr(ob, "emb_v", ev)


def wren(ctx):
    """Elite mixed martial artist: fitted spats with reinforced knees, laced wrestling shoes, a dark
    rash guard under a cropped heavy-weave judo-style jacket (crossed lapels, sleeves rolled to
    the forearm, hem at the hip), a faded red belt knotted at the front, open-finger gloves, and a
    small teal glass bead on the belt."""
    H, s, V = ctx.H, ctx.s, ctx.V
    nb = ctx.head("neck_01")
    sole = ctx.cfg.get("sole_m", 0.012)
    # ---- spats, knees, shoes
    leg_shell(ctx, "trousers", "trousers", 0.60 * H, 0.052 * H, 0.0045, folds=0.0012, gather=0.002, smooth=14,
              medial=0.7, gap=0.0035, thickness=0.003)
    knee_panels(ctx, ctx.parts["trousers"])
    z_shoe = 0.082 * H
    shoes(ctx, "shoe", "shoes", "sole", z_shoe, sole)
    laces(ctx, "laces", "laces", "shoe", 0.040 * H, z_shoe - 0.008 * s)
    # ---- rash guard under everything above the waist
    crossed_top(ctx, "rashguard", "rashguard", 0.53 * H, 0.22, 0.0035, neck_top=nb[2] + 0.022 * s,
                smooth=12, gap=0.003, thickness=0.0025, chest=6)
    # ---- jacket: crossed lapels, wide sleeves rolled to mid-forearm
    R = neck_radius(ctx)
    cross = Cross(R + 0.008 * s, nb[2] + 0.004 * s, 0.075 * s, 0.585 * H, -0.012 * s, over=1)
    crossed_top(ctx, "jacket", "jacket", 0.545 * H, 0.36, 0.013, cross=cross, flare=0.016, under=0.008,
                taper=False, folds=0.003, smooth=12, gap=0.009, thickness=0.004, chest=12,
                collider=geo.Collider(ctx.colliders, outer_only=True))
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        am = (ctx.w("lowerarm_" + side) > 0.3) & side_mask(ctx, side)
        wd.ring_band(ctx, "cuff_" + side, "lapel", am, el, wr, [0.25, 0.30, 0.35, 0.40],
                     [0.030, 0.037, 0.038, 0.032])
    # skirt below the belt: two wrap-over panels, the left (outer) one ending right of centre
    yc = float(ctx.head("pelvis")[1])
    for k, (arc, gap) in enumerate((((-math.pi - 0.15, 0.30), 0.011), ((-0.55, math.pi + 0.15), 0.018))):
        # the thigh share ramps in from the skirt top (not the hip joint): a hem this short would
        # otherwise stay on the pelvis while a raised thigh passes through it
        ob, _, _, _ = wd.wrap_skirt(ctx, "jacket_skirt%d" % k, "jacket", 0.592 * H, 0.462 * H, gap=gap, flare=0.012,
                                    folds=(0.003, 7), share=(0.85, 0.55, 0.2), thickness=0.004, row=0.022, arc=arc,
                                    nu=30, y_c=yc, hips_z=0.59 * H)
        panel_uv(ob, yc, arc)
    # lapels: a thick band along the crossing and round the back of the neck
    coll = geo.Collider(ctx.colliders + [ctx.parts[k] for k in ("jacket_skirt0", "jacket_skirt1")], outer_only=True)
    W = 0.046 * s
    path = cross.collar_path(ctx, coll, W, 0.575 * H, cross.zc - 0.9 * W, nb[2] + 0.004 * s,
                             ring_lift=0.012 * s, lift_cross=0.011 * s)
    ribbon_part(ctx, "lapel", "lapel", path, W, 0.008 * s, coll)
    # ---- belt, knot and tails at the front, a teal glass bead on the left tail
    zb0, zb1 = 0.562 * H, 0.594 * H
    belt = girdle(ctx, "belt", "belt", zb0, zb1, 0.034, [ctx.parts["jacket_skirt0"], ctx.parts["jacket_skirt1"]],
                  gap=0.010, thickness=0.007)
    cb = geo.co(belt)
    ang = np.arctan2(cb[:, 0], -(cb[:, 1] - yc))
    Rb = float(np.median(np.hypot(cb[:, 0], cb[:, 1] - yc)))
    set_attr(belt, "emb_u", (ang + math.pi) * Rb / (zb1 - zb0))
    set_attr(belt, "emb_v", (cb[:, 2] - zb0) / (zb1 - zb0))
    belt["emb_period"] = 2 * math.pi * Rb / (zb1 - zb0)
    coll = ctx.collider([ctx.parts[k] for k in ("jacket_skirt0", "jacket_skirt1", "lapel", "belt")])
    zk = 0.5 * (zb0 + zb1)
    loc, n = hit_from(coll, (0.0, -3.0, zk), (0, 1, 0))
    kc = loc + np.array([0, -0.011 * s, 0])
    wd.lathe_part(ctx, "belt_knot", "belt", [(0, -0.012 * s), (0.016 * s, -0.011 * s), (0.024 * s, -0.004 * s),
                                             (0.025 * s, 0.003 * s), (0.017 * s, 0.01 * s), (0, 0.012 * s)],
                  kc, (0, -1, 0), 10, rigid="pelvis")
    knot = ctx.parts["belt_knot"]
    ck = geo.co(knot)
    ck[:, 2] = kc[2] + (ck[:, 2] - kc[2]) * 0.72          # a flattened square knot
    geo.set_co(knot, ck)
    tail_coll = ctx.collider([ctx.parts[k] for k in ("jacket_skirt0", "jacket_skirt1", "belt")])
    starts = [kc + np.array([-0.012 * s, -0.004 * s, -0.012 * s]), kc + np.array([0.012 * s, -0.006 * s, -0.012 * s])]
    ends = [kc + np.array([-0.05 * s, 0.0, -0.24 * s]), kc + np.array([0.045 * s, 0.0, -0.205 * s])]
    tails = hanging_tails(ctx, "belt_tail", "belt", starts, ends, 0.040, 0.006, tail_coll, ("thigh_r", "thigh_l"),
                          gap=0.006, emb=True, share=0.85)
    # bead: a little teal glass sphere tied near the end of the left tail
    tl = tails[1]
    ct = geo.co(tl)
    low = ct[np.argsort(ct[:, 2])[:6]].mean(0) + np.array([0.0, -0.008 * s, 0.03 * s])
    prof = [(0.0, -0.0075 * s)] + [(0.0075 * s * math.sin(a), -0.0075 * s * math.cos(a)) for a in np.linspace(0.3, math.pi - 0.3, 6)] + [(0.0, 0.0075 * s)]
    bead = wd.lathe_part(ctx, "bead", "bead", prof, low, (0, 0, 1), 10)
    geo.transfer_weights(tl, bead)
    # ---- gloves
    mma_gloves(ctx, "gloves", "strap")
    braided_hair(ctx, wd.head_frame(ctx))


# ============================================================================ The Grandmaster

def goatee(ctx, hf):
    """A neat grey-white goatee and a thin moustache as two fur shells (fighter_tex draws the
    strands, as for Bram's mutton chops). The outline is set from the face's profile: the
    moustache covers the skin between the nose and the upper lip and runs thinly down past the
    mouth corners; the goatee starts under the lower lip and covers the chin and the front of its
    underside."""
    s, V = ctx.s, ctx.V
    hc, ez = hf["c"], hf["eye_z"]
    r0 = 0.08 * s
    head = ctx.w("head") > 0.5
    # profile down the middle of the face: (z, y) of the most forward skin at x ~ 0
    mid = head & (np.abs(V[:, 0] - hc[0]) < 0.004 * s) & (V[:, 1] < hc[1]) & (V[:, 2] < ez) & (V[:, 2] > ez - 0.16 * s)
    P = V[mid]
    zs = np.linspace(ez - 0.15 * s, ez - 0.02 * s, 66)
    ys = np.array([P[np.abs(P[:, 2] - z) < 0.003 * s, 1].min() if (np.abs(P[:, 2] - z) < 0.003 * s).any() else np.nan for z in zs])
    ok = ~np.isnan(ys)
    ys = np.interp(zs, zs[ok], ys[ok])
    i_tip = int(np.argmin(ys))                              # nose tip: the most forward point
    below = np.arange(len(zs)) < i_tip
    # walking down from the nose tip: subnasale (back), upper lip (fwd), stomion (back), lower lip (fwd),
    # labiomental fold (back), chin (fwd)
    ext = []
    want_max = True                                         # y max = furthest back
    i = i_tip
    while i > 1 and len(ext) < 6:
        j = i - 1
        while j > 0 and ((ys[j - 1] >= ys[j]) if want_max else (ys[j - 1] <= ys[j])):
            j -= 1
        ext.append(zs[j])
        want_max = not want_max
        i = j
    while len(ext) < 6:
        ext.append(ext[-1] - 0.01 * s)
    z_sn, z_ul, z_st, z_ll, z_lm, z_ch = ext
    core.log("goatee profile (cm below the eyes):", [round((ez - z) * 100, 1) for z in ext])
    dz_top = (z_sn - 0.001 * s - ez) / s                    # moustache: under the nose ...
    dz_lip = (z_ul + 0.0025 * s - ez) / s                   # ... to the top of the upper lip
    dz_ll = (z_ll - ez) / s
    dz_g = (z_ll - 0.004 * s - ez) / s                      # goatee from under the lower lip

    def coords(c):
        A = np.degrees(np.abs(np.arctan2(c[:, 0] - hc[0], -(c[:, 1] - hc[1]))))
        return A, (c[:, 2] - ez) / s

    def outline(c):                       # metres, >= 0 inside
        A, dz = coords(c)
        # thin moustache: full width over the philtrum to the mouth corners, then thin drooping ends
        drop = np.clip((A - 16) / 8, 0, 1)                  # past the mouth corners the ends droop
        top = dz_top - np.minimum(A, 16) / 16 * 0.004 - drop * (dz_top - 0.004 - dz_ll + 0.002)
        bot = np.where(A < 16, dz_lip - np.clip((A - 10) / 6, 0, 1) * 0.003, top - 0.009)
        mous = np.minimum(np.minimum(top - dz, dz - bot) * s, np.radians(25 - A) * r0)
        # goatee: under the lower lip, wider over the chin, the underside of the chin to the throat
        half = np.where(dz > dz_g - 0.02, 9.0 + (dz_g - dz) / 0.02 * 5.0, np.minimum(14.0 + (dz_g - 0.02 - dz) / 0.02 * 6.0, 21.0))
        goat = np.minimum((dz_g - dz) * s, np.radians(half - A) * r0)
        return np.maximum(mous, goat)

    A, dzv = coords(V)
    out_dir = np.stack([V[:, 0] - hc[0], V[:, 1] - hc[1], np.zeros(len(V))], 1)
    out_dir /= np.linalg.norm(out_dir, axis=1)[:, None] + 1e-9
    facing = ((ctx.N * out_dir).sum(1) > 0.15) | ((ctx.N[:, 2] < -0.3) & (dzv < (z_ch - ez) / s + 0.01) & (A < 40))
    sel = head & (outline(V) > -0.01 * s) & facing & (A < 60)
    body_only = geo.Collider([ctx.body])
    zv = V[sel, 2]
    z_lo, z_hi = float(zv.min()) - 0.01 * s, float(zv.max()) + 0.01 * s
    a_max = 60.0
    for key, off, shrink in (("beard", 0.0028, 0.0), ("beard_outer", 0.007, 0.003)):
        ob = wd.field_shell(ctx, key, "beard", sel, off * s, lambda c, k=shrink: outline(c) - k * s, smooth=2,
                            gap=off * 0.6, thickness=0.0, hide=False, collider=body_only, collide=False)
        c = geo.co(ob)
        me = ob.data
        lv = np.empty(len(me.loops), np.int64)
        me.loops.foreach_get("vertex_index", lv)
        p = c[lv]
        a = np.degrees(np.arctan2(p[:, 0] - hc[0], -(p[:, 1] - hc[1])))
        uv = np.stack([(a + a_max) / (2 * a_max), (p[:, 2] - z_lo) / (z_hi - z_lo)], 1)
        me.uv_layers.active.data.foreach_set("uv", np.clip(uv, 0, 1).ravel())
        rad = float(np.mean(np.hypot(c[:, 0] - hc[0], c[:, 1] - hc[1])))
        ob["uv_span_m"] = [np.radians(2 * a_max) * rad, z_hi - z_lo]
        if key == "beard":
            tuft(ctx, ob, hf, z_ch, z_lo, z_hi, a_max)
    return {"z_chin": z_ch}


def tuft(ctx, beard, hf, z_ch, z_lo, z_hi, a_max, length=0.058, radius=0.019):
    """The goatee's point: a short cone of fur hanging from under the chin, its UVs in the chin
    part of the beard tile (so it gets the same strands), rigid on the head; joined into the beard."""
    import bpy
    s = ctx.s
    hc = hf["c"]
    front = ctx.V[(ctx.w("head") > 0.8) & (np.abs(ctx.V[:, 0] - hc[0]) < 0.01 * s) & (np.abs(ctx.V[:, 2] - z_ch) < 0.006 * s)]
    y0 = float(front[:, 1].min()) if len(front) else hc[1] - 0.09 * s
    top = np.array([hc[0], y0 + 0.012 * s, z_ch - 0.004 * s])
    tip = top + np.array([0.0, -0.012 * s, -length * s])
    verts, fcs, uvs = [], [], []
    nr, ns = 6, 8
    for i in range(nr + 1):
        f = i / nr
        p = top + (tip - top) * f
        r = radius * s * (1 - f) ** 0.8 * (1 + 0.3 * math.sin(math.pi * f * 0.8)) + 0.0006 * s
        for k in range(ns + 1):
            a = 2 * math.pi * k / ns
            q = p + np.array([r * math.cos(a), 0.75 * r * math.sin(a), 0.0])
            verts.append(tuple(q))
            uvs.append(q)
    for i in range(nr):
        for k in range(ns):
            a = i * (ns + 1) + k
            fcs.append((a, a + 1, a + ns + 2, a + ns + 1))
    ob = geo.new_mesh_object("tuft", verts, fcs, None, ctx.body)
    geo.recalc_normals(ob)
    me = ob.data
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    P = np.array(uvs)[lv]
    ang = np.degrees(np.arctan2(P[:, 0] - hc[0], -(P[:, 1] - hc[1])))
    zz = z_lo + (P[:, 2] - tip[2]) / (top[2] - tip[2]) * 0.18 * (z_hi - z_lo)
    uv = np.stack([0.5 + ang / (2 * a_max) * 0.6, (zz - z_lo) / (z_hi - z_lo)], 1)
    me.uv_layers.new(name="UVMap").data.foreach_set("uv", np.clip(uv, 0.01, 0.99).ravel())
    geo.set_rigid(ob, "head")
    for vg in beard.vertex_groups:
        if vg.name not in ob.vertex_groups:
            ob.vertex_groups.new(name=vg.name)
    with bpy.context.temp_override(active_object=beard, object=beard, selected_editable_objects=[beard, ob],
                                   selected_objects=[beard, ob]):
        bpy.ops.object.join()


def grandmaster(ctx):
    """The Grandmaster: a long ink-blue jiaoling robe (left over right) over a bone-grey inner robe whose
    collar shows at the neck; moderately wide sleeves tapering to the cuffs; a broad black sash
    knotted at the left; edge bands with pale-gold cloud embroidery at the collar, cuffs, front
    edge and hem; side slits from the hip; dark wide trousers gathered at the ankle; black cloth
    shoes on pale soles; a jade disc on a cord at the right hip; a grey-white goatee."""
    H, s, V = ctx.H, ctx.s, ctx.V
    hf = wd.head_frame(ctx)
    nb = ctx.head("neck_01")
    sole = ctx.cfg.get("sole_m", 0.02)
    # ---- trousers and shoes
    leg_shell(ctx, "trousers", "trousers", 0.58 * H, 0.05 * H, 0.03, folds=0.008, gather=0.026, smooth=10,
              medial=0.55, gap=0.008, thickness=0.004)
    z_shoe = 0.052 * H
    shoes(ctx, "shoe", "shoes", "sole", z_shoe, sole, wall=0.0055, toe=0.003)
    # ---- inner robe: torso only, a higher, narrower crossing, its collar band
    R = neck_radius(ctx)
    inner = Cross(R + 0.002 * s, nb[2] + 0.014 * s, 0.03 * s, 0.70 * H, -0.004 * s, over=1)
    flatten_chest(ctx, 0.60 * H, inner.zc)
    V = ctx.V
    crossed_top(ctx, "inner_robe", "inner", 0.66 * H, 0.0, 0.006, cross=inner, neck_top=nb[2] + 0.03 * s,
                sleeves=False, smooth=12, gap=0.004, thickness=0.003)
    coll = geo.Collider(ctx.colliders, outer_only=True)
    Wi = 0.024 * s
    path = inner.collar_path(ctx, coll, Wi, inner.zc - 0.02 * s, inner.zc - 0.9 * Wi, nb[2] + 0.018 * s,
                             ring_lift=0.006 * s, lift_cross=0.006 * s, n_edge=10)
    ribbon_part(ctx, "inner_collar", "inner", path, Wi, 0.004 * s, coll)
    # ---- outer robe: deep crossing to the right side of the waist, tapering sleeves
    outer = Cross(R + 0.012 * s, nb[2] + 0.004 * s, 0.105 * s, 0.585 * H, -0.006 * s, over=1)
    crossed_top(ctx, "robe", "robe", 0.54 * H, 0.90, 0.012, cross=outer, flare=0.024, under=0.018, taper=True,
                folds=0.004, smooth=12, gap=0.008, thickness=0.004, body_ease=0.012, loose_front=True,
                collider=geo.Collider(ctx.colliders + [ctx.parts["inner_collar"]], outer_only=True))
    # cuffs: embroidered edge bands at the sleeve ends
    Wc = 0.06 * s
    for side in ("l", "r"):
        el, wr = ctx.head("lowerarm_" + side), ctx.head("hand_" + side)
        am = (ctx.w("lowerarm_" + side, "hand_" + side) > 0.3) & side_mask(ctx, side)
        L = float(np.linalg.norm(wr - el))
        t1 = 0.905
        t0 = t1 - Wc / L
        cb = wd.ring_band(ctx, "cuff_" + side, "robe_band", am, el, wr, [t0 - 0.01, t0, t1 - 0.01, t1],
                          [0.022, 0.024, 0.024, 0.022], n=24)
        ring_emb(cb, el, wr, t0, t1 - 0.005, Wc)
    # ---- skirt: front-under, back and front-outer panels with side slits from the hip
    yc = float(ctx.head("pelvis")[1])
    z_top, z_hem = 0.60 * H, 0.085 * H
    Wh = 0.075 * s
    r_w = 0.15 * s
    phi_e = -math.asin(min(0.95, (0.105 * s) / r_w))
    panels = (("robe_skirt_under", (-math.pi / 2, 0.40), 0.012, (0.8, 0.7, 0.6), None),
              ("robe_skirt_back", (math.pi / 2, 3 * math.pi / 2), 0.012, (0.3, 0.3, 0.2), None),
              ("robe_skirt_outer", (phi_e, math.pi / 2), 0.021, (0.8, 0.7, 0.6), phi_e))
    for key, arc, gap, share, edge in panels:
        ob, _, _, _ = wd.wrap_skirt(ctx, key, "robe", z_top, z_hem, gap=gap, flare=0.07, folds=(0.009, 9),
                                    share=share, thickness=0.004, row=0.03, arc=arc, nu=30, y_c=yc,
                                    calf_share=0.9)
        panel_uv(ob, yc, arc)
        skirt_emb(ob, yc, z_hem, Wh, arc, edge, 1)
    # ---- outer collar band along the crossing and round the neck
    coll = geo.Collider(ctx.colliders + [ctx.parts[k] for k in ("robe_skirt_under", "robe_skirt_back", "robe_skirt_outer")],
                        outer_only=True)
    Wo = 0.052 * s
    path = outer.collar_path(ctx, coll, Wo, 0.60 * H, outer.zc - 0.9 * Wo, nb[2] + 0.006 * s,
                             ring_lift=0.008 * s, lift_cross=0.007 * s, n_edge=20)
    ribbon_part(ctx, "collar", "robe_band", path, Wo, 0.0045 * s, coll, emb_width=Wo)
    # ---- broad black sash, knotted at the front left, two short ends
    zs0, zs1 = 0.553 * H, 0.618 * H
    skirts = [ctx.parts[k] for k in ("robe_skirt_under", "robe_skirt_back", "robe_skirt_outer", "collar")]
    girdle(ctx, "sash", "sash", zs0, zs1, 0.04, skirts, gap=0.010, thickness=0.006)
    coll = ctx.collider([ctx.parts[k] for k in ("robe_skirt_under", "robe_skirt_back", "robe_skirt_outer", "collar", "sash")])
    zk = 0.5 * (zs0 + zs1)
    kx = 0.10 * s
    loc, n = hit_from(coll, (kx, -3.0, zk), (0, 1, 0))
    kc = loc + np.array([0, -0.010 * s, 0])
    knot = wd.lathe_part(ctx, "sash_knot", "sash", [(0, -0.013 * s), (0.02 * s, -0.012 * s), (0.028 * s, -0.003 * s),
                                                    (0.026 * s, 0.006 * s), (0.017 * s, 0.012 * s), (0, 0.013 * s)],
                         kc, n * np.array([1, 1, 0]) - np.array([0, 0.3, 0]), 12, rigid="pelvis")
    ck = geo.co(knot)
    ck[:, 2] = kc[2] + (ck[:, 2] - kc[2]) * 0.75
    geo.set_co(knot, ck)
    tail_coll = ctx.collider([ctx.parts[k] for k in ("robe_skirt_under", "robe_skirt_back", "robe_skirt_outer", "sash")])
    starts = [kc + np.array([-0.010 * s, -0.004 * s, -0.014 * s]), kc + np.array([0.012 * s, -0.006 * s, -0.014 * s])]
    ends = [kc + np.array([-0.035 * s, 0.0, -0.30 * s]), kc + np.array([0.035 * s, 0.0, -0.24 * s])]
    hanging_tails(ctx, "sash_tail", "sash", starts, ends, 0.05, 0.004, tail_coll, ("thigh_l", "thigh_l"), gap=0.008,
                  share=0.7, flare=0.5)
    # ---- jade disc on a cord from the sash at the right hip, under a bronze bead
    px = -0.135 * s
    loc, n = hit_from(coll, (px, -3.0, zs0 + 0.004 * s), (0, 1, 0))
    top = loc + np.array([0, -0.006 * s, 0])
    bead_c = top + np.array([0, -0.004 * s, -0.07 * s])
    wd.tube_part(ctx, "jade_cord", "cord", [top, top + np.array([0, -0.002 * s, -0.035 * s]), bead_c], 0.0016 * s, 5,
                 rigid="pelvis")
    wd.lathe_part(ctx, "jade_bead", "bronze", [(0, -0.008 * s), (0.006 * s, -0.006 * s), (0.0075 * s, 0.0),
                                               (0.006 * s, 0.006 * s), (0, 0.008 * s)], bead_c, (0, 0, 1), 10, rigid="pelvis")
    dc = bead_c + np.array([0, 0, -0.008 * s - 0.026 * s])
    ring = [dc + np.array([0.0165 * s * math.cos(a), 0.0, 0.0165 * s * math.sin(a)]) for a in np.linspace(0, 2 * math.pi, 25)]
    v, f = geo.tube(ring, 0.0078 * s, 10)
    v = np.array(v)
    v[:, 1] = dc[1] + (v[:, 1] - dc[1]) * 0.42          # a flat bi disc facing forward
    jade = geo.new_mesh_object("jade", [tuple(p) for p in v], f, ctx.rig)
    geo.recalc_normals(jade)
    geo.set_rigid(jade, "pelvis")
    ctx.add("jade", jade, "jade", collide=False)
    # ---- face: goatee and moustache
    goatee(ctx, hf)


fo.BUILDERS.update({"wren": wren, "grandmaster": grandmaster})
