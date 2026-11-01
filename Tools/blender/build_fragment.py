"""The Ebony Heart Fragment's bodies, from the sheet: an amoeba whose body is a galaxy, and the worm the
Grandmaster sees. Neither is a person, but the fight's poses, hit boxes and rig all drive a two-armed,
two-legged skeleton, so each mass is grown around that skeleton from metaballs and skinned to it with
wide, soft weights: arms become pseudopods, legs the mass where it meets the water.

    Tools/blender/run_blender.sh Tools/blender/build_fragment.py -- slopbat|slopbat_worm

Writes SourceArt/Fighters/<id>/<id>.fbx and <id>_report.json. The skin is M_CharacterFragment
(Tools/import_assets.py), textures from Tools/blender/fragment_textures.py.
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fighter_core as core      # noqa: E402
import fighter_geo as geo        # noqa: E402
import fighter_joints as joints  # noqa: E402
import fighter_outfits as outfits  # noqa: E402

BONES = ["pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head"] + [
    "%s_%s" % (b, s) for b in ("clavicle", "upperarm", "lowerarm", "hand", "thigh", "calf", "foot") for s in "lr"]


def metaball(name, balls, res, threshold=0.9):
    mb = bpy.data.metaballs.new(name)
    mb.resolution = mb.render_resolution = res
    mb.threshold = threshold
    ob = bpy.data.objects.new(name, mb)
    bpy.context.scene.collection.objects.link(ob)
    for co, r in balls:
        e = mb.elements.new()
        e.co = Vector(co)
        e.radius = r
    bpy.context.view_layer.update()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    bpy.data.objects.remove(ob)
    out = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(out)
    return out


def along(a, b, r0, r1, n=None):
    """Balls from a to b, close enough to melt into one lobe."""
    a, b = np.array(a), np.array(b)
    n = max(2, int(np.linalg.norm(b - a) / (0.45 * min(r0, r1)))) if n is None else n
    return [(a + (b - a) * t, r0 + (r1 - r0) * t) for t in np.linspace(0, 1, n)]


def amoeba(L):
    """A mass low on the water, narrowing to a blunt rise; long pseudopods where arms would be, and two
    lobes where legs would be, spread on the water."""
    h = lambda b: L[b][0]
    balls = []
    balls += along(h("pelvis") + np.array([0, 0, -0.3]), h("spine_03"), 0.5, 0.38)
    balls += along(h("spine_03"), h("head") + np.array([0, 0, 0.04]), 0.3, 0.2)
    for s, x in (("l", 1), ("r", -1)):
        balls += along(h("spine_02") * np.array([0, 1, 1]), h("upperarm_" + s), 0.3, 0.2)
        balls += along(h("upperarm_" + s), h("lowerarm_" + s), 0.2, 0.15)
        balls += along(h("lowerarm_" + s), h("hand_" + s), 0.15, 0.11)
        balls += along(h("hand_" + s), L["middle_03_" + s][1] + np.array([0.08 * x, 0, -0.08]), 0.11, 0.06)
        balls += along(h("pelvis") + np.array([0.2 * x, 0, -0.4]), np.array([0.62 * x, -0.1, 0.12]), 0.46, 0.34)
        balls += along(np.array([0.62 * x, -0.1, 0.12]), np.array([1.15 * x, 0.05, 0.08]), 0.34, 0.2)
    balls += along(h("pelvis") + np.array([0, 0, -0.5]), np.array([0, 0.5, 0.1]), 0.46, 0.3)
    balls += along(h("pelvis") + np.array([0, 0, -0.5]), np.array([0, -0.55, 0.1]), 0.46, 0.3)
    return balls


def tube(name, path, radius, segments=32):
    """A skin swept along `path` (points) with `radius` per point."""
    path = [np.array(p, float) for p in path]
    verts, faces = [], []
    x = None
    for i, p in enumerate(path):
        t = path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]
        t /= np.linalg.norm(t) + 1e-9
        # the frame carried along the path, so the rings never twist
        x = np.cross(t, (0.31, 0.27, 0.91)) if x is None else x - t * float(x @ t)
        x /= np.linalg.norm(x) + 1e-9
        y = np.cross(x, t)
        for k in range(segments):
            a = k / segments * math.tau
            verts.append(tuple(p + (x * math.cos(a) + y * math.sin(a)) * radius[i]))
    n = len(path)
    for i in range(n - 1):
        for k in range(segments):
            k2 = (k + 1) % segments
            faces.append((i * segments + k, i * segments + k2, (i + 1) * segments + k2, (i + 1) * segments + k))
    # close both ends on a point
    verts.append(tuple(path[0]))
    verts.append(tuple(path[-1]))
    a, b = len(verts) - 2, len(verts) - 1
    for k in range(segments):
        k2 = (k + 1) % segments
        faces.append((a, k2, k))
        faces.append((b, (n - 1) * segments + k, (n - 1) * segments + k2))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def smooth_path(points, n):
    """Catmull-Rom through `points`, n samples."""
    P = [np.array(p, float) for p in points]
    P = [P[0]] + P + [P[-1]]
    out = []
    for t in np.linspace(0, len(P) - 3 - 1e-6, n):
        i = int(t)
        u = t - i
        a, b, c, d = P[i], P[i + 1], P[i + 2], P[i + 3]
        out.append(0.5 * (2 * b + (c - a) * u + (2 * a - 5 * b + 4 * c - d) * u * u + (3 * b - a - 3 * c + d) * u ** 3))
    return out


def worm(L):
    """The worm: a ringed body rising out of the water from a tail coiled along it, a blunt head with a
    round mouth, and two thin feelers. Parts are joined, not fused: rings must stay crisp."""
    h = lambda b: L[b][0]
    floor = 0.28
    top = float(L["head"][1][2])
    path = smooth_path([(2.0, 1.2, floor), (1.0, 1.5, floor), (-0.4, 1.1, floor), (-0.8, 0.3, floor + 0.04), (-0.35, -0.15, 0.45),
                        (0.0, -0.1, 1.1), (0.04, 0.0, 1.6), (0.0, 0.04, 2.05), (-0.02, 0.0, top - 0.12), (0.0, -0.1, top)], 320)
    n = len(path)
    radius = []
    for i in range(n):
        t = i / (n - 1)
        body = 0.09 + 0.33 * min(t * 3.0, 1.0) ** 0.8
        body *= 1.0 - 0.34 * max(t - 0.7, 0.0) / 0.3
        ring = 1.0 + 0.16 * abs(math.sin(t * math.pi * 30)) ** 4
        mouth = 1.0 if t < 0.985 else 0.4 + (1.0 - t) / 0.015 * 0.6
        radius.append(body * ring * mouth)
    parts = [tube("worm_body", path, radius)]
    for s, x in (("l", 1), ("r", -1)):
        feeler = smooth_path([tuple(h("upperarm_" + s)), tuple(h("lowerarm_" + s)), tuple(h("hand_" + s)), tuple(L["middle_03_" + s][1] + np.array([0.1 * x, -0.1, 0.1]))], 40)
        parts.append(tube("worm_feeler_" + s, feeler, [0.09 - 0.07 * i / 39 for i in range(40)], 10))
    return parts


def weights(ob, L, sigma):
    """Wide, soft weights: each vertex follows the bones nearest it, none of them all the way."""
    V = geo.co(ob)
    W = {}
    for b in BONES:
        a, c = L[b]
        t, r, _ = geo.axis_t(V, a, c)
        t = np.clip(t, 0, 1)
        d = np.linalg.norm(V - (np.array(a) + np.outer(t, np.array(c) - np.array(a))), axis=1)
        W[b] = np.exp(-(d / sigma) ** 2)
    names = list(W)
    M = np.stack([W[n] for n in names], 1)
    # the strongest six, summing to one
    cut = np.sort(M, axis=1)[:, -6][:, None]
    M = np.where(M >= cut, M, 0.0)
    M /= M.sum(1, keepdims=True) + 1e-9
    return {n: M[:, i] for i, n in enumerate(names)}


def spherical_uv(ob):
    me = ob.data
    uv = me.uv_layers.new(name="UVMap")
    V = geo.co(ob)
    c = V.mean(0)
    d = V - c
    u = 0.5 + np.arctan2(d[:, 1], d[:, 0]) / math.tau
    v = 0.5 + np.arcsin(np.clip(d[:, 2] / (np.linalg.norm(d, axis=1) + 1e-9), -1, 1)) / math.pi
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    uv.data.foreach_set("uv", np.stack([u[lv], v[lv]], 1).ravel())


def main():
    fid = sys.argv[sys.argv.index("--") + 1]
    with open(os.path.join(core.HERE, "fighters.json")) as f:
        cfg = dict(json.load(f)["fighters"]["slopbat"])
    out_dir = os.path.join(core.PROJECT, "SourceArt", "Fighters", fid)
    os.makedirs(out_dir, exist_ok=True)
    core.enable_mpfb()
    cfg.update({"brows": None, "lashes": None, "hair": None})
    basemesh, rig, parts, hinfo = core.build_body(cfg)
    meshes = core.bake_body(basemesh, rig, parts, fid)
    L = core.landmarks(rig)
    for ob in meshes.values():
        bpy.data.objects.remove(ob)
    if fid.endswith("worm"):
        ob = worm(L)
        with bpy.context.temp_override(active_object=ob[0], object=ob[0], selected_objects=ob, selected_editable_objects=ob):
            bpy.ops.object.join()
        ob = ob[0]
        ob.name = ob.data.name = fid + "_body"
        geo.recalc_normals(ob)
    else:
        ob = metaball(fid + "_body", amoeba(L), 0.045)
        # low on the water it spreads and is cut flat at the surface
        V = geo.co(ob)
        V[:, 2] = np.maximum(V[:, 2], 0.0)
        geo.set_co(ob, V)
        bpy.context.view_layer.objects.active = ob
        mod = ob.modifiers.new("Smooth", "SMOOTH")
        mod.factor, mod.iterations = 0.6, 8
        dec = ob.modifiers.new("Decimate", "DECIMATE")
        dec.ratio = min(1.0, 26000.0 / max(len(ob.data.polygons) * 2, 1))
        geo.apply_modifier(ob, "Smooth")
        geo.apply_modifier(ob, "Decimate")
    for p in ob.data.polygons:
        p.use_smooth = True
    spherical_uv(ob)
    ob.parent = rig
    ob.modifiers.new("Armature", "ARMATURE").object = rig
    geo.set_weights(ob, weights(ob, L, 0.34))
    geo.clean_weights(ob, limit=6)
    ob.data.materials.append(bpy.data.materials.new("M_%s_ebony" % fid))
    joints.add_half_joints(rig, [ob])
    built = {"objects": {"body": ob}, "surface": {ob.name: "ebony"}, "skull_top": float(geo.co(ob)[:, 2].max())}
    stats = outfits.export(fid, cfg, rig, built, os.path.join(out_dir, fid + ".fbx"))
    with open(os.path.join(out_dir, fid + "_report.json"), "w") as f:
        json.dump({"id": fid, **stats}, f, indent=1)
    core.log("SUMMARY", fid, "tris=%s" % stats.get("triangles_total"), "bones=%s" % stats.get("bones"))


if __name__ == "__main__":
    main()
