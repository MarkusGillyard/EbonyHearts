"""Build one fighter from Tools/blender/fighters.json into SourceArt/Fighters/<id>/.

    Tools/blender/run_blender.sh Tools/blender/build_fighter.py -- <id> [--stage body|outfit|all]

Outputs: <id>.blend (assembled, metres), <id>.fbx (Unreal: cm, Z-up, -Y forward, Armature node,
root bone 'root', no leaf bones, face smoothing groups, textures not embedded),
<id>_<slot>_basecolor|normal|orm[|emissive].png beside the FBX, <id>_report.json.
Review renders come from Tools/blender/render_fighter.py, which re-imports the FBX.
"""
import argparse
import json
import os
import resource
import sys
import time

import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fighter_core as core      # noqa: E402
import fighter_geo as geo        # noqa: E402
import fighter_joints as joints  # noqa: E402
import fighter_outfits as outfits  # noqa: E402
import fighter_outfits2  # noqa: E402,F401  (registers the rest of the roster)
import fighter_outfits3  # noqa: E402,F401  (Wren and the Grandmaster)
import fighter_tex as tex        # noqa: E402
import fighter_render as fr      # noqa: E402

TIMES = {}


def step(name, t0):
    TIMES[name] = round(time.time() - t0, 2)
    core.log("done", name, TIMES[name], "s")
    return time.time()


def parse():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("id")
    p.add_argument("--stage", default="all", choices=["body", "outfit", "all"])
    p.add_argument("--out", default=None, help="output folder (default SourceArt/Fighters/<id>)")
    return p.parse_args(argv)


def quick_renders(objs, out, tag):
    cam = fr.setup_studio(768, 16)
    fr.frame_ortho(cam, objs, (0, -1, 0))
    fr.render("%s_%s_front.png" % (out, tag))
    fr.frame_ortho(cam, objs, (1, 0, 0))
    fr.render("%s_%s_side.png" % (out, tag))


def main():
    a = parse()
    with open(os.path.join(core.HERE, "fighters.json")) as f:
        spec = json.load(f)
    cfg = spec["fighters"][a.id]
    if cfg.get("status") != "build":
        raise SystemExit("%s is not ready to build (status %s)" % (a.id, cfg.get("status")))
    out_dir = os.path.abspath(a.out or os.path.join(core.PROJECT, "SourceArt", "Fighters", a.id))
    os.makedirs(out_dir, exist_ok=True)
    base = os.path.join(out_dir, a.id)
    t = time.time()
    core.enable_mpfb()
    t = step("enable_mpfb", t)
    basemesh, rig, parts, hinfo = core.build_body(cfg)
    core.log("height", hinfo)
    t = step("mpfb_body", t)
    meshes = core.bake_body(basemesh, rig, parts, a.id)
    t = step("bake_body", t)
    if a.stage == "body":
        for ob in meshes.values():
            ob.data.materials.append(tex.preview_material("dev_" + ob.name, (0.6, 0.45, 0.35)))
        quick_renders(list(meshes.values()), base, "body")
        return
    built = outfits.build(a.id, cfg, rig, meshes)
    t = step("outfit", t)
    if a.stage == "outfit":
        tex.dev_materials(built)
        quick_renders([o for o in built["objects"].values()], base, "outfit")
        bpy.ops.wm.save_as_mainfile(filepath=base + "_dev.blend", compress=True)
        return
    report = tex.texture_and_materials(a.id, cfg, built, out_dir)
    t = step("textures", t)
    outfits.finalize(a.id, cfg, rig, built)
    joints.add_half_joints(rig, built["objects"].values())
    t = step("finalize", t)
    bpy.ops.wm.save_as_mainfile(filepath=base + ".blend", compress=True)
    t = step("save_blend", t)
    stats = outfits.export(a.id, cfg, rig, built, base + ".fbx")
    t = step("export_fbx", t)
    report.update(stats)
    report.update({"id": a.id, "name": cfg["name"], "height": hinfo, "timings_s": TIMES,
                   "script_wall_s": round(time.time() - core.T0, 1),
                   "peak_rss_mb": round(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024, 1)})
    with open(base + "_report.json", "w") as f:
        json.dump(report, f, indent=1)
    core.log("SUMMARY", a.id, "tris=%s" % report.get("triangles_total"), "slots=%s" % len(report.get("slots", {})),
             "height_cm=%s" % report.get("standing_height_cm"), "wall=%ss" % report["script_wall_s"])


main()
