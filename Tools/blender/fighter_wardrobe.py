"""Garment library for the roster beyond the vertical slice (built on fighter_geo and the
helpers in fighter_outfits): field-cut shells, wrap-around skirts/coat tails/cloaks, oriented
lathe and tube props, and section rings. All parts go through ctx.add (name, surface)."""
import math

import numpy as np
from mathutils import Vector

import fighter_geo as geo
from fighter_outfits import (smoothstep, section_ring, loft_rings, convex_hull)


# ----------------------------------------------------------------------------- fields
# A field maps (n,3) points to signed values; >= 0 is kept. Combine with f_and / f_or.

def f_plane(p0, n):
    p0, n = np.asarray(p0, float), np.asarray(n, float)
    n = n / np.linalg.norm(n)
    return lambda c: (c - p0) @ n


def f_limb(a, b, t, keep="proximal", side=None, radius=None):
    """Cut across the bone axis a->b at parameter t. Points on the other side (x sign) or
    further than `radius` from the axis are untouched (+1)."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    L = float(np.linalg.norm(d))

    def f(c):
        tt = (c - a) @ d / (L * L)
        v = (t - tt) * L if keep == "proximal" else (tt - t) * L
        if radius is not None:
            foot = a + np.outer(tt, d)
            v = np.where(np.linalg.norm(c - foot, axis=1) > radius, 1.0, v)
        if side is not None:
            v = np.where((c[:, 0] > 0) != (side == "l"), 1.0, v)
        return v
    return f


def f_and(*fs):
    return lambda c: np.min([f(c) for f in fs], axis=0)


def f_or(*fs):
    return lambda c: np.max([f(c) for f in fs], axis=0)


def f_not(f):
    return lambda c: -f(c)


def f_ellipse(cx, cz, ax, az, front_y=None):
    """Positive outside an ellipse in the front (x, z) view; with front_y only points in front
    of that y are cut (an opening in the face of a hood)."""
    def f(c):
        e = np.sqrt(((c[:, 0] - cx) / ax) ** 2 + ((c[:, 2] - cz) / az) ** 2) - 1.0
        v = e * min(ax, az)
        if front_y is not None:
            v = np.where(c[:, 1] > front_y, 1.0, v)
        return v
    return f


# ----------------------------------------------------------------------------- shells

def field_shell(ctx, key, surface, vsel, offset, field=None, smooth=8, gap=0.006, thickness=0.004,
                extra=None, collide=True, rigid=None, hide=True, hide_margin=0.012, collider=None, decimate=1.0,
                largest=False):
    """Shell over the selected body faces, cut along field >= 0, solidified, added to ctx.
    Body faces well inside the kept region are deleted under it."""
    s = ctx.s
    fm = geo.face_mask(ctx.FV, vsel)
    ob = geo.shell(ctx.body, key, fm, offset, ctx.rig, smooth_iters=smooth, gap=None if gap is None else gap * s,
                   collider=collider or ctx.collider(), extra=extra)
    if field is not None:
        geo.cut_field(ob, field)
    if largest:
        geo.keep_largest(ob)
    geo.decimate(ob, decimate)
    if thickness:
        geo.solidify(ob, thickness * s)
    if rigid:
        geo.set_rigid(ob, rigid)
    ctx.add(key, ob, surface, collide=collide)
    if hide:
        inside = vsel.copy()
        if field is not None:
            inside &= field(ctx.V) > hide_margin * s
        ctx.hide_faces(geo.face_mask(ctx.FV, inside), 1)
    return ob


def nonarm_masks(ctx, objs):
    """Per-object face masks without arm faces (rays for skirts must not wrap the hanging arms)."""
    masks = []
    for ob in objs:
        if "src_vi" in ob.data.attributes:
            aw = ctx.arm_w[geo.source_index(ob)]
        elif ob is ctx.body:
            aw = ctx.arm_w
        else:
            masks.append(None)
            continue
        masks.append(geo.face_mask(geo.faces(ob), aw < 0.05, "all"))
    return masks


# ----------------------------------------------------------------------------- skirts

def wrap_skirt(ctx, key, surface, z_top, z_hem, open_frac=0.14, open_at="front", gap=0.012,
               flare=0.03, folds=(0.006, 7), slit_z=None, share=(0.85, 0.45, 0.2), thickness=0.004,
               nu=40, row=0.028, y_c=None, objs=None, tatter=None, hips_z=None, weight_mode="legs",
               snug_rows=1, arc=None, calf_share=0.0):
    """A sheet hanging round a vertical axis from z_top to z_hem: each column hangs from the
    widest point above it (so it drapes over hips, belly or shoulders), flares and folds
    towards the hem. `open_frac` of the circumference is left open at the front (coats,
    gowns, cloaks) or back (aprons); a back slit runs up to slit_z. Legs mode weights: body
    weights above the hips, then a blend from pelvis to the thigh on that side whose share
    falls from share[0] at the front edges through share[1] at the sides to share[2] at the
    back. `tatter` = (depth, teeth) cuts a ragged hem. `arc` = (from, to) angles (0 front, +x
    left) makes an explicit panel instead (wrap-over skirts); `calf_share` hands the hem below
    the knee to the shins and caps the pelvis share by height, so a long hem stays above the
    floor in a deep crouch."""
    s, V = ctx.s, ctx.V
    if y_c is None:
        y_c = float(ctx.head("pelvis")[1])
    objs = objs or ctx.colliders
    coll = geo.Collider(objs, nonarm_masks(ctx, objs))
    half_open = math.pi * open_frac
    if arc is not None:                     # explicit panel (wrap-over skirts): angles from the front, +x = left
        phis = np.linspace(arc[0], arc[1], nu + 1)
    elif open_at == "front":
        phis = np.linspace(half_open, 2 * math.pi - half_open, nu + 1)
    else:
        phis = np.linspace(math.pi + half_open, 3 * math.pi - half_open, nu + 1)
    nz = max(3, int(round((z_top - z_hem) / (row * s))))
    zs = np.linspace(z_top, z_hem, nz + 1)
    R = np.full((len(zs), len(phis)), np.nan)
    for i, zz in enumerate(zs):
        for j, ph in enumerate(phis):
            d = Vector((math.sin(ph), -math.cos(ph), 0.0))
            o = Vector((0.0, y_c, zz))
            loc, _, _, _ = coll.tree.ray_cast(o + d * 2.0, -d)
            if loc is not None:
                R[i, j] = (loc - o).length
    for i in range(len(zs)):
        row_ = R[i]
        good = ~np.isnan(row_)
        if good.sum() >= 2:
            row_[~good] = np.interp(np.nonzero(~good)[0], np.nonzero(good)[0], row_[good])
        elif i > 0:
            R[i] = R[i - 1]
        else:
            R[i] = 0.12 * s
    env = R.copy()
    for i in range(snug_rows, len(zs)):
        env[i] = np.maximum(env[i], env[i - 1])
    for _ in range(2):
        env[:, 1:-1] = 0.5 * env[:, 1:-1] + 0.25 * (env[:, :-2] + env[:, 2:])
    below = (z_top - zs) / max(z_top - z_hem, 1e-6)
    grid = np.zeros((len(zs), len(phis), 3))
    for i, zz in enumerate(zs):
        b = below[i]
        r = env[i] + gap * s + flare * s * b ** 1.5 + folds[0] * s * np.sin(folds[1] * phis) * b ** 1.2
        grid[i, :, 0] = np.sin(phis) * r
        grid[i, :, 1] = y_c - np.cos(phis) * r
        grid[i, :, 2] = zz
    # vertices, with the back column duplicated below the slit
    back_j = None
    if slit_z is not None and open_at == "front":
        back_j = int(np.argmin(np.abs(phis - math.pi)))
    verts, vid = [], {}

    def vi(i, j, part):
        dup = back_j is not None and j == back_j and zs[i] < slit_z
        key_ = (i, j, part if dup else 0)
        if key_ not in vid:
            p = grid[i, j].copy()
            if dup:
                p[0] += part * 0.003 * s
            vid[key_] = len(verts)
            verts.append(tuple(p))
        return vid[key_]

    fcs = []
    for i in range(len(zs) - 1):
        for j in range(len(phis) - 1):
            part = -1 if (back_j is not None and j >= back_j) else 1
            fcs.append((vi(i, j, part), vi(i + 1, j, part), vi(i + 1, j + 1, part), vi(i, j + 1, part)))
    ob = geo.new_mesh_object(key, verts, fcs, None, ctx.body)
    geo.recalc_normals(ob)
    c = geo.co(ob)
    # outward normals (away from the axis)
    me = ob.data
    me.update()
    if np.mean([(np.array(p.center) - [0, y_c, p.center.z]) @ np.array(p.normal) for p in me.polygons]) < 0:
        for p in me.polygons:
            p.flip()
    uv = me.uv_layers.new(name="UVMap")
    rm = float(np.nanmean(env)) + gap * s
    for li, loop in enumerate(me.loops):
        p = c[loop.vertex_index]
        ph = math.atan2(p[0], -(p[1] - y_c)) % (2 * math.pi)
        if arc is not None:     # a panel may cross the front: keep its angle continuous
            ph = (ph - arc[0]) % (2 * math.pi) + arc[0]
        uv.data[li].uv = (ph * rm, p[2])
    if tatter:
        depth, teeth = tatter

        def ragged(cc):
            ph = np.arctan2(cc[:, 0], -(cc[:, 1] - y_c))
            saw = np.abs(((ph * teeth / (2 * math.pi)) % 1.0) - 0.5) * 2      # 0..1 triangle wave
            jitter = 0.35 * np.sin(ph * teeth * 2.7 + 1.3)
            return cc[:, 2] - (z_hem + depth * s * np.clip(saw + jitter, 0, 1.3))
        geo.cut_field(ob, ragged)
        c = geo.co(ob)
    # weights
    geo.transfer_weights(ctx.body, ob)
    if weight_mode == "legs":
        hz = float(hips_z if hips_z is not None else ctx.head("thigh_l")[2])
        kz = float(ctx.head("calf_l")[2])
        W = geo.weights(ob)
        sb = smoothstep((hz - c[:, 2]) / max(hz - (hz + kz) / 2, 1e-6))
        ph = np.arctan2(c[:, 0], -(c[:, 1] - y_c))            # 0 front, +-pi back
        a = np.abs(ph) / math.pi                               # 0 front .. 1 back
        sh = np.where(a < 0.5, share[0] + (share[1] - share[0]) * a / 0.5,
                      share[1] + (share[2] - share[1]) * (a - 0.5) / 0.5)
        left = np.clip(c[:, 0] / (0.06 * s) * 0.5 + 0.5, 0, 1)
        if back_j is not None:
            left = np.where((a > 0.9) & (c[:, 2] < slit_z), (c[:, 0] > 0).astype(float), left)
        zero = np.zeros(len(c))
        newW = {}
        for bname, w in W.items():
            if bname.startswith(("calf", "foot", "ball", "thigh")):
                continue
            newW[bname] = w * (1 - sb)
        newW["pelvis"] = newW.get("pelvis", zero) + sb * (1 - sh)
        newW["thigh_l"] = sb * sh * left + (W.get("thigh_l", zero) * (1 - sb))
        newW["thigh_r"] = sb * sh * (1 - left) + (W.get("thigh_r", zero) * (1 - sb))
        if calf_share:
            # below the knee the hem is handed over to the shin on its side (from the pelvis and
            # thigh shares alike; fully at the back, half at the front, where a forward-tilting
            # shin would push it down), so a long hem rides the shins in a deep crouch instead of
            # sinking through the floor with the pelvis
            sc = calf_share * np.clip((kz - c[:, 2]) / max(kz - z_hem, 1e-6), 0, 1) ** 0.6 * (0.5 + 0.5 * a)
            for b in ("pelvis", "thigh_l", "thigh_r"):
                newW[b] = newW[b] * (1 - sc)
            newW["calf_l"] = sc * left
            newW["calf_r"] = sc * (1 - left)
            # the pelvis drops about 40% of the height in a deep crouch: cap its share by the
            # height above the floor and hand the rest to the thigh (above the knee) or shin
            cap = np.clip((c[:, 2] - 0.025 * s) / (0.6 * ctx.H), 0, 1)
            ex = np.maximum(newW["pelvis"] - cap, 0)
            newW["pelvis"] = newW["pelvis"] - ex
            above = c[:, 2] > kz
            for b, lr in (("l", left), ("r", 1 - left)):
                newW["thigh_" + b] = newW["thigh_" + b] + ex * lr * above
                newW["calf_" + b] = newW["calf_" + b] + ex * lr * ~above
        geo.set_weights(ob, newW)
    if thickness:
        geo.solidify(ob, thickness * s)
    geo.bind(ob, ctx.rig)
    ctx.add(key, ob, surface, collide=False)
    return ob, grid, zs, phis


# ----------------------------------------------------------------------------- props

def frame(axis_z, hint=(0, 0, 1)):
    z = np.asarray(axis_z, float)
    z = z / np.linalg.norm(z)
    x = np.cross(hint, z)
    if np.linalg.norm(x) < 1e-3:
        x = np.cross((1, 0, 0), z)
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    return np.stack([x, y, z], 1)      # columns


def lathe_part(ctx, key, surface, profile, origin, axis, segs=12, rigid=None, hint=(0, 0, 1)):
    v, f = geo.lathe(profile, segs, (0, 0, 0))
    F = frame(axis, hint)
    v = [tuple(np.asarray(origin) + F @ np.asarray(p)) for p in v]
    ob = geo.new_mesh_object(key, v, f, ctx.rig)
    geo.recalc_normals(ob)
    if rigid:
        geo.set_rigid(ob, rigid)
    ctx.add(key, ob, surface, collide=False)
    return ob


def box_part(ctx, key, surface, centre, size, axis=(0, 0, 1), hint=(1, 0, 0), rigid=None):
    F = frame(axis, hint)
    hx, hy, hz = np.asarray(size) / 2
    vs = [tuple(np.asarray(centre) + F @ np.array([sx * hx, sy * hy, sz * hz]))
          for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    fs = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    ob = geo.new_mesh_object(key, vs, fs, ctx.rig)
    geo.recalc_normals(ob)
    if rigid:
        geo.set_rigid(ob, rigid)
    ctx.add(key, ob, surface, collide=False)
    return ob


def tube_part(ctx, key, surface, points, radius, sides=6, rigid=None, transfer=False):
    v, f = geo.tube(points, radius, sides)
    ob = geo.new_mesh_object(key, v, f, ctx.rig)
    if rigid:
        geo.set_rigid(ob, rigid)
    elif transfer:
        geo.transfer_weights(ctx.body, ob)
    ctx.add(key, ob, surface, collide=False)
    return ob


def merge(ctx, keys, key, surface):
    """Join several added parts (same rig) into one object."""
    import bpy
    obs = [ctx.parts.pop(k) for k in keys]
    for o in obs:
        ctx.surface.pop(o.name, None)
    with bpy.context.temp_override(active_object=obs[0], selected_editable_objects=obs, object=obs[0]):
        bpy.ops.object.join()
    ob = obs[0]
    ctx.add(key, ob, surface, collide=False)
    return ob


def ring_band(ctx, key, surface, vmask, a, b, ts, offsets, n=20, rigid=None):
    """Band lofted through convex cross-sections of the body across a->b at parameters ts."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    rings = [section_ring(ctx, vmask, a + (b - a) * t, b - a, n, off * ctx.s) for t, off in zip(ts, offsets)]
    ob = loft_rings(rings, key, ctx.rig, ctx.body)
    geo.solidify(ob, 0.003 * ctx.s)
    if rigid:
        geo.set_rigid(ob, rigid)
    else:
        geo.transfer_weights(ctx.body, ob)
    ctx.add(key, ob, surface, collide=False)
    return ob


def head_frame(ctx):
    """Head centre, radius estimates and key heights from the body's head vertices."""
    V = ctx.V
    hw = ctx.w("head")
    hv = V[hw > 0.9]
    c = (hv.min(0) + hv.max(0)) / 2
    top = float(hv[:, 2].max())
    eyes = ctx.parts.get("eyes")
    ze = float(geo.co(eyes)[:, 2].mean()) if eyes is not None else top - 0.12 * ctx.s
    return {"c": c, "top": top, "eye_z": ze, "half": (hv.max(0) - hv.min(0)) / 2,
            "front": float(hv[:, 1].min()), "back": float(hv[:, 1].max())}
