"""MPFB body, shape targets, height fitting, rig and MakeHuman body parts for build_fighter.py."""
import importlib
import json
import os
import time

import bpy
import numpy as np

ADDON = "bl_ext.user_default.mpfb"
HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", ".."))
MH_ROOT = os.path.join(PROJECT, "SourceArt", "ThirdParty", "MakeHuman", "extracted",
                       "makehuman_system_assets")
ACG_ROOT = os.path.join(PROJECT, "SourceArt", "ThirdParty", "ambientCG")
T0 = time.time()


def log(*a):
    print("[fighter %6.2fs]" % (time.time() - T0), *a, flush=True)


def mpfb(mod, name):
    return getattr(importlib.import_module(ADDON + "." + mod), name)


def enable_mpfb():
    if ADDON not in bpy.context.preferences.addons:
        if bpy.ops.preferences.addon_enable(module=ADDON) != {"FINISHED"}:
            raise RuntimeError("could not enable " + ADDON)
    bpy.context.preferences.addons[ADDON].preferences.mpfb_second_root = MH_ROOT
    mpfb("services.locationservice", "LocationService").update_second_root()
    AS = mpfb("services.assetservice", "AssetService")
    if MH_ROOT not in AS.get_available_data_roots():
        raise RuntimeError("MPFB did not pick up " + MH_ROOT)
    AS.update_all_asset_lists()
    bpy.context.preferences.filepaths.save_version = 0


def find_asset(filename, subdir):
    p = mpfb("services.assetservice", "AssetService").find_asset_absolute_path(filename, asset_subdir=subdir)
    if not p or not os.path.isfile(p):
        raise FileNotFoundError("%s/%s" % (subdir, filename))
    return p


def clean_scene():
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.cameras,
                 bpy.data.lights, bpy.data.images, bpy.data.armatures):
        for idb in list(coll):
            coll.remove(idb)


def with_active(obj, extra=()):
    sel = [obj] + list(extra)
    return bpy.context.temp_override(object=obj, active_object=obj, selected_objects=sel,
                                     selected_editable_objects=sel)


# ----------------------------------------------------------------------------- shape

def body_extent(obj):
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    co = co.reshape(-1, 3) @ np.array(obj.matrix_world)[:3, :3].T + np.array(obj.matrix_world)[:3, 3]
    return float(co[:, 2].max() - co[:, 2].min()), float(co[:, 2].min())


_TARGET_INDEX = None


def target_index():
    global _TARGET_INDEX
    if _TARGET_INDEX is None:
        LS = mpfb("services.locationservice", "LocationService")
        with open(LS.get_mpfb_data("targets/target.json")) as f:
            tj = json.load(f)
        _TARGET_INDEX = {}
        for cat, block in tj.items():
            for c in block["categories"]:
                _TARGET_INDEX[cat + "/" + c["name"]] = c
    return _TARGET_INDEX


def apply_targets(basemesh, targets):
    """Load region modifiers ('category/modifier': value) as MPFB target shape keys."""
    TS = mpfb("services.targetservice", "TargetService")
    LS = mpfb("services.locationservice", "LocationService")
    idx = target_index()
    applied = []
    for key, value in targets.items():
        if key not in idx:
            near = [k for k in idx if k.split("/")[0] == key.split("/")[0]]
            raise KeyError("unknown modifier %s; that category has: %s" % (key, near))
        c = idx[key]
        op = c.get("opposites", {})
        sign = "positive" if value >= 0 else "negative"
        names = [op.get(sign + "-left"), op.get(sign + "-right")] if c["has_left_and_right"] \
            else [op.get(sign + "-unsided")]
        names = [n for n in names if n]
        if not names:   # single-target modifier (e.g. head-square)
            names = c["targets"][:1] if value >= 0 else []
        cat = key.split("/")[0]
        for n in names:
            path = LS.get_mpfb_data("targets/%s/%s.target.gz" % (cat, n))
            sk = TS.load_target(basemesh, path, weight=0.0, name=n)
            sk.slider_max = max(1.0, abs(value))
            sk.value = abs(value)
            applied.append((n, abs(value)))
    return applied


def set_height_macro(basemesh, value):
    HOP = mpfb("entities.objectproperties", "HumanObjectProperties")
    TS = mpfb("services.targetservice", "TargetService")
    HOP.set_value("height", value, entity_reference=basemesh)
    TS.reapply_macro_details(basemesh)
    return body_extent(basemesh)[0]


def fit_height(basemesh, target_m):
    """Bisect the height macro, then take up the rest (MPFB's 1% target cutoff makes the
    response jumpy, and the macro range is limited) with a uniform scale of the base mesh."""
    lo, hi = 0.0, 1.0
    h_lo, h_hi = set_height_macro(basemesh, lo), set_height_macro(basemesh, hi)
    best = min([(abs(h_lo - target_m), lo, h_lo), (abs(h_hi - target_m), hi, h_hi)])
    if h_lo < target_m < h_hi:
        for _ in range(30):
            mid = 0.5 * (lo + hi)
            h = set_height_macro(basemesh, mid)
            best = min(best, (abs(h - target_m), mid, h))
            if abs(h - target_m) < 0.0002:
                break
            lo, hi = (mid, hi) if h < target_m else (lo, mid)
    _, macro, _ = best
    h = set_height_macro(basemesh, macro)
    info = {"macro_range_m": [round(h_lo, 4), round(h_hi, 4)], "height_macro": round(macro, 6),
            "height_before_scale_m": round(h, 5), "uniform_scale": round(target_m / h, 6)}
    s = target_m / h
    bpy.ops.object.select_all(action="DESELECT")
    basemesh.select_set(True)
    bpy.context.view_layer.objects.active = basemesh
    if abs(s - 1) > 1e-5:
        basemesh.scale = (s, s, s)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    _, zmin = body_extent(basemesh)
    basemesh.location.z -= zmin
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    info["body_height_m"] = round(body_extent(basemesh)[0], 4)
    return info


def build_body(cfg):
    """MPFB human with macros + region targets at the body height, rig, eyes/brows/lashes/hair."""
    HS = mpfb("services.humanservice", "HumanService")
    TS = mpfb("services.targetservice", "TargetService")
    OS = mpfb("services.objectservice", "ObjectService")
    clean_scene()
    m = cfg["macros"]
    macro = TS.get_default_macro_info_dict()
    for k in ("gender", "age", "muscle", "weight", "proportions", "cupsize", "firmness"):
        if k in m:
            macro[k] = m[k]
    macro["race"] = dict(m.get("race", macro["race"]))
    basemesh = HS.create_human(macro_detail_dict=macro)
    applied = apply_targets(basemesh, cfg.get("targets", {}))
    body_h = cfg["height_m"] - cfg.get("sole_m", 0.0)
    hinfo = fit_height(basemesh, body_h)
    hinfo["targets_applied"] = len(applied)
    rig = HS.add_builtin_rig(basemesh, "game_engine")
    parts = {}

    def add(key, filename, subdir, atype):
        parts[key] = HS.add_mhclo_asset(find_asset(filename, subdir), basemesh, asset_type=atype,
                                        subdiv_levels=0, material_type="NONE")

    add("eyes", cfg["eyes"]["mesh"] + ".mhclo", "eyes", "Eyes")
    if cfg.get("brows"):
        add("brows", cfg["brows"]["asset"] + ".mhclo", "eyebrows", "Eyebrows")
    if cfg.get("lashes"):
        add("lashes", cfg["lashes"]["asset"] + ".mhclo", "eyelashes", "Eyelashes")
    if cfg.get("hair"):
        add("hair", cfg["hair"]["asset"] + ".mhclo", "hair", "Hair")
    return basemesh, rig, parts, hinfo


def bake_body(basemesh, rig, parts, fid):
    """Bake shape keys and helper masks; leave Body + parts skinned to the rig with deform-only
    vertex groups; rename to the export conventions (Armature / root)."""
    TS = mpfb("services.targetservice", "TargetService")
    TS.bake_targets(basemesh)
    for mod in list(basemesh.modifiers):
        if mod.type == "MASK":
            with with_active(basemesh):
                bpy.ops.object.modifier_apply(modifier=mod.name)
    meshes = {"body": basemesh, **parts}
    rig.name = "Armature"
    rig.data.name = fid + "_Skeleton"
    if "Root" in rig.data.bones:
        rig.data.bones["Root"].name = "root"
    deform = {b.name for b in rig.data.bones if b.use_deform} | {"root"}
    for key, ob in meshes.items():
        for mod in list(ob.modifiers):
            if mod.type != "ARMATURE":
                ob.modifiers.remove(mod)
        if ob.data.shape_keys:
            TS.bake_targets(ob)
        for vg in list(ob.vertex_groups):
            if vg.name not in deform:
                ob.vertex_groups.remove(vg)
        ob.data.materials.clear()
        ob.name = ob.data.name = "%s_%s" % (fid, key)
    return meshes


def landmarks(rig):
    """Bone heads/tails (rest, world space = armature space at the origin) as numpy arrays."""
    out = {}
    M = np.array(rig.matrix_world)
    for b in rig.data.bones:
        h = M[:3, :3] @ np.array(b.head_local) + M[:3, 3]
        t = M[:3, :3] @ np.array(b.tail_local) + M[:3, 3]
        out[b.name] = (h, t)
    return out
