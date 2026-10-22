"""Sculpts every fighter's model from its exported rig and look, and writes
assets/characters/<id>.wfm.

    godot --headless --path . -s tools/assets/export_rigs.gd [-- --only=ashka]
    .venv/bin/python tools/assets/sculpt/build.py [--only=ashka] [--voxel=0.002] [--fast]
"""
import argparse
import hashlib
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))

import cage  # noqa: E402
import hair  # noqa: E402
import mesher  # noqa: E402
import outfits  # noqa: E402
import weights  # noqa: E402
from human import SPREAD, Body, HandLayout  # noqa: E402
from materials import Palette  # noqa: E402
from rig import Rig  # noqa: E402
from sdf import Union  # noqa: E402
from wfm import Writer  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
RIG_DIR = os.path.join(ROOT, "build", "characters")


def quat_axis(axis, angle):
    axis = np.asarray(axis, np.float64)
    axis = axis / np.linalg.norm(axis)
    s = np.sin(angle * 0.5)
    return [float(axis[0] * s), float(axis[1] * s), float(axis[2] * s), float(np.cos(angle * 0.5))]


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz]


def finger_frame(d, palm=(0.0, 0.0, 1.0)):
    """Bone basis whose -y runs along `d` with +z toward the palm side."""
    y = -np.asarray(d, np.float32)
    y /= np.linalg.norm(y)
    z = np.asarray(palm, np.float32) - y * np.dot(palm, y)
    z /= np.linalg.norm(z)
    x = np.cross(y, z)
    return np.stack([x, y, z], axis=1)


def skeleton(rig, body):
    """Bone list: the Rig's joints, then the fingers and eyes, with bind transforms (rig space),
    rest transforms relative to the parent, and open/fist rotations for the fingers."""
    bones = []
    binds = {}
    for j in rig.names:
        binds[j] = rig.xform(j)
        bones.append({"name": j, "parent": rig.parent[j]})
    hands = {}
    for s in ("l", "r"):
        lay = HandLayout(body, s, 0.0)
        hands[s] = lay
        hand_bind = rig.xform("hand_" + s)
        for b in lay.bones:
            local = np.eye(4, dtype=np.float32)
            if b["seg"] == 0:
                local[:3, :3] = finger_frame(b["dir"])
                local[:3, 3] = b["local_pos"]
                parent_bind = hand_bind
            else:
                prev = [x for x in lay.bones if x["finger"] == b["finger"] and x["seg"] == b["seg"] - 1][0]
                seg_len = float(np.linalg.norm(np.asarray(b["local_pos"]) - np.asarray(prev["local_pos"])))
                local[:3, 3] = [0.0, -seg_len, 0.0]
                parent_bind = binds[prev["name"]]
            binds[b["name"]] = parent_bind @ local
            open_q, fist_q = finger_curl(b, s)
            bones.append({"name": b["name"], "parent": b["parent"], "open": open_q, "fist": fist_q})
    eyes, er = body.eye_centres()
    for s, c in zip(("l", "r"), eyes):
        m = np.eye(4, dtype=np.float32)
        m[:3, 3] = c
        binds["eye_" + s] = m
        bones.append({"name": "eye_" + s, "parent": "head"})
    names = [b["name"] for b in bones]
    for b in bones:
        bind = binds[b["name"]]
        parent = b["parent"]
        rest = bind if parent == "" else np.linalg.inv(binds[parent]) @ bind
        b["bind"] = _xf(bind)
        b["rest"] = _xf(rest)
    return bones, binds, hands, names


def finger_curl(b, s):
    """Local rotations for an open hand (fingers straight and together) and a closed fist."""
    seg = b["seg"]
    if b["finger"] == "thumb":
        t = 1.0 if s == "l" else -1.0
        opened = quat_axis([0, 0, 1], -0.18 * t) if seg == 0 else quat_axis([1, 0, 0], -0.05)
        if seg == 0:
            fist = quat_mul(quat_axis([0, 0, 1], -0.55 * t), quat_axis([1, 0, 0], -0.55))
        else:
            fist = quat_axis([1, 0, 0], -(0.45 if seg == 1 else 0.6))
        return opened, fist
    spread = SPREAD[b["finger"]] * (1.0 if s == "l" else -1.0)
    flex = [1.5, 1.75, 1.1][seg]
    together = quat_axis([0, 0, 1], -spread * 0.85) if seg == 0 else [0.0, 0.0, 0.0, 1.0]
    opened = quat_mul(together, quat_axis([1, 0, 0], -0.04))
    fist = quat_mul(together, quat_axis([1, 0, 0], -flex))
    return opened, fist


def _xf(m):
    b = m[:3, :3]
    return [float(x) for x in (b[0, 0], b[1, 0], b[2, 0], b[0, 1], b[1, 1], b[2, 1], b[0, 2], b[1, 2], b[2, 2], m[0, 3], m[1, 3], m[2, 3])]


def segments(rig, body, binds, hands):
    """Each bone as a line segment in the bind pose, for distance-based skin weights."""
    seg = {}
    child = {"hips": "spine", "spine": "chest", "chest": "neck", "neck": "head", "clav_l": "upperarm_l", "clav_r": "upperarm_r",
             "upperarm_l": "forearm_l", "upperarm_r": "forearm_r", "forearm_l": "hand_l", "forearm_r": "hand_r",
             "thigh_l": "shin_l", "thigh_r": "shin_r", "shin_l": "foot_l", "shin_r": "foot_r"}
    for a, b in child.items():
        seg[a] = (rig.pos(a), rig.pos(b))
    hp = rig.pos("head")
    seg["head"] = (hp + np.array([0, 0.06, 0], np.float32) * body.s, hp + np.array([0, 0.2, 0], np.float32) * body.s)
    for s in ("l", "r"):
        seg["hand_" + s] = (rig.pos("hand_" + s), rig.at("hand_" + s, [0, -0.08 * body.s, 0]))
        a = rig.pos("foot_" + s)
        seg["foot_" + s] = (a, np.array([a[0], 0.02, a[2] - 0.16 * body.s], np.float32))
        lay = hands[s]
        for name, pts, radii, d in lay.chains:
            for j in range(3):
                seg["%s%d_%s" % (name, j + 1, s)] = (lay.to_bind(pts[j]), lay.to_bind(pts[j + 1]))
        c = binds["eye_" + s][:3, 3]
        seg["eye_" + s] = (c, c + np.array([0, 0, -0.01], np.float32))
    return seg


def mesh_part(tree, voxel, target, log, fallback_bones=("hips",), focus=None, bounds=(None, None)):
    """Meshes an SDF tree; returns vertices, faces, normals, occlusion, material keys and the
    bones each vertex may follow."""
    leaves = tree.leaves()
    for i, lf in enumerate(leaves):
        lf.index = i
    v, f = mesher.polygonize(tree, voxel, lo=bounds[0], hi=bounds[1], log=log)
    f = mesher.orient_outward(tree, v, f)
    v, f = mesher.simplify(v, f, target, log=log, focus=focus)
    v = mesher.project(tree, v)
    n = mesher.normals(tree, v)
    f = cage.orient(v, n, f.reshape(-1)).reshape(-1, 3)
    ao = mesher.ambient_occlusion(tree, v, n)
    _, leaf_of = mesher.evaluate(tree, v)
    keys = [leaves[i].material if i >= 0 and leaves[i].material else "skin" for i in leaf_of]
    allowed = [leaves[i].bones if i >= 0 and leaves[i].bones else list(fallback_bones) for i in leaf_of]
    return v, f, n, ao, keys, allowed


def surface_arrays(pal, body, v, f, n, ao, keys, bones, wts):
    albedo, params, detail = pal.assign(keys, ao)
    # The scalp under the hair takes the hair's colour, so skin never shows between strands.
    if body.rig.look.get("hair"):
        scalp = hair.hairline_mask(body.head_model(), v) * np.array([k == "skin" for k in keys])
        if scalp.any():
            hc, hp, _ = pal.params("hair")
            albedo[:, :3] = albedo[:, :3] * (1 - scalp[:, None]) + (hc * 0.7)[None, :] * scalp[:, None]
            params[:, 0] = params[:, 0] * (1 - scalp) + 0.6 * scalp
            params[:, 1] = params[:, 1] * (1 - scalp)
    # Lips: tint the skin of the mouth toward the lip colour.
    lip = body.head_model().lip_mask(v) * np.array([k == "skin" for k in keys])
    if lip.any():
        lc, lp, _ = pal.params("lips")
        albedo[:, :3] = albedo[:, :3] * (1 - lip[:, None]) + lc[None, :] * lip[:, None]
        params[:, 0] = params[:, 0] * (1 - lip) + lp[0] * lip
    return {"vertex": v, "normal": n, "params": params, "albedo": albedo,
            "bind": np.concatenate([v, detail[:, None]], axis=1).astype(np.float32), "bones": bones, "weights": wts,
            "index": f.reshape(-1).astype(np.int32)}


def eye_mesh(c, r, rings=18, segs=28):
    """A UV sphere looking down -z; uv.x is the angle from the front (0 at the pupil)."""
    verts, uvs, idx = [], [], []
    for i in range(rings + 1):
        t = np.pi * i / rings
        for j in range(segs + 1):
            a = 2 * np.pi * j / segs
            d = np.array([np.sin(t) * np.cos(a), np.sin(t) * np.sin(a), -np.cos(t)], np.float32)
            verts.append(c + d * r)
            uvs.append([t / np.pi, j / segs])
    for i in range(rings):
        for j in range(segs):
            a = i * (segs + 1) + j
            b = a + segs + 1
            idx += [a, b, a + 1, a + 1, b, b + 1]
    v = np.array(verts, np.float32)
    return v, (v - c) / r, np.array(uvs, np.float32), np.array(idx, np.int32)


def cloth_arrays(pal, key, v, n, bones, wts, idx):
    """Vertex attributes for a cage-driven garment: its material everywhere, with the inside of
    the garment a little darker."""
    keys = [key] * len(v)
    ao = np.full(len(v), 0.95, np.float32)
    albedo, params, detail = pal.assign(keys, ao)
    return {"vertex": v, "normal": n, "params": params, "albedo": albedo,
            "bind": np.concatenate([v, detail[:, None]], axis=1).astype(np.float32), "bones": bones, "weights": wts, "index": idx}


def hair_meshes(rig, body, outfit, w, bone_index, log):
    """Cloth sheets (skirts and long hair), short hair, brows and lashes. Returns the cloth
    skeletons for the header."""
    head = body.head_model()
    head_tree = Union([head.node(), body.neck()], 0.0)
    for i, lf in enumerate(head_tree.leaves()):
        lf.index = i
    look = rig.look
    style = look.get("hair", {})
    rng = np.random.default_rng(int(hashlib.sha256(rig.id.encode()).hexdigest()[:8], 16))
    skeletons = []
    for ci, spec in enumerate(rig.cloth):
        grid = cage.Grid(spec)
        kind = spec["spec"].get("kind", "skirt")
        bones = [{"bind": _xf(m)} for m in grid.binds]
        if kind == "hair":
            origin = rig.pos(grid.joint)
            st = hair.long_hair(head, head_tree, grid, origin, int(style.get("strands", 420)), 0.25, float(style.get("volume", 1.0)), 0.015, rng)
            if style.get("locks"):
                hair.locks(head, head_tree, st, float(style.get("lock_length", 0.2)), rng)
            arr = st.arrays(origin)
            w.add_mesh("hair_%d" % ci, "cloth:%d" % ci, [{"material": "hair", "start": 0, "count": len(arr["index"])}], arr)
            skeletons.append({"joint": grid.joint, "head": True, "bones": bones})
            log("  hair on sheet %d: %d triangles" % (ci, len(arr["index"]) // 3))
        else:
            sp = spec["spec"]
            v, n, b, wt, idx = cage.skirt(grid, body.m(float(sp.get("thickness", 0.004))), body.m(float(sp.get("folds", 0.012))), int(sp.get("fold_count", 14)),
                                          seed=ci, tatter=float(sp.get("tatter", 0.0)), curve=float(sp.get("hem_curve", 0.0)))
            mat = spec["spec"].get("material", "cloth")
            w.add_mesh("cloth_%d" % ci, "cloth:%d" % ci, [{"material": "body", "start": 0, "count": len(idx)}], cloth_arrays(Palette(look), mat, v, n, b, wt, idx))
            skeletons.append({"joint": grid.joint, "head": False, "bones": bones})
            log("  cloth sheet %d: %d triangles" % (ci, len(idx) // 3))
    if style.get("style") == "short":
        st = hair.short_hair(head, head_tree, int(style.get("strands", 650)), float(style.get("length", 0.05)), float(style.get("wave", 0.006)),
                             float(style.get("lift", 0.014)), 0.015, rng)
        arr = st.arrays()
        arr["bones"][:, 0] = bone_index["head"]
        w.add_mesh("hair", "body", [{"material": "hair", "start": 0, "count": len(arr["index"])}], arr)
    face_hair = outfit.face_hair(body, head, head_tree, rng)
    if face_hair is not None:
        arr = face_hair.arrays()
        arr["bones"][:, 0] = bone_index["head"]
        w.add_mesh("face_hair", "body", [{"material": "hair", "start": 0, "count": len(arr["index"])}], arr)
    st = hair.brows_and_lashes(head, head_tree, rng, float(style.get("brows", 1.0)))
    arr = st.arrays()
    arr["bones"][:, 0] = bone_index["head"]
    w.add_mesh("brows", "body", [{"material": "brows", "start": 0, "count": len(arr["index"])}], arr)
    return skeletons


## Grid cells a rigid piece may span: about what the body meshes in at 2 mm (~1.2 GB).
CELL_BUDGET = 150e6


def surface_bounds(tree, steps=64):
    """The box a piece's surface actually occupies, from a coarse sample. A piece cut from a
    bigger shape (a cuff from the whole arm) inherits that shape's box, which would waste grid."""
    lo, hi = np.asarray(tree.lo, np.float32), np.asarray(tree.hi, np.float32)
    axes = [np.linspace(lo[i], hi[i], steps, dtype=np.float32) for i in range(3)]
    pts = np.stack(np.meshgrid(*axes, indexing="ij"), -1).reshape(-1, 3)
    d, _ = mesher.evaluate(tree, pts)
    cell = (hi - lo) / (steps - 1)
    # Every point of the surface lies within one cell diagonal of some sample.
    near = pts[d < float(np.linalg.norm(cell))]
    if len(near) == 0:
        return lo, hi
    return near.min(axis=0) - cell, near.max(axis=0) + cell


def rigid(bone, n):
    return np.tile(np.array([[bone, 0, 0, 0]], np.int32), (n, 1)), np.tile(np.array([[1, 0, 0, 0]], np.float32), (n, 1))


def build(char_id, voxel, target, log=print):
    t0 = time.time()
    rig = Rig(os.path.join(ROOT, "build", "characters", char_id + ".rig.json"))
    body = Body(rig)
    outfit = outfits.for_fighter(rig)
    pal = Palette(rig.look)
    bones, binds, hands, names = skeleton(rig, body)
    bone_index = {n: i for i, n in enumerate(names)}
    segs = segments(rig, body, binds, hands)
    log("%s: %.2f m" % (char_id, rig.h))
    w = Writer()
    face = mesher.Focus(body.head_model().P(0, 0.095, -0.03), 0.09 * body.s, 0.36 * body.s, 1.8)
    v, f, n, ao, keys, allowed = mesh_part(outfit.body(body), voxel, getattr(outfit, "triangles", target), log, focus=face)
    bi, bw = weights.compute(v, f, allowed, segs, bone_index)
    total = len(f)
    w.add_mesh("body", "body", [{"material": "body", "start": 0, "count": int(len(f) * 3)}], surface_arrays(pal, body, v, f, n, ao, keys, bi, bw))
    for extra in outfit.extras(body):
        name, node, bone = extra[:3]
        opts = extra[3] if len(extra) > 3 else {}
        # Rigid pieces (rings, claws, a helmet) mesh finely; the voxel only grows if a piece would
        # need more grid than the body does (a 5 m box at 1.6 mm once took over 12 GB).
        lo, hi = surface_bounds(node)
        ev = max(min(voxel, opts.get("voxel", 0.001)), voxel * 0.25, float(np.prod(hi - lo) / CELL_BUDGET) ** (1.0 / 3.0))
        v, f, n, ao, keys, _ = mesh_part(node, ev, opts.get("triangles", 4000), log, bounds=(lo, hi))
        bi, bw = rigid(bone_index[bone], len(v))
        total += len(f)
        w.add_mesh(name, "body", [{"material": "body", "start": 0, "count": int(len(f) * 3)}], surface_arrays(pal, body, v, f, n, ao, keys, bi, bw))
    eyes, er = body.eye_centres()
    for s, c in zip(("l", "r"), eyes):
        ev, en, euv, ei = eye_mesh(c, er)
        ei = cage.orient(ev, en, ei)
        bi, bw = rigid(bone_index["eye_" + s], len(ev))
        w.add_mesh("eye_" + s, "body", [{"material": "eye", "start": 0, "count": int(len(ei))}],
                   {"vertex": ev, "normal": en, "uv": euv, "bones": bi, "weights": bw, "index": ei})
    cloth_skeletons = hair_meshes(rig, body, outfit, w, bone_index, log)
    header = {"id": char_id, "look_hash": rig.look_hash, "height": rig.h, "bones": bones, "cloth": cloth_skeletons,
              "eye_radius": float(er), "generator": "tools/assets/sculpt/build.py"}
    out = os.path.join(ROOT, "assets", "characters", char_id + ".wfm")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    size = w.save(out, header)
    log("  wrote %s: %d triangles, %.1f MB in %.0fs" % (os.path.relpath(out, ROOT), total, size / 1e6, time.time() - t0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--voxel", type=float, default=0.002)
    ap.add_argument("--target", type=int, default=120000)
    ap.add_argument("--fast", action="store_true", help="coarser voxels for quick previews")
    a = ap.parse_args()
    voxel = 0.004 if a.fast else a.voxel
    ids = sorted(f[:-len(".rig.json")] for f in os.listdir(RIG_DIR) if f.endswith(".rig.json"))
    for cid in ids:
        if a.only and cid not in a.only.split(","):
            continue
        build(cid, voxel, a.target)


if __name__ == "__main__":
    main()
