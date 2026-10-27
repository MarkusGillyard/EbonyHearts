"""Review renders of a built fighter, from its FBX (so they show what Unreal receives).

    Tools/blender/run_blender.sh Tools/blender/render_fighter.py -- <id> [--dir SourceArt/Fighters/<id>]

Re-imports <id>.fbx, rebuilds each material from the slot PNGs beside it (DirectX normals
converted for Blender), and writes into <dir>/renders/:
  <id>_front.png, <id>_side.png, <id>_threequarter.png      rest pose, 1024 px
  <id>_pose_<name>.png  for stance, kick, crouch, punch, guard_up: side view (the game camera)
                        on the left, three-quarter view on the right, 1024 px
Eevee, neutral grey, warm key + cool rim + soft camera-side fill.
"""
import argparse
import glob
import math
import os
import sys
import time

import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fighter_core as core      # noqa: E402
import fighter_render as fr      # noqa: E402
import fighter_tex as tex        # noqa: E402

T0 = time.time()


def log(*a):
    print("[render %6.1fs]" % (time.time() - T0), *a, flush=True)


def parse():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("id")
    p.add_argument("--dir", default=None)
    p.add_argument("--samples", type=int, default=48)
    p.add_argument("--only", default=None, help="comma list: views,stance,kick,crouch,punch,guard_up")
    return p.parse_args(argv)


def rebuild_materials(fid, d, meshes):
    for ob in meshes:
        for slot in ob.material_slots:
            m = slot.material
            if m is None or not m.name.startswith("M_%s_" % fid):
                continue
            sname = m.name[len("M_%s_" % fid):].split(".")[0]
            files = {}
            for k in ("basecolor", "normal", "orm", "emissive"):
                p = os.path.join(d, "%s_%s_%s.png" % (fid, sname, k))
                if os.path.isfile(p):
                    files[k] = p
            img = bpy.data.images.load(files["basecolor"], check_existing=True)
            has_alpha = img.depth == 32
            new = tex.build_material(m.name + "_review", files, alpha=has_alpha or sname in ("hairface", "face"),
                                     flip_green=True)
            slot.material = new


def frame_portrait(cam, objs, view, lens=67.0, margin=1.1):
    """Perspective framing that fits the posed figure's extents seen from `view`."""
    lo, hi = fr.bbox(objs)
    c = Vector((lo + hi) / 2)
    v = Vector(view).normalized()
    hax = v.cross(Vector((0, 0, 1)))
    hax = hax.normalized() if hax.length > 1e-6 else Vector((1, 0, 0))
    corners = [Vector((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    W = max(p.dot(hax) for p in corners) - min(p.dot(hax) for p in corners)
    Hh = hi[2] - lo[2]
    D = max(p.dot(v) for p in corners) - min(p.dot(v) for p in corners)
    cam.data.type = "PERSP"
    cam.data.lens = lens
    cam.data.sensor_fit = "AUTO"
    sc = bpy.context.scene
    w, h = sc.render.resolution_x, sc.render.resolution_y
    big = 2 * math.atan(18.0 / lens)
    hfov = big if w >= h else 2 * math.atan(math.tan(big / 2) * w / h)
    vfov = big if h >= w else 2 * math.atan(math.tan(big / 2) * h / w)
    dist = max(Hh / 2 / math.tan(vfov / 2), W / 2 / math.tan(hfov / 2)) * margin + D / 2
    fr.look_at(cam, c + v * dist, c)


def main():
    a = parse()
    d = os.path.abspath(a.dir or os.path.join(core.PROJECT, "SourceArt", "Fighters", a.id))
    out = os.path.join(d, "renders")
    os.makedirs(out, exist_ok=True)
    core.clean_scene()
    cam = fr.setup_studio(1024, a.samples)
    bpy.ops.import_scene.fbx(filepath=os.path.join(d, a.id + ".fbx"))
    arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name != "Floor"]
    rebuild_materials(a.id, d, meshes)
    if os.environ.get("ONLY"):   # debugging aid: render only matching parts
        keep = os.environ["ONLY"].split(",")
        for ob in meshes:
            ob.hide_render = not any(k in ob.name for k in keep)
    lo, hi = fr.bbox(meshes)
    log("imported", arm.name, len(arm.data.bones), "bones; root", [b.name for b in arm.data.bones if b.parent is None],
        "height %.4f m, lowest %.4f" % (hi[2] - lo[2], lo[2]))
    only = set(a.only.split(",")) if a.only else None
    sc = bpy.context.scene
    if not only or "views" in only:
        sc.render.resolution_x = sc.render.resolution_y = 1024
        fr.frame_ortho(cam, meshes, (0, -1, 0.0), 1.06)
        fr.render(os.path.join(out, a.id + "_front.png"))
        fr.frame_ortho(cam, meshes, (1, 0, 0.0), 1.06)
        fr.render(os.path.join(out, a.id + "_side.png"))
        frame_portrait(cam, meshes, (math.sin(math.radians(35)), -math.cos(math.radians(35)), 0.12), lens=50, margin=1.0)
        fr.render(os.path.join(out, a.id + "_threequarter.png"))
        log("views done")
    for pose in fr.POSES:
        if only and pose not in only:
            continue
        fr.apply_pose(arm, pose)
        fr.ground(arm, meshes)
        sc.render.resolution_x, sc.render.resolution_y = 512, 1024
        tmp = []
        for k, view in enumerate([(1, 0, 0.06), (math.sin(math.radians(40)), -math.cos(math.radians(40)), 0.15)]):
            frame_portrait(cam, meshes, view)
            p = os.path.join(out, "_tmp_%s_%d.png" % (pose, k))
            fr.render(p)
            tmp.append(p)
        tiles = []
        for p in tmp:
            img = bpy.data.images.load(p)
            px = np.empty(512 * 1024 * 4, np.float32)
            img.pixels.foreach_get(px)
            tiles.append(px.reshape(1024, 512, 4))
            bpy.data.images.remove(img)
            os.remove(p)
        tex.save_png(os.path.join(out, "%s_pose_%s.png" % (a.id, pose)), np.concatenate(tiles, 1))
        log("pose", pose)
        fr.reset_pose(arm)
    log("done")


main()
