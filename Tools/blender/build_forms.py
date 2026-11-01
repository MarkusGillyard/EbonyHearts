"""What Hollis becomes and summons in a fight, from his sheet: the Ebony Heart Greatsword, the anchor, the
shark and the eel. Static meshes the game shows at his hand or in his place (`form` in a move), all in
the Fragment's skin, because the power is its own. Writes SourceArt/Forms/form_<name>.fbx.

    Tools/blender/run_blender.sh Tools/blender/build_forms.py
"""
import math
import os
import sys

import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_fragment as bf      # noqa: E402
import fighter_core as core      # noqa: E402
import fighter_geo as geo        # noqa: E402

OUT = os.path.join(core.PROJECT, "SourceArt", "Forms")


def prism(name, rings):
    """A closed skin through rings of points (each ring the same length)."""
    verts, faces = [], []
    n = len(rings[0])
    for r in rings:
        verts += [tuple(p) for p in r]
    for i in range(len(rings) - 1):
        for k in range(n):
            k2 = (k + 1) % n
            faces.append((i * n + k, i * n + k2, (i + 1) * n + k2, (i + 1) * n + k))
    faces.append(tuple(range(n - 1, -1, -1)))
    faces.append(tuple((len(rings) - 1) * n + k for k in range(n)))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def diamond(z, w, t):
    return [(w, 0, z), (0, t, z), (-w, 0, z), (0, -t, z)]


def box(name, c, size):
    x, y, z = c
    a, b, h = size[0] / 2, size[1] / 2, size[2] / 2
    return prism(name, [[(x - a, y - b, z - h), (x + a, y - b, z - h), (x + a, y + b, z - h), (x - a, y + b, z - h)],
                        [(x - a, y - b, z + h), (x + a, y - b, z + h), (x + a, y + b, z + h), (x - a, y + b, z + h)]])


def greatsword():
    """Grip at the origin, the blade up +Z: as long as a tall man, broad and straight, a hollow at the guard."""
    return [prism("blade", [diamond(0.2, 0.11, 0.022), diamond(0.26, 0.085, 0.02), diamond(0.42, 0.1, 0.024), diamond(1.62, 0.085, 0.02),
                            diamond(1.86, 0.03, 0.01), diamond(1.92, 0.002, 0.002)]),
            box("guard", (0, 0, 0.18), (0.52, 0.07, 0.05)), bf.tube("grip", [(0, 0, -0.26), (0, 0, 0.18)], [0.024, 0.026], 10),
            prism("pommel", [diamond(-0.31, 0.03, 0.03), diamond(-0.27, 0.05, 0.05), diamond(-0.23, 0.03, 0.03)])]


def anchor():
    """A diver's anchor, shank up +Z from the grip, crown at the far end."""
    return [bf.tube("shank", [(0, 0, -0.1), (0, 0, 1.5)], [0.05, 0.07], 12),
            bf.tube("crown", bf.smooth_path([(-0.6, 0, 1.15), (-0.45, 0, 1.45), (0, 0, 1.6), (0.45, 0, 1.45), (0.6, 0, 1.15)], 30), [0.06] * 30, 10),
            box("stock", (0, 0, 0.0), (0.6, 0.08, 0.08))]


def fin(name, pts, t=0.03):
    n = np.array((0, 1, 0)) * t
    return prism(name, [[tuple(np.array(p) - n) for p in pts], [tuple(np.array(p) + n) for p in pts]])


def shark():
    """Nose along +X, a thick body, tall fins: read at a glance from the side."""
    path = [(1.5 - 3.0 * i / 59, 0, 0.1 * math.sin(i / 59 * math.pi)) for i in range(60)]
    r = [0.03 + 0.36 * math.sin(min((i / 59) * 1.15, 1.0) * math.pi) ** 0.8 for i in range(60)]
    return [bf.tube("body", path, r, 22), fin("dorsal", [(0.25, 0, 0.3), (-0.5, 0, 0.3), (-0.5, 0, 0.98), (-0.3, 0, 0.72)]),
            fin("tail", [(-1.35, 0, 0.1), (-1.95, 0, 0.82), (-1.65, 0, 0.1), (-1.9, 0, -0.5)]),
            fin("pectoral", [(0.45, 0.3, -0.2), (-0.1, 0.3, -0.2), (-0.35, 0.3, -0.72)])]


def eel():
    """Long and low, nose along +X, snaking."""
    path = [(1.9 - 3.8 * i / 89, 0.3 * math.sin(i / 89 * math.tau * 1.5), 0.2 * math.sin(i / 89 * math.tau * 1.5 + 1.0)) for i in range(90)]
    r = [0.05 + 0.15 * math.sin(min(i / 89 * 1.05, 1.0) * math.pi) ** 0.6 for i in range(90)]
    return [bf.tube("body", path, r, 14)]


def katana_curve(z):
    return 0.05 * max(0.0, z / 0.8) ** 2


def code_sea():
    """Nix's Code Sea: a jet-black katana, grip at the origin, the blade up +Z, curving a little, a line of
    lime-green water along it (its own material, from the two maps blade_maps writes)."""
    rings = []
    for z in np.linspace(0.02, 0.8, 12):
        k = min(1.0, (0.82 - z) / 0.06)
        c = katana_curve(z)
        rings.append([(p[0] + c, p[1], p[2]) for p in diamond(z, 0.016 * k + 0.0005, 0.0045 * k + 0.0005)])
    return [prism("blade", rings), bf.tube("guard", [(0, 0, 0.0), (0, 0, 0.012)], [0.04, 0.04], 14),
            bf.tube("grip", [(0, 0, -0.26), (0, 0, 0.0)], [0.0155, 0.0155], 10)]


def blade_uv(ob):
    """u across the blade, centred on its ridge; everything off the blade at u = 0, which is plain black."""
    me = ob.data
    uv = me.uv_layers.new(name="UVMap")
    V = geo.co(ob)
    u = np.array([0.5 + (v[0] - katana_curve(v[2])) / 0.033 if 0.015 < v[2] else 0.0 for v in V])
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    uvs = np.stack([np.clip(u[lv], 0.0, 1.0), V[lv, 2] / 0.8], 1)
    uv.data.foreach_set("uv", uvs.ravel())


def blade_maps(name):
    """Jet black, and a lime-green line down the middle that swells and thins like water."""
    w, h = 32, 256
    for kind in ("basecolor", "emissive"):
        px = np.zeros((h, w, 4))
        px[..., 3] = 1.0
        if kind == "basecolor":
            px[..., :3] = 0.002
        else:
            half = (0.09 + 0.05 * np.sin(np.arange(h) / h * math.tau * 5.0))[:, None]
            band = np.abs((np.arange(w)[None, :] + 0.5) / w - 0.5) < half
            px[band] = (0.32, 0.8, 0.02, 1.0)
        img = bpy.data.images.new("%s_%s" % (name, kind), w, h, alpha=False)
        img.pixels = px.ravel()
        img.filepath_raw = os.path.join(OUT, "%s_%s.png" % (name, kind))
        img.file_format = "PNG"
        img.save()


def export(name, parts):
    core.clean_scene()
    return name, parts


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, make in (("greatsword", greatsword), ("anchor", anchor), ("shark", shark), ("eel", eel), ("code_sea", code_sea)):
        core.clean_scene()
        parts = make()
        with bpy.context.temp_override(active_object=parts[0], object=parts[0], selected_objects=parts, selected_editable_objects=parts):
            if len(parts) > 1:
                bpy.ops.object.join()
        ob = parts[0]
        ob.name = ob.data.name = "form_" + name
        geo.recalc_normals(ob)
        for p in ob.data.polygons:
            p.use_smooth = True
        if name == "code_sea":
            blade_uv(ob)
            blade_maps("form_code_sea")
        else:
            bf.spherical_uv(ob)
        ob.data.materials.append(bpy.data.materials.new("M_form"))
        sc = bpy.context.scene
        sc.unit_settings.system = "METRIC"
        sc.unit_settings.scale_length = 0.01
        ob.scale = (100.0, 100.0, 100.0)
        bpy.ops.object.select_all(action="DESELECT")
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "form_%s.fbx" % name), use_selection=True, object_types={"MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0, axis_forward="Y", axis_up="Z",
                                 bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE", use_tspace=True, path_mode="STRIP",
                                 embed_textures=False, colors_type="NONE", bake_anim=False)
        core.log("SUMMARY", name, len(ob.data.polygons))


main()
