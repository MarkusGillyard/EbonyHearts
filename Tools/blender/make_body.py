"""Generate a rigged MakeHuman/MPFB base body and export it for Unreal.

Run through Tools/blender/run_blender.sh (isolated Blender user config, memory cap):

    Tools/blender/run_blender.sh Tools/blender/make_body.py -- \
        --out SourceArt/Bodies/athletic_woman --gender 0.0 --height-m 1.74 \
        --muscle 0.75 --weight 0.45 --proportions 0.8 --skin young_caucasian_female

Writes <out>.blend (MPFB-native, metres, still editable with MPFB), <out>.fbx (Unreal-ready,
centimetres) and <out>_report.json (counts, bones, materials, timings).

FBX conventions (see export_fbx()):
  * +Z up, character faces -Y (Blender's native axes; no axis conversion is baked, and the file
    declares the same Z-up / -Y-front / right-handed system that Unreal's importer converts to).
  * Data is in centimetres: the export copy is scaled x100 with scene unit scale 0.01, so the
    file's UnitScaleFactor is 1.0 (cm) and every node has scale 1.
  * Armature object is named "Armature" (Unreal's FBX importer drops a Blender armature node
    with that name), so the skeleton starts at the bone "root".
  * No leaf bones, deform bones only, face smoothing groups, tangent space, textures embedded.

All assets come from MPFB's bundled data (base mesh, targets, rigs, weights) and the
makehuman_system_assets pack (skins, eyes, eyebrows, eyelashes, proxies), all CC0.
"""

import argparse
import importlib
import json
import os
import resource
import sys
import time

import bpy
import numpy as np

T_START = time.time()
ADDON = "bl_ext.user_default.mpfb"
HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", ".."))
ASSET_ROOT = os.path.join(PROJECT, "SourceArt", "ThirdParty", "MakeHuman", "extracted",
                          "makehuman_system_assets")
TIMINGS = {}


def log(*a):
    print("[make_body %6.2fs]" % (time.time() - T_START), *a, flush=True)


def step(name, t0):
    TIMINGS[name] = round(time.time() - t0, 3)
    log("done", name, TIMINGS[name], "s")
    return time.time()


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser(prog="make_body.py")
    p.add_argument("--out", required=True, help="output path without extension")
    p.add_argument("--name", default=None, help="character name used for objects/materials")
    p.add_argument("--gender", type=float, default=0.5, help="0 = female, 1 = male")
    p.add_argument("--height-m", type=float, required=True, help="standing height in metres")
    p.add_argument("--age", type=float, default=0.5, help="MakeHuman age macro (0.5 = 25 years)")
    p.add_argument("--muscle", type=float, default=0.5)
    p.add_argument("--weight", type=float, default=0.5)
    p.add_argument("--proportions", type=float, default=0.5)
    p.add_argument("--cupsize", type=float, default=0.5)
    p.add_argument("--firmness", type=float, default=0.5)
    p.add_argument("--race", default="0.333,0.333,0.333", help="african,asian,caucasian weights")
    p.add_argument("--skin", default="young_caucasian_female", help="skin .mhmat name in skins/")
    p.add_argument("--topology", default="base",
                   help="'base' (hm08 body, 13378 quads) or a proxy name from proxymeshes/, "
                        "e.g. female_generic, male_muscle_13290, male1591")
    p.add_argument("--eyes", default="high-poly", help="eyes mesh: high-poly | low-poly")
    p.add_argument("--eye-material", default="brown", help="eyes/materials/<name>.mhmat")
    p.add_argument("--eyebrows", default="eyebrow001", help="eyebrows asset or 'none'")
    p.add_argument("--eyelashes", default="eyelashes01", help="eyelashes asset or 'none'")
    p.add_argument("--hair", default="none", help="hair asset or 'none' (bald)")
    p.add_argument("--rig", default="game_engine", help="MPFB built-in rig name")
    p.add_argument("--max-influences", type=int, default=8, help="bone influences per vertex")
    p.add_argument("--no-fbx", action="store_true")
    return p.parse_args(argv)


# ----------------------------------------------------------------------------- MPFB setup

def enable_mpfb():
    if ADDON not in bpy.context.preferences.addons:
        res = bpy.ops.preferences.addon_enable(module=ADDON)
        if res != {"FINISHED"}:
            raise RuntimeError("could not enable " + ADDON + ": " + str(res))
    prefs = bpy.context.preferences.addons[ADDON].preferences
    if not os.path.isdir(ASSET_ROOT):
        raise RuntimeError("system assets not found at " + ASSET_ROOT)
    prefs.mpfb_second_root = ASSET_ROOT
    mpfb = {}
    for mod, name in [("services.locationservice", "LocationService"),
                      ("services.assetservice", "AssetService"),
                      ("services.humanservice", "HumanService"),
                      ("services.targetservice", "TargetService"),
                      ("services.objectservice", "ObjectService"),
                      ("entities.objectproperties", "HumanObjectProperties")]:
        mpfb[name] = getattr(importlib.import_module(ADDON + "." + mod), name)
    mpfb["LocationService"].update_second_root()
    roots = mpfb["AssetService"].get_available_data_roots()
    if ASSET_ROOT not in roots:
        raise RuntimeError("MPFB did not pick up the asset root; roots=" + str(roots))
    mpfb["AssetService"].update_all_asset_lists()
    return mpfb


def find_asset(mpfb, filename, subdir):
    path = mpfb["AssetService"].find_asset_absolute_path(filename, asset_subdir=subdir)
    if not path or not os.path.isfile(path):
        raise FileNotFoundError("asset not found: %s/%s" % (subdir, filename))
    return path


# ----------------------------------------------------------------------------- body shape

def body_extent(obj):
    """(height, min_z) of the evaluated body (helper geometry masked out), world space."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    co = co.reshape(-1, 3) @ np.array(obj.matrix_world)[:3, :3].T + np.array(obj.matrix_world)[:3, 3]
    return float(co[:, 2].max() - co[:, 2].min()), float(co[:, 2].min())


def set_height_macro(mpfb, basemesh, value):
    mpfb["HumanObjectProperties"].set_value("height", value, entity_reference=basemesh)
    mpfb["TargetService"].reapply_macro_details(basemesh)
    return body_extent(basemesh)[0]


def fit_height(mpfb, basemesh, target_m):
    """Bisect the MakeHuman height macro towards target_m, then correct the rest with a uniform scale.

    The height response is not continuous: MPFB drops macro targets whose combined weight is
    below 1% (TargetService.calculate_target_stack_from_macro_info_dict cutoff=0.01), so a
    target popping in or out makes the body jump by several millimetres. Bisection therefore
    only gets close; the remaining error (and anything outside the macro's range) is taken up
    by scaling the whole base mesh before the rig and assets are fitted to it.
    """
    lo, hi = 0.0, 1.0
    h_lo, h_hi = set_height_macro(mpfb, basemesh, lo), set_height_macro(mpfb, basemesh, hi)
    info = {"macro_range_m": [round(h_lo, 4), round(h_hi, 4)]}
    best = min([(abs(h_lo - target_m), lo, h_lo), (abs(h_hi - target_m), hi, h_hi)])
    if h_lo < target_m < h_hi:
        for _ in range(30):
            mid = 0.5 * (lo + hi)
            h = set_height_macro(mpfb, basemesh, mid)
            best = min(best, (abs(h - target_m), mid, h))
            if abs(h - target_m) < 0.0002:
                break
            if h < target_m:
                lo = mid
            else:
                hi = mid
    _, macro, h = best
    h = set_height_macro(mpfb, basemesh, macro)
    info["height_macro"] = round(macro, 6)
    info["height_before_scale_m"] = round(h, 5)
    info["uniform_scale"] = round(target_m / h, 6)
    if abs(target_m / h - 1.0) > 1e-5:
        s = target_m / h
        basemesh.scale = (s, s, s)
        bpy.ops.object.select_all(action="DESELECT")
        basemesh.select_set(True)
        bpy.context.view_layer.objects.active = basemesh
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # feet on ground again (same as MPFB's deserialize does after changing targets)
    h, zmin = body_extent(basemesh)
    basemesh.location.z -= zmin
    bpy.ops.object.select_all(action="DESELECT")
    basemesh.select_set(True)
    bpy.context.view_layer.objects.active = basemesh
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    info["basemesh_height_m"] = round(body_extent(basemesh)[0], 4)
    return info


# ----------------------------------------------------------------------------- build

def build(args, mpfb):
    HS, TS = mpfb["HumanService"], mpfb["TargetService"]
    t = time.time()
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.cameras,
                 bpy.data.lights):
        for idb in list(coll):
            coll.remove(idb)

    afr, asi, cau = [float(x) for x in args.race.split(",")]
    macro = TS.get_default_macro_info_dict()
    macro.update(gender=args.gender, age=args.age, muscle=args.muscle, weight=args.weight,
                 proportions=args.proportions, cupsize=args.cupsize, firmness=args.firmness)
    macro["race"] = {"african": afr, "asian": asi, "caucasian": cau}
    basemesh = HS.create_human(macro_detail_dict=macro)
    t = step("create_human", t)

    height_info = fit_height(mpfb, basemesh, args.height_m)
    log("height", height_info)
    t = step("fit_height", t)

    rig = HS.add_builtin_rig(basemesh, args.rig)
    if rig is None:
        raise RuntimeError("rig not created: " + args.rig)
    t = step("add_rig_and_weights", t)

    parts = {}
    eyes_path = find_asset(mpfb, args.eyes + ".mhclo", "eyes")
    parts["Eyes"] = (eyes_path, "Eyes")
    if args.eyebrows != "none":
        parts["Eyebrows"] = (find_asset(mpfb, args.eyebrows + ".mhclo", "eyebrows"), "Eyebrows")
    if args.eyelashes != "none":
        parts["Eyelashes"] = (find_asset(mpfb, args.eyelashes + ".mhclo", "eyelashes"), "Eyelashes")
    if args.hair != "none":
        parts["Hair"] = (find_asset(mpfb, args.hair + ".mhclo", "hair"), "Hair")
    part_objs = {}
    for key, (path, atype) in parts.items():
        obj = HS.add_mhclo_asset(path, basemesh, asset_type=atype, subdiv_levels=0,
                                 material_type="GAMEENGINE")
        part_objs[key] = obj
    t = step("add_bodyparts", t)

    # swap the eye material to the requested colour (MPFB loads the mhclo's default brown)
    if args.eye_material != "brown":
        eye_mhmat = find_asset(mpfb, os.path.join("materials", args.eye_material + ".mhmat"), "eyes")
        MS = importlib.import_module(ADDON + ".services.materialservice").MaterialService
        NW = importlib.import_module(ADDON + ".entities.nodemodel.v2.materials.nodewrappergameengine").NodeWrapperGameEngine
        MH = importlib.import_module(ADDON + ".entities.material.mhmaterial").MhMaterial
        eyes = part_objs["Eyes"]
        MS.delete_all_materials(eyes)
        mat = MS.create_empty_material(eyes.name, eyes)
        mh = MH()
        mh.populate_from_mhmat(eye_mhmat)
        NW.create_instance(mat.node_tree, mhmat=mh)

    proxy = None
    if args.topology != "base":
        HS._check_add_proxy({"proxy": args.topology + ".proxy", "name": ""}, basemesh,
                            subdiv_levels=0)
        proxy = mpfb["ObjectService"].find_object_of_type_amongst_nearest_relatives(
            basemesh, "Proxymeshes")
        if proxy is None:
            raise RuntimeError("proxy failed to load: " + args.topology)
    t = step("add_proxy", t)

    skin = find_asset(mpfb, args.skin + ".mhmat", "skins")
    HS.set_character_skin(skin, basemesh, skin_type="GAMEENGINE", material_instances=False)
    # The GAMEENGINE wrapper also wires the diffuse texture's alpha into Alpha. Skin textures are
    # RGB (alpha is always 1), but the link makes the FBX carry a transparency map, which Unreal
    # turns into a translucent material. Keep the skin opaque.
    for ob in [basemesh] + ([proxy] if proxy else []):
        for slot in ob.material_slots:
            nt = slot.material.node_tree if slot.material else None
            if nt and "AlphaMapTexture" in nt.nodes:
                nt.nodes.remove(nt.nodes["AlphaMapTexture"])
    t = step("set_skin", t)
    return basemesh, rig, proxy, part_objs, height_info, {"skin": skin, "eyes": eyes_path,
                                                         **{k: v[0] for k, v in parts.items()}}


# ----------------------------------------------------------------------------- export prep

def with_active(obj):
    return bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj],
                                     selected_editable_objects=[obj])


def prepare_for_export(args, name, basemesh, rig, proxy, part_objs):
    """Bake the character down to: Armature + Body + parts, deform-only weights."""
    TS = importlib.import_module(ADDON + ".services.targetservice").TargetService
    if proxy is not None:
        body = proxy
        bpy.data.objects.remove(basemesh, do_unlink=True)
    else:
        body = basemesh
        TS.bake_targets(body)
        for m in list(body.modifiers):
            if m.type == "MASK":
                with with_active(body):
                    bpy.ops.object.modifier_apply(modifier=m.name)
    meshes = {"Body": body, **part_objs}
    for key, ob in meshes.items():
        for m in list(ob.modifiers):
            if m.type != "ARMATURE":
                ob.modifiers.remove(m)
        if ob.data.shape_keys:
            TS.bake_targets(ob)
        arm_mods = [m for m in ob.modifiers if m.type == "ARMATURE"]
        if not arm_mods or arm_mods[0].object is not rig or ob.parent is not rig:
            raise RuntimeError("%s is not skinned to the rig" % key)

    # names: UE drops a Blender armature node called "Armature"; skeleton starts at "root"
    rig.name = "Armature"
    rig.data.name = name + "_Skeleton"
    if "Root" in rig.data.bones:
        rig.data.bones["Root"].name = "root"
    bone_names = {b.name for b in rig.data.bones if b.use_deform}
    bone_names.add("root")
    for key, ob in meshes.items():
        ob.name = ob.data.name = "%s_%s" % (name, key)
        for vg in list(ob.vertex_groups):
            if vg.name not in bone_names:
                ob.vertex_groups.remove(vg)
        with with_active(ob):
            bpy.ops.object.vertex_group_clean(group_select_mode="BONE_DEFORM", limit=0.001)
            bpy.ops.object.vertex_group_limit_total(group_select_mode="BONE_DEFORM",
                                                    limit=args.max_influences)
            bpy.ops.object.vertex_group_normalize_all(group_select_mode="BONE_DEFORM",
                                                      lock_active=False)
        for i, slot in enumerate(ob.material_slots):
            if slot.material:
                slot.material.name = "M_%s_%s%s" % (name, key, "" if i == 0 else "_%d" % i)
    return meshes


def scale_to_centimetres(rig, meshes):
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    rig.scale = (100.0, 100.0, 100.0)
    objs = [rig] + list(meshes.values())
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for ob in objs:
        if any(abs(s - 1.0) > 1e-6 for s in ob.scale):
            raise RuntimeError("scale not applied on " + ob.name)


def export_fbx(path):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=False,
        object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True,          # with unit scale 0.01 -> factor 1.0, file unit = cm
        apply_scale_options="FBX_SCALE_NONE",
        global_scale=1.0,
        axis_forward="Y", axis_up="Z",  # identity: Blender native Z-up, character faces -Y
        bake_space_transform=False,
        use_mesh_modifiers=True,        # armature modifier is never applied by the exporter
        mesh_smooth_type="FACE",        # smoothing groups for Unreal
        use_tspace=True,
        use_triangles=False,
        use_custom_props=False,
        add_leaf_bones=False,
        primary_bone_axis="Y", secondary_bone_axis="X",
        use_armature_deform_only=True,
        armature_nodetype="NULL",
        bake_anim=False,
        path_mode="COPY", embed_textures=True,
        colors_type="NONE",
    )


# ----------------------------------------------------------------------------- report

def mesh_stats(ob):
    me = ob.data
    nsides = np.empty(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("loop_total", nsides)
    groups = {vg.index: vg.name for vg in ob.vertex_groups}
    infl = np.array([sum(1 for g in v.groups if g.weight > 0 and g.group in groups)
                     for v in me.vertices])
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ np.array(ob.matrix_world)[:3, :3].T + np.array(ob.matrix_world)[:3, 3]
    return {
        "vertices": len(me.vertices),
        "faces": len(me.polygons),
        "triangles": int((nsides - 2).sum()),
        "quads": int((nsides == 4).sum()),
        "tris_in_source": int((nsides == 3).sum()),
        "ngons": int((nsides > 4).sum()),
        "uv_layers": [uv.name for uv in me.uv_layers],
        "materials": [s.material.name if s.material else None for s in ob.material_slots],
        "vertex_groups": len(ob.vertex_groups),
        "max_influences": int(infl.max()) if len(infl) else 0,
        "mean_influences": round(float(infl.mean()), 2) if len(infl) else 0,
        "unweighted_vertices": int((infl == 0).sum()),
        "bbox_min": [round(float(x), 3) for x in co.min(0)],
        "bbox_max": [round(float(x), 3) for x in co.max(0)],
    }


def material_stats():
    out = {}
    for mat in bpy.data.materials:
        if not mat.users or not mat.node_tree:
            continue
        imgs = []
        for n in mat.node_tree.nodes:
            if n.type == "TEX_IMAGE" and n.image:
                links = [l.to_socket.name for o in n.outputs for l in o.links]
                imgs.append({"node": n.name, "image": os.path.basename(n.image.filepath),
                             "size": list(n.image.size), "feeds": links})
        out[mat.name] = imgs
    return out


def bone_stats(rig):
    def walk(b):
        return {"name": b.name, "deform": b.use_deform,
                "head_cm": [round(x, 2) for x in b.head_local],
                "tail_cm": [round(x, 2) for x in b.tail_local],
                "children": [walk(c) for c in b.children]}
    roots = [b for b in rig.data.bones if b.parent is None]
    return {"count": len(rig.data.bones), "hierarchy": [walk(r) for r in roots]}


def main():
    args = parse_args()
    out = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    name = args.name or os.path.basename(out)
    t = time.time()
    mpfb = enable_mpfb()
    t = step("enable_mpfb", t)

    basemesh, rig, proxy, parts, height_info, sources = build(args, mpfb)
    basemesh.name = name + ".body"

    t = time.time()
    bpy.context.preferences.filepaths.save_version = 0   # no .blend1 backups (session only)
    bpy.ops.wm.save_as_mainfile(filepath=out + ".blend", compress=True)
    t = step("save_blend", t)

    meshes = prepare_for_export(args, name, basemesh, rig, proxy, parts)
    scale_to_centimetres(rig, meshes)
    t = step("prepare_export", t)
    if not args.no_fbx:
        export_fbx(out + ".fbx")
        t = step("export_fbx", t)

    body = meshes["Body"]
    stats = {k: mesh_stats(ob) for k, ob in meshes.items()}
    report = {
        "args": vars(args),
        "blender": bpy.app.version_string,
        "mpfb_version": next((m.bl_info.get("version") for m in [importlib.import_module(ADDON)]
                              if hasattr(m, "bl_info")), None),
        "sources": {k: os.path.relpath(v, PROJECT) for k, v in sources.items()},
        "height": height_info,
        "final_body_height_cm": round(stats["Body"]["bbox_max"][2] - stats["Body"]["bbox_min"][2], 2),
        "meshes": stats,
        "totals": {"vertices": sum(s["vertices"] for s in stats.values()),
                   "triangles": sum(s["triangles"] for s in stats.values())},
        "materials": material_stats(),
        "skeleton": bone_stats(rig),
        "timings_s": TIMINGS,
        "script_wall_s": round(time.time() - T_START, 2),
        "peak_rss_mb": round(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024, 1),
        "outputs": [out + ".blend"] + ([] if args.no_fbx else [out + ".fbx"]),
    }
    with open(out + "_report.json", "w") as f:
        json.dump(report, f, indent=1)
    log("wrote", out + "_report.json")
    log("SUMMARY height_cm=%s tris=%s verts=%s wall=%ss rss=%sMB" % (
        report["final_body_height_cm"], report["totals"]["triangles"],
        report["totals"]["vertices"], report["script_wall_s"], report["peak_rss_mb"]))


main()
