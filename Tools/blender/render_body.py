"""Render an exported body FBX headless for review (re-imports the FBX, so what is rendered is
what the FBX contains: mesh, UVs, embedded textures, skeleton and skin weights).

    Tools/blender/run_blender.sh Tools/blender/render_body.py -- \
        --fbx SourceArt/Bodies/athletic_woman.fbx --out SourceArt/Bodies/athletic_woman

Writes <out>_front.png, <out>_side.png (rest pose, orthographic), <out>_bent.png (posed,
3/4 perspective), <out>_bent_closeups.png (2x2, all on the body's left side: shoulder,
inside of the elbow, knee, hip/buttock from behind),
<out>_face.png (eyes/brows/lashes), <out>_wire.png (wireframe over the upper body) and
<out>_uv.png (body UV layout over its diffuse texture).
"""
import argparse
import math
import os
import sys
import time

import bpy
import numpy as np
from mathutils import Matrix, Vector

T0 = time.time()


def log(*a):
    print("[render_body %6.2fs]" % (time.time() - T0), *a, flush=True)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--fbx", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--res", type=int, default=1024)
    p.add_argument("--samples", type=int, default=32)
    return p.parse_args(argv)


def clean_scene():
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.cameras,
                 bpy.data.lights, bpy.data.images):
        for idb in list(coll):
            coll.remove(idb)


def setup_scene(res, samples):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.eevee.taa_render_samples = samples
    sc.render.resolution_x = sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = "PNG"
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("World") if not sc.world else sc.world
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.32, 0.33, 0.35, 1.0)
    bg.inputs["Strength"].default_value = 0.9
    key = bpy.data.objects.new("Key", bpy.data.lights.new("Key", "SUN"))
    key.data.energy = 3.2
    key.data.angle = math.radians(8)
    key.rotation_euler = (math.radians(50), 0, math.radians(-35))
    fill = bpy.data.objects.new("Fill", bpy.data.lights.new("Fill", "SUN"))
    fill.data.energy = 1.0
    fill.rotation_euler = (math.radians(70), 0, math.radians(140))
    for ob in (key, fill):
        sc.collection.objects.link(ob)
    floor_me = bpy.data.meshes.new("Floor")
    floor_me.from_pydata([(-5, -5, 0), (5, -5, 0), (5, 5, 0), (-5, 5, 0)], [], [(0, 1, 2, 3)])
    floor = bpy.data.objects.new("Floor", floor_me)
    mat = bpy.data.materials.new("FloorMat")
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.42, 0.42, 0.42, 1)
    mat.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.9
    floor_me.materials.append(mat)
    sc.collection.objects.link(floor)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.clip_start = 0.01
    cam.data.clip_end = 100
    return cam


def look_at(cam, eye, target):
    cam.location = Vector(eye)
    cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    log("wrote", path)


def world_bbox(objs):
    pts = []
    dg = bpy.context.evaluated_depsgraph_get()
    for ob in objs:
        ev = ob.evaluated_get(dg)
        me = ev.to_mesh()
        co = np.empty(len(me.vertices) * 3)
        me.vertices.foreach_get("co", co)
        ev.to_mesh_clear()
        M = np.array(ob.matrix_world)
        pts.append(co.reshape(-1, 3) @ M[:3, :3].T + M[:3, 3])
    pts = np.concatenate(pts)
    return pts.min(0), pts.max(0)


# ----------------------------------------------------------------------------- posing

def rotate_bone(arm, name, axis_world, angle_deg, want_world):
    """Rotate pose bone `name` about a world axis through its head; the sign is chosen so the
    bone's direction moves towards want_world (i.e. the anatomically intended side)."""
    bpy.context.view_layer.update()
    pb = arm.pose.bones[name]
    to_arm = arm.matrix_world.to_3x3().normalized().inverted()
    axis = (to_arm @ Vector(axis_world)).normalized()
    want = (to_arm @ Vector(want_world)).normalized()
    d = (pb.tail - pb.head).normalized()
    best = None
    for sgn in (1, -1):
        R = Matrix.Rotation(math.radians(sgn * angle_deg), 4, axis)
        score = (R.to_3x3() @ d).dot(want)
        if best is None or score > best[0]:
            best = (score, R)
    head = pb.head.copy()
    pb.matrix = Matrix.Translation(head) @ best[1] @ Matrix.Translation(-head) @ pb.matrix
    bpy.context.view_layer.update()
    return best[1].to_3x3()


def world_dir(arm, name):
    pb = arm.pose.bones[name]
    return (arm.matrix_world.to_3x3() @ (pb.tail - pb.head)).normalized()


def bend(arm):
    """Fighting-style stress pose: guard arm, raised arm, high knee, wide stance."""
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    # left leg: hip flexion 90 (knee up), knee flexion 95 (hinge = lateral X axis)
    rotate_bone(arm, "thigh_l", X, 90, -Y)
    rotate_bone(arm, "calf_l", X, 95, -Z)
    # right leg: hip abduction 40 (about the front-back Y axis), knee flexion 80 about the
    # abducted hinge, heel going back
    R = rotate_bone(arm, "thigh_r", Y, 40, -X)
    rotate_bone(arm, "calf_r", R @ X, 80, Y)
    # left arm: shoulder abduction 65 from the A-pose (upper arm about horizontal), elbow
    # flexion 90 with the forearm pointing forward
    hinge_l = world_dir(arm, "upperarm_l").cross(Y).normalized()
    R = rotate_bone(arm, "upperarm_l", Y, 65, Z)
    rotate_bone(arm, "lowerarm_l", R @ hinge_l, 90, -Y)
    # right arm: shoulder flexion 85 (arm forward), elbow flexion 120 (fist towards the chin)
    hinge_r = world_dir(arm, "upperarm_r").cross(Y).normalized()
    rotate_bone(arm, "upperarm_r", hinge_r, 85, -Y)
    rotate_bone(arm, "lowerarm_r", hinge_r, 120, Z)


def bone_world_head(arm, name):
    return arm.matrix_world @ arm.pose.bones[name].head


# ----------------------------------------------------------------------------- extras

def composite(paths, out, res):
    tiles = []
    for p in paths:
        img = bpy.data.images.load(p)
        a = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)
        tiles.append(a)
        bpy.data.images.remove(img)
    # layout: top row = tiles 0, 1; bottom row = tiles 2, 3 (Blender pixel rows go bottom-up)
    top = np.concatenate([tiles[0], tiles[1]], axis=1)
    bottom = np.concatenate([tiles[2], tiles[3]], axis=1)
    full = np.concatenate([bottom, top], axis=0)
    img = bpy.data.images.new("comp", full.shape[1], full.shape[0], alpha=True)
    img.pixels = full.ravel()
    img.filepath_raw = out
    img.file_format = "PNG"
    img.save()
    for p in paths:
        os.remove(p)
    log("wrote", out)


def add_wire_overlay(body):
    wire = body.copy()
    wire.data = body.data.copy()
    wire.name = "WireOverlay"
    bpy.context.scene.collection.objects.link(wire)
    mod = wire.modifiers.new("Wire", "WIREFRAME")
    mod.thickness = 0.0011 / body.matrix_world.to_scale()[0]   # ~1.1 mm in world units
    mod.use_replace = True
    mod.use_even_offset = False
    mat = bpy.data.materials.new("WireMat")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (0.02, 0.02, 0.03, 1)
    bsdf.inputs["Roughness"].default_value = 1.0
    wire.data.materials.clear()
    wire.data.materials.append(mat)
    return wire


def export_uv_layout(body, path, res):
    """UV wireframe of the body drawn over its diffuse texture."""
    import gpu
    if bpy.app.background and hasattr(gpu, "init"):
        gpu.init()  # the PNG UV exporter draws with the gpu module
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    tmp = path + ".lines.png"
    bpy.ops.uv.export_layout(filepath=tmp, export_all=True, mode="PNG", size=(res, res),
                             opacity=0.0)
    bpy.ops.object.mode_set(mode="OBJECT")
    lines = bpy.data.images.load(tmp)
    la = np.array(lines.pixels[:], dtype=np.float32).reshape(res, res, 4)
    bpy.data.images.remove(lines)
    os.remove(tmp)
    tex = next((n.image for s in body.material_slots if s.material
                for n in s.material.node_tree.nodes if n.type == "TEX_IMAGE" and n.image), None)
    if tex is not None:
        t = tex.copy()
        t.scale(res, res)
        base = np.array(t.pixels[:], dtype=np.float32).reshape(res, res, 4)
        bpy.data.images.remove(t)
    else:
        base = np.full((res, res, 4), 0.5, dtype=np.float32)
    a = la[..., 3:4]
    out = base.copy()
    out[..., :3] = base[..., :3] * (1 - a) + np.array([0.0, 0.15, 0.6]) * a
    out[..., 3] = 1.0
    img = bpy.data.images.new("uvcomp", res, res, alpha=True)
    img.pixels = out.ravel()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)
    log("wrote", path)


# ----------------------------------------------------------------------------- main

def main():
    args = parse_args()
    out = os.path.abspath(args.out)
    clean_scene()
    cam = setup_scene(args.res, args.samples)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(args.fbx))
    arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name != "Floor"]
    body = next(o for o in meshes if o.name.endswith("_Body"))
    log("imported", arm.name, "bones=%d" % len(arm.data.bones), "root=%s" %
        [b.name for b in arm.data.bones if b.parent is None], "meshes=%s" % [m.name for m in meshes],
        "arm scale=%s" % (tuple(round(s, 4) for s in arm.matrix_world.to_scale()),))
    for m in meshes:
        log("  material", m.name, [(s.material.name, [n.image.name for n in s.material.node_tree.nodes
                                     if n.type == "TEX_IMAGE" and n.image]) for s in m.material_slots])
    lo, hi = world_bbox([body])
    h = hi[2] - lo[2]
    log("imported body height (m) = %.4f" % h, "bbox", lo.round(3), hi.round(3))

    export_uv_layout(body, out + "_uv.png", args.res)

    cam.data.type = "ORTHO"
    cam.data.ortho_scale = h * 1.08
    mid = h * 0.5
    look_at(cam, (0, -10, mid), (0, 0, mid))
    render(out + "_front.png")
    look_at(cam, (10, 0, mid), (0, 0, mid))
    render(out + "_side.png")

    # face close-up (eyes, eyebrows, eyelashes), slightly from the left
    cam.data.ortho_scale = h * 0.085
    hb = bone_world_head(arm, "head")
    c = Vector((0, hb.y, lo[2] + h * 0.935))
    look_at(cam, c + Vector((0.35, -1, 0.05)).normalized() * 10, c)
    render(out + "_face.png")

    # wireframe over upper body (front 3/4)
    wire = add_wire_overlay(body)
    cam.data.ortho_scale = h * 0.42
    c = Vector((0, 0, h * 0.80))
    look_at(cam, c + Vector((4, -9, 0.6)).normalized() * 10, c)
    render(out + "_wire.png")
    bpy.data.objects.remove(wire)

    bend(arm)
    lo, hi = world_bbox(meshes)
    center = (lo + hi) / 2
    size = float(max(hi - lo))
    cam.data.type = "PERSP"
    cam.data.lens = 50
    dist = size * 2.1
    d = Vector((math.sin(math.radians(35)), -math.cos(math.radians(35)), 0.18)).normalized()
    look_at(cam, Vector(center) + d * dist, center)
    render(out + "_bent.png")

    # close-ups of the stressed joints, 2x2 at half resolution
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = args.res // 2
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = h * 0.24
    shots = [
        ("upperarm_l", Vector((0.55, -0.75, 0.35))),   # shoulder, front 3/4 from above
        ("lowerarm_l", Vector((-0.5, -0.7, 0.5))),     # inside of the elbow, front-medial
        ("calf_l", Vector((1.0, -0.15, 0.1))),         # knee, from the side
        ("thigh_l", Vector((0.55, 0.8, -0.1))),        # hip / buttock, from behind
    ]
    tiles = []
    for i, (bone, view) in enumerate(shots):
        target = bone_world_head(arm, bone)
        look_at(cam, target + view.normalized() * 10, target)
        p = out + "_tile%d.png" % i
        render(p)
        tiles.append(p)
    composite(tiles, out + "_bent_closeups.png", args.res)
    log("total", round(time.time() - T0, 1), "s")


main()
