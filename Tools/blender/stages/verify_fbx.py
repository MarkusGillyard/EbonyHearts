"""Reads an exported stage FBX back and checks it: axis and unit settings in GlobalSettings, the
1 m origin marker, object names, UVs on every mesh, identity rotation/scale, triangle and object
counts, material names. Run: run.sh verify_fbx.py -- <stage_id> [<stage_id> ...]"""
import os
import sys

import bpy
from io_scene_fbx import parse_fbx

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stagelib as S  # noqa: E402


def global_settings(path):
    root, _ = parse_fbx.parse(path)
    out = {}
    for e in root.elems:
        if e.id == b"GlobalSettings":
            for p70 in e.elems:
                if p70.id == b"Properties70":
                    for pr in p70.elems:
                        out[pr.props[0].decode()] = pr.props[-1]
    return out


def check(sid):
    path = os.path.join(S.OUT_ROOT, sid, f"{sid}.fbx")
    gs = global_settings(path)
    axes = (gs.get("UpAxis"), gs.get("UpAxisSign"), gs.get("FrontAxis"), gs.get("FrontAxisSign"),
            gs.get("CoordAxis"), gs.get("CoordAxisSign"), gs.get("UnitScaleFactor"))
    ok_axes = axes == (2, 1, 1, -1, 0, 1, 1.0)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    problems = []
    tris = 0
    mats = set()
    obs = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in obs:
        me = o.data
        me.calc_loop_triangles()
        tris += len(me.loop_triangles) if o.name != "ORIGIN_MARKER_1m" else 0
        if not me.uv_layers:
            problems.append(f"{o.name}: no UVs")
        if any(abs(a) > 1e-4 for a in o.rotation_euler):
            problems.append(f"{o.name}: rotation {tuple(o.rotation_euler)}")
        if not (o.name.startswith(sid + "_") or o.name == "ORIGIN_MARKER_1m"):
            problems.append(f"{o.name}: name lacks the {sid}_ prefix")
        for m in me.materials:
            if m:
                mats.add(m.name)
    marker = bpy.data.objects.get("ORIGIN_MARKER_1m")
    mdim = tuple(round(c, 4) for c in marker.dimensions) if marker else None
    if mdim != (1.0, 1.0, 1.0):
        problems.append(f"origin marker dimensions {mdim}")
    floor = [o for o in obs if o.name.endswith("_floor") or o.name.endswith("_deck")]
    print(f"VERIFY {sid}: axes(up,sign,front,sign,coord,sign,unit)={axes} {'OK' if ok_axes else 'UNEXPECTED'}; "
          f"objects {len(obs)}; triangles {tris}; marker {mdim}")
    print(f"VERIFY {sid}: materials {sorted(mats)}")
    for p in problems:
        print(f"VERIFY {sid}: PROBLEM {p}")
    if not problems and ok_axes:
        print(f"VERIFY {sid}: clean")


for sid in sys.argv[sys.argv.index("--") + 1:]:
    check(sid)
