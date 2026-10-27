"""Mesh helpers for authoring garments from the MPFB body: region selection by bone weights and
bone-axis coordinates, shells duplicated from body faces (exact copies of the body's skin
weights and UVs), offsets, smoothing, push-out against a collider, solidify, rigid weights,
hide-under-clothes face deletion, parametric sheets and small lathe props."""
import math

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


# ----------------------------------------------------------------------------- access

def co(ob):
    a = np.empty(len(ob.data.vertices) * 3)
    ob.data.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


def set_co(ob, a):
    ob.data.vertices.foreach_set("co", np.ascontiguousarray(a, dtype=np.float64).ravel())
    ob.data.update()


def vnormals(ob):
    ob.data.update()
    a = np.empty(len(ob.data.vertices) * 3)
    ob.data.vertices.foreach_get("normal", a)
    return a.reshape(-1, 3)


def faces(ob):
    me = ob.data
    n = len(me.polygons)
    ls, lt = np.empty(n, np.int64), np.empty(n, np.int64)
    me.polygons.foreach_get("loop_start", ls)
    me.polygons.foreach_get("loop_total", lt)
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    return [lv[s:s + t] for s, t in zip(ls, lt)]


def weights(ob, names=None):
    """{group name: per-vertex weight array}."""
    n = len(ob.data.vertices)
    idx = {vg.index: vg.name for vg in ob.vertex_groups}
    want = set(idx.values()) if names is None else set(names)
    out = {nm: np.zeros(n) for nm in want}
    for v in ob.data.vertices:
        for g in v.groups:
            nm = idx.get(g.group)
            if nm in out:
                out[nm][v.index] = g.weight
    return out


def wsum(W, names):
    s = None
    for nm in names:
        if nm in W:
            s = W[nm].copy() if s is None else s + W[nm]
    return s if s is not None else np.zeros(len(next(iter(W.values()))))


def axis_t(points, a, b):
    """Parameter of each point projected on segment a->b (0 at a, 1 at b) and radial distance."""
    a, b = np.asarray(a), np.asarray(b)
    d = b - a
    L2 = float(d @ d)
    t = (points - a) @ d / L2
    foot = a + np.outer(t, d)
    r = np.linalg.norm(points - foot, axis=1)
    return t, r, foot


def face_mask(fv, vmask, mode="all"):
    if mode == "all":
        return np.array([vmask[f].all() for f in fv])
    if mode == "any":
        return np.array([vmask[f].any() for f in fv])
    return np.array([vmask[f].mean() >= 0.5 for f in fv])


def erode_faces(fv, fmask, rings=1):
    """Shrink a face selection by `rings` vertex rings (faces touching unselected faces drop)."""
    nv = max(int(f.max()) for f in fv) + 1
    m = fmask.copy()
    for _ in range(rings):
        out_v = np.zeros(nv, bool)
        for f, sel in zip(fv, m):
            if not sel:
                out_v[f] = True
        m = np.array([sel and not out_v[f].any() for f, sel in zip(fv, m)])
    return m


# ----------------------------------------------------------------------------- objects

def link(ob, like=None):
    coll = like.users_collection[0] if like is not None and like.users_collection else bpy.context.scene.collection
    coll.objects.link(ob)
    return ob


def duplicate(ob, name):
    new = ob.copy()
    new.data = ob.data.copy()
    new.name = new.data.name = name
    link(new, ob)
    return new


def tag_source(ob):
    """Remember each vertex's index in the source mesh (survives face deletion)."""
    a = ob.data.attributes.get("src_vi") or ob.data.attributes.new("src_vi", "INT", "POINT")
    a.data.foreach_set("value", np.arange(len(ob.data.vertices), dtype=np.int32))


def source_index(ob):
    a = np.empty(len(ob.data.vertices), np.int32)
    ob.data.attributes["src_vi"].data.foreach_get("value", a)
    return a.astype(np.int64)


def keep_faces(ob, fmask):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.faces.ensure_lookup_table()
    kill = [f for f, k in zip(bm.faces, fmask) if not k]
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()


def delete_faces(ob, fmask):
    keep_faces(ob, ~np.asarray(fmask))


def new_mesh_object(name, verts, faces_, rig=None, like=None):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces_])
    me.update()
    ob = bpy.data.objects.new(name, me)
    link(ob, like)
    if rig is not None:
        bind(ob, rig)
    for p in me.polygons:
        p.use_smooth = True
    return ob


def bind(ob, rig):
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    if not any(m.type == "ARMATURE" for m in ob.modifiers):
        mod = ob.modifiers.new("Armature", "ARMATURE")
        mod.object = rig


def apply_modifier(ob, name):
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob],
                                   selected_editable_objects=[ob]):
        bpy.ops.object.modifier_apply(modifier=name)


def move_armature_last(ob):
    names = [m.name for m in ob.modifiers if m.type == "ARMATURE"]
    for nm in names:
        with bpy.context.temp_override(object=ob, active_object=ob):
            bpy.ops.object.modifier_move_to_index(modifier=nm, index=len(ob.modifiers) - 1)


def solidify(ob, thickness, offset=-1.0, rim=True):
    """Apply a Solidify (vertex groups and UVs are copied onto the new shell)."""
    arm = [m for m in ob.modifiers if m.type == "ARMATURE"]
    for m in arm:
        m.show_viewport = False
    mod = ob.modifiers.new("Solid", "SOLIDIFY")
    mod.thickness = thickness
    mod.offset = offset
    mod.use_rim = rim
    mod.use_even_offset = False   # even offset explodes on the sliver triangles plane cuts leave
    mod.use_quality_normals = True
    with bpy.context.temp_override(object=ob, active_object=ob):
        bpy.ops.object.modifier_move_to_index(modifier="Solid", index=0)
    apply_modifier(ob, "Solid")
    for m in arm:
        m.show_viewport = True


def trim(ob, plane_co, plane_no, region=None):
    """Cut the mesh with a plane and delete everything on the +plane_no side. `region(c)` limits
    the cut to faces whose centre passes the test (so one leg can be trimmed without the other).
    Edge splits interpolate the skin weights and UVs."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    if region is None:
        fs = list(bm.faces)
    else:
        fs = [f for f in bm.faces if region(np.array(f.calc_center_median()))]
    if fs:
        es = {e for f in fs for e in f.edges}
        vs = {v for f in fs for v in f.verts}
        bmesh.ops.bisect_plane(bm, geom=list(vs) + list(es) + fs, dist=1e-7,
                               plane_co=Vector(plane_co), plane_no=Vector(plane_no), clear_outer=True)
        loose = [v for v in bm.verts if not v.link_faces]
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        ng = [f for f in bm.faces if len(f.verts) > 4]
        if ng:
            bmesh.ops.triangulate(bm, faces=ng)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()


def cut_field(ob, field):
    """Keep the part of the mesh where field(coords) >= 0, cutting exactly along the zero level
    set: crossing edges are split at the interpolated point (weights and UVs interpolate), faces
    are split through the new points, negative faces are deleted. Garment outlines are written
    as min/max combinations of signed distances, so edges come out straight and clean."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    verts = list(bm.verts)
    f = np.asarray(field(np.array([v.co[:] for v in verts])), float)
    f[np.abs(f) < 1e-9] = 1e-9
    fv = {v: f[i] for i, v in enumerate(verts)}
    zero = set()
    for e in list(bm.edges):
        a, b = e.verts
        fa, fb = fv[a], fv[b]
        if (fa < 0) != (fb < 0):
            t = fa / (fa - fb)
            _, nv = bmesh.utils.edge_split(e, a, t)
            fv[nv] = 0.0
            zero.add(nv)
    for _ in range(4):
        todo = False
        for face in list(bm.faces):
            vs = list(face.verts)
            zs = [i for i, v in enumerate(vs) if v in zero]
            if len(zs) < 2:
                continue
            n = len(vs)
            done = False
            for ia in range(len(zs)):
                for ib in range(ia + 1, len(zs)):
                    i, j = zs[ia], zs[ib]
                    if (j - i) % n in (1, n - 1):
                        continue
                    side1 = [fv[vs[k % n]] for k in range(i + 1, j)]
                    side2 = [fv[vs[k % n]] for k in range(j + 1, i + n)]
                    if any(x < 0 for x in side1) != any(x < 0 for x in side2):
                        try:
                            bmesh.utils.face_split(face, vs[i], vs[j])
                            done = True
                        except ValueError:
                            pass
                        break
                if done:
                    break
            todo |= done
        if not todo:
            break
    kill = [fc for fc in bm.faces if any(fv[v] < 0 for v in fc.verts)]
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    ng = [fc for fc in bm.faces if len(fc.verts) > 4]
    if ng:
        bmesh.ops.triangulate(bm, faces=ng)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()


def recalc_normals(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data)
    bm.free()


def keep_largest(ob):
    """Drop small disconnected islands (fragments left by field cuts)."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    seen, comps = set(), []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, comp = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            comp.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        comps.append(comp)
    if len(comps) > 1:
        comps.sort(key=len)
        kill = [f for c in comps[:-1] for f in c]
        bmesh.ops.delete(bm, geom=kill, context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(ob.data)
    bm.free()


def decimate(ob, ratio):
    """Collapse decimation (vertex groups and UVs are interpolated)."""
    if ratio >= 0.999:
        return
    arm = [m for m in ob.modifiers if m.type == "ARMATURE"]
    for m in arm:
        m.show_viewport = False
    mod = ob.modifiers.new("Dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = ratio
    mod.use_collapse_triangulate = False
    with bpy.context.temp_override(object=ob, active_object=ob):
        bpy.ops.object.modifier_move_to_index(modifier="Dec", index=0)
    apply_modifier(ob, "Dec")
    for m in arm:
        m.show_viewport = True


def triangulate_ngons(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    ng = [f for f in bm.faces if len(f.verts) > 4]
    if ng:
        bmesh.ops.triangulate(bm, faces=ng)
    bm.to_mesh(ob.data)
    bm.free()


def fill_holes(ob, vmask, rings=2):
    """Close the boundary loops made only of `vmask` vertices with a fan round a new centre
    vertex, its spokes cut `rings` times so the patch can bend. New vertices take the mean
    weights, UVs and src_vi of their loop. Returns the number of holes filled."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.verts.ensure_lookup_table()
    edges = [e for e in bm.edges if e.is_boundary and vmask[e.verts[0].index] and vmask[e.verts[1].index]]
    if not edges:
        bm.free()
        return 0
    uvl = bm.loops.layers.uv.active
    dl = bm.verts.layers.deform.active
    src = bm.verts.layers.int.get("src_vi")
    holes = bmesh.ops.holes_fill(bm, edges=edges, sides=0)["faces"]
    for f in holes:
        ring = list(f.verts)
        uv_of = {}
        for v in ring:
            uvs = [lp[uvl].uv.copy() for lp in v.link_loops if lp.face is not f]
            uv_of[v] = sum(uvs, Vector((0.0, 0.0))) / max(len(uvs), 1)
        uv_c = sum(uv_of.values(), Vector((0.0, 0.0))) / len(ring)
        wsum = {}
        if dl is not None:
            for v in ring:
                for g, w in v[dl].items():
                    wsum[g] = wsum.get(g, 0.0) + w / len(ring)
        src_val = ring[0][src] if src is not None else 0
        r = bmesh.ops.poke(bm, faces=[f], center_mode="MEAN")
        c = r["verts"][0]
        for g, w in wsum.items():
            c[dl][g] = w
        if src is not None:
            c[src] = src_val
        for nf in r["faces"]:
            for lp in nf.loops:
                lp[uvl].uv = uv_of.get(lp.vert, uv_c)
        if rings:
            spokes = [e for e in c.link_edges]
            sub = bmesh.ops.subdivide_edges(bm, edges=spokes, cuts=rings, use_grid_fill=False)
            new_v = [x for x in sub["geom_inner"] if isinstance(x, bmesh.types.BMVert)]
            for v in new_v:
                if src is not None:
                    v[src] = src_val      # (an interpolated index would point anywhere)
                if dl is not None and not v[dl].items():
                    for g, w in wsum.items():
                        v[dl][g] = w
            fan = {g for v in new_v for g in v.link_faces}   # (subdivide invalidates c)
            bmesh.ops.triangulate(bm, faces=[g for g in fan if len(g.verts) > 3])
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return len(holes)


# ----------------------------------------------------------------------------- shaping

class Collider:
    """Nearest-surface queries against the rest pose of some meshes (world space)."""

    def __init__(self, objs, face_masks=None):
        verts, polys, off = [], [], 0
        for k, ob in enumerate(objs):
            M = np.array(ob.matrix_world)
            c = co(ob) @ M[:3, :3].T + M[:3, 3]
            verts.extend(map(tuple, c))
            fm = None if face_masks is None else face_masks[k]
            for fi, f in enumerate(faces(ob)):
                if fm is None or fm[fi]:
                    polys.append(tuple(int(i) + off for i in f))
            off += len(c)
        self.tree = BVHTree.FromPolygons(verts, polys, all_triangles=False)

    def nearest(self, p):
        loc, n, _, d = self.tree.find_nearest(Vector(p))
        return loc, n, d


def push_out(ob, collider, gap, vmask=None, iters=1):
    """Move vertices that are inside or closer than `gap` to the collider surface outwards."""
    c = co(ob)
    moved = 0
    for _ in range(iters):
        for i in range(len(c)):
            if vmask is not None and not vmask[i]:
                continue
            loc, n, _ = collider.nearest(c[i])
            if loc is None:
                continue
            s = (Vector(c[i]) - loc).dot(n)
            if s < gap:
                c[i] = np.array(loc + n * gap)
                moved += 1
    set_co(ob, c)
    return moved


def neighbours(ob):
    nb = [set() for _ in ob.data.vertices]
    for e in ob.data.edges:
        a, b = e.vertices
        nb[a].add(b)
        nb[b].add(a)
    return [np.fromiter(s, np.int64) for s in nb]


def boundary_verts(ob):
    count = {}
    for p in ob.data.polygons:
        for k in p.edge_keys:
            count[k] = count.get(k, 0) + 1
    bv = np.zeros(len(ob.data.vertices), bool)
    bnb = [[] for _ in ob.data.vertices]
    for (a, b), c in count.items():
        if c == 1:
            bv[a] = bv[b] = True
            bnb[a].append(b)
            bnb[b].append(a)
    return bv, bnb


def smooth(ob, iters=4, factor=0.5, pin=None, boundary_along=True):
    """Laplacian smoothing. Boundary vertices are smoothed only along the boundary loop so
    hems and openings become clean curves; `pin` vertices never move."""
    c = co(ob)
    nb = neighbours(ob)
    bv, bnb = boundary_verts(ob)
    for _ in range(iters):
        new = c.copy()
        for i in range(len(c)):
            if pin is not None and pin[i]:
                continue
            if bv[i]:
                if not boundary_along or len(bnb[i]) != 2:
                    continue
                avg = c[bnb[i]].mean(0)
            else:
                if len(nb[i]) == 0:
                    continue
                avg = c[nb[i]].mean(0)
            new[i] = c[i] + (avg - c[i]) * factor
        c = new
    set_co(ob, c)


def offset_along(ob, normals, amount):
    c = co(ob)
    amount = np.broadcast_to(np.asarray(amount, float), (len(c),))
    set_co(ob, c + normals * amount[:, None])


def shell(body, name, fmask, offset, rig, smooth_iters=6, gap=None, collider=None,
          thickness=0.0, rim=True, extra=None):
    """Duplicate the masked body faces into a garment shell: exact copy of the body's skin
    weights and UVs, pushed out along the body normals, smoothed, kept outside the collider
    (body + earlier garments) and optionally solidified inwards."""
    ob = duplicate(body, name)
    n_body = vnormals(body)
    tag_source(ob)
    keep_faces(ob, fmask)
    c = co(ob)
    nrm = n_body[source_index(ob)]
    nrm /= np.linalg.norm(nrm, axis=1)[:, None] + 1e-12
    amt = offset(c, nrm) if callable(offset) else offset
    offset_along(ob, nrm, amt)
    if extra is not None:
        extra(ob, nrm)
    if smooth_iters:
        # interior only: smoothing open boundaries shortens them like a rubber band and eats
        # narrow parts (straps); edges are cleaned up with plane trims instead
        smooth(ob, smooth_iters, 0.5, boundary_along=False)
        smooth(ob, 1, 0.25, boundary_along=True)
    if collider is not None and gap is not None:
        push_out(ob, collider, gap)
        if smooth_iters:
            smooth(ob, 2, 0.35, boundary_along=False)
            push_out(ob, collider, gap * 0.8)
    if thickness > 0:
        solidify(ob, thickness, -1.0, rim)
    bind(ob, rig)
    return ob


# ----------------------------------------------------------------------------- weights

def set_rigid(ob, bone):
    for vg in list(ob.vertex_groups):
        ob.vertex_groups.remove(vg)
    g = ob.vertex_groups.new(name=bone)
    g.add(range(len(ob.data.vertices)), 1.0, "REPLACE")


def set_weights(ob, W):
    """W: {bone: per-vertex weight array}; replaces all groups."""
    for vg in list(ob.vertex_groups):
        ob.vertex_groups.remove(vg)
    for bone, w in W.items():
        idx = np.nonzero(w > 1e-4)[0]
        if len(idx) == 0:
            continue
        g = ob.vertex_groups.new(name=bone)
        for i in idx:
            g.add([int(i)], float(w[i]), "REPLACE")


def transfer_weights(src, dst, mapping="POLYINTERP_NEAREST"):
    """Data Transfer of all vertex groups, nearest face interpolated, applied."""
    for vg in list(dst.vertex_groups):
        dst.vertex_groups.remove(vg)
    for vg in src.vertex_groups:
        dst.vertex_groups.new(name=vg.name)
    mod = dst.modifiers.new("WT", "DATA_TRANSFER")
    mod.object = src
    mod.use_vert_data = True
    mod.data_types_verts = {"VGROUP_WEIGHTS"}
    mod.vert_mapping = mapping
    mod.layers_vgroup_select_src = "ALL"
    mod.layers_vgroup_select_dst = "NAME"
    with bpy.context.temp_override(object=dst, active_object=dst):
        bpy.ops.object.modifier_move_to_index(modifier="WT", index=0)
    apply_modifier(dst, "WT")


def clean_weights(ob, limit=8, smooth_iters=0):
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob],
                                   selected_editable_objects=[ob]):
        if smooth_iters:
            bpy.ops.object.vertex_group_smooth(group_select_mode="BONE_DEFORM", factor=0.5,
                                               repeat=smooth_iters)
        bpy.ops.object.vertex_group_clean(group_select_mode="BONE_DEFORM", limit=0.002)
        bpy.ops.object.vertex_group_limit_total(group_select_mode="BONE_DEFORM", limit=limit)
        bpy.ops.object.vertex_group_normalize_all(group_select_mode="BONE_DEFORM", lock_active=False)
    mx = {vg.index: 0.0 for vg in ob.vertex_groups}
    for v in ob.data.vertices:
        for g in v.groups:
            if g.group in mx and g.weight > mx[g.group]:
                mx[g.group] = g.weight
    drop = [vg.name for vg in ob.vertex_groups if mx.get(vg.index, 0.0) < 0.002]   # names first:
    for name in drop:                                                              # removing re-indexes
        ob.vertex_groups.remove(ob.vertex_groups[name])


# ----------------------------------------------------------------------------- primitives

def lathe(profile, segments, center, axis_up=(0, 0, 1)):
    """Revolve a (radius, height) profile around a vertical axis; returns verts, quads."""
    verts, fcs = [], []
    n = len(profile)
    for s in range(segments):
        a = 2 * math.pi * s / segments
        for r, h in profile:
            verts.append((center[0] + r * math.cos(a), center[1] + r * math.sin(a), center[2] + h))
    for s in range(segments):
        s2 = (s + 1) % segments
        for k in range(n - 1):
            fcs.append((s * n + k, s2 * n + k, s2 * n + k + 1, s * n + k + 1))
    # caps
    top, bot = len(verts), len(verts) + 1
    verts.append((center[0], center[1], center[2] + profile[-1][1]))
    verts.append((center[0], center[1], center[2] + profile[0][1]))
    for s in range(segments):
        s2 = (s + 1) % segments
        fcs.append((s * n + n - 1, s2 * n + n - 1, top))
        fcs.append((s2 * n, s * n, bot))
    return verts, fcs


def tube(points, radius, sides=6):
    """Polyline tube (quads) with open ends capped by fans."""
    pts = [np.asarray(p, float) for p in points]
    verts, fcs = [], []
    up = np.array([0, 0, 1.0])
    for i, p in enumerate(pts):
        t = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        t /= np.linalg.norm(t) + 1e-12
        a = np.cross(t, up)
        if np.linalg.norm(a) < 1e-3:
            a = np.cross(t, [1, 0, 0])
        a /= np.linalg.norm(a)
        b = np.cross(t, a)
        for k in range(sides):
            ang = 2 * math.pi * k / sides
            verts.append(tuple(p + radius * (math.cos(ang) * a + math.sin(ang) * b)))
    for i in range(len(pts) - 1):
        for k in range(sides):
            k2 = (k + 1) % sides
            fcs.append((i * sides + k, i * sides + k2, (i + 1) * sides + k2, (i + 1) * sides + k))
    return verts, fcs


def ring_by_angle(poly, n, centre=None):
    """Resample a star-shaped (e.g. convex) 2D polygon at n evenly spaced angles around its
    centre, starting at +Y (the back) so stacked rings line up for lofting."""
    poly = np.asarray(poly, float)
    c = poly.mean(0) if centre is None else np.asarray(centre, float)
    a, b = poly - c, np.roll(poly, -1, 0) - c
    out = []
    for k in range(n):
        th = math.pi / 2 + 2 * math.pi * k / n
        d = np.array([math.cos(th), math.sin(th)])
        best = 0.0
        for p, q in zip(a, b):
            e = q - p
            den = d[0] * (-e[1]) - d[1] * (-e[0])
            if abs(den) < 1e-12:
                continue
            t = (p[0] * (-e[1]) - p[1] * (-e[0])) / den
            u = (d[0] * p[1] - d[1] * p[0]) / den
            if t > 0 and -1e-9 <= u <= 1 + 1e-9:
                best = max(best, t)
        out.append(c + d * best)
    return np.array(out), c
